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

// Two-process coverage for the part of Lua-declarative networking a single-process PIE
// session cannot prove.
//
// LuaNetClientServerTest asserts where each RPC kind executes, that arguments survive the
// wire, and that a Lua-declared replicated field reaches the client. What it cannot prove
// is the one thing the docs listed as an open boundary: **that both peers number the RPCs
// the same way**. An RPC travels as an id, not a name, and in a single-process PIE session
// both peers share one UClass - so the ids agree by construction, whatever the code does.
// Two processes build the class independently, which is the case that can actually differ.
//
// This file runs the declaration in two separate `-game` processes and compares what each
// one ended up with:
//
//   UnLuaNova.API.Net.DedicatedServer.Report   runs inside a peer process
//       (-UnLuaNovaNetRole=Server|Client). It spawns the net actor so the Lua module binds
//       and declares, then writes what *this* process sees to
//       Saved/UnLuaNovaNet/DedicatedServer/<Role>.json.
//
//   UnLuaNova.API.Net.DedicatedServer          runs in the process driving the test. It
//       launches one peer per role, waits for both reports, and compares them.
//
// The topology - launch, wait, terminate, where the reports go - is FNetSession (see
// NetSession.h). What is specific to this test is only the two peers and the comparison.
//
// Two deliberate choices:
//
//   - The peers do not connect to each other. RPC numbering depends only on the module and
//     the declaration order, both identical on the two sides, so a connection is not needed
//     to prove the ids match - and every port this test does not bind is one less way for a
//     nightly run to fail for the wrong reason. The connected case is
//     UnLuaNova.Slow.Net.Multicast (LuaNetMulticastTest.cpp).
//   - The peers are `-game`, not editor, processes: a peer has no reason to load the editor,
//     and two editors starting up put this test on the wrong side of a minute. -server is
//     what makes the server peer a real dedicated server (NM_DedicatedServer), a net mode
//     PIE never reaches.

#include "NetSession.h"
#include "UnLuaNovaTestCommon.h"
#include "UnLuaNovaTestHelpers.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "UnLuaNovaNet.h"
#include "UnLuaNovaNetDecl.h"

namespace UnLuaNovaTestSuite
{
    static const TCHAR* DedicatedServerSession = TEXT("DedicatedServer");
    static const TCHAR* DedicatedServerReportTest = TEXT("UnLuaNova.API.Net.DedicatedServer.Report");
    static const TCHAR* DedicatedServerMap = TEXT("/UnLuaNovaTestSuite/Tests/Misc/Empty");

    /** "Name:Id" per RPC - the form that makes a diff readable in a failure message. */
    static FString DescribeRpcs(const TSharedPtr<FJsonObject>& Report)
    {
        TArray<FString> Parts;
        for (const TSharedPtr<FJsonValue>& Value : Report->GetArrayField(TEXT("Rpcs")))
        {
            const TSharedPtr<FJsonObject> Entry = Value->AsObject();
            Parts.Add(FString::Printf(TEXT("%s:%d"), *Entry->GetStringField(TEXT("Name")),
                (int32)Entry->GetNumberField(TEXT("RpcId"))));
        }
        return FString::Join(Parts, TEXT(", "));
    }

    /**
     * Compares every peer's report against the first one's.
     *
     * Written for N peers rather than for two: the point is that the ids agree across
     * processes, and "agree" is a property of the whole set.
     */
    class FCompareNetReportsCommand : public IAutomationLatentCommand
    {
    public:
        FCompareNetReportsCommand(FAutomationTestBase& InTest, TSharedRef<FNetSession> InSession)
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
                    Test.AddError(FString::Printf(TEXT("the %s report could not be read"), *Peer.Spec.Label));
                    return true;
                }
                Labels.Add(Peer.Spec.Label);
                Reports.Add(Peer.Spec.Label, Report);
            }

            if (Labels.Num() < 2)
            {
                Test.AddError(TEXT("fewer than two peer reports - nothing was compared"));
                return true;
            }

            const FString Reference = DescribeRpcs(Reports[Labels[0]]);
            Test.TestTrue(TEXT("the declaration report is not empty"), Reference.Len() > 0);

            for (int32 i = 1; i < Labels.Num(); ++i)
            {
                const FString Other = DescribeRpcs(Reports[Labels[i]]);
                Test.TestEqual(FString::Printf(TEXT("RPC ids agree: %s vs %s"), *Labels[0], *Labels[i]),
                    Other, Reference);
                // 0 is EnsureRpcId() never having run on that side. Equal-and-zero would
                // pass the comparison above while proving nothing.
                Test.TestFalse(FString::Printf(TEXT("every RPC got an id on %s"), *Labels[i]),
                    Other.Contains(TEXT(":0")));
            }

            for (const FString& Label : Labels)
            {
                const TSharedPtr<FJsonObject>& Report = Reports[Label];
                const int32 PropCount = Report->GetArrayField(TEXT("Props")).Num();
                Test.TestTrue(FString::Printf(TEXT("%s declared at least one replicated field"), *Label),
                    PropCount > 0);

                const int32 NetMode = (int32)Report->GetNumberField(TEXT("NetMode"));
                if (Label == TEXT("Server"))
                {
                    // Why this test is called DedicatedServer: -server puts the peer in a net
                    // mode PIE never reaches, and the declaration has to hold there too.
                    Test.TestEqual(TEXT("the server peer runs as NM_DedicatedServer"), NetMode, (int32)NM_DedicatedServer);
                }
                Test.AddInfo(FString::Printf(TEXT("%s: NetMode=%d, RPCs=%d, fields=%d"),
                    *Label, NetMode, Report->GetArrayField(TEXT("Rpcs")).Num(), PropCount));
            }

            return true;
        }

    private:
        FAutomationTestBase& Test;
        TSharedRef<FNetSession> Session;
    };
}

// ------------------------------------------------------------- the report ----

// ANY_CONTEXT: this runs inside the peer, and the server peer is a dedicated server -
// without ServerContext the framework would quietly not run it there.
BEGIN_TESTSUITE_ANY_CONTEXT(FLuaNetDedicatedServerReportTest,
    TEXT("UnLuaNova.API.Net.DedicatedServer.Report 在 -UnLuaNovaNetRole 指定的 -game 进程里导出本端的网络声明"))
    bool FLuaNetDedicatedServerReportTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;

        // Only when launched as a peer. In the driving process this would write a report for
        // the wrong role and the comparison would end up passing against itself.
        if (GetNetRole().IsEmpty())
        {
            AddInfo(TEXT("not running as a peer (no -UnLuaNovaNetRole) - nothing to report"));
            return true;
        }

        // No FOpenMapLatentCommand: a -game process is already in the map named on its
        // command line, and waiting for a map that is loaded is what the command does here.
        ADD_LATENT_AUTOMATION_COMMAND(FNetSpawnActorCommand(*this));
        // A tick between spawn and export: the Lua module binds on spawn, but the report
        // reads UFunction::RPCId, which is only filled once the declaration has run.
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitOneTick());
        ADD_LATENT_AUTOMATION_COMMAND(FNetWriteReportCommand(*this, []() -> TSharedRef<FJsonObject>
        {
            TSharedRef<FJsonObject> Report = BuildNetDeclarationReport();
            AppendWorldFacts(GetRunWorld(), *Report);
            return Report;
        }));
        return true;
    }
END_TESTSUITE(FLuaNetDedicatedServerReportTest)

// --------------------------------------------------------- the comparison ----

BEGIN_TESTSUITE(FLuaNetDedicatedServerTest,
    TEXT("UnLuaNova.API.Net.DedicatedServer 两个 -game 进程各自声明出的 RPC 编号一致"))
    bool FLuaNetDedicatedServerTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;

        if (!GetNetRole().IsEmpty())
        {
            AddInfo(TEXT("running as a peer - the comparison belongs to the driving process"));
            return true;
        }

        TSharedRef<FNetSession> Session = MakeShared<FNetSession>(DedicatedServerSession, DedicatedServerMap);

        FNetPeerSpec Server;
        Server.Label = TEXT("Server");
        Server.Role = ENetPeerRole::Server;
        Server.MapName = DedicatedServerMap;
        Server.ReportTestName = DedicatedServerReportTest;
        Server.bHeadless = true;   // a dedicated server has nothing to show
        Session->AddPeer(Server);

        FNetPeerSpec Client;
        Client.Label = TEXT("Client");
        Client.Role = ENetPeerRole::Client;
        Client.MapName = DedicatedServerMap;
        Client.ReportTestName = DedicatedServerReportTest;
        Client.bHeadless = true;
        Session->AddPeer(Client);

        // Deleted up front: a leftover from a previous run is indistinguishable from a peer
        // that answered, and this test exists to catch a peer that did not.
        Session->CleanReports();

        // Both at once: these two peers never talk to each other, so there is nothing for one
        // to wait for in the other - launching them in the same tick is what keeps this test
        // inside a minute. (A topology that connects launches the server first, waits, then
        // launches the clients - see LuaNetMulticastTest.cpp.)
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 150.0));
        ADD_LATENT_AUTOMATION_COMMAND(FCompareNetReportsCommand(*this, Session));
        // Last, and unconditionally: a peer that is still running keeps a second process
        // alive after the run has finished.
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FLuaNetDedicatedServerTest)
