// Tencent is pleased to support the open source community by making UnLuaNova available.
// 
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#include "Compat/UObjectHash.h"
#include "UnLuaNovaEditorStyle.h"
#include "UnLuaNovaEditorCommands.h"
#include "Misc/CoreDelegates.h"
#include "Editor.h"
#include "BlueprintEditorModule.h"
#include "UnLuaNovaIntelliSenseGenerator.h"
#include "UnLuaNova.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "UnLuaNovaEditorSettings.h"
#include "UnLuaNovaEditorFunctionLibrary.h"
#include "UnLuaNovaFunctionLibrary.h"
#include "Kismet2/DebuggerCommands.h"
#include "Interfaces/IMainFrameModule.h"
#include "Interfaces/IPluginManager.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Toolbars/AnimationBlueprintToolbar.h"
#include "Toolbars/BlueprintToolbar.h"
#include "Toolbars/MainMenuToolbar.h"
#if ENGINE_MAJOR_VERSION > 4
#include "UObject/ObjectSaveContext.h"
#endif

#define LOCTEXT_NAMESPACE "FUnLuaNovaEditorModule"

class FUnLuaNovaEditorModule : public IModuleInterface
{
public:
    FUnLuaNovaEditorModule()
    {
    }

    virtual void StartupModule() override
    {
        Style = FUnLuaNovaEditorStyle::GetInstance();

        FUnLuaNovaEditorCommands::Register();

        // OnPostEngineInit 被弃用：新接口是 GetOnPostEngineInit()（返回同一个 delegate 的引用）
        FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FUnLuaNovaEditorModule::OnPostEngineInit);

        MainMenuToolbar = MakeShareable(new FMainMenuToolbar);
        BlueprintToolbar = MakeShareable(new FBlueprintToolbar);
        AnimationBlueprintToolbar = MakeShareable(new FAnimationBlueprintToolbar);

        UUnLuaNovaEditorFunctionLibrary::WatchScriptDirectory();

#if ENGINE_MAJOR_VERSION > 4
        UPackage::PreSavePackageWithContextEvent.AddRaw(this, &FUnLuaNovaEditorModule::OnPackageSavingWithContext);
        UPackage::PackageSavedWithContextEvent.AddRaw(this, &FUnLuaNovaEditorModule::OnPackageSavedWithContext);
#else
        UPackage::PreSavePackageEvent.AddRaw(this, &FUnLuaNovaEditorModule::OnPackageSaving);
        UPackage::PackageSavedEvent.AddRaw(this, &FUnLuaNovaEditorModule::OnPackageSaved);
#endif
        SetupPackagingSettings();
    }

    virtual void ShutdownModule() override
    {
        FUnLuaNovaEditorCommands::Unregister();
        FCoreDelegates::GetOnPostEngineInit().RemoveAll(this);
        UnregisterSettings();

#if ENGINE_MAJOR_VERSION > 4
        UPackage::PreSavePackageWithContextEvent.RemoveAll(this);
        UPackage::PackageSavedWithContextEvent.RemoveAll(this);
#else
        UPackage::PreSavePackageEvent.RemoveAll(this);
        UPackage::PackageSavedEvent.RemoveAll(this);
#endif
    }

private:
    void OnPostEngineInit()
    {
        RegisterSettings();

        if (!GEditor)
        {
            // Loading MainFrame module with '-game' is not supported
            return;
        }

        MainMenuToolbar->Initialize();
        BlueprintToolbar->Initialize();
        AnimationBlueprintToolbar->Initialize();
        FUnLuaNovaIntelliSenseGenerator::Get()->Initialize();

        IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
        MainFrameModule.OnMainFrameCreationFinished().AddRaw(this, &FUnLuaNovaEditorModule::OnMainFrameCreationFinished);
    }

    void OnMainFrameCreationFinished(TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow)
    {
        // register default key input to 'HotReload' Lua
        FPlayWorldCommands::GlobalPlayWorldActions->MapAction(
            FUnLuaNovaEditorCommands::Get().HotReload, FExecuteAction::CreateStatic(UUnLuaNovaFunctionLibrary::HotReload), FCanExecuteAction());
    }

    void RegisterSettings() const
    {
        ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
        if (SettingsModule)
        {
            const TSharedPtr<ISettingsSection> Section = SettingsModule->RegisterSettings("Project", "Plugins", "UnLuaNova Editor",
                                                                                          LOCTEXT("UnLuaNovaEditorSettingsName", "UnLuaNova Editor"),
                                                                                          LOCTEXT("UnLuaNovaEditorSettingsDescription", "UnLuaNova Editor Settings"),
                                                                                          GetMutableDefault<UUnLuaNovaEditorSettings>());
            Section->OnModified().BindRaw(this, &FUnLuaNovaEditorModule::OnSettingsModified);
        }
    }

    void UnregisterSettings() const
    {
        ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
        if (SettingsModule)
            SettingsModule->UnregisterSettings("Project", "Plugins", "UnLuaNova Editor");
    }

    bool OnSettingsModified() const
    {
        const auto BuildFile = IPluginManager::Get().FindPlugin(TEXT("UnLuaNova"))->GetBaseDir() / "Source/UnLuaNova/UnLuaNova.Build.cs";
        auto& FileManager = IFileManager::Get();
        if (FileManager.FileExists(*BuildFile))
            FileManager.SetTimeStamp(*BuildFile, FDateTime::UtcNow());
        return true;
    }

#if ENGINE_MAJOR_VERSION > 4
    void OnPackageSavingWithContext(UPackage* Package, FObjectPreSaveContext Context)
    {
        OnPackageSaving(Package);
    }

    void OnPackageSavedWithContext(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext Context)
    {
        OnPackageSaved(PackageFileName, Package);
    }
#endif

    void OnPackageSaving(UPackage* Package)
    {
        if (!GEditor || !GEditor->PlayWorld)
            return;

        ForEachObjectWithPackage(Package, [this, Package](UObject* Object)
        {
            const auto Class = Cast<UClass>(Object);
            if (!Class || Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
                return true;
            SuspendedPackages.Add(Package, Class);
            return false;
            // 第三个参数原来是 bool bIncludeNestedObjects，5.x 换成了 EGetObjectsFlags
        }, EGetObjectsFlags::None);

        for (const auto Pair : SuspendedPackages)
            ULuaFunction::SuspendOverrides(Pair.Value);
    }

    void OnPackageSaved(const FString& PackageFileName, UObject* Outer)
    {
        if (!GEditor || !GEditor->PlayWorld)
            return;

        UPackage* Package = (UPackage*)Outer;
        if (SuspendedPackages.Contains(Package))
        {
            ULuaFunction::ResumeOverrides(SuspendedPackages[Package]);
            SuspendedPackages.Remove(Package);
        }
    }

    void SetupPackagingSettings()
    {
        auto ScriptPaths = TArray<FString>{TEXT("Script"), TEXT("../Plugins/UnLuaNova/Content/Script")};

        const auto Plugins = IPluginManager::Get().GetEnabledPlugins();
        for (const auto Plugin : Plugins)
        {
            if (!Plugin->CanContainContent())
                continue;

            const auto ContentDir = Plugin->GetContentDir();
            if (!ContentDir.Contains("UnLuaNovaExtensions"))
                continue;

            auto ScriptPath = ContentDir / "Script";
            if (!FPaths::DirectoryExists(ScriptPath))
                continue;

            if (FPaths::MakePathRelativeTo(ScriptPath, *FPaths::ProjectContentDir()))
                ScriptPaths.Add(ScriptPath);
        }

        const auto PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();
        bool bModified = false;
        auto Exists = [&](const auto Path)
        {
            for (const auto& DirPath : PackagingSettings->DirectoriesToAlwaysStageAsUFS)
            {
                if (DirPath.Path == Path)
                    return true;
            }
            return false;
        };

        for (auto& ScriptPath : ScriptPaths)
        {
            if (Exists(ScriptPath))
                continue;

            FDirectoryPath DirectoryPath;
            DirectoryPath.Path = ScriptPath;
            PackagingSettings->DirectoriesToAlwaysStageAsUFS.Add(DirectoryPath);
            bModified = true;
        }

        if (bModified)
        {
#if ENGINE_MAJOR_VERSION >= 5
            PackagingSettings->TryUpdateDefaultConfigFile();
#else
            PackagingSettings->UpdateDefaultConfigFile();
#endif
        }
    }

    TSharedPtr<FBlueprintToolbar> BlueprintToolbar;
    TSharedPtr<FAnimationBlueprintToolbar> AnimationBlueprintToolbar;
    TSharedPtr<FMainMenuToolbar> MainMenuToolbar;
    TSharedPtr<FUnLuaNovaIntelliSenseGenerator> IntelliSenseGenerator;
    TSharedPtr<ISlateStyle> Style;
    TMap<UPackage*, UClass*> SuspendedPackages;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnLuaNovaEditorModule, UnLuaNovaEditor)
