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

// A multicast RPC declared in Lua, reaching two clients in two other processes.
//
// This is the case the single-process tests cannot reach and the two-process one does not
// attempt:
//
//   - LuaNetClientServerTest proves a multicast runs on the server and on its PIE client,
//     but both peers share a process, a UClass and a net driver.
//   - LuaNetDedicatedServerTest now connects a DS and two clients, checking ownership,
//     bidirectional RPCs and two rounds of Lua field replication on two actors.
//
// This focused broadcast test checks that one call on
// the server lands in Lua on *every* client, where each client has its own process, its own
// UClass and its own Lua state. That is this test: one dedicated server peer plus two client
// peers, all `-game`, connected over localhost. The server waits until both clients are
// there, calls ToAll once, and every peer writes what it saw. The driving process compares.
//
// It is in the slow tier (UnLuaNova.Slow.Net.*) on purpose: it binds a port and waits on
// three processes, which is not what a "did my change break anything" run should pay for.
// RunTests.ps1 -Tier Fast leaves it out; -Tier All (the default) runs it.

#include "NetSession.h"
#include "UnLuaNovaTestCommon.h"
#include "UnLuaNovaTestHelpers.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "UObject/CoreNet.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UnLuaNovaTestSuite
{
    static const TCHAR* MulticastSession = TEXT("Multicast");
    static const TCHAR* MulticastReportTest = TEXT("UnLuaNova.Slow.Net.Multicast.Report");
    static const TCHAR* MulticastMap = TEXT("/UnLuaNovaTestSuite/Tests/Misc/Empty");
    static const TCHAR* MulticastCall = TEXT("ToAll");
    /** What the Lua module appends to CallLog: "<Name>:<Value>|" (Tests/Net/NetActor.lua). */
    static const TCHAR* MulticastToken = TEXT("ToAll:99");
    constexpr int32 MulticastValue = 99;
    static TSharedRef<FJsonObject> BuildMulticastReport();

    /** How many clients the server peer waits for; passed on the peer's command line. */
    static int32 ExpectedClientsFromCommandLine()
    {
        int32 Value = 0;
        FParse::Value(FCommandLine::Get(), TEXT("-UnLuaNovaNetExpect="), Value);
        return Value;
    }

    static AUnLuaNovaTestNetActor* FindNetActor(UWorld* World)
    {
        if (!World)
        {
            return nullptr;
        }

        for (TActorIterator<AUnLuaNovaTestNetActor> It(World); It; ++It)
        {
            return *It;
        }
        return nullptr;
    }

    // InvokeRpc / DescribeRpcs live in UnLuaNovaTestCommon.h: three net tests use them and
    // unity builds put those files in one translation unit.

    /** Server: waits until the expected number of clients are actually connected. */
    class FMulticastWaitForClientsCommand : public IAutomationLatentCommand
    {
    public:
        FMulticastWaitForClientsCommand(FAutomationTestBase& InTest, int32 InExpected)
            : Test(InTest), Expected(InExpected) {}

        virtual bool Update() override
        {
            if (Expected <= 0)
            {
                return true;
            }

            const double Now = FPlatformTime::Seconds();
            if (StartedAt == 0.0)
            {
                StartedAt = Now;
            }

            UWorld* World = GetRunWorld();
            int32 Clients = 0;
            if (World)
            {
                if (UNetDriver* Driver = World->GetNetDriver())
                {
                    if (Driver->IsServer())
                    {
                        Clients = Driver->ClientConnections.Num();
                    }
                }
            }

            bool bClientsReady = Clients >= Expected;
            for (int32 Index = 1; Index <= Expected; ++Index)
            {
                TSharedPtr<FJsonObject> Ready;
                bClientsReady &= ReadNetReport(GetNetSessionName(), FString::Printf(TEXT("Client%d-ready"), Index), Ready);
            }
            if (bClientsReady)
            {
                Test.AddInfo(FString::Printf(TEXT("%d client(s) connected after %.1fs"), Clients, Now - StartedAt));
                return true;
            }

            if (Now - StartedAt > TimeoutSeconds)
            {
                // Not an assertion: whether clients can connect at all depends on the
                // machine, and the clients' own reports say it better - NetMode=Standalone
                // there means "never connected", which the driver turns into a failure with
                // the log path next to it.
                Test.AddInfo(FString::Printf(
                    TEXT("only %d of %d client(s) connected after %.0fs - the multicast may reach nobody"),
                    Clients, Expected, TimeoutSeconds));
                return true;
            }

            return false;
        }

    private:
        FAutomationTestBase& Test;
        int32 Expected;
        double StartedAt = 0.0;
        double TimeoutSeconds = 90.0;
    };

    /** Server: one ToAll call, after the clients are in. */
    class FMulticastInvokeCommand : public IAutomationLatentCommand
    {
    public:
        explicit FMulticastInvokeCommand(FAutomationTestBase& InTest) : Test(InTest) {}

        virtual bool Update() override
        {
            AUnLuaNovaTestNetActor* Actor = FindNetActor(GetRunWorld());
            if (!Actor)
            {
                Test.AddError(TEXT("no AUnLuaNovaTestNetActor on the server - nothing to multicast from"));
                return true;
            }

            InvokeRpc(Actor, MulticastCall, MulticastValue);
            Test.AddInfo(FString::Printf(TEXT("called %s(%d) on the server"), MulticastCall, MulticastValue));
            // Snapshot topology before successful clients can finish and disconnect.
            Test.TestTrue(TEXT("Server multicast report written"), WriteNetReport(GetNetRole(), BuildMulticastReport()));
            return true;
        }

    private:
        FAutomationTestBase& Test;
    };

    /**
     * Client: waits until the multicast arrives - that is, until an actor exists here whose
     * CallLog carries the token. Both halves matter: the actor arriving proves replication,
     * the CallLog line proves the RPC ran in Lua on *this* peer.
     */
    class FMulticastWaitForCallCommand : public IAutomationLatentCommand
    {
    public:
        explicit FMulticastWaitForCallCommand(FAutomationTestBase& InTest) : Test(InTest) {}

        virtual bool Update() override
        {
            const double Now = FPlatformTime::Seconds();
            if (StartedAt == 0.0)
            {
                StartedAt = Now;
            }

            const AUnLuaNovaTestNetActor* Actor = FindNetActor(GetRunWorld());
            if (Actor && !bReadyWritten)
            {
                bReadyWritten = WriteNetReport(GetNetRole() + TEXT("-ready"), MakeShared<FJsonObject>());
                if (!bReadyWritten)
                    Test.AddError(TEXT("Could not write multicast client readiness"));
            }
            if (Actor && Actor->CallLog.Contains(MulticastToken))
            {
                Test.AddInfo(FString::Printf(TEXT("%s arrived after %.1fs (CallLog=%s)"),
                    MulticastToken, Now - StartedAt, *Actor->CallLog));
                return true;
            }

            if (Now - StartedAt > TimeoutSeconds)
            {
                Test.AddInfo(FString::Printf(
                    TEXT("%s did not arrive after %.0fs (actor=%s, CallLog=%s)"),
                    MulticastToken, TimeoutSeconds,
                    Actor ? TEXT("found") : TEXT("missing"),
                    Actor ? *Actor->CallLog : TEXT("")));
                return true;
            }

            return false;
        }

    private:
        FAutomationTestBase& Test;
        bool bReadyWritten = false;
        double StartedAt = 0.0;
        double TimeoutSeconds = 90.0;
    };

    class FMulticastWaitForReportsCommand : public IAutomationLatentCommand
    {
    public:
        explicit FMulticastWaitForReportsCommand(FAutomationTestBase& InTest) : Test(InTest) {}

        virtual bool Update() override
        {
            if (StartedAt == 0.0)
                StartedAt = FPlatformTime::Seconds();
            bool bComplete = true;
            for (int32 Index = 1; Index <= ExpectedClientsFromCommandLine(); ++Index)
            {
                TSharedPtr<FJsonObject> Report;
                bComplete &= ReadNetReport(GetNetSessionName(), FString::Printf(TEXT("Client%d"), Index), Report);
            }
            if (bComplete)
                return true;
            if (FPlatformTime::Seconds() - StartedAt > 120.0)
            {
                Test.AddError(TEXT("Clients did not finish multicast reports before server shutdown"));
                return true;
            }
            return false;
        }

    private:
        FAutomationTestBase& Test;
        double StartedAt = 0.0;
    };

    /** What a peer writes: the declaration, what the world is, and what arrived here. */
    static TSharedRef<FJsonObject> BuildMulticastReport()
    {
        TSharedRef<FJsonObject> Report = BuildNetDeclarationReport();
        AppendWorldFacts(GetRunWorld(), *Report);

        const AUnLuaNovaTestNetActor* Actor = FindNetActor(GetRunWorld());
        if (Actor && GetRunWorld()->GetNetDriver())
        {
            const FClassNetCache* Cache = GetRunWorld()->GetNetDriver()->NetCache->GetClassNetCache(Actor->GetClass());
            Report->SetNumberField(TEXT("NetMaxIndex"), Cache->GetMaxIndex());
            TArray<TSharedPtr<FJsonValue>> Fields;
            for (int32 Index = 0; Index < Cache->GetMaxIndex(); ++Index)
            {
                const FFieldNetCache* Field = Cache->GetFromIndex(Index);
                if (Field)
                    Fields.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%d:%s"), Index, *Field->Field.GetName())));
            }
            Report->SetArrayField(TEXT("NetFields"), Fields);
        }
        Report->SetBoolField(TEXT("ActorFound"), Actor != nullptr);
        Report->SetStringField(TEXT("CallLog"), Actor ? Actor->CallLog : FString());
        Report->SetBoolField(TEXT("MulticastSeen"), Actor && Actor->CallLog.Contains(MulticastToken));
        return Report;
    }

    // ------------------------------------------------------ driver asserts ----

    class FCompareMulticastReportsCommand : public IAutomationLatentCommand
    {
    public:
        FCompareMulticastReportsCommand(FAutomationTestBase& InTest, TSharedRef<FNetSession> InSession)
            : Test(InTest), Session(InSession) {}

        virtual bool Update() override
        {
            TArray<FString> Labels;
            TMap<FString, TSharedPtr<FJsonObject>> Reports;
            for (const FNetPeerRun& Peer : Session->GetPeers())
            {
                TSharedPtr<FJsonObject> Report;
                if (!Session->ReadReport(Peer.Spec.Label, Report))
                {
                    Test.AddError(FString::Printf(TEXT("the %s report could not be read - see %s"),
                        *Peer.Spec.Label, *Session->LogPath(Peer.Spec.Label)));
                    return true;
                }
                Labels.Add(Peer.Spec.Label);
                Reports.Add(Peer.Spec.Label, Report);
            }

            // 1. ids agree across every process - the same property the dedicated-server test
            //    checks, now with three independent processes instead of two.
            const FString Reference = DescribeRpcs(Reports[Labels[0]]);
            Test.TestTrue(TEXT("the declaration report is not empty"), Reference.Len() > 0);
            for (const FString& Label : Labels)
            {
                Test.TestEqual(FString::Printf(TEXT("RPC ids agree: %s vs %s"), *Labels[0], *Label),
                    DescribeRpcs(Reports[Label]), Reference);
                Test.TestEqual(FString::Printf(TEXT("Network field cache size agrees: %s"), *Label),
                    Reports[Label]->GetNumberField(TEXT("NetMaxIndex")), Reports[Labels[0]]->GetNumberField(TEXT("NetMaxIndex")));
                const auto& Fields = Reports[Label]->GetArrayField(TEXT("NetFields"));
                const auto& ReferenceFields = Reports[Labels[0]]->GetArrayField(TEXT("NetFields"));
                Test.TestEqual(TEXT("Network field count agrees"), Fields.Num(), ReferenceFields.Num());
                for (int32 Index = 0; Index < FMath::Min(Fields.Num(), ReferenceFields.Num()); ++Index)
                    Test.TestEqual(TEXT("Network field index agrees"), Fields[Index]->AsString(), ReferenceFields[Index]->AsString());
            }

            // 2. the topology is what was asked for. A client that came up standalone proves
            //    nothing about a multicast, so it is a failure with the peer's log beside it.
            for (const FString& Label : Labels)
            {
                const TSharedPtr<FJsonObject>& Report = Reports[Label];
                const int32 NetMode = (int32)Report->GetNumberField(TEXT("NetMode"));
                const bool bServer = Label == TEXT("Server");
                Test.TestEqual(FString::Printf(TEXT("%s runs in the net mode it was launched for"), *Label),
                    NetMode, (int32)(bServer ? NM_DedicatedServer : NM_Client));
            }

            // Found, not indexed: TMap[] would happily insert a null pointer for a label this
            // topology does not have, and the next line would dereference it. Renaming the
            // server peer is a one-word edit that should not be able to crash the suite.
            const TSharedPtr<FJsonObject>* ServerPtr = Reports.Find(TEXT("Server"));
            if (!ServerPtr || !ServerPtr->IsValid())
            {
                Test.AddError(TEXT("no 'Server' peer in this topology - there is nothing to compare the clients against"));
                return true;
            }

            Test.TestTrue(TEXT("both clients connected to the server"),
                (*ServerPtr)->GetNumberField(TEXT("Clients")) >= 2.0);

            // 3. the point of the test: one call on the server, seen by every client.
            for (const FString& Label : Labels)
            {
                if (Label == TEXT("Server"))
                {
                    continue;
                }
                const TSharedPtr<FJsonObject>& Report = Reports[Label];
                Test.TestTrue(FString::Printf(TEXT("%s ran %s in its own Lua state"), *Label, MulticastCall),
                    Report->GetBoolField(TEXT("MulticastSeen")));
            }

            return true;
        }

    private:
        FAutomationTestBase& Test;
        TSharedRef<FNetSession> Session;
    };
}

// ------------------------------------------------------------- the peers ----

// ANY_CONTEXT: the server peer is a dedicated server (ServerContext), the client peers are
// clients (ClientContext); a peer test needs both.
BEGIN_TESTSUITE_ANY_CONTEXT(FLuaNetMulticastReportTest,
    TEXT("UnLuaNova.Slow.Net.Multicast.Report 在 -UnLuaNovaNetRole 指定的联网进程里记录本端收到什么"))
    bool FLuaNetMulticastReportTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;

        const FString Role = GetNetRole();
        if (Role.IsEmpty())
        {
            AddInfo(TEXT("not running as a peer (no -UnLuaNovaNetRole) - nothing to report"));
            return true;
        }

        if (Role == TEXT("Server"))
        {
            ADD_LATENT_AUTOMATION_COMMAND(FNetSpawnActorCommand(*this));
            ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitOneTick());
            // Wait for the clients before calling: a multicast that goes out before anyone
            // is connected is not sent to them, and the engine does not queue it either.
            ADD_LATENT_AUTOMATION_COMMAND(FMulticastWaitForClientsCommand(*this, ExpectedClientsFromCommandLine()));
            ADD_LATENT_AUTOMATION_COMMAND(FMulticastInvokeCommand(*this));
        }
        else
        {
            ADD_LATENT_AUTOMATION_COMMAND(FMulticastWaitForCallCommand(*this));
            ADD_LATENT_AUTOMATION_COMMAND(FNetWriteReportCommand(*this, []() -> TSharedRef<FJsonObject>
            {
                return BuildMulticastReport();
            }));
        }
        // Keep the world alive until clients have recorded their received RPC.
        if (Role == TEXT("Server"))
            ADD_LATENT_AUTOMATION_COMMAND(FMulticastWaitForReportsCommand(*this));
        return true;
    }
END_TESTSUITE(FLuaNetMulticastReportTest)

// ------------------------------------------------------------ the driver ----

BEGIN_TESTSUITE(FLuaNetMulticastTest,
    TEXT("UnLuaNova.Slow.Net.Multicast 服务器一次 Lua 声明的 multicast 调用到达两个客户端进程"))
    bool FLuaNetMulticastTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;

        if (!GetNetRole().IsEmpty())
        {
            AddInfo(TEXT("running as a peer - the comparison belongs to the driving process"));
            return true;
        }

        TSharedRef<FNetSession> Session = MakeShared<FNetSession>(MulticastSession, MulticastMap);

        FNetPeerSpec Server;
        Server.Label = TEXT("Server");
        Server.Role = ENetPeerRole::Server;
        Server.MapName = MulticastMap;
        Server.ReportTestName = MulticastReportTest;
        Server.bHeadless = true;
        Server.ExtraArgs = TEXT("-UnLuaNovaNetExpect=2");
        Session->AddPeer(Server);

        for (int32 i = 1; i <= 2; ++i)
        {
            FNetPeerSpec Client;
            Client.Label = FString::Printf(TEXT("Client%d"), i);
            Client.Role = ENetPeerRole::Client;
            Client.MapName = MulticastMap;
            Client.ReportTestName = MulticastReportTest;
            // A window on purpose: this is the peer a recording is worth having - see
            // topology.json, which is what RunTests.ps1 picks the recorded windows from.
            Client.bHeadless = false;
            Client.ConnectTo = TEXT("127.0.0.1");
            Session->AddPeer(Client);
        }

        Session->CleanReports();

        // The server first, and given time to start listening: a client that dials a port
        // nobody is listening on fails in a way that looks like a networking bug.
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Server")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(10.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 240.0));
        ADD_LATENT_AUTOMATION_COMMAND(FCompareMulticastReportsCommand(*this, Session));
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FLuaNetMulticastTest)

#endif // WITH_DEV_AUTOMATION_TESTS
