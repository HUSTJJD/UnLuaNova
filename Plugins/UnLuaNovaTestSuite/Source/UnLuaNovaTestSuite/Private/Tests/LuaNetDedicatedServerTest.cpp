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

#include "NetSession.h"
#include "UnLuaNovaTestCommon.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "LuaEnv.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "UnLuaNovaModule.h"
#include "UnLuaNovaReplicationComponent.h"

namespace UnLuaNovaTestSuite
{
    static const TCHAR* DedicatedServerSession = TEXT("DedicatedServer");
    static const TCHAR* DedicatedServerReportTest = TEXT("UnLuaNova.API.Net.DedicatedServer.Report");
    static const TCHAR* DedicatedServerMap = TEXT("/UnLuaNovaTestSuite/Tests/Misc/Empty");

    static FString ConnectedRpcToken(const TCHAR* Name, int32 Value)
    {
        return FString::Printf(TEXT("%s:%d|"), Name, Value);
    }

    static bool ReadConnectedHp(AUnLuaNovaTestNetActor* Actor, int32& Value)
    {
        UnLuaNova::FLuaEnv* Env = IUnLuaNovaModule::Get().GetEnv(Actor);
        if (!Env) return false;
        lua_State* L = Env->GetMainState();
        const int32 Ref = Env->GetObjectRegistry()->GetBoundRef(Actor);
        if (Ref == LUA_NOREF) return false;
        const int32 Top = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, Ref);
        lua_getfield(L, -1, "HP");
        const bool bValid = lua_isnumber(L, -1) != 0;
        if (bValid) Value = static_cast<int32>(lua_tointeger(L, -1));
        lua_settop(L, Top);
        return bValid;
    }

    static FString ExpectedConnectedLog(int32 Seed, int32 Rounds, bool bServer, bool bOwner, bool bAck = false)
    {
        FString Result;
        for (int32 Round = 0; Round < Rounds; ++Round)
        {
            const int32 Value = Seed + Round * 10;
            if (Round == 1)
            {
                if (bServer) Result += ConnectedRpcToken(TEXT("FromClient"), Seed + 1);
                else if (bOwner) Result += ConnectedRpcToken(TEXT("FromServer"), Seed + 4);
            }
            if (bServer) Result += ConnectedRpcToken(TEXT("FromClient"), Value + 1);
            else if (bOwner) Result += ConnectedRpcToken(TEXT("FromServer"), Value + 2);
            Result += ConnectedRpcToken(TEXT("ToAll"), Value + 3);
        }
        if (bAck) Result += ConnectedRpcToken(TEXT("FromClient"), Seed + 11);
        return Result;
    }

    // Only gameplay traffic supplies challenges, ownership, HP and RPC results. Files are
    // output evidence and a shutdown rendezvous; clients never read gameplay values from them.
    class FConnectedPeerCommand : public IAutomationLatentCommand
    {
    public:
        explicit FConnectedPeerCommand(FAutomationTestBase& InTest) : Test(InTest)
        {
            bServer = GetNetRole() == TEXT("Server");
            FParse::Value(FCommandLine::Get(), TEXT("-UnLuaNovaNetFault="), Fault);
        }

        virtual bool Update() override
        {
            if (StartedAt == 0.0) StartedAt = FPlatformTime::Seconds();
            UWorld* World = GetRunWorld();
            if (FPlatformTime::Seconds() - StartedAt > (Fault.IsEmpty() ? 150.0 : 40.0))
                return Fail(FString::Printf(TEXT("connected session timed out: role=%s phase=%d actors=%d fault=%s"),
                    *GetNetRole(), Phase, Actors.Num(), *Fault));
            if (!World || !World->GetNetDriver()) return false;
            UNetDriver* Driver = World->GetNetDriver();
            if (bServer)
            {
                if (World->GetNetMode() != NM_DedicatedServer) return Fail(TEXT("server is not dedicated"));
                if (Actors.IsEmpty())
                {
                    TArray<APlayerController*> Owners;
                    for (UNetConnection* Connection : Driver->ClientConnections)
                        if (Connection && Connection->GetConnectionState() == USOCK_Open && Connection->PlayerController)
                            Owners.Add(Connection->PlayerController);
                    if (Owners.Num() != 2) return false;
                    SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
                    const int32 Seed = 1000 + static_cast<int32>(FGuid::NewGuid().A % 1000000);
                    for (int32 Index = 0; Index < 2; ++Index)
                    {
                        FActorSpawnParameters Params;
                        Params.Owner = Owners[Index];
                        auto* Actor = World->SpawnActor<AUnLuaNovaTestNetActor>(Params);
                        if (!Actor) return Fail(TEXT("server actor spawn failed"));
                        Actor->NetTestSessionId = SessionId;
                        Actor->NetTestActorId = Index + 1;
                        Actor->EngineReplicatedInt = Seed + Index * 100;
                        if (Fault == TEXT("SkipReplication")) Actor->Replication->SetIsReplicated(false);
                        Actor->ForceNetUpdate();
                        Actors.Add(Actor);
                    }
                }
                return UpdateServer();
            }
            if (World->GetNetMode() != NM_Client || !Driver->ServerConnection
                || Driver->ServerConnection->GetConnectionState() != USOCK_Open) return false;
            if (Actors.IsEmpty())
            {
                TArray<AUnLuaNovaTestNetActor*> Found;
                for (TActorIterator<AUnLuaNovaTestNetActor> It(World); It; ++It)
                    if (!It->NetTestSessionId.IsEmpty() && It->NetTestActorId > 0 && It->EngineReplicatedInt >= 1000)
                        Found.Add(*It);
                if (Found.Num() != 2) return false;
                Found.Sort([](const AUnLuaNovaTestNetActor& A, const AUnLuaNovaTestNetActor& B)
                    { return A.NetTestActorId < B.NetTestActorId; });
                APlayerController* LocalPC = World->GetFirstPlayerController();
                for (auto* Actor : Found)
                    if (LocalPC && Actor->GetOwner() == LocalPC) OwnedActorId = Actor->NetTestActorId;
                if (OwnedActorId == 0) return false;
                SessionId = Found[0]->NetTestSessionId;
                for (auto* Actor : Found)
                {
                    if (Actor->NetTestSessionId != SessionId || Actor->HasAuthority())
                        return Fail(TEXT("client actor identity/authority mismatch"));
                    Actors.Add(Actor);
                }
            }
            return UpdateClient();
        }

    private:
        bool Fail(const FString& Message)
        {
            Test.AddError(Message);
            WriteNetReport(GetNetRole(), Report(false));
            return true;
        }

        TSharedRef<FJsonObject> ActorReport(AUnLuaNovaTestNetActor* Actor)
        {
            TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
            int32 Hp = -1;
            const bool bHpRead = ReadConnectedHp(Actor, Hp);
            Data->SetNumberField(TEXT("ActorId"), Actor->NetTestActorId);
            Data->SetStringField(TEXT("SessionId"), Actor->NetTestSessionId);
            Data->SetNumberField(TEXT("Challenge"), Actor->EngineReplicatedInt);
            Data->SetNumberField(TEXT("HP"), Hp);
            Data->SetBoolField(TEXT("HpReadable"), bHpRead);
            Data->SetBoolField(TEXT("Authority"), Actor->HasAuthority());
            Data->SetBoolField(TEXT("Owned"), Actor->NetTestActorId == OwnedActorId);
            Data->SetStringField(TEXT("CallLog"), Actor->CallLog);
            Data->SetNumberField(TEXT("CallCount"), Actor->CallCount);
            return Data;
        }

        TArray<TSharedPtr<FJsonValue>> ActorReports()
        {
            TArray<TSharedPtr<FJsonValue>> Result;
            for (const auto& Actor : Actors)
                if (Actor.IsValid()) Result.Add(MakeShared<FJsonValueObject>(ActorReport(Actor.Get())));
            return Result;
        }

        void ObserveRound()
        {
            if (!bServer && Phase == 0)
                FirstRoundObservationMs = (FPlatformTime::Seconds() - FirstRequestAt) * 1000.0;
            auto Observation = MakeShared<FJsonObject>();
            Observation->SetNumberField(TEXT("Round"), Phase + 1);
            Observation->SetArrayField(TEXT("Actors"), ActorReports());
            Observations.Add(MakeShared<FJsonValueObject>(Observation));
            Test.AddInfo(FString::Printf(TEXT("session=%s role=%s completed round=%d ownedActor=%d"),
                *SessionId, *GetNetRole(), Phase + 1, OwnedActorId));
        }

        TSharedRef<FJsonObject> Report(bool bComplete)
        {
            auto Data = BuildNetDeclarationReport();
            AppendWorldFacts(GetRunWorld(), *Data);
            Data->SetNumberField(TEXT("SchemaVersion"), 2);
            Data->SetBoolField(TEXT("Complete"), bComplete);
            Data->SetStringField(TEXT("SessionId"), SessionId);
            Data->SetNumberField(TEXT("OwnedActorId"), OwnedActorId);
            Data->SetArrayField(TEXT("Actors"), ActorReports());
            Data->SetArrayField(TEXT("Observations"), Observations);
            Data->SetStringField(TEXT("Fault"), Fault);
            Data->SetNumberField(TEXT("FirstRoundObservationMs"), FirstRoundObservationMs);
            return Data;
        }

        bool UpdateServer()
        {
            for (const auto& Actor : Actors)
                if (!Actor.IsValid()) return Fail(TEXT("server lost a replicated actor"));
            if (Phase < 2)
            {
                if (Phase == 1 && !bSecondRoundReleased)
                {
                    // HP is replicated state, so an early round-two write can coalesce away
                    // round one on a slower client. Both clients acknowledge observing all
                    // round-one state before the server authorizes any round-two request.
                    for (const auto& Actor : Actors)
                        if (Actor->CallLog != ExpectedConnectedLog(Actor->EngineReplicatedInt, 1, true, false)
                            + ConnectedRpcToken(TEXT("FromClient"), Actor->EngineReplicatedInt + 1)) return false;
                    for (const auto& Actor : Actors)
                        InvokeRpc(Actor.Get(), TEXT("FromServer"), Actor->EngineReplicatedInt + 4);
                    bSecondRoundReleased = true;
                }
                for (const auto& Actor : Actors)
                {
                    FString Expected = ExpectedConnectedLog(Actor->EngineReplicatedInt, Phase, true, false);
                    if (Phase == 1) Expected += ConnectedRpcToken(TEXT("FromClient"), Actor->EngineReplicatedInt + 1);
                    Expected += ConnectedRpcToken(TEXT("FromClient"), Actor->EngineReplicatedInt + Phase * 10 + 1);
                    if (Actor->CallLog != Expected) return false;
                    int32 Hp = -1;
                    if (!ReadConnectedHp(Actor.Get(), Hp) || Hp != Actor->EngineReplicatedInt + Phase * 10 + 1)
                        return false;
                }
                for (const auto& Actor : Actors)
                {
                    const int32 Value = Actor->EngineReplicatedInt + Phase * 10;
                    if (Fault != TEXT("SkipClientRpc")) InvokeRpc(Actor.Get(), TEXT("FromServer"), Value + 2);
                    InvokeRpc(Actor.Get(), TEXT("ToAll"), Value + 3);
                    Actor->ForceNetUpdate();
                }
                ObserveRound();
                ++Phase;
                return false;
            }
            // A second identical final request is an acknowledgement sent only after that
            // client has checked both actors' final HP and exact owner/non-owner RPC logs.
            for (const auto& Actor : Actors)
                if (Actor->CallLog != ExpectedConnectedLog(Actor->EngineReplicatedInt, 2, true, false, true)) return false;
            if (!bReportWritten)
            {
                bReportWritten = WriteNetReport(GetNetRole(), Report(true));
                if (!bReportWritten) return Fail(TEXT("server report write failed"));
            }
            for (const TCHAR* Label : {TEXT("Client1"), TEXT("Client2")})
            {
                TSharedPtr<FJsonObject> Client;
                if (!ReadNetReport(DedicatedServerSession, Label, Client)) return false;
            }
            return true;
        }

        bool UpdateClient()
        {
            for (const auto& Actor : Actors)
                if (!Actor.IsValid()) return Fail(TEXT("client lost a replicated actor"));
            if (Phase < 2)
            {
                if (Phase == 1)
                    for (const auto& Actor : Actors)
                        if (Actor->NetTestActorId == OwnedActorId && !Actor->CallLog.Contains(
                            ConnectedRpcToken(TEXT("FromServer"), Actor->EngineReplicatedInt + 4))) return false;
                if (!bRequestSent)
                {
                    for (const auto& Actor : Actors)
                        if (Actor->NetTestActorId == OwnedActorId && Fault != TEXT("SkipServerRpc"))
                            InvokeRpc(Actor.Get(), TEXT("FromClient"), Actor->EngineReplicatedInt + Phase * 10 + 1);
                    bRequestSent = true;
                    if (Phase == 0) FirstRequestAt = FPlatformTime::Seconds();
                }
                // Force a slow observer so a fast client cannot accidentally hide a missing
                // round barrier. Network/RPC processing continues while this check waits.
                if (Phase == 0 && GetNetRole() == TEXT("Client1")
                    && FPlatformTime::Seconds() - FirstRequestAt < 5.0) return false;
                for (const auto& Actor : Actors)
                {
                    int32 Hp = -1;
                    if (!ReadConnectedHp(Actor.Get(), Hp) || Hp != Actor->EngineReplicatedInt + Phase * 10 + 1)
                        return false;
                    if (Actor->CallLog != ExpectedConnectedLog(Actor->EngineReplicatedInt, Phase + 1,
                        false, Actor->NetTestActorId == OwnedActorId)) return false;
                }
                ObserveRound();
                if (Phase == 0)
                    for (const auto& Actor : Actors)
                        if (Actor->NetTestActorId == OwnedActorId)
                            InvokeRpc(Actor.Get(), TEXT("FromClient"), Actor->EngineReplicatedInt + 1);
                ++Phase;
                bRequestSent = false;
                return false;
            }
            if (!bReportWritten)
            {
                for (const auto& Actor : Actors)
                    if (Actor->NetTestActorId == OwnedActorId)
                        InvokeRpc(Actor.Get(), TEXT("FromClient"), Actor->EngineReplicatedInt + 11);
                bReportWritten = WriteNetReport(GetNetRole(), Report(true));
                if (!bReportWritten) return Fail(TEXT("client report write failed"));
            }
            TSharedPtr<FJsonObject> Server;
            return ReadNetReport(DedicatedServerSession, TEXT("Server"), Server);
        }

        FAutomationTestBase& Test;
        bool bServer = false;
        bool bRequestSent = false;
        bool bReportWritten = false;
        bool bSecondRoundReleased = false;
        double StartedAt = 0.0;
        double FirstRequestAt = 0.0;
        double FirstRoundObservationMs = 0.0;
        int32 Phase = 0;
        int32 OwnedActorId = 0;
        FString SessionId;
        FString Fault;
        TArray<TWeakObjectPtr<AUnLuaNovaTestNetActor>> Actors;
        TArray<TSharedPtr<FJsonValue>> Observations;
    };

    class FCompareConnectedReportsCommand : public IAutomationLatentCommand
    {
    public:
        FCompareConnectedReportsCommand(FAutomationTestBase& InTest, TSharedRef<FNetSession> InSession)
            : Test(InTest), Session(InSession) {}
        virtual bool Update() override
        {
            FString SessionId, RpcReference;
            TSet<int32> Pids, OwnerIds;
            for (const FNetPeerRun& Peer : Session->GetPeers())
            {
                TSharedPtr<FJsonObject> Data;
                if (!Session->ReadReport(Peer.Spec.Label, Data))
                {
                    Test.AddError(TEXT("connected peer report missing: ") + Peer.Spec.Label);
                    continue;
                }
                const bool bServer = Peer.Spec.Role == ENetPeerRole::Server;
                Test.TestTrue(TEXT("peer completed both network rounds"), Data->GetBoolField(TEXT("Complete")));
                Test.TestTrue(TEXT("peer has an open gameplay connection"), Data->GetBoolField(TEXT("Connected")));
                Test.TestEqual(TEXT("peer network mode"), (int32)Data->GetNumberField(TEXT("NetMode")), bServer ? 1 : 3);
                Test.TestEqual(TEXT("report belongs to launched process"), (uint32)Data->GetNumberField(TEXT("ProcessId")), Peer.Pid);
                Pids.Add((int32)Data->GetNumberField(TEXT("ProcessId")));
                if (SessionId.IsEmpty()) SessionId = Data->GetStringField(TEXT("SessionId"));
                Test.TestTrue(TEXT("server generated a session identity"), !SessionId.IsEmpty());
                Test.TestEqual(TEXT("all peers share the server replicated session"), Data->GetStringField(TEXT("SessionId")), SessionId);
                const FString Rpcs = DescribeRpcs(Data);
                if (RpcReference.IsEmpty()) RpcReference = Rpcs;
                Test.TestTrue(TEXT("RPC IDs are assigned"), !Rpcs.IsEmpty() && !Rpcs.Contains(TEXT(":0")));
                Test.TestEqual(TEXT("RPC IDs agree across processes"), Rpcs, RpcReference);
                Test.TestEqual(TEXT("both rounds observed"), Data->GetArrayField(TEXT("Observations")).Num(), 2);
                Test.TestEqual(TEXT("two server-spawned actors observed"), Data->GetArrayField(TEXT("Actors")).Num(), 2);
                const int32 Owner = (int32)Data->GetNumberField(TEXT("OwnedActorId"));
                if (!bServer) OwnerIds.Add(Owner);
                else Test.TestEqual(TEXT("two clients connected to this server"), (int32)Data->GetNumberField(TEXT("Clients")), 2);
                for (const auto& Entry : Data->GetArrayField(TEXT("Actors")))
                {
                    const auto Actor = Entry->AsObject();
                    const int32 Seed = (int32)Actor->GetNumberField(TEXT("Challenge"));
                    const bool bOwner = (int32)Actor->GetNumberField(TEXT("ActorId")) == Owner;
                    Test.TestEqual(TEXT("Lua HP reached final server value"), (int32)Actor->GetNumberField(TEXT("HP")), Seed + 11);
                    Test.TestEqual(TEXT("RPC execution occurs on exactly the intended peers"), Actor->GetStringField(TEXT("CallLog")),
                        ExpectedConnectedLog(Seed, 2, bServer, bOwner, bServer));
                }
            }
            Test.TestEqual(TEXT("three independent processes"), Pids.Num(), 3);
            Test.TestTrue(TEXT("each client owns a different actor"), OwnerIds.Num() == 2 && OwnerIds.Contains(1) && OwnerIds.Contains(2));
            return true;
        }
    private:
        FAutomationTestBase& Test;
        TSharedRef<FNetSession> Session;
    };
}

BEGIN_TESTSUITE_ANY_CONTEXT(FLuaNetDedicatedServerReportTest,
    TEXT("UnLuaNova.API.Net.DedicatedServer.Report Connected peer verifies RPCs and replicated Lua fields"))
    bool FLuaNetDedicatedServerReportTest::RunTest(const FString& Parameters)
    {
        if (GetNetRole().IsEmpty())
        {
            AddInfo(TEXT("Peer helper; the driver test supplies connected session coverage."));
            return true;
        }
        ADD_LATENT_AUTOMATION_COMMAND(FConnectedPeerCommand(*this));
        return true;
    }
END_TESTSUITE(FLuaNetDedicatedServerReportTest)

BEGIN_TESTSUITE(FLuaNetDedicatedServerTest,
    TEXT("UnLuaNova.API.Net.DedicatedServer Two clients share a DS session and verify bidirectional RPCs and replication"))
    bool FLuaNetDedicatedServerTest::RunTest(const FString& Parameters)
    {
        if (!GetNetRole().IsEmpty()) return true;
        TSharedRef<FNetSession> Session = MakeShared<FNetSession>(DedicatedServerSession, DedicatedServerMap);
        FString Fault;
        FParse::Value(FCommandLine::Get(), TEXT("-UnLuaNovaNetFault="), Fault);
        if (!Fault.IsEmpty() && Fault != TEXT("SkipServerRpc") && Fault != TEXT("SkipClientRpc") && Fault != TEXT("SkipReplication"))
        {
            AddError(TEXT("Unknown networking fault injection"));
            return false;
        }
        for (int32 Index = 0; Index < 3; ++Index)
        {
            FNetPeerSpec Peer;
            Peer.Label = Index == 0 ? TEXT("Server") : FString::Printf(TEXT("Client%d"), Index);
            Peer.Role = Index == 0 ? ENetPeerRole::Server : ENetPeerRole::Client;
            Peer.MapName = DedicatedServerMap;
            Peer.ReportTestName = DedicatedServerReportTest;
            Peer.bHeadless = Index == 0;
            if (Index != 0) Peer.ConnectTo = TEXT("127.0.0.1");
            if (!Fault.IsEmpty()) Peer.ExtraArgs = TEXT("-UnLuaNovaNetFault=") + Fault;
            Session->AddPeer(Peer);
        }
        Session->CleanReports();
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Server")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(10.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 210.0));
        ADD_LATENT_AUTOMATION_COMMAND(FCompareConnectedReportsCommand(*this, Session));
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FLuaNetDedicatedServerTest)

#endif
