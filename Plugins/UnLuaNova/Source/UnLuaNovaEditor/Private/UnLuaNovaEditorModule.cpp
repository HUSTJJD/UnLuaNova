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
#include "Commandlets/UnLuaNovaStaticBindingGenerator.h"
#include "UnLuaNova.h"
#include "UnLuaNovaBase.h"   // LogUnLuaNova
#include "UnLuaNovaSettings.h"   // 特性开关（bDebugAdapter / DebugAdapterPort）
#include "LuaDebugAdapter.h"
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
        RegisterDapCommand();
    }

    virtual void ShutdownModule() override
    {
        if (FLuaDebugAdapter::Get().IsRunning())
            FLuaDebugAdapter::Get().StopServer();

        if (DapCommand)
        {
            IConsoleManager::Get().UnregisterConsoleObject(DapCommand);
            DapCommand = nullptr;
        }

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
        MaybeAutoGenerateIntelliSense();
        MaybeAutoGenerateStaticBinding();

        IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
        MainFrameModule.OnMainFrameCreationFinished().AddRaw(this, &FUnLuaNovaEditorModule::OnMainFrameCreationFinished);
    }

    /**
     * lua.dap —— 把 headless 的 FLuaDebugService 暴露成 DAP 服务，IDE 直接 attach。
     *
     * FLuaDebugService 刻意不带 socket / 线协议（"that belongs in the DAP layer"），
     * 缺的这一层在 FLuaDebugAdapter 里。命令只是开关，协议实现不在本文件。
     */
    void RegisterDapCommand()
    {
        DapCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("lua.dap"),
            TEXT("Lua DAP server for IDE attach. usage: lua.dap start [port] | stop | status"),
            FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaEditorModule::OnLuaDapCommand),
            ECVF_Default);
    }

    void OnLuaDapCommand(const TArray<FString>& Args)
    {
        auto& Adapter = FLuaDebugAdapter::Get();

        if (Args.Num() == 0 || Args[0] == TEXT("status"))
        {
            UE_LOG(LogUnLuaNova, Log, TEXT("lua.dap: %s (port=%d)"),
                Adapter.IsRunning() ? TEXT("running") : TEXT("stopped"), Adapter.GetPort());
            return;
        }

        if (Args[0] == TEXT("start"))
        {
            // 端口默认值来自设置（UUnLuaNovaSettings::DebugAdapterPort），命令行参数只做临时覆盖
            const int32 DefaultPort = GetDefault<UUnLuaNovaSettings>()->DebugAdapterPort;
            const int32 Port = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : DefaultPort;

            // 和其他特性一样走统一开关：关掉就起不来，而不是"起来了但没人连"
            if (!GetDefault<UUnLuaNovaSettings>()->bDebugAdapter)
            {
                UE_LOG(LogUnLuaNova, Warning,
                    TEXT("lua.dap: 该特性已关闭（DefaultUnLuaNovaSettings.ini 的 bDebugAdapter=false）"));
                return;
            }

            Adapter.Start(Port);
            return;
        }

        if (Args[0] == TEXT("stop"))
        {
            Adapter.StopServer();
            return;
        }

        UE_LOG(LogUnLuaNova, Log, TEXT("usage: lua.dap [start [port] | stop | status]"));
    }

    /**
     * 首次启动自动导出一次 IntelliSense。
     *
     * 判据是"产物目录在不在"，不是"这次会话要不要刷新"：已经生成过就交给工具栏与
     * 目录监听去做增量，避免每次开编辑器都全量导出一遍蓝图表。
     */
    void MaybeAutoGenerateIntelliSense() const
    {
        const auto* Settings = GetDefault<UUnLuaNovaEditorSettings>();
        if (!Settings || !Settings->bGenerateIntelliSense || !Settings->bAutoGenerateIntelliSenseOnStartup)
            return;

        const auto& Generator = FUnLuaNovaIntelliSenseGenerator::Get();
        const FString& OutputDir = Generator->GetOutputDir();
        if (OutputDir.IsEmpty() || IFileManager::Get().DirectoryExists(*OutputDir))
            return;

        UE_LOG(LogUnLuaNova, Display, TEXT("IntelliSense: first-time generation into %s"), *OutputDir);
        Generator->UpdateAll();
    }

    /**
     * 静态绑定产物的自动维护。
     *
     * 和 IntelliSense 那套同一个形状（ini 开关 + 启动检查），判据换成了**指纹**：
     * 产物里存了一份类型集合的 CRC，启动时重算一遍。不同就说明有类型增删，重新生成。
     *
     * 这里必须说清一件事，否则日志会被误读成"开了没生效"：
     * 产物是 .cpp，写完盘之后还要 UBT 编一次才进二进制。所以自动生成之后，
     * thunk **本次启动还不会生效** —— 要下次编译之后。属性/函数的描述表部分在产物
     * 编进去之前也用不上。这一点在日志里直接写出来，不让人猜。
     *
     * 另外签名变化（不是类型增删）指纹抓不到，那是故意的：逐类型逐成员哈希等于每次
     * 启动都把整个反射遍历一遍。签名变了由 FStaticBindingRegistry::Resolve() 兜底 ——
     * 它注册时会用真实 FProperty 复算 kind，不符就停用 thunk，退化而不是出错。
     */
    void MaybeAutoGenerateStaticBinding() const
    {
        const auto* Settings = GetDefault<UUnLuaNovaEditorSettings>();
        if (!Settings || !Settings->bStaticBinding || !Settings->bAutoGenerateStaticBinding)
            return;

        FStaticBindingGenerateOptions Options;
        Options.OutputDir = Settings->StaticBindingOutputDir;
        Options.Modules = Settings->StaticBindingModules;

        switch (Settings->StaticBindingSelection)
        {
        case EUnLuaNovaStaticBindingSelection::Modules: Options.Selection = EStaticBindingSelection::Modules; break;
        case EUnLuaNovaStaticBindingSelection::All:     Options.Selection = EStaticBindingSelection::All; break;
        default:                                        Options.Selection = EStaticBindingSelection::ExportedReflected; break;
        }

        const FString File = FPaths::Combine(
            Options.OutputDir.IsEmpty() ? UnLuaNova::GetDefaultStaticBindingOutputDir() : Options.OutputDir,
            TEXT("UnLuaNovaStaticBinding_Generated.cpp"));

        const bool bExists = IFileManager::Get().FileExists(*File);

        if (bExists)
        {
            const FString Stored = UnLuaNova::ReadStaticBindingFingerprint(File);
            const FString Current = UnLuaNova::ComputeStaticBindingFingerprint(Options);

            // An unreadable or marker-less file counts as up to date: regenerating on every
            // start because a header could not be parsed would be worse than a stale file.
            if (Stored.IsEmpty() || Stored == Current)
                return;

            UE_LOG(LogUnLuaNova, Display,
                TEXT("StaticBinding: type set changed (%s -> %s), regenerating."), *Stored, *Current);
        }
        else
        {
            UE_LOG(LogUnLuaNova, Display, TEXT("StaticBinding: no generated file yet, generating into %s"), *File);
        }

        const FStaticBindingGenerateResult Result = UnLuaNova::GenerateStaticBindings(Options);
        if (Result.NumTypes == 0)
        {
            UE_LOG(LogUnLuaNova, Warning, TEXT("StaticBinding: generation produced nothing (%s)"), *Result.Message);
            return;
        }

        UE_LOG(LogUnLuaNova, Display,
            TEXT("StaticBinding: wrote %s (%d types, thunk coverage %d/%d). "
                 "It is C++ - rebuild once for it to take effect; until then the reflective path stays in use."),
            *Result.OutputFile, Result.NumTypes, Result.NumThunks, Result.NumFuncs);
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

        // 凡名字以 UnLuaNova 开头、且带 Content/Script 的插件，都把它的脚本目录 stage 进去。
        //
        // 早先这里只认 "UnLuaNovaExtensions"，于是嵌套布局（submodule 是"工程套插件"，
        // 真正的插件在 Plugins/UnLuaNova/Plugins/UnLuaNova/）下**插件自己的运行时模块
        // 会被漏掉**：编辑器里一切正常，打包后 require('UnLuaNova.Vector') 才失败。
        // 按插件实例枚举而不是拼字符串，工程套插件 / 插件套插件都不会漏。
        const auto Plugins = IPluginManager::Get().GetEnabledPlugins();
        for (const auto Plugin : Plugins)
        {
            if (!Plugin->CanContainContent())
                continue;

            if (!Plugin->GetName().StartsWith(TEXT("UnLuaNova")))
                continue;

            auto ScriptPath = Plugin->GetContentDir() / "Script";
            if (!FPaths::DirectoryExists(ScriptPath))
                continue;

            if (FPaths::MakePathRelativeTo(ScriptPath, *FPaths::ProjectContentDir()))
                ScriptPaths.AddUnique(ScriptPath);
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
    IConsoleObject* DapCommand = nullptr;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnLuaNovaEditorModule, UnLuaNovaEditor)
