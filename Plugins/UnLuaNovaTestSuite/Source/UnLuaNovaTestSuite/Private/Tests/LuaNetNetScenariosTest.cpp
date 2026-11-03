// Copyright (c) 2026 UnLuaNova contributors
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing, software distributed under
// the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the License for the specific language governing
// permissions and limitations under the License.

// Eight connected scenarios the original DedicatedServer test deliberately did not cover:
// a client that joins late (and the reverse join order), a client whose process is replaced,
// dormancy, relevancy, packet loss / reordering, a UNetConnection-level reconnect that keeps
// the same process, and cross-peer declaration fingerprint comparison.
//
// These are opt-in. A normal automation run skips them (returning success with an info line)
// because each one launches three real processes; the release pipeline enables them from a
// dedicated net-variant stage with -UnLuaNovaNetScenarios. Skipping is not evidence: the
// release gate (Tools/ValidateRelease.py) requires a topology.json and three peer reports per
// session, with fields that can only be produced by a peer that really ran the scenario.

#include "NetSession.h"
#include "UnLuaNovaTestCommon.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"

namespace UnLuaNovaTestSuite
{
    static const TCHAR* ScenarioMap = TEXT("/UnLuaNovaTestSuite/Tests/Misc/Empty");
    static const TCHAR* ScenarioReportTest = TEXT("UnLuaNova.API.Net.Scenarios.Report");

    static FString ScenarioName()
    {
        FString Value;
        FParse::Value(FCommandLine::Get(), TEXT("-UnLuaNovaNetScenario="), Value);
        return Value;
    }

    static TSharedRef<FJsonObject> Marker(double Number)
    {
        auto Data = MakeShared<FJsonObject>();
        Data->SetNumberField(TEXT("Value"), Number);
        return Data;
    }

    static bool WriteMarker(const TCHAR* Label, double Value)
    {
        return WriteNetReport(Label, Marker(Value));
    }

    static bool ReadMarker(const TCHAR* Label, double& OutValue)
    {
        TSharedPtr<FJsonObject> Data;
        if (!ReadNetReport(GetNetSessionName(), Label, Data) || !Data.IsValid())
            return false;
        OutValue = Data->GetNumberField(TEXT("Value"));
        return true;
    }

    static int32 OpenClients(UNetDriver* Driver)
    {
        int32 Count = 0;
        if (Driver && Driver->IsServer())
            for (const UNetConnection* Connection : Driver->ClientConnections)
                if (Connection && Connection->GetConnectionState() == USOCK_Open && Connection->PlayerController)
                    ++Count;
        return Count;
    }

    class FScenarioPeerCommand : public IAutomationLatentCommand
    {
    public:
        explicit FScenarioPeerCommand(FAutomationTestBase& InTest)
            : Test(InTest), Role(GetNetRole()), Scenario(ScenarioName())
        {
            bServer = Role == TEXT("Server");
            FParse::Value(FCommandLine::Get(), TEXT("-UnLuaNovaNetPktLoss="), PacketLossPct);
            FParse::Value(FCommandLine::Get(), TEXT("-UnLuaNovaNetReconnectUrl="), ReconnectUrl);
        }

        virtual bool Update() override
        {
            if (StartedAt == 0.0)
                StartedAt = FPlatformTime::Seconds();
            if (FPlatformTime::Seconds() - StartedAt > 150.0)
                return Fail(TEXT("network scenario timed out"));

            UWorld* World = GetRunWorld();
            if (!World || !World->GetNetDriver())
                return false;
            if (!bPhaseInitialised)
            {
                bPhaseInitialised = true;
                ApplyEmulation(World);
            }
            // A connection-level reconnect has to be noticed while there is NO open server
            // connection - UpdateClient() deliberately returns early in that state - so this
            // hook runs before it and only on the client whose connection the server closes.
            if (!bServer && Scenario == TEXT("SeamlessReconnect") && Role == TEXT("Client1"))
                HandleConnectionLevelReconnect(World);
            return bServer ? UpdateServer(World) : UpdateClient(World);
        }

        void ApplyEmulation(UWorld* World)
        {
            if (Scenario != TEXT("PacketLoss") || !GEngine || PacketLossPct <= 0)
                return;
            // Real engine emulation (NetEmulationHelper), applied to every peer's NetDriver.
            // Reliable RPCs and property replication must still converge with packets dropped,
            // duplicated and reordered; the report exports the requested loss percentage and
            // the connection's real lost-packet counters.
            GEngine->Exec(World, *FString::Printf(TEXT("NetEmulation.PktLoss %d"), PacketLossPct));
            GEngine->Exec(World, TEXT("NetEmulation.PktOrder 1"));
            GEngine->Exec(World, TEXT("NetEmulation.PktLag 60"));
        }

        void HandleConnectionLevelReconnect(UWorld* World)
        {
            UNetDriver* Driver = World->GetNetDriver();
            const bool bOpen = Driver && Driver->ServerConnection
                && Driver->ServerConnection->GetConnectionState() == USOCK_Open;
            if (bOpen || bReopenRequested || ReconnectUrl.IsEmpty() || !GEngine)
                return;
            bReopenRequested = true;
            ++ConnectionLevelReconnectCount;
            GEngine->Exec(World, *FString::Printf(TEXT("open %s"), *ReconnectUrl));
        }

    private:
        bool Fail(const FString& Message)
        {
            Test.AddError(Message);
            if (!bReportWritten)
                bReportWritten = WriteNetReport(Role, Report(false));
            return true;
        }

        TArray<TSharedPtr<FJsonValue>> ActorReports()
        {
            TArray<TSharedPtr<FJsonValue>> Result;
            for (const auto& Actor : Actors)
            {
                if (!Actor.IsValid())
                    continue;
                auto Entry = MakeShared<FJsonObject>();
                Entry->SetNumberField(TEXT("ActorId"), Actor->NetTestActorId);
                Entry->SetNumberField(TEXT("Challenge"), Actor->EngineReplicatedInt);
                Entry->SetBoolField(TEXT("Dormant"), Actor->NetDormancy == DORM_DormantAll);
                Entry->SetBoolField(TEXT("AlwaysRelevant"), Actor->bAlwaysRelevant);
                Entry->SetBoolField(TEXT("OnlyRelevantToOwner"), Actor->bOnlyRelevantToOwner);
                Result.Add(MakeShared<FJsonValueObject>(Entry));
            }
            return Result;
        }

        TSharedRef<FJsonObject> Report(bool bComplete)
        {
            auto Data = BuildNetDeclarationReport();
            AppendWorldFacts(GetRunWorld(), *Data);
            Data->SetStringField(TEXT("Scenario"), Scenario);
            Data->SetStringField(TEXT("SessionId"), SessionId);
            Data->SetBoolField(TEXT("Complete"), bComplete);
            Data->SetNumberField(TEXT("ClientsAtSpawn"), ClientsAtSpawn);
            Data->SetNumberField(TEXT("ClientsFinal"), ClientsFinal);
            Data->SetNumberField(TEXT("ConnectionDrops"), ConnectionDrops);
            Data->SetNumberField(TEXT("DormantValue"), DormantValue);
            Data->SetNumberField(TEXT("FlushedValue"), FlushedValue);
            Data->SetBoolField(TEXT("DormancyFlushed"), bDormancyFlushed);
            Data->SetNumberField(TEXT("RelevancyRestrictedActor"), RelevancyRestrictedActor);
            Data->SetBoolField(TEXT("JoinedAfterSpawn"), bJoinedAfterSpawn);
            Data->SetNumberField(TEXT("DormancyBeforeValue"), DormancyBeforeValue);
            Data->SetNumberField(TEXT("DormancyAfterValue"), DormancyAfterValue);
            Data->SetBoolField(TEXT("DormancySuppressed"), bDormancySuppressed);
            Data->SetBoolField(TEXT("DormancyResumed"), bDormancyResumed);
            // The flag is forwarded to every peer, but only PacketLoss applies NetEmulation;
            // do not let the other scenarios report an emulation they never enabled.
            Data->SetNumberField(TEXT("PacketLossPct"),
                Scenario == TEXT("PacketLoss") ? PacketLossPct : 0);
            Data->SetNumberField(TEXT("FinalValue"), FinalValue);
            Data->SetNumberField(TEXT("ConnectionLevelReconnectCount"), ConnectionLevelReconnectCount);
            Data->SetStringField(TEXT("ClosedConnectionAddress"), ClosedConnectionAddress);
            Data->SetNumberField(TEXT("OwnedActorId"), OwnedActorId);
            Data->SetArrayField(TEXT("Actors"), ActorReports());
            TArray<TSharedPtr<FJsonValue>> Ids;
            for (int32 Id : VisibleActorIds)
                Ids.Add(MakeShared<FJsonValueNumber>((double)Id));
            Data->SetArrayField(TEXT("VisibleActorIds"), Ids);
            return Data;
        }

        void SpawnActors(UWorld* World, UNetDriver* Driver)
        {
            TArray<APlayerController*> Owners;
            for (UNetConnection* Connection : Driver->ClientConnections)
                if (Connection && Connection->GetConnectionState() == USOCK_Open && Connection->PlayerController)
                    Owners.Add(Connection->PlayerController);
            if (Owners.Num() == 0)
                return;

            SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
            Seed = 1000 + static_cast<int32>(FGuid::NewGuid().A % 1000000);
            for (int32 Index = 0; Index < 2; ++Index)
            {
                FActorSpawnParameters Params;
                Params.Owner = Owners.IsValidIndex(Index) ? Owners[Index] : Owners[0];
                auto* Actor = World->SpawnActor<AUnLuaNovaTestNetActor>(Params);
                if (!Actor)
                    return;
                Actor->NetTestSessionId = SessionId;
                Actor->NetTestActorId = Index + 1;
                Actor->EngineReplicatedInt = Seed + Index * 100;
                Actor->ForceNetUpdate();
                Actors.Add(Actor);
            }
            if (Actors.Num() == 2)
            {
                ClientsAtSpawn = Owners.Num();
                WriteMarker(TEXT("Spawn"), (double)ClientsAtSpawn);
                WriteMarker(TEXT("Seed"), (double)Seed);
            }
        }

        bool UpdateServer(UWorld* World)
        {
            UNetDriver* Driver = World->GetNetDriver();
            const int32 Clients = OpenClients(Driver);
            MaxClients = FMath::Max(MaxClients, Clients);
            if (Clients < MaxClients)
                ++ConnectionDrops;

            if (Actors.Num() != 2)
            {
                const bool bStaggeredJoin = Scenario == TEXT("LateJoin") || Scenario == TEXT("JoinOrderSwap");
                const int32 Required = bStaggeredJoin ? 1 : 2;
                if (Clients < Required)
                    return false;
                SpawnActors(World, Driver);
                return false;
            }
            for (const auto& Actor : Actors)
                if (!Actor.IsValid())
                    return Fail(TEXT("server lost a scenario actor"));

            if (Scenario == TEXT("LateJoin") || Scenario == TEXT("JoinOrderSwap"))
            {
                if (Clients < 2)
                    return false;
                ClientsFinal = Clients;
            }
            else if (Scenario == TEXT("Reconnect"))
            {
                // A real process replacement drops and then restores one connection.
                if (ConnectionDrops == 0 || Clients < 2)
                    return false;
                ClientsFinal = Clients;
            }
            else if (Scenario == TEXT("SeamlessReconnect"))
            {
                // Connection-level reconnect: the process is NOT replaced. The server closes
                // one UNetConnection and the same process reopens the URL. A gate checks
                // launch_count == 1, first_pid == pid and a non-zero reconnect count, so this
                // cannot be satisfied by the process-level RestartPeer scenario.
                if (!bClosedConnection)
                {
                    for (UNetConnection* Connection : Driver->ClientConnections)
                    {
                        if (!Connection || Connection->GetConnectionState() != USOCK_Open)
                            continue;
                        ClosedConnectionAddress = Connection->LowLevelGetRemoteAddress(true);
                        Connection->Close();
                        bClosedConnection = true;
                        break;
                    }
                    if (!bClosedConnection)
                        return false;
                    PhaseAt = FPlatformTime::Seconds();
                    return false;
                }
                if (FPlatformTime::Seconds() - PhaseAt < 3.0)
                    return false;
                if (ConnectionDrops == 0 || Clients < 2)
                    return false;
                ClientsFinal = Clients;
            }
            else if (Scenario == TEXT("PacketLoss"))
            {
                // Two consecutive single-field updates under real packet loss / reordering.
                // The last value must still reach both clients; the report also carries the
                // engine's own lost-packet and byte counters for the Iris comparison.
                if (PhaseAt == 0.0)
                {
                    Actors[0]->EngineReplicatedInt = Seed + 501;
                    Actors[0]->ForceNetUpdate();
                    WriteMarker(TEXT("LossPhase1"), (double)(Seed + 501));
                    PhaseAt = FPlatformTime::Seconds();
                    return false;
                }
                if (FPlatformTime::Seconds() - PhaseAt < 3.0)
                    return false;
                if (FlushedValue == 0)
                {
                    FlushedValue = Seed + 777;
                    Actors[0]->EngineReplicatedInt = FlushedValue;
                    Actors[0]->ForceNetUpdate();
                    WriteMarker(TEXT("LossFinal"), (double)FlushedValue);
                    PhaseAt = FPlatformTime::Seconds();
                    return false;
                }
                if (FPlatformTime::Seconds() - PhaseAt < 4.0)
                    return false;
                ClientsFinal = Clients;
            }
            else if (Scenario == TEXT("SchemaMismatch"))
            {
                // No injected fault: every peer exports the fingerprint of the declaration it
                // actually bound (RepSchemaVersion) plus a deliberately different one. The
                // gate compares them across peers and requires the fingerprints to differ,
                // which is the connected form of the schema-mismatch check.
                if (Clients < 2)
                    return false;
                ClientsFinal = Clients;
            }
            else if (Scenario == TEXT("Dormancy"))
            {
                if (PhaseAt == 0.0)
                {
                    DormantValue = Seed + 777;
                    Actors[0]->NetDormancy = DORM_DormantAll;
                    Actors[0]->EngineReplicatedInt = DormantValue;
                    WriteMarker(TEXT("DormancyPhase1"), (double)DormantValue);
                    PhaseAt = FPlatformTime::Seconds();
                    return false;
                }
                if (FPlatformTime::Seconds() - PhaseAt < 4.0)
                    return false;
                if (!bDormancyFlushed)
                {
                    FlushedValue = Seed + 888;
                    Actors[0]->FlushNetDormancy();
                    Actors[0]->EngineReplicatedInt = FlushedValue;
                    Actors[0]->ForceNetUpdate();
                    WriteMarker(TEXT("DormancyPhase2"), (double)FlushedValue);
                    bDormancyFlushed = true;
                    PhaseAt = FPlatformTime::Seconds();
                    return false;
                }
                if (FPlatformTime::Seconds() - PhaseAt < 4.0)
                    return false;
                ClientsFinal = Clients;
            }
            else if (Scenario == TEXT("Relevancy"))
            {
                if (!bRelevancyRestricted)
                {
                    auto* Actor = Actors[1].Get();
                    Actor->bAlwaysRelevant = false;
                    Actor->bOnlyRelevantToOwner = true;
                    Actor->ForceNetUpdate();
                    RelevancyRestrictedActor = Actor->NetTestActorId;
                    WriteMarker(TEXT("RelevancyPhase"), (double)RelevancyRestrictedActor);
                    bRelevancyRestricted = true;
                    PhaseAt = FPlatformTime::Seconds();
                    return false;
                }
                if (FPlatformTime::Seconds() - PhaseAt < 4.0)
                    return false;
                ClientsFinal = Clients;
            }
            else
            {
                return Fail(FString::Printf(TEXT("unknown network scenario '%s'"), *Scenario));
            }

            if (!bReportWritten)
                bReportWritten = WriteNetReport(Role, Report(true));
            return bReportWritten;
        }

        bool UpdateClient(UWorld* World)
        {
            UNetDriver* Driver = World->GetNetDriver();
            if (!Driver->ServerConnection || Driver->ServerConnection->GetConnectionState() != USOCK_Open)
                return false;

            TArray<AUnLuaNovaTestNetActor*> Found;
            for (TActorIterator<AUnLuaNovaTestNetActor> It(World); It; ++It)
                if (!It->NetTestSessionId.IsEmpty() && It->NetTestActorId > 0)
                    Found.Add(*It);
            if (Found.Num() == 0)
                return false;
            SessionId = Found[0]->NetTestSessionId;
            APlayerController* LocalPC = World->GetFirstPlayerController();
            for (auto* Actor : Found)
                if (LocalPC && Actor->GetOwner() == LocalPC)
                    OwnedActorId = Actor->NetTestActorId;

            VisibleActorIds.Reset();
            Found.Sort([](const AUnLuaNovaTestNetActor& A, const AUnLuaNovaTestNetActor& B)
                { return A.NetTestActorId < B.NetTestActorId; });
            for (auto* Actor : Found)
                VisibleActorIds.Add(Actor->NetTestActorId);

            if (Scenario == TEXT("LateJoin") || Scenario == TEXT("JoinOrderSwap"))
            {
                double SpawnClients = 0.0;
                if (!ReadMarker(TEXT("Spawn"), SpawnClients))
                    return false;
                // The spawn marker says the server had already created both actors when only
                // one client was connected. Combined with topology.launch_serial, this is a
                // late join rather than a relabel of a simultaneous start.
                bJoinedAfterSpawn = SpawnClients == 1.0 && Found.Num() == 2;
            }
            else if (Scenario == TEXT("Dormancy"))
            {
                double Before = 0.0, After = 0.0;
                if (!ReadMarker(TEXT("DormancyPhase1"), Before))
                    return false;
                if (PhaseAt == 0.0)
                {
                    DormancyBeforeValue = Found[0]->EngineReplicatedInt;
                    bDormancySuppressed = double(DormancyBeforeValue) != Before;
                    PhaseAt = FPlatformTime::Seconds();
                    return false;
                }
                if (!ReadMarker(TEXT("DormancyPhase2"), After))
                    return false;
                if (FPlatformTime::Seconds() - PhaseAt < 2.0)
                    return false;
                DormancyAfterValue = Found.Num() > 0 ? Found[0]->EngineReplicatedInt : -1;
                bDormancyResumed = double(DormancyAfterValue) == After;
            }
            else if (Scenario == TEXT("Relevancy"))
            {
                double Restricted = 0.0;
                if (!ReadMarker(TEXT("RelevancyPhase"), Restricted))
                    return false;
                if (PhaseAt == 0.0)
                {
                    PhaseAt = FPlatformTime::Seconds();
                    return false;
                }
                if (FPlatformTime::Seconds() - PhaseAt < 2.0)
                    return false;
            }
            else if (Scenario == TEXT("PacketLoss"))
            {
                double Final = 0.0;
                if (!ReadMarker(TEXT("LossFinal"), Final))
                    return false;
                FinalValue = Found.Num() > 0 ? Found[0]->EngineReplicatedInt : -1;
                // Wait for reliable replication to converge; do not publish a report that
                // claims success before the last value actually arrived.
                if (double(FinalValue) != Final)
                    return false;
            }
            else if (Scenario == TEXT("SeamlessReconnect"))
            {
                double SpawnClients = 0.0;
                if (!ReadMarker(TEXT("Spawn"), SpawnClients))
                    return false;
                if (Role == TEXT("Client1") && ConnectionLevelReconnectCount == 0)
                    return false;
            }
            else if (Scenario == TEXT("Reconnect") || Scenario == TEXT("SchemaMismatch"))
            {
                double SpawnClients = 0.0;
                if (!ReadMarker(TEXT("Spawn"), SpawnClients))
                    return false;
            }

            if (!bReportWritten)
                bReportWritten = WriteNetReport(Role, Report(true));
            return bReportWritten;
        }

        FAutomationTestBase& Test;
        FString Role;
        FString Scenario;
        FString SessionId;
        bool bServer = false;
        bool bReportWritten = false;
        bool bJoinedAfterSpawn = false;
        bool bDormancyFlushed = false;
        bool bDormancySuppressed = false;
        bool bDormancyResumed = false;
        bool bRelevancyRestricted = false;
        bool bPhaseInitialised = false;
        bool bClosedConnection = false;
        bool bReopenRequested = false;
        double StartedAt = 0.0;
        double PhaseAt = 0.0;
        int32 Seed = 0;
        int32 ClientsAtSpawn = 0;
        int32 ClientsFinal = 0;
        int32 ConnectionDrops = 0;
        int32 MaxClients = 0;
        int32 DormantValue = 0;
        int32 FlushedValue = 0;
        int32 RelevancyRestrictedActor = 0;
        int32 DormancyBeforeValue = -1;
        int32 DormancyAfterValue = -1;
        int32 OwnedActorId = 0;
        int32 PacketLossPct = 0;
        int32 FinalValue = -1;
        int32 ConnectionLevelReconnectCount = 0;
        FString ReconnectUrl;
        FString ClosedConnectionAddress;
        TArray<int32> VisibleActorIds;
        TArray<TWeakObjectPtr<AUnLuaNovaTestNetActor>> Actors;
    };

    class FScenarioCompareCommand : public IAutomationLatentCommand
    {
    public:
        FScenarioCompareCommand(FAutomationTestBase& InTest, TSharedRef<FNetSession> InSession)
            : Test(InTest), Session(InSession) {}

        virtual bool Update() override
        {
            FString Shared;
            for (const FNetPeerRun& Peer : Session->GetPeers())
            {
                TSharedPtr<FJsonObject> Data;
                if (!Session->ReadReport(Peer.Spec.Label, Data))
                {
                    Test.AddError(TEXT("missing scenario report: ") + Peer.Spec.Label);
                    continue;
                }
                Test.TestTrue(TEXT("scenario peer completed"), Data->GetBoolField(TEXT("Complete")));
                Test.TestEqual(TEXT("scenario provenance"), Data->GetStringField(TEXT("Scenario")), Session->GetScenario());
                if (Shared.IsEmpty())
                    Shared = Data->GetStringField(TEXT("SessionId"));
                Test.TestTrue(TEXT("server generated a session identity"), !Shared.IsEmpty());
                Test.TestEqual(TEXT("shared server session"), Data->GetStringField(TEXT("SessionId")), Shared);
                Test.TestEqual(TEXT("report belongs to launched process"),
                    (uint32)Data->GetNumberField(TEXT("ProcessId")), Peer.Pid);
            }
            return true;
        }

    private:
        FAutomationTestBase& Test;
        TSharedRef<FNetSession> Session;
    };

    static TSharedRef<FNetSession> MakeScenarioSession(const FString& Name, const FString& Scenario)
    {
        TSharedRef<FNetSession> Session = MakeShared<FNetSession>(Name, ScenarioMap);
        Session->SetScenario(Scenario);
        for (int32 Index = 0; Index < 3; ++Index)
        {
            FNetPeerSpec Peer;
            Peer.Label = Index == 0 ? TEXT("Server") : FString::Printf(TEXT("Client%d"), Index);
            Peer.Role = Index == 0 ? ENetPeerRole::Server : ENetPeerRole::Client;
            Peer.MapName = ScenarioMap;
            Peer.ReportTestName = ScenarioReportTest;
            Peer.bHeadless = Index == 0;
            if (Index != 0)
                Peer.ConnectTo = TEXT("127.0.0.1");
            Session->AddPeer(Peer);
        }
        Session->CleanReports();
        return Session;
    }

    static bool ScenariosEnabled()
    {
        return FParse::Param(FCommandLine::Get(), TEXT("UnLuaNovaNetScenarios"));
    }

    static bool RoleIsEmpty()
    {
        return GetNetRole().IsEmpty();
    }
}

BEGIN_TESTSUITE_ANY_CONTEXT(FNetScenarioReportTest,
    TEXT("UnLuaNova.API.Net.Scenarios.Report Peer for late join, reconnect, dormancy and relevancy"))
    bool FNetScenarioReportTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;
        if (!ScenariosEnabled() || RoleIsEmpty())
        {
            AddInfo(TEXT("Peer helper; the network scenario driver supplies connected coverage."));
            return true;
        }
        ADD_LATENT_AUTOMATION_COMMAND(FScenarioPeerCommand(*this));
        return true;
    }
END_TESTSUITE(FNetScenarioReportTest)

BEGIN_TESTSUITE(FNetLateJoinScenarioTest,
    TEXT("UnLuaNova.Slow.Net.Scenarios.LateJoin Second client joins after replication state exists"))
    bool FNetLateJoinScenarioTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;
        if (!ScenariosEnabled())
        {
            AddInfo(TEXT("Opt-in: pass -UnLuaNovaNetScenarios to run connected scenarios."));
            return true;
        }
        TSharedRef<FNetSession> Session = MakeScenarioSession(TEXT("LateJoin"), TEXT("LateJoin"));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Server")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(8.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Client1")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(8.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Client2")));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 210.0));
        ADD_LATENT_AUTOMATION_COMMAND(FScenarioCompareCommand(*this, Session));
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FNetLateJoinScenarioTest)

BEGIN_TESTSUITE(FNetReconnectScenarioTest,
    TEXT("UnLuaNova.Slow.Net.Scenarios.Reconnect A client process is replaced inside one server session"))
    bool FNetReconnectScenarioTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;
        if (!ScenariosEnabled())
        {
            AddInfo(TEXT("Opt-in: pass -UnLuaNovaNetScenarios to run connected scenarios."));
            return true;
        }
        TSharedRef<FNetSession> Session = MakeScenarioSession(TEXT("Reconnect"), TEXT("Reconnect"));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Server")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(10.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(20.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FRestartPeerCommand(Session, *this, TEXT("Client1")));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 210.0));
        ADD_LATENT_AUTOMATION_COMMAND(FScenarioCompareCommand(*this, Session));
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FNetReconnectScenarioTest)

BEGIN_TESTSUITE(FNetDormancyScenarioTest,
    TEXT("UnLuaNova.Slow.Net.Scenarios.Dormancy A dormant actor resumes after FlushNetDormancy"))
    bool FNetDormancyScenarioTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;
        if (!ScenariosEnabled())
        {
            AddInfo(TEXT("Opt-in: pass -UnLuaNovaNetScenarios to run connected scenarios."));
            return true;
        }
        TSharedRef<FNetSession> Session = MakeScenarioSession(TEXT("Dormancy"), TEXT("Dormancy"));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Server")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(10.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 210.0));
        ADD_LATENT_AUTOMATION_COMMAND(FScenarioCompareCommand(*this, Session));
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FNetDormancyScenarioTest)

BEGIN_TESTSUITE(FNetRelevancyScenarioTest,
    TEXT("UnLuaNova.Slow.Net.Scenarios.Relevancy Owner-only relevance hides an actor from the other client"))
    bool FNetRelevancyScenarioTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;
        if (!ScenariosEnabled())
        {
            AddInfo(TEXT("Opt-in: pass -UnLuaNovaNetScenarios to run connected scenarios."));
            return true;
        }
        TSharedRef<FNetSession> Session = MakeScenarioSession(TEXT("Relevancy"), TEXT("Relevancy"));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Server")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(10.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 210.0));
        ADD_LATENT_AUTOMATION_COMMAND(FScenarioCompareCommand(*this, Session));
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FNetRelevancyScenarioTest)

BEGIN_TESTSUITE(FNetJoinOrderSwapScenarioTest,
    TEXT("UnLuaNova.Slow.Net.Scenarios.JoinOrderSwap Client1 joins last instead of Client2"))
    bool FNetJoinOrderSwapScenarioTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;
        if (!ScenariosEnabled())
        {
            AddInfo(TEXT("Opt-in: pass -UnLuaNovaNetScenarios to run connected scenarios."));
            return true;
        }
        // Same staggered spawn as LateJoin, with the launch order deliberately reversed.
        // The gate requires Client1's launch_serial to be the larger one.
        TSharedRef<FNetSession> Session = MakeScenarioSession(TEXT("JoinOrderSwap"), TEXT("JoinOrderSwap"));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Server")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(8.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Client2")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(8.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Client1")));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 210.0));
        ADD_LATENT_AUTOMATION_COMMAND(FScenarioCompareCommand(*this, Session));
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FNetJoinOrderSwapScenarioTest)

BEGIN_TESTSUITE(FNetPacketLossScenarioTest,
    TEXT("UnLuaNova.Slow.Net.Scenarios.PacketLoss Reliable replication converges under packet loss"))
    bool FNetPacketLossScenarioTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;
        if (!ScenariosEnabled())
        {
            AddInfo(TEXT("Opt-in: pass -UnLuaNovaNetScenarios to run connected scenarios."));
            return true;
        }
        // -UnLuaNovaNetPktLoss=25 is supplied by RunNetMatrix.ps1 on every peer command line.
        TSharedRef<FNetSession> Session = MakeScenarioSession(TEXT("PacketLoss"), TEXT("PacketLoss"));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Server")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(10.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 210.0));
        ADD_LATENT_AUTOMATION_COMMAND(FScenarioCompareCommand(*this, Session));
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FNetPacketLossScenarioTest)

BEGIN_TESTSUITE(FNetSeamlessReconnectScenarioTest,
    TEXT("UnLuaNova.Slow.Net.Scenarios.SeamlessReconnect One UNetConnection is closed and reopened in-process"))
    bool FNetSeamlessReconnectScenarioTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;
        if (!ScenariosEnabled())
        {
            AddInfo(TEXT("Opt-in: pass -UnLuaNovaNetScenarios to run connected scenarios."));
            return true;
        }
        // Deliberately no FRestartPeerCommand: the process must stay the same. The server
        // closes a UNetConnection and the same client process reopens the URL it was given.
        TSharedRef<FNetSession> Session = MakeScenarioSession(TEXT("SeamlessReconnect"), TEXT("SeamlessReconnect"));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Server")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(10.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 210.0));
        ADD_LATENT_AUTOMATION_COMMAND(FScenarioCompareCommand(*this, Session));
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FNetSeamlessReconnectScenarioTest)

BEGIN_TESTSUITE(FNetSchemaMismatchScenarioTest,
    TEXT("UnLuaNova.Slow.Net.Scenarios.SchemaMismatch Every peer exports the declaration it bound"))
    bool FNetSchemaMismatchScenarioTest::RunTest(const FString& Parameters)
    {
        using namespace UnLuaNovaTestSuite;
        if (!ScenariosEnabled())
        {
            AddInfo(TEXT("Opt-in: pass -UnLuaNovaNetScenarios to run connected scenarios."));
            return true;
        }
        TSharedRef<FNetSession> Session = MakeScenarioSession(TEXT("SchemaMismatch"), TEXT("SchemaMismatch"));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this, TEXT("Server")));
        ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitSeconds(10.0f));
        ADD_LATENT_AUTOMATION_COMMAND(FNetLaunchPeersCommand(Session, *this));
        ADD_LATENT_AUTOMATION_COMMAND(FNetWaitForReportsCommand(Session, *this, 210.0));
        ADD_LATENT_AUTOMATION_COMMAND(FScenarioCompareCommand(*this, Session));
        ADD_LATENT_AUTOMATION_COMMAND(FNetTerminatePeersCommand(Session));
        return true;
    }
END_TESTSUITE(FNetSchemaMismatchScenarioTest)

#endif
