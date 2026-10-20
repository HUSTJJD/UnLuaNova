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
#include "LuaDeadLoopCheck.h"
#include "LuaEnvLocator.h"
#include "LuaOverrides.h"
#include "UnLuaNovaDebugBase.h"
#include "UnLuaNovaInterface.h"
#include "UnLuaNovaSettings.h"
#include "GameFramework/PlayerController.h"
#include "Registries/ClassRegistry.h"
#include "Registries/EnumRegistry.h"
#include "Misc/EngineVersionComparison.h"

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

            PostLoadMapWithWorldHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FUnLuaNovaModule::PostLoadMapWithWorld);
#if UE_VERSION_OLDER_THAN(5, 8, 0)
            PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FUnLuaNovaModule::PreBindConfiguredClasses);
#else
            PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FUnLuaNovaModule::PreBindConfiguredClasses);
#endif

            CreateDefaultParamCollection();

#if AUTO_UNLUA_STARTUP
#if WITH_EDITOR
            if (!IsRunningGame())
            {
                PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(this, &FUnLuaNovaModule::OnPreBeginPIE);
                PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FUnLuaNovaModule::OnPostPIEStarted);
                EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FUnLuaNovaModule::OnEndPIE);
                EndPlayMapHandle = FGameDelegates::Get().GetEndPlayMapDelegate().AddRaw(this, &FUnLuaNovaModule::OnEndPlayMap);
            }

            if (IsRunningGame() || IsRunningDedicatedServer())
#endif
                SetActive(true);
#endif
        }

        virtual void ShutdownModule() override
        {
            // Everything above was bound with AddRaw, so the handles have to come back off
            // here: a module unload (Live Coding, editor shutdown) destroys this object
            // while the delegates still point at it, and the next map load then calls
            // into freed memory.
            FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapWithWorldHandle);
            PostLoadMapWithWorldHandle.Reset();
#if UE_VERSION_OLDER_THAN(5, 8, 0)
            FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
#else
            FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
#endif
            PostEngineInitHandle.Reset();

#if AUTO_UNLUA_STARTUP
#if WITH_EDITOR
            FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
            FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
            FEditorDelegates::EndPIE.Remove(EndPIEHandle);
            FGameDelegates::Get().GetEndPlayMapDelegate().Remove(EndPlayMapHandle);
            PreBeginPIEHandle.Reset();
            PostPIEStartedHandle.Reset();
            EndPIEHandle.Reset();
            EndPlayMapHandle.Reset();
#endif
#endif

            UnregisterSettings();
            SetActive(false);

            // Last: the environments are gone now, so nothing can arm a guard any more and
            // the watchdog thread can be joined. It is a process-wide thread, so this is
            // the only place that shuts it down.
            FDeadLoopCheck::Shutdown();
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

                PreBindConfiguredClasses();
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
                PreBoundClasses.Empty();
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
        void PreBindConfiguredClasses()
        {
            if (!EnvLocator)
                return;

            // Default-phase modules may register classes after this PreDefault module starts.
            const auto& Paths = GetDefault<UUnLuaNovaSettings>()->PreBindClasses;
            for (UClass* Class : TObjectRange<UClass>())
            {
                if (PreBoundClasses.Contains(Class))
                    continue;
                for (const FSoftClassPath& Path : Paths)
                {
                    UClass* Target = Path.ResolveClass();
                    if (Target && Class->IsChildOf(Target))
                    {
                        if (FLuaEnv* Env = EnvLocator->Locate(Class))
                        {
                            if (Env->TryBind(Class))
                                PreBoundClasses.Add(Class);
                        }
                        break;
                    }
                }
            }
        }

        virtual void NotifyUObjectCreated(const UObjectBase* ObjectBase, int32 Index) override
        {
            // UE_LOG(LogTemp, Log, TEXT("NotifyUObjectCreated : %p"), ObjectBase);
            if (!bIsActive)
                return;

            UObject* Object = (UObject*)ObjectBase;

            const auto Env = EnvLocator->Locate(Object);
            if (!Env)
                return;

            Env->TryBind(Object);
            Env->TryReplaceInputs(Object);
        }

        virtual void NotifyUObjectDeleted(const UObjectBase* Object, int32 Index) override
        {
            // Intentionally empty. This listener exists so GUObjectArray keeps calling
            // NotifyUObjectCreated; deletion is already accounted for elsewhere -
            // FClassRegistry::NotifyUObjectDeleted drops stale descriptors and the object
            // registry holds everything by weak reference. Do not "fill this in" without
            // a concrete case: a delete listener is the easiest place to add a use after
            // free, because the object is already gone when it runs.
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
            // Deliberately a no-op. SetActive(false) here would tear the environments down
            // while PIE is still unwinding (EndPIE fires before the world is torn down),
            // and OnEndPlayMap below already does it at the point where it is safe. The
            // commented-out call used to sit here with no explanation - this is the answer
            // to "is this unfinished?": it is finished, and empty on purpose.
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
        FDelegateHandle PostLoadMapWithWorldHandle;
        FDelegateHandle PostEngineInitHandle;
        TSet<TWeakObjectPtr<UClass>> PreBoundClasses;
#if AUTO_UNLUA_STARTUP
#if WITH_EDITOR
        FDelegateHandle PreBeginPIEHandle;
        FDelegateHandle PostPIEStartedHandle;
        FDelegateHandle EndPIEHandle;
        FDelegateHandle EndPlayMapHandle;
#endif
#endif
#if ALLOW_CONSOLE
        TUniquePtr<FUnLuaNovaConsoleCommands> ConsoleCommands;
#endif
    };
}

IMPLEMENT_MODULE(UnLuaNova::FUnLuaNovaModule, UnLuaNova)

#undef LOCTEXT_NAMESPACE
