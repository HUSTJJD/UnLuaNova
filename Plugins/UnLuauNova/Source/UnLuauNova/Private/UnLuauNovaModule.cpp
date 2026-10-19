// Tencent is pleased to support the open source community by making UnLuauNova available.
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

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Modules/ModuleManager.h"
#endif

#if ALLOW_CONSOLE
#include "Engine/Console.h"
#include "UnLuauNovaConsoleCommands.h"
#endif

#include "Engine/World.h"
#include "UnLuauNovaModule.h"
#include "DefaultParamCollection.h"
#include "GameDelegates.h"
#include "LuaEnvLocator.h"
#include "LuaOverrides.h"
#include "UnLuauNovaDebugBase.h"
#include "UnLuauNovaInterface.h"
#include "UnLuauNovaSettings.h"
#include "GameFramework/PlayerController.h"
#include "Registries/ClassRegistry.h"
#include "Registries/EnumRegistry.h"

#define LOCTEXT_NAMESPACE "FUnLuauNovaModule"

namespace UnLuauNova
{
    class FUnLuauNovaModule : public IUnLuauNovaModule,
                         public FUObjectArray::FUObjectCreateListener,
                         public FUObjectArray::FUObjectDeleteListener
    {
    public:
        virtual void StartupModule() override
        {
#if WITH_EDITOR
            FModuleManager::Get().LoadModule(TEXT("UnLuauNovaEditor"));
#endif

            RegisterSettings();

#if ALLOW_CONSOLE
            ConsoleCommands = MakeUnique<FUnLuauNovaConsoleCommands>(this);
#endif

            FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FUnLuauNovaModule::PostLoadMapWithWorld);

            CreateDefaultParamCollection();

#if AUTO_UNLUA_STARTUP
#if WITH_EDITOR
            if (!IsRunningGame())
            {
                FEditorDelegates::PreBeginPIE.AddRaw(this, &FUnLuauNovaModule::OnPreBeginPIE);
                FEditorDelegates::PostPIEStarted.AddRaw(this, &FUnLuauNovaModule::OnPostPIEStarted);
                FEditorDelegates::EndPIE.AddRaw(this, &FUnLuauNovaModule::OnEndPIE);
                FGameDelegates::Get().GetEndPlayMapDelegate().AddRaw(this, &FUnLuauNovaModule::OnEndPlayMap);
            }

            if (IsRunningGame() || IsRunningDedicatedServer())
#endif
                SetActive(true);
#endif
        }

        virtual void ShutdownModule() override
        {
            UnregisterSettings();
            SetActive(false);
        }

        virtual bool IsActive() override
        {
            return bIsActive;
        }

        virtual void SetActive(const bool bActive) override
        {
            if (bIsActive == bActive)
                return;

            if (bActive)
            {
                OnHandleSystemErrorHandle = FCoreDelegates::OnHandleSystemError.AddRaw(this, &FUnLuauNovaModule::OnSystemError);
                OnHandleSystemEnsureHandle = FCoreDelegates::OnHandleSystemEnsure.AddRaw(this, &FUnLuauNovaModule::OnSystemError);
                GUObjectArray.AddUObjectCreateListener(this);
                GUObjectArray.AddUObjectDeleteListener(this);

                const auto& Settings = *GetMutableDefault<UUnLuauNovaSettings>();
                const auto EnvLocatorClass = *Settings.EnvLocatorClass == nullptr ? ULuaEnvLocator::StaticClass() : *Settings.EnvLocatorClass;
                EnvLocator = NewObject<ULuaEnvLocator>(GetTransientPackage(), EnvLocatorClass);
                EnvLocator->AddToRoot();
                FDeadLoopCheck::Timeout = Settings.DeadLoopCheck;
                FDanglingCheck::Enabled = Settings.DanglingCheck;

                for (const auto Class : TObjectRange<UClass>())
                {
                    for (const auto& ClassPath : Settings.PreBindClasses)
                    {
                        if (!ClassPath.IsValid())
                            continue;

                        const auto TargetClass = ClassPath.ResolveClass();
                        if (!TargetClass)
                            continue;

                        if (Class->IsChildOf(TargetClass))
                        {
                            const auto Env = EnvLocator->Locate(Class);
                            Env->TryBind(Class);
                            break;
                        }
                    }
                }
            }
            else
            {
                FCoreDelegates::OnHandleSystemError.Remove(OnHandleSystemErrorHandle);
                FCoreDelegates::OnHandleSystemEnsure.Remove(OnHandleSystemEnsureHandle);
                GUObjectArray.RemoveUObjectCreateListener(this);
                GUObjectArray.RemoveUObjectDeleteListener(this);
                EnvLocator->Reset();
                EnvLocator->RemoveFromRoot();
                EnvLocator = nullptr;
                FLuaOverrides::Get().RestoreAll();
            }

            bIsActive = bActive;
        }

        virtual FLuaEnv* GetEnv(UObject* Object) override
        {
            if (!bIsActive)
                return nullptr;
            return EnvLocator->Locate(Object);
        }

        virtual void HotReload() override
        {
            if (!bIsActive)
                return;
            EnvLocator->HotReload();
        }

    private:
        virtual void NotifyUObjectCreated(const UObjectBase* ObjectBase, int32 Index) override
        {
            // UE_LOG(LogTemp, Log, TEXT("NotifyUObjectCreated : %p"), ObjectBase);
            if (!bIsActive)
                return;

            UObject* Object = (UObject*)ObjectBase;

            const auto Env = EnvLocator->Locate(Object);
            // UE_LOG(LogTemp, Log, TEXT("Locate %s for %s"), *Env->GetName(), *ObjectBase->GetFName().ToString());
            Env->TryBind(Object);
            Env->TryReplaceInputs(Object);
        }

        virtual void NotifyUObjectDeleted(const UObjectBase* Object, int32 Index) override
        {
            // UE_LOG(LogTemp, Log, TEXT("NotifyUObjectDeleted : %p"), Object);
        }

        virtual void OnUObjectArrayShutdown() override
        {
            if (!bIsActive)
                return;

            GUObjectArray.RemoveUObjectCreateListener(this);
            GUObjectArray.RemoveUObjectDeleteListener(this);

            bIsActive = false;
        }

        void OnSystemError() const
        {
            if (!bPrintLuaStackOnSystemError)
                return;

            if (!IsInGameThread())
                return;

            for (auto& Pair : FLuaEnv::GetAll())
            {
                if (!Pair.Key || !Pair.Value)
                    continue;

                UE_LOG(LogUnLuauNova, Log, TEXT("%s:"), *Pair.Value->GetName())
                PrintCallStack(Pair.Key);
                UE_LOG(LogUnLuauNova, Log, TEXT(""))
            }

            if (GLog)
                GLog->Flush();
        }

#if WITH_EDITOR

        void OnPreBeginPIE(bool bIsSimulating)
        {
            SetActive(true);
        }

        void OnPostPIEStarted(bool bIsSimulating)
        {
            UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
            if (EditorEngine)
                PostLoadMapWithWorld(EditorEngine->PlayWorld);
        }

        void OnEndPIE(bool bIsSimulating)
        {
            // SetActive(false);
        }

        void OnEndPlayMap()
        {
            SetActive(false);
        }

#endif

        void RegisterSettings()
        {
#if WITH_EDITOR
            ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
            if (!SettingsModule)
                return;

            const auto Section = SettingsModule->RegisterSettings("Project", "Plugins", "UnLuauNova",
                                                                  LOCTEXT("UnLuauNovaEditorSettingsName", "UnLuauNova"),
                                                                  LOCTEXT("UnLuauNovaEditorSettingsDescription", "UnLuauNova Runtime Settings"),
                                                                  GetMutableDefault<UUnLuauNovaSettings>());
            Section->OnModified().BindRaw(this, &FUnLuauNovaModule::OnSettingsModified);
#endif

#if ENGINE_MAJOR_VERSION >=5 && !WITH_EDITOR
            // UE5下打包后没有从{PROJECT}/Config/DefaultUnLua.ini加载，这里强制刷新一下
            FString UnLuauNovaIni = TEXT("UnLuauNova");
            GConfig->LoadGlobalIniFile(UnLuauNovaIni, *UnLuauNovaIni, nullptr, true);
            UUnLuauNovaSettings::StaticClass()->GetDefaultObject()->ReloadConfig();
#endif

            auto& Settings = *GetDefault<UUnLuauNovaSettings>();
            bPrintLuaStackOnSystemError = Settings.bPrintLuaStackOnSystemError;
        }

        void UnregisterSettings()
        {
#if WITH_EDITOR
            ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
            if (SettingsModule)
                SettingsModule->UnregisterSettings("Project", "Plugins", "UnLuauNova");
#endif
        }

        bool OnSettingsModified()
        {
            auto& Settings = *GetDefault<UUnLuauNovaSettings>();
            bPrintLuaStackOnSystemError = Settings.bPrintLuaStackOnSystemError;
            return true;
        }

        void PostLoadMapWithWorld(UWorld* World) const
        {
            if (!World || !bIsActive)
                return;

            const auto Env = EnvLocator->Locate(World);
            if (!Env)
                return;

            const auto Manager = Env->GetManager();
            if (!Manager)
                return;

            Manager->OnMapLoaded(World);
        }

        bool bIsActive = false;
        bool bPrintLuaStackOnSystemError = false;
        ULuaEnvLocator* EnvLocator = nullptr;
        FDelegateHandle OnHandleSystemErrorHandle;
        FDelegateHandle OnHandleSystemEnsureHandle;
#if ALLOW_CONSOLE
        TUniquePtr<FUnLuauNovaConsoleCommands> ConsoleCommands;
#endif
    };
}

IMPLEMENT_MODULE(UnLuauNova::FUnLuauNovaModule, UnLuauNova)

#undef LOCTEXT_NAMESPACE
