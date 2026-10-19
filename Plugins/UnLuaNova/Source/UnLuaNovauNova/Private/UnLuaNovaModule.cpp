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

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Modules/ModuleManager.h"
#endif

#if ALLOW_CONSOLE
#include "Engine/Console.h"
#include "UnLuaNovaConsoleCommands.h"
#endif

#include "Engine/World.h"
#include "UnLuaNovaModule.h"
#include "DefaultParamCollection.h"
#include "GameDelegates.h"
#include "LuaEnvLocator.h"
#include "LuaOverrides.h"
#include "UnLuaNovaDebugBase.h"
#include "UnLuaNovaInterface.h"
#include "UnLuaNovaSettings.h"
#include "GameFramework/PlayerController.h"
#include "Registries/ClassRegistry.h"
#include "Registries/EnumRegistry.h"

#define LOCTEXT_NAMESPACE "FUnLuaNovaModule"

namespace UnLuaNova
{
    class FUnLuaNovaModule : public IUnLuaNovaModule,
                         public FUObjectArray::FUObjectCreateListener,
                         public FUObjectArray::FUObjectDeleteListener
    {
    public:
        virtual void StartupModule() override
        {
#if WITH_EDITOR
            FModuleManager::Get().LoadModule(TEXT("UnLuaNovaEditor"));
#endif

            RegisterSettings();

#if ALLOW_CONSOLE
            ConsoleCommands = MakeUnique<FUnLuaNovaConsoleCommands>(this);
#endif

            FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FUnLuaNovaModule::PostLoadMapWithWorld);

            CreateDefaultParamCollection();

#if AUTO_UNLUA_STARTUP
#if WITH_EDITOR
            if (!IsRunningGame())
            {
                FEditorDelegates::PreBeginPIE.AddRaw(this, &FUnLuaNovaModule::OnPreBeginPIE);
                FEditorDelegates::PostPIEStarted.AddRaw(this, &FUnLuaNovaModule::OnPostPIEStarted);
                FEditorDelegates::EndPIE.AddRaw(this, &FUnLuaNovaModule::OnEndPIE);
                FGameDelegates::Get().GetEndPlayMapDelegate().AddRaw(this, &FUnLuaNovaModule::OnEndPlayMap);
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
                OnHandleSystemErrorHandle = FCoreDelegates::OnHandleSystemError.AddRaw(this, &FUnLuaNovaModule::OnSystemError);
                OnHandleSystemEnsureHandle = FCoreDelegates::OnHandleSystemEnsure.AddRaw(this, &FUnLuaNovaModule::OnSystemError);
                GUObjectArray.AddUObjectCreateListener(this);
                GUObjectArray.AddUObjectDeleteListener(this);

                const auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
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

                UE_LOG(LogUnLuaNova, Log, TEXT("%s:"), *Pair.Value->GetName())
                PrintCallStack(Pair.Key);
                UE_LOG(LogUnLuaNova, Log, TEXT(""))
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

            const auto Section = SettingsModule->RegisterSettings("Project", "Plugins", "UnLuaNova",
                                                                  LOCTEXT("UnLuaNovaEditorSettingsName", "UnLuaNova"),
                                                                  LOCTEXT("UnLuaNovaEditorSettingsDescription", "UnLuaNova Runtime Settings"),
                                                                  GetMutableDefault<UUnLuaNovaSettings>());
            Section->OnModified().BindRaw(this, &FUnLuaNovaModule::OnSettingsModified);
#endif

#if ENGINE_MAJOR_VERSION >=5 && !WITH_EDITOR
            // UE5下打包后没有从{PROJECT}/Config/DefaultUnLua.ini加载，这里强制刷新一下
            FString UnLuaNovaIni = TEXT("UnLuaNova");
            GConfig->LoadGlobalIniFile(UnLuaNovaIni, *UnLuaNovaIni, nullptr, true);
            UUnLuaNovaSettings::StaticClass()->GetDefaultObject()->ReloadConfig();
#endif

            auto& Settings = *GetDefault<UUnLuaNovaSettings>();
            bPrintLuaStackOnSystemError = Settings.bPrintLuaStackOnSystemError;
        }

        void UnregisterSettings()
        {
#if WITH_EDITOR
            ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
            if (SettingsModule)
                SettingsModule->UnregisterSettings("Project", "Plugins", "UnLuaNova");
#endif
        }

        bool OnSettingsModified()
        {
            auto& Settings = *GetDefault<UUnLuaNovaSettings>();
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
        TUniquePtr<FUnLuaNovaConsoleCommands> ConsoleCommands;
#endif
    };
}

IMPLEMENT_MODULE(UnLuaNova::FUnLuaNovaModule, UnLuaNova)

#undef LOCTEXT_NAMESPACE
