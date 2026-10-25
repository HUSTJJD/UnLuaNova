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
#include "LuaBackend.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UnLuaNovaTestHelpers.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "UnLuaNovaNet.h"
#include "UnLuaNovaNetDecl.h"

namespace UnLuaNovaTestSuite
{
    static FString NetReportRoot()
    {
        const FString Override = FPlatformMisc::GetEnvironmentVariable(TEXT("UNLUANOVA_TEST_NET_ROOT"));
        return Override.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnLuaNovaNet")) : Override;
    }

    static bool PublishNetJson(const FString& Path, const TSharedRef<FJsonObject>& Report)
    {
        FString Text;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
        if (!FJsonSerializer::Serialize(Report, Writer))
            return false;

        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        // Readers must never observe the destination while its JSON is still being written.
        const FString Pending = Path + FString::Printf(TEXT(".pending-%u"), FPlatformProcess::GetCurrentProcessId());
        const bool bPublished = FFileHelper::SaveStringToFile(Text, *Pending, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
            && IFileManager::Get().Move(*Path, *Pending, true, true, false, true);
        if (!bPublished)
            IFileManager::Get().Delete(*Pending, false, true, true);
        return bPublished;
    }
    // ------------------------------------------------------------- paths ----

    FNetSession::FNetSession(FString InName, FString InMapName)
        : Name(MoveTemp(InName)), MapName(MoveTemp(InMapName))
    {
    }

    int32 FNetSession::AddPeer(FNetPeerSpec Spec)
    {
        // Peers share one machine, and a port is per-process. Shifting the default by the
        // peer's index is the difference between "three peers, three ports" and "the second
        // client cannot connect, and nothing in the log says why".
        if (Spec.Port == DefaultNetPort)
        {
            Spec.Port = DefaultNetPort + Peers.Num();
        }

        FNetPeerRun Run;
        Run.Spec = MoveTemp(Spec);
        return Peers.Add(MoveTemp(Run));
    }

    FString FNetSession::GetDir() const
    {
        return FPaths::Combine(NetReportRoot(), Name);
    }

    FString FNetSession::GetLogDir() const
    {
        return FPaths::Combine(GetDir(), TEXT("logs"));
    }

    FString FNetSession::ReportPath(const FString& Label) const
    {
        return FPaths::Combine(GetDir(), Label + TEXT(".json"));
    }

    FString FNetSession::LogPath(const FString& Label) const
    {
        return FPaths::Combine(GetLogDir(), Label + TEXT(".log"));
    }

    void FNetSession::CleanReports()
    {
        for (const FNetPeerRun& Peer : Peers)
        {
            IFileManager::Get().Delete(*ReportPath(Peer.Spec.Label), false, true, true);
            IFileManager::Get().Delete(*ReportPath(Peer.Spec.Label + TEXT("-ready")), false, true, true);
        }
        // The topology file too: it is what tells the runner which windows to record, and a
        // stale one would point it at pids that belong to a previous run.
        IFileManager::Get().Delete(*FPaths::Combine(GetDir(), TEXT("topology.json")), false, true, true);
    }

    // ------------------------------------------------------- peer launch ----

    FString FNetSession::ResolvePeerExe(ENetPeerRole Role)
    {
        if (Role == ENetPeerRole::Server)
        {
            const FString Server = FPlatformMisc::GetEnvironmentVariable(TEXT("UNLUANOVA_TEST_SERVER_EXE"));
            if (!Server.IsEmpty()) return Server;
#if WITH_EDITOR
            // Only the editor executable supports selecting dedicated mode with -server.
            return FPlatformProcess::ExecutablePath();
#else
            return FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64"),
                FString(FApp::GetProjectName()) + TEXT("Server.exe"));
#endif
        }
        const FString Override = FPlatformMisc::GetEnvironmentVariable(TEXT("UNLUANOVA_TEST_PEER_EXE"));
        if (!Override.IsEmpty()) return Override;
#if PLATFORM_WINDOWS
        const FString ExeName = FString(FApp::GetProjectName()) + TEXT(".exe");
#else
        const FString ExeName = FString(FApp::GetProjectName());
#endif
        const FString ProjectExe = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries"), TEXT("Win64"), ExeName);
        if (FPaths::FileExists(ProjectExe))
        {
            return ProjectExe;
        }

        // Fallback: this process's own binary. -game on the editor executable still starts a
        // standalone game, so the topology is correct either way - just slower to come up.
        return FPlatformProcess::ExecutablePath();
    }

    int32 FNetSession::GetServerPort() const
    {
        for (const FNetPeerRun& Peer : Peers)
        {
            if (Peer.Spec.Role == ENetPeerRole::Server)
            {
                return Peer.Spec.Port;
            }
        }
        return DefaultNetPort;
    }

    FString FNetSession::BuildPeerCommandLine(const FNetPeerSpec& Spec) const
    {
        const TCHAR* OverrideName = Spec.Role == ENetPeerRole::Server
            ? TEXT("UNLUANOVA_TEST_SERVER_EXE") : TEXT("UNLUANOVA_TEST_PEER_EXE");
        const bool bPackagedPeer = !FPlatformMisc::GetEnvironmentVariable(OverrideName).IsEmpty();
        FString Args = bPackagedPeer ? FString() : FString::Printf(TEXT("\"%s\" "), *FPaths::GetProjectFilePath());

        if (Spec.Role == ENetPeerRole::Server)
        {
            Args += FString::Printf(TEXT("%s -server "), *Spec.MapName);
        }
        else if (!Spec.ConnectTo.IsEmpty())
        {
            // A client that connects is handed the address instead of a map. Without a port
            // it dials 7777 by luck rather than by agreement - fill in the *server's* port
            // (not this peer's, which was shifted by AddPeer) so moving the server to
            // another port does not silently break every client.
            FString Address = Spec.ConnectTo;
            if (!Address.Contains(TEXT(":")))
            {
                Address = FString::Printf(TEXT("%s:%d"), *Address, GetServerPort());
            }
            Args += FString::Printf(TEXT("%s "), *Address);
        }
        else
        {
            Args += FString::Printf(TEXT("%s "), *Spec.MapName);
        }

        Args += FString::Printf(TEXT("-game -unattended -NoSplash -nosound -stdout -port=%d "), Spec.Port);
        if (Spec.bHeadless)
        {
            Args += TEXT("-NullRHI ");
        }

        // Quoted: the value is one argument holding spaces and a semicolon, and UE reads
        // -ExecCmds straight off the command line.
        Args += FString::Printf(TEXT("-ExecCmds=\"Automation RunTests %s; Quit\" "), *Spec.ReportTestName);
        Args += FString::Printf(TEXT("-UnLuaNovaNetRole=%s -UnLuaNovaNetSession=%s "), *Spec.Label, *Name);
        // Its own log, next to the report: a headless server has no window to look at, so
        // "the peer never wrote a report" is only diagnosable from here.
        Args += FString::Printf(TEXT("-abslog=\"%s\" "), *LogPath(Spec.Label));

        if (!Spec.ExtraArgs.IsEmpty())
        {
            Args += Spec.ExtraArgs;
            Args += TEXT(" ");
        }

        return Args;
    }

    bool FNetSession::LaunchPending(FAutomationTestBase& Test, const FString& OnlyLabel)
    {
        int32 Launched = 0;

        for (FNetPeerRun& Peer : Peers)
        {
            if (Peer.bLaunched)
            {
                continue;
            }
            if (!OnlyLabel.IsEmpty() && Peer.Spec.Label != OnlyLabel)
            {
                continue;
            }

            Peer.bLaunched = true;

            const FString Exe = ResolvePeerExe(Peer.Spec.Role);
            if (!FPaths::FileExists(Exe))
            {
                Test.AddError(FString::Printf(
                    TEXT("no executable at %s to launch the %s peer with - build the game target, or run the suite with -Carrier Editor"),
                    *Exe, *Peer.Spec.Label));
                bLaunchFailed = true;
                break;
            }

            const FString Args = BuildPeerCommandLine(Peer.Spec);
            uint32 Pid = 0;
            Peer.Handle = FPlatformProcess::CreateProc(*Exe, *Args, false, false, false, &Pid, 0, nullptr, nullptr);
            Peer.Pid = Pid;

            if (!Peer.Handle.IsValid())
            {
                Test.AddError(FString::Printf(TEXT("launching the %s peer failed: %s %s"),
                    *Peer.Spec.Label, *Exe, *Args));
                bLaunchFailed = true;
                break;
            }

            ++Launched;
            UE_LOG(LogTemp, Display, TEXT("[NetSession:%s] %s peer pid=%u port=%d: %s %s"),
                *Name, *Peer.Spec.Label, Pid, Peer.Spec.Port, *Exe, *Args);
        }

        // One topology file for the batch: the runner reads it to find the peer windows, and
        // it has to be written after the pids are known.
        if (Launched > 0)
        {
            IFileManager::Get().MakeDirectory(*GetLogDir(), true);
            WriteTopology();
        }

        return true;
    }

    bool FNetSession::AllReportsPresent() const
    {
        for (const FNetPeerRun& Peer : Peers)
        {
            TSharedPtr<FJsonObject> Report;
            if (!ReadReport(Peer.Spec.Label, Report))
            {
                return false;
            }
        }
        return true;
    }

    void FNetSession::TerminateAll()
    {
        for (FNetPeerRun& Peer : Peers)
        {
            if (Peer.Handle.IsValid())
            {
                if (FPlatformProcess::IsProcRunning(Peer.Handle))
                {
                    FPlatformProcess::TerminateProc(Peer.Handle, true);
                }
                FPlatformProcess::CloseProc(Peer.Handle);
            }
        }
    }

    bool FNetSession::ReadReport(const FString& Label, TSharedPtr<FJsonObject>& Out) const
    {
        return ReadNetReport(Name, Label, Out);
    }

    bool FNetSession::WriteTopology() const
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("session"), Name);
        Root->SetStringField(TEXT("exe"), ResolvePeerExe());
        Root->SetStringField(TEXT("dir"), GetDir());

        TArray<TSharedPtr<FJsonValue>> PeersJson;
        for (const FNetPeerRun& Peer : Peers)
        {
            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("label"), Peer.Spec.Label);
            Entry->SetStringField(TEXT("exe"), ResolvePeerExe(Peer.Spec.Role));
            Entry->SetStringField(TEXT("role"), Peer.Spec.Role == ENetPeerRole::Server ? TEXT("Server") : TEXT("Client"));
            Entry->SetBoolField(TEXT("headless"), Peer.Spec.bHeadless);
            Entry->SetNumberField(TEXT("pid"), (double)Peer.Pid);
            Entry->SetStringField(TEXT("log"), LogPath(Peer.Spec.Label));
            PeersJson.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Root->SetArrayField(TEXT("peers"), PeersJson);

        return PublishNetJson(FPaths::Combine(GetDir(), TEXT("topology.json")), Root);
    }

    // ---------------------------------------------------------- peer side ----

    FString GetNetRole()
    {
        FString Role;
        FParse::Value(FCommandLine::Get(), TEXT("-UnLuaNovaNetRole="), Role);
        return Role;
    }

    FString GetNetSessionName()
    {
        FString Name;
        FParse::Value(FCommandLine::Get(), TEXT("-UnLuaNovaNetSession="), Name);
        return Name.IsEmpty() ? TEXT("Default") : Name;
    }

    bool WriteNetReport(const FString& Label, const TSharedRef<FJsonObject>& Report)
    {
        const FString Path = FPaths::Combine(NetReportRoot(),
            GetNetSessionName(), Label + TEXT(".json"));

        // A stale report is the dangerous case: the driving process would read last run's
        // file and pass on a peer that never answered.
        IFileManager::Get().Delete(*Path, false, true, true);
        return PublishNetJson(Path, Report);
    }

    bool ReadNetReport(const FString& SessionName, const FString& Label, TSharedPtr<FJsonObject>& Out)
    {
        const FString Path = FPaths::Combine(NetReportRoot(),
            SessionName, Label + TEXT(".json"));

        FString Text;
        if (!FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(Text, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
    }

    TSharedRef<FJsonObject> BuildNetDeclarationReport()
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

        UClass* Class = AUnLuaNovaTestNetActor::StaticClass();

        TArray<TSharedPtr<FJsonValue>> Rpcs;
        Class->SetUpRuntimeReplicationData();
        for (const UField* Field : Class->NetFields)
        {
            const UFunction* Function = Cast<UFunction>(Field);
            if (!Function || !Function->HasAnyFunctionFlags(FUNC_Net))
            {
                continue;
            }

            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("Name"), Function->GetName());
            // The whole point of the cross-process tests. 0 means EnsureRpcId() never ran
            // here, which is the failure being hunted, so it is reported and not skipped.
            Entry->SetNumberField(TEXT("RpcId"), (double)Function->RPCId);
            Entry->SetBoolField(TEXT("Reliable"), Function->HasAnyFunctionFlags(FUNC_NetReliable));
            Rpcs.Add(MakeShared<FJsonValueObject>(Entry));
        }

        Rpcs.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
        {
            return A->AsObject()->GetStringField(TEXT("Name")) < B->AsObject()->GetStringField(TEXT("Name"));
        });
        Root->SetArrayField(TEXT("Rpcs"), Rpcs);

        TArray<TSharedPtr<FJsonValue>> Props;
        // Both live in namespace UnLuaNova, and this file is in UnLuaNovaTestSuite -
        // unqualified they are simply not found.
        for (const UnLuaNova::FLuaRepDecl& Decl : UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Class))
        {
            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("Name"), Decl.Name.ToString());
            Entry->SetNumberField(TEXT("Type"), (double)Decl.Type);
            Entry->SetNumberField(TEXT("Condition"), (double)Decl.Condition);
            Props.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Root->SetArrayField(TEXT("Props"), Props);

        // Recorded so a mismatch can be told apart from "the wrong process answered".
        Root->SetStringField(TEXT("Role"), GetNetRole());
        Root->SetStringField(TEXT("Backend"), UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU ? TEXT("Luau") : TEXT("Lua"));
        return Root;
    }

    void AppendWorldFacts(UWorld* World, FJsonObject& Report)
    {
        if (!World)
        {
            Report.SetNumberField(TEXT("NetMode"), (double)NM_Standalone);
            Report.SetBoolField(TEXT("Connected"), false);
            Report.SetNumberField(TEXT("Clients"), 0.0);
            return;
        }

        const ENetMode Mode = World->GetNetMode();
        Report.SetNumberField(TEXT("NetMode"), (double)Mode);
        Report.SetBoolField(TEXT("Connected"), Mode != NM_Standalone);

        int32 Clients = 0;
        if (UNetDriver* Driver = World->GetNetDriver())
        {
            // ClientConnections only exists on the server side of a driver; on a client the
            // single ServerConnection is what is open, and counting *clients* there is 0.
            if (Driver->IsServer())
            {
                Clients = Driver->ClientConnections.Num();
            }
        }
        Report.SetNumberField(TEXT("Clients"), (double)Clients);
    }

    UWorld* GetRunWorld()
    {
        if (!GEngine)
        {
            return nullptr;
        }

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            // PIE is listed too: which of the two a -game run reports is not worth depending
            // on, and both mean "the world this peer is playing in".
            if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.World())
            {
                return Context.World();
            }
        }
        return nullptr;
    }

    // -------------------------------------------------------- latent cmds ----

    bool FNetLaunchPeersCommand::Update()
    {
        return Session->LaunchPending(Test, OnlyLabel);
    }

    bool FNetWaitForReportsCommand::Update()
    {
        // Wall clock, not FApp::GetDeltaTime(): a latent command can be pumped from a context
        // where the engine is not ticking, and a delta of 0 would spin forever instead of
        // ever reaching the timeout below.
        const double Now = FPlatformTime::Seconds();
        if (StartedAt == 0.0)
        {
            StartedAt = Now;
        }

        // A launch that failed will never produce reports. Waiting out the timeout would turn
        // "the peer never started" into a three-minute hang that ends on the same message.
        if (Session->HasLaunchFailure())
        {
            Test.AddError(TEXT("a peer could not be launched - see the errors above; the rest of the reports were not waited for"));
            return true;
        }

        if (Session->AllReportsPresent())
        {
            return true;
        }

        // Fail as soon as a peer is gone: a process that exited without writing its report is
        // already decided, and its log is on disk. Sitting out the timeout only buries which
        // peer it was under a wall of identical "never produced" lines.
        for (const FNetPeerRun& Peer : Session->GetPeers())
        {
            TSharedPtr<FJsonObject> Report;
            if (Session->ReadReport(Peer.Spec.Label, Report))
            {
                continue;
            }
            // Copied out: IsProcRunning takes FProcHandle& and Peer is const here, so the
            // member cannot be handed over directly. The handle is a value, not ownership.
            FProcHandle Handle = Peer.Handle;
            if (Handle.IsValid() && !FPlatformProcess::IsProcRunning(Handle))
            {
                Test.AddError(FString::Printf(TEXT("%s peer exited (%.0fs in) without a readable report - see %s"),
                    *Peer.Spec.Label, Now - StartedAt, *Session->LogPath(Peer.Spec.Label)));
                return true;
            }
        }

        if (Now - StartedAt > TimeoutSeconds)
        {
            for (const FNetPeerRun& Peer : Session->GetPeers())
            {
                TSharedPtr<FJsonObject> Report;
                if (!Session->ReadReport(Peer.Spec.Label, Report))
                {
                    Test.AddError(FString::Printf(
                        TEXT("%s peer never produced readable JSON at %s (%.0fs) - see %s"),
                        *Peer.Spec.Label, *Session->ReportPath(Peer.Spec.Label), TimeoutSeconds,
                        *Session->LogPath(Peer.Spec.Label)));
                }
            }
            return true;
        }

        return false;
    }

    bool FNetTerminatePeersCommand::Update()
    {
        Session->TerminateAll();
        return true;
    }

    bool FNetSpawnActorCommand::Update()
    {
        UWorld* World = GetRunWorld();
        if (!World)
        {
            // Bounded: a peer whose map never loaded would otherwise sit here for the rest
            // of the run, and "the peer hung" is a much worse failure message than "no world".
            const double Now = FPlatformTime::Seconds();
            if (StartedAt == 0.0)
            {
                StartedAt = Now;
                return false;
            }
            if (Now - StartedAt > 60.0)
            {
                Test.AddError(TEXT("no world came up after 60s - check the peer's log (was a map passed on the command line?)"));
                return true;
            }
            return false;
        }

        if (!World->SpawnActor<AUnLuaNovaTestNetActor>())
        {
            Test.AddError(TEXT("spawn AUnLuaNovaTestNetActor failed - the Lua module never binds, "
                "so nothing is declared and the report would be empty"));
        }
        return true;
    }

    bool FNetWriteReportCommand::Update()
    {
        const FString Label = GetNetRole();
        if (Label.IsEmpty())
        {
            Test.AddError(TEXT("no -UnLuaNovaNetRole on the command line - a peer cannot name its own report"));
            return true;
        }

        if (!WriteNetReport(Label, Build()))
        {
            Test.AddError(FString::Printf(TEXT("could not write the %s report"), *Label));
        }
        return true;
    }
}

#endif
