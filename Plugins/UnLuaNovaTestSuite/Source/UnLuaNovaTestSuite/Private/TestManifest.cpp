#include "LuaBackend.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    void ExportTestManifest()
    {
        const FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("UNLUANOVA_TEST_MANIFEST"));
        if (Path.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("UNLUANOVA_TEST_MANIFEST is required"));
            return;
        }
        auto& Framework = FAutomationTestFramework::Get();
        Framework.SetRequestedTestFilter(EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ProductFilter);
        TArray<FAutomationTestInfo> Tests;
        Framework.GetValidTestNames(Tests);
        TArray<FString> Names;
        for (const auto& Test : Tests)
        {
            if (Test.GetFullTestPath().StartsWith(TEXT("UnLuaNova")))
                Names.Add(Test.GetFullTestPath());
        }
        Names.Sort();
        TArray<TSharedPtr<FJsonValue>> JsonNames;
        for (const auto& Name : Names) JsonNames.Add(MakeShared<FJsonValueString>(Name));
        auto Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("backend"), UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU ? TEXT("Luau") : TEXT("Lua"));
        Root->SetBoolField(TEXT("editor"), GIsEditor);
        Root->SetBoolField(TEXT("cooked"), FPlatformProperties::RequiresCookedData());
        Root->SetBoolField(TEXT("timer"), UNLUANOVA_HAS_STDLIB_TIMER != 0);
        Root->SetBoolField(TEXT("event"), UNLUANOVA_HAS_STDLIB_EVENT != 0);
        Root->SetBoolField(TEXT("oop"), UNLUANOVA_HAS_STDLIB_OOP != 0);
        Root->SetArrayField(TEXT("tests"), JsonNames);
        FString Text;
        const auto Writer = TJsonWriterFactory<>::Create(&Text);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        // MakeShared() hands back a TSharedRef, and Serialize() wants exactly that - there
        // is no ToSharedRef() on a ref, only on a TSharedPtr.
        if (!FJsonSerializer::Serialize(Root, Writer) || !FFileHelper::SaveStringToFile(Text, *Path))
            UE_LOG(LogTemp, Error, TEXT("Could not write test manifest: %s"), *Path);
    }

    FAutoConsoleCommand ExportManifestCommand(TEXT("UnLuaNovaTest.ExportManifest"),
        TEXT("Export registered automation leaf paths for release validation."),
        FConsoleCommandDelegate::CreateStatic(&ExportTestManifest));
}
#endif
