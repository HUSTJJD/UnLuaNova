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

// One net topology, declared once, run as real processes.
//
// The suite used to test networking two ways, and neither was enough:
//
//   - LuaNetClientServerTest opens a PIE listen server *inside the editor*, so one process
//     holds every peer. It can assert where each RPC ran, which needs pointers into both
//     sides - but it cannot prove anything about RPC ids, because in-process there is one
//     UClass and therefore trivially one id.
//   - LuaNetDedicatedServerTest launched two peers by hand: the command line was built
//     inline, the handles lived in a static array and the reports were found by name. It
//     worked, and it was unrepeatable - a second topology meant copying the whole file.
//
// FNetSession is that second half, generalised: a test declares the peers it wants (roles,
// map, who connects to whom), the session launches them as separate `-game` processes, and
// every peer writes one JSON report into a directory named after the session. The driving
// test waits for those files and compares them. That is the FileRendezvous channel: no
// IPC, no listening port of our own, and a peer that hangs is visible as a missing file
// rather than as a deadlock.
//
// Two things are deliberately *not* done here:
//
//   - No Gauntlet. Running peers means exactly one thing - FPlatformProcess::CreateProc -
//     and Gauntlet would add a device/role/session model the suite does not otherwise use.
//   - No requirement that peers connect. A topology may connect them (ConnectTo); the
//     declaration-comparison tests do not, because the thing they compare - the RPC ids two
//     processes derive independently - does not depend on a connection, and every port a
//     test does not bind is one less way for CI to fail for the wrong reason.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Templates/Function.h"

#if WITH_DEV_AUTOMATION_TESTS

class UWorld;

namespace UnLuaNovaTestSuite
{
    enum class ENetPeerRole : uint8
    {
        Server,
        Client,
    };

    /** What Spec.Port means when the test leaves it alone. */
    constexpr int32 DefaultNetPort = 7777;

    /** One process in the topology. */
    struct FNetPeerSpec
    {
        /** Report file name, log file name and the label the runner shows. "Server", "Client1". */
        FString Label;
        ENetPeerRole Role = ENetPeerRole::Client;

        /** The automation test this peer runs. It is what writes the peer's report. */
        FString ReportTestName;

        /** Server: the map to open. Client without ConnectTo: the map to open standalone. */
        FString MapName;

        /** Client only: host (or URL) of the server. Empty = this peer does not connect. */
        FString ConnectTo;

        /**
         * Left at DefaultNetPort (7777) it is shifted by the peer's index, so three peers on
         * one machine get 7777 / 7778 / 7779: two clients both asking for 7777 fail in a way
         * that reads as "the client could not reach the server", which is a day's work spent
         * on the wrong thing. Set it explicitly when the peer must be on a known port.
         */
        int32 Port = DefaultNetPort;

        /** A dedicated server has no window: -NullRHI, and its log is all it leaves behind. */
        bool bHeadless = false;

        FString ExtraArgs;
    };

    struct FNetPeerRun
    {
        FNetPeerSpec Spec;
        FProcHandle Handle;
        uint32 Pid = 0;
        bool bLaunched = false;
    };

    /**
     * A set of peers, launched as separate processes, that report back through files.
     *
     * Shared by pointer (TSharedRef) between the latent commands below: launching, waiting
     * and terminating are separate ticks, and the session is the state they hand to each
     * other.
     */
    class FNetSession
    {
    public:
        explicit FNetSession(FString InName, FString InMapName);

        int32 AddPeer(FNetPeerSpec Spec);

        const TArray<FNetPeerRun>& GetPeers() const { return Peers; }
        const FString& GetName() const { return Name; }
        const FString& GetMapName() const { return MapName; }

        FString GetDir() const;
        FString GetLogDir() const;
        FString ReportPath(const FString& Label) const;
        FString LogPath(const FString& Label) const;

        /** Deletes this session's reports (and topology) so a stale file cannot pass for a peer. */
        void CleanReports();

        FString BuildPeerCommandLine(const FNetPeerSpec& Spec) const;

        /**
         * Select the configured packaged executable for the role. Editor diagnostics can
         * use the editor with -server; cooked runs require a separate Server target.
         */
        static FString ResolvePeerExe(ENetPeerRole Role = ENetPeerRole::Client);

        /**
         * Launches every peer that is still pending (or just OnlyLabel), in one call.
         *
         * A batch, not one per tick. Ordering - "the server has to be listening before a
         * client dials it" - is not expressed by trickling launches out one frame at a time,
         * which would make every topology pay a frame per peer and hide the real reason for
         * the delay; it is expressed by the driving test launching the server with
         * OnlyLabel, waiting, and then launching the rest. Two peers that never talk to each
         * other (the declaration-comparison tests) start together, which is what keeps that
         * test inside a minute.
         */
        bool LaunchPending(FAutomationTestBase& Test, const FString& OnlyLabel = FString());

        /** True when launching a peer failed - the wait for reports then stops instead of hanging. */
        bool HasLaunchFailure() const { return bLaunchFailed; }

        /** True only after every published report can be parsed. */
        bool AllReportsPresent() const;
        void TerminateAll();
        bool ReadReport(const FString& Label, TSharedPtr<FJsonObject>& Out) const;

        /** Peers, roles and pids - read by RunTests.ps1 to name the recordings and keep the logs. */
        bool WriteTopology() const;

        /** Port of the Server peer, for clients that connect without naming one. */
        int32 GetServerPort() const;

    private:
        FString Name;
        FString MapName;
        TArray<FNetPeerRun> Peers;
        bool bLaunchFailed = false;
    };

    // ---------------------------------------------------------------- peer side ----

    /** -UnLuaNovaNetRole=<label>; empty in the process that drives the test. */
    FString GetNetRole();
    /** -UnLuaNovaNetSession=<name>; "Default" when absent. */
    FString GetNetSessionName();

    /** Deletes stale output, then publishes a closed temporary file in the session directory. */
    bool WriteNetReport(const FString& Label, const TSharedRef<FJsonObject>& Report);
    bool ReadNetReport(const FString& SessionName, const FString& Label, TSharedPtr<FJsonObject>& Out);

    /** The Lua-declared net surface as *this* process sees it: RPC name -> id, plus fields. */
    TSharedRef<FJsonObject> BuildNetDeclarationReport();

    /** Net mode and connection count of World, appended to Report for cross-peer assertions. */
    void AppendWorldFacts(UWorld* World, FJsonObject& Report);

    // ------------------------------------------------------------ latent cmds ----

    /**
     * Launches every pending peer (or just OnlyLabel) in one call.
     *
     * The delay between "server up" and "client dials it" is the test's business - it puts a
     * wait command between two of these - not something this command does by trickling
     * launches out one frame at a time.
     */
    class FNetLaunchPeersCommand : public IAutomationLatentCommand
    {
    public:
        FNetLaunchPeersCommand(TSharedRef<FNetSession> InSession, FAutomationTestBase& InTest,
            FString InOnlyLabel = FString())
            : Session(InSession), Test(InTest), OnlyLabel(MoveTemp(InOnlyLabel)) {}

        virtual bool Update() override;

    private:
        TSharedRef<FNetSession> Session;
        FAutomationTestBase& Test;
        FString OnlyLabel;
    };

    /** Waits until every peer has written its report. */
    class FNetWaitForReportsCommand : public IAutomationLatentCommand
    {
    public:
        FNetWaitForReportsCommand(TSharedRef<FNetSession> InSession, FAutomationTestBase& InTest,
            double InTimeoutSeconds = 180.0)
            : Session(InSession), Test(InTest), TimeoutSeconds(InTimeoutSeconds) {}

        virtual bool Update() override;

    private:
        TSharedRef<FNetSession> Session;
        FAutomationTestBase& Test;
        double TimeoutSeconds;
        double StartedAt = 0.0;
    };

    /** Kills whatever is still running. Always queued last, including after a failure. */
    class FNetTerminatePeersCommand : public IAutomationLatentCommand
    {
    public:
        explicit FNetTerminatePeersCommand(TSharedRef<FNetSession> InSession) : Session(InSession) {}

        virtual bool Update() override;

    private:
        TSharedRef<FNetSession> Session;
    };

    /** Spawns AUnLuaNovaTestNetActor so its Lua module binds and declares. */
    class FNetSpawnActorCommand : public IAutomationLatentCommand
    {
    public:
        explicit FNetSpawnActorCommand(FAutomationTestBase& InTest) : Test(InTest) {}

        virtual bool Update() override;

    private:
        FAutomationTestBase& Test;
        double StartedAt = 0.0;
    };

    /** Builds and writes this peer's report. Build runs once, when the command is reached. */
    class FNetWriteReportCommand : public IAutomationLatentCommand
    {
    public:
        FNetWriteReportCommand(FAutomationTestBase& InTest, TFunction<TSharedRef<FJsonObject>()> InBuild)
            : Test(InTest), Build(MoveTemp(InBuild)) {}

        virtual bool Update() override;

    private:
        FAutomationTestBase& Test;
        TFunction<TSharedRef<FJsonObject>()> Build;
    };

    /** Finds the world this process is running: Game or PIE, whichever exists. */
    UWorld* GetRunWorld();
}
#endif
