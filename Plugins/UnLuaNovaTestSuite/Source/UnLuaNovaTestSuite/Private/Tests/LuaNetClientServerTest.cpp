// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

// The client/server half of Lua-declared networking.
//
// UnLuaNova.API.Net checks everything that can be checked without a network: the
// declarations are parsed, the UFunctions carry the right FUNC_Net* flags and typed
// parameters, they land in the class's function map, and they come off again on teardown.
// What it cannot check is the thing the feature exists for - that a call actually reaches
// the other peer, arrives as the same function there, and ends up in Lua on the right side.
// That needs a session with two peers, which is what this test is.
//
// It runs a listen server in PIE with one client, both in this process. That is the only
// shape a single automation test can reach: bLaunchSeparateServer is left off precisely so
// one process holds every peer and the test can hold pointers into all of them.
//
// Two things this shape does NOT settle, stated so they are not mistaken for covered:
//
//   - RPC ids across processes. An RPC travels as an id, and two processes that build the
//     same UFunction from the same module must land on the same id. In-process there is one
//     UClass, so there is trivially one id - the interesting case needs two processes, which
//     a single automation test cannot drive.
//   - Ownership rules. The test sets actor ownership directly on both peers so it can ask
//     about RPCs; how an actor comes to be owned is a networking question of its own and is
//     not what is under test here.

#include "TestCommands.h"
#include "UnLuaNovaTestCommon.h"
#include "UnLuaNovaTestHelpers.h"

#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "LuaEnv.h"
#include "LuaFunction.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaModule.h"
#include "UnLuaNovaNet.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"

namespace UnLuaNovaTestSuite
{
    // Distinct on purpose: a log line that says "it ran" tells you much less than one that
    // says which argument arrived where.
    constexpr int32 NetControlValue = 7;   // replicated by the engine, not by Lua
    constexpr int32 ServerRpcArg = 77;
    constexpr int32 ClientRpcArg = 88;
    constexpr int32 MulticastRpcArg = 99;

    // Per step, not per test: each step is a network round trip or a spawn, and a step that
    // hangs should say which one rather than fail the whole test at the end.
    constexpr double NetStepTimeoutSeconds = 20.0;

    /**
     * Read a plain Lua field off the table bound to Object.
     *
     * Not a UPROPERTY read: the whole point is to see the value the script sees, which is
     * what tells us the client's OnRep actually pushed the replicated value back into Lua
     * rather than the value merely arriving on the component.
     */
    static bool ReadLuaIntField(UObject* Object, const char* Name, int32& OutValue)
    {
        if (!Object)
            return false;

        IUnLuaNovaModule& Module = IUnLuaNovaModule::Get();
        if (!Module.IsActive())
            return false;

        UnLuaNova::FLuaEnv* Env = Module.GetEnv(Object);
        if (!Env)
            return false;

        lua_State* L = Env->GetMainState();
        const int32 Ref = Env->GetObjectRegistry()->GetBoundRef(Object);
        if (Ref == LUA_NOREF)
            return false;

        const int32 Top = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, Ref);
        lua_getfield(L, -1, Name);
        const bool bOk = lua_isnumber(L, -1) != 0;
        if (bOk)
            OutValue = static_cast<int32>(lua_tointeger(L, -1));
        lua_settop(L, Top);
        return bOk;
    }

    /**
     * Call an RPC the way a blueprint call would - ProcessEvent, which is the entry that
     * decides whether a FUNC_Net function is executed here or sent to the other peer.
     * Going through CallRemoteFunction instead would test our own idea of when to send
     * rather than the engine's, and the engine's is the one that has to agree on both ends.
     */
    static void InvokeRpc(AActor* Actor, const TCHAR* Name, int32 Arg)
    {
        if (!Actor)
            return;

        UFunction* Function = Actor->FindFunction(FName(Name));
        if (!Function)
            return;

        struct
        {
            int32 Value;
        } Params;
        Params.Value = Arg;
        Actor->ProcessEvent(Function, &Params);
    }

    class FLuaNetScenarioCommand : public IAutomationLatentCommand
    {
    public:
        explicit FLuaNetScenarioCommand(FAutomationTestBase& InTest)
            : Test(InTest)
        {
        }

        virtual bool Update() override;

    private:
        enum class EStep : uint8
        {
            WaitForPeers,
            SpawnOnServer,
            WaitForClientActor,
            AssignOwners,
            CallServerRpc,
            WaitServerRpc,
            CallClientRpc,
            WaitClientRpc,
            CallMulticast,
            WaitMulticast,
            WaitReplication,
            Assert,
            Done
        };

        /** Not ready yet - come back next frame, unless this step has run out of time. */
        bool Retry(const TCHAR* What);
        bool Next(EStep InStep);
        void Assert();

        FAutomationTestBase& Test;
        EStep Step = EStep::WaitForPeers;
        double Deadline = 0.0;
        TWeakObjectPtr<UWorld> ServerWorld;
        TWeakObjectPtr<UWorld> ClientWorld;
        TWeakObjectPtr<AUnLuaNovaTestNetActor> ServerActor;
        TWeakObjectPtr<AUnLuaNovaTestNetActor> ClientActor;
    };

    bool FLuaNetScenarioCommand::Next(EStep InStep)
    {
        Step = InStep;
        Deadline = 0.0;
        return false;
    }

    bool FLuaNetScenarioCommand::Retry(const TCHAR* What)
    {
        if (Deadline <= 0.0)
            Deadline = FPlatformTime::Seconds() + NetStepTimeoutSeconds;

        if (FPlatformTime::Seconds() <= Deadline)
            return false;

        Test.AddError(FString::Printf(TEXT("C/S 测试在「%s」这一步等了 %.0fs 仍未等到；后续断言不再执行。"),
            What, NetStepTimeoutSeconds));
        Step = EStep::Done;
        return true;
    }

    bool FLuaNetScenarioCommand::Update()
    {
        if (Step == EStep::Done)
            return true;

        // Everything below the spawn assumes both actors are alive. A destroyed actor here
        // means the session came apart, and dereferencing a stale pointer to find out would
        // turn a failed test into a crashed run.
        if (static_cast<uint8>(Step) >= static_cast<uint8>(EStep::AssignOwners)
            && (!ServerActor.IsValid() || !ClientActor.IsValid()))
        {
            Test.AddError(TEXT("C/S 测试过程中有一端的 Actor 被销毁了。"));
            Step = EStep::Done;
            return true;
        }

        switch (Step)
        {
        case EStep::WaitForPeers:
        {
            const FPieNetWorlds Peers = GetPieNetWorlds();
            if (!Peers.Server || Peers.Clients.Num() == 0)
                return Retry(TEXT("出现一个服务器世界和至少一个客户端世界"));

            ServerWorld = Peers.Server;
            ClientWorld = Peers.Clients[0];
            return Next(EStep::SpawnOnServer);
        }

        case EStep::SpawnOnServer:
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AUnLuaNovaTestNetActor* Actor = ServerWorld->SpawnActor<AUnLuaNovaTestNetActor>(
                FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
            if (!Actor)
            {
                Test.AddError(TEXT("在服务器世界上生成 AUnLuaNovaTestNetActor 失败。"));
                return Next(EStep::Done);
            }

            // Set before the actor channel opens so the first replication already carries it:
            // this is the control. If it does not arrive, the session or the actor's
            // replication setup is broken and nothing below says anything about Lua.
            Actor->EngineReplicatedInt = NetControlValue;
            Actor->ForceNetUpdate();
            ServerActor = Actor;
            return Next(EStep::WaitForClientActor);
        }

        case EStep::WaitForClientActor:
        {
            AUnLuaNovaTestNetActor* Found = nullptr;
            for (TActorIterator<AUnLuaNovaTestNetActor> It(ClientWorld.Get()); It; ++It)
            {
                if (It->EngineReplicatedInt == NetControlValue)
                {
                    Found = *It;
                    break;
                }
            }

            if (!Found)
                return Retry(TEXT("引擎把 Actor 连同它的复制字段送到客户端"));

            ClientActor = Found;
            return Next(EStep::AssignOwners);
        }

        case EStep::AssignOwners:
        {
            // The engine only sends a client RPC to the connection that owns the actor, and
            // only accepts a server RPC from the client that owns it. That rule is not what
            // is under test, so it is satisfied directly instead of built up: on the server
            // by taking the owning PC off the client connection, on the client by pointing
            // the actor at the local PC - which is a shortcut only a test can take, since a
            // client may not set Owner over the wire.
            if (UNetDriver* Driver = ServerWorld->GetNetDriver())
            {
                for (UNetConnection* Connection : Driver->ClientConnections)
                {
                    if (Connection && Connection->PlayerController)
                    {
                        ServerActor->SetOwner(Connection->PlayerController);
                        break;
                    }
                }
            }

            if (APlayerController* LocalPC = ClientWorld->GetFirstPlayerController())
                ClientActor->SetOwner(LocalPC);

            ServerActor->ForceNetUpdate();
            return Next(EStep::CallServerRpc);
        }

        case EStep::CallServerRpc:
            InvokeRpc(ClientActor.Get(), TEXT("FromClient"), ServerRpcArg);
            return Next(EStep::WaitServerRpc);

        case EStep::WaitServerRpc:
            if (!ServerActor->CallLog.Contains(TEXT("FromClient:77")))
                return Retry(TEXT("Lua 声明的 Server RPC 在服务器上执行"));
            return Next(EStep::CallClientRpc);

        case EStep::CallClientRpc:
            InvokeRpc(ServerActor.Get(), TEXT("FromServer"), ClientRpcArg);
            return Next(EStep::WaitClientRpc);

        case EStep::WaitClientRpc:
            if (!ClientActor->CallLog.Contains(TEXT("FromServer:88")))
                return Retry(TEXT("Lua 声明的 Client RPC 在客户端上执行"));
            return Next(EStep::CallMulticast);

        case EStep::CallMulticast:
            InvokeRpc(ServerActor.Get(), TEXT("ToAll"), MulticastRpcArg);
            return Next(EStep::WaitMulticast);

        case EStep::WaitMulticast:
            if (!ServerActor->CallLog.Contains(TEXT("ToAll:99")) || !ClientActor->CallLog.Contains(TEXT("ToAll:99")))
                return Retry(TEXT("Lua 声明的 Multicast RPC 在两端都执行"));
            return Next(EStep::WaitReplication);

        case EStep::WaitReplication:
        {
            // The server RPC's handler moved HP on the server; the client should end up with
            // the same number in its Lua table.
            int32 ClientHp = -1;
            if (!ReadLuaIntField(ClientActor.Get(), "HP", ClientHp) || ClientHp != ServerRpcArg)
                return Retry(TEXT("Lua 声明的复制字段 HP 到达客户端"));
            return Next(EStep::Assert);
        }

        case EStep::Assert:
            Assert();
            return Next(EStep::Done);

        case EStep::Done:
            return true;
        }

        return true;
    }

    void FLuaNetScenarioCommand::Assert()
    {
        const FString ServerLog = ServerActor->CallLog;
        const FString ClientLog = ClientActor->CallLog;

        // Control first: without this the rest is noise.
        Test.TestEqual(TEXT("对照项：引擎自己的复制字段到达客户端"), ClientActor->EngineReplicatedInt, NetControlValue);

        Test.TestTrue(TEXT("Lua 声明的 Server RPC 在服务器上执行"), ServerLog.Contains(TEXT("FromClient:77")));
        Test.TestFalse(TEXT("Lua 声明的 Server RPC 不在发起它的客户端上执行"), ClientLog.Contains(TEXT("FromClient")));

        Test.TestTrue(TEXT("Lua 声明的 Client RPC 在客户端上执行"), ClientLog.Contains(TEXT("FromServer:88")));
        Test.TestFalse(TEXT("Lua 声明的 Client RPC 不在发起它的服务器上执行"), ServerLog.Contains(TEXT("FromServer")));

        Test.TestTrue(TEXT("Lua 声明的 Multicast RPC 在服务器上执行"), ServerLog.Contains(TEXT("ToAll:99")));
        Test.TestTrue(TEXT("Lua 声明的 Multicast RPC 在客户端上执行"), ClientLog.Contains(TEXT("ToAll:99")));

        int32 ServerHp = -1;
        int32 ClientHp = -1;
        Test.TestTrue(TEXT("服务端上能读到 Lua 声明的复制字段"), ReadLuaIntField(ServerActor.Get(), "HP", ServerHp));
        Test.TestTrue(TEXT("客户端上能读到 Lua 声明的复制字段"), ReadLuaIntField(ClientActor.Get(), "HP", ClientHp));
        Test.TestEqual(TEXT("客户端上的 HP 与服务端写入的值一致"), ClientHp, ServerRpcArg);
    }

    /**
     * Take the Lua-declared RPCs back off the class.
     *
     * After the session has ended, so teardown has already run its own restore; this is the
     * belt to that braces - plus the declarations themselves, which are ours and outlive
     * both the session and the restore.
     */
    class FLuaNetTeardownCommand : public IAutomationLatentCommand
    {
    public:
        virtual bool Update() override
        {
            ULuaFunction::RestoreOverrides(AUnLuaNovaTestNetActor::StaticClass());
            UnLuaNova::FUnLuaNovaNet::Cleanup();
            return true;
        }
    };
}

BEGIN_TESTSUITE(FLuaNetClientServerTest, TEXT("UnLuaNova.API.Net.ClientServer Lua 声明的 RPC 在 C/S 会话里跨端执行"))

    bool FLuaNetClientServerTest::RunTest(const FString& Parameters)
    {
        // Empty on purpose: nothing in the level should be able to influence whether an RPC
        // arrives, and an actor placed in the map would be a second instance of the class for
        // the test to trip over.
        const FString MapName = TEXT("/UnLuaNovaTestSuite/Tests/Misc/Empty");

        // Two clients requested rather than one: how the editor reads "number of players"
        // for a listen server is not the same number everywhere, and what this test needs is
        // "a server and a client", not a specific count.
        ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(MapName, false, 2, EUnLuaNovaPlayNetMode::ListenServer));
        ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FLuaNetScenarioCommand(*this));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPieSessionCommand());
        ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FLuaNetTeardownCommand());
        return true;
    }

END_TESTSUITE(FLuaNetClientServerTest)

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
