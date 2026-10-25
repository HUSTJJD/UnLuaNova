// Licensed under the MIT License. See LICENSE in the repository root.

#include "NetSession.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNetReportReadinessTest,
    TEXT("UnLuaNova.API.Net.ReportReadiness"),
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)

bool FNetReportReadinessTest::RunTest(const FString& Parameters)
{
    using namespace UnLuaNovaTestSuite;
    FNetSession Session(TEXT("ReportReadiness-") + FGuid::NewGuid().ToString(), FString());
    FNetPeerSpec Peer;
    Peer.Label = TEXT("Server");
    Peer.Role = ENetPeerRole::Server;
    Session.AddPeer(Peer);

    const FString Path = Session.ReportPath(Peer.Label);
    const FString Pending = Path + TEXT(".pending-test");
    IFileManager::Get().MakeDirectory(*Session.GetDir(), true);
    TestTrue(TEXT("Write unpublished JSON"), FFileHelper::SaveStringToFile(TEXT("{}"), *Pending));
    TestFalse(TEXT("Pending file is not a published report"), Session.AllReportsPresent());

    TestTrue(TEXT("Write incomplete JSON"), FFileHelper::SaveStringToFile(TEXT("{"), *Path));
    TestFalse(TEXT("File existence does not establish report readiness"), Session.AllReportsPresent());

    TestTrue(TEXT("Complete report"), FFileHelper::SaveStringToFile(TEXT("{\"published\":true}"), *Path));
    TestTrue(TEXT("Complete JSON is ready"), Session.AllReportsPresent());
    TSharedPtr<FJsonObject> Report;
    TestTrue(TEXT("Ready report can be consumed"), Session.ReadReport(Peer.Label, Report));
    if (Report.IsValid())
        TestTrue(TEXT("Published payload is intact"), Report->GetBoolField(TEXT("published")));

    IFileManager::Get().Delete(*Path);
    IFileManager::Get().Delete(*Pending);
    IFileManager::Get().DeleteDirectory(*Session.GetDir());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
