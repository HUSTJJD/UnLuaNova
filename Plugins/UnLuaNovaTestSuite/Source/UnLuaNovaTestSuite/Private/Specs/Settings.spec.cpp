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

// Settings that have to *do* something when changed.
//
// UUnLuaNovaSettings is process-global and is read at environment construction time, so the
// hazard these cases exist for is not "a setting was set wrong" - it is that one case sets a
// setting and every later case inherits it, producing a failure in an unrelated spec with
// nothing pointing back here. Each case therefore saves and restores, and AfterEach restores
// again as a backstop.
//
// Registered under UnLuaNova.Settings.* rather than reusing UnLuaNova.Settings, which
// LuaDeadLoopCheck.spec.cpp already owns. The prefix still matches -Filter UnLuaNova.Settings.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "UnLuaNovaSettings.h"
#include "LuaEnvLocator.h"
#include "LuaModuleLocator.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUnLuaNovaSettingsSpec, "UnLuaNova.Settings.General", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    FString SavedStartupModuleName;
    int32 SavedDeadLoopCheck;
    bool SavedDanglingCheck;
    TSubclassOf<ULuaEnvLocator> SavedEnvLocatorClass;
    TSubclassOf<ULuaModuleLocator> SavedModuleLocatorClass;

    void Restore()
    {
        auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
        Settings.StartupModuleName = SavedStartupModuleName;
        Settings.DeadLoopCheck = SavedDeadLoopCheck;
        Settings.DanglingCheck = SavedDanglingCheck;
        Settings.EnvLocatorClass = SavedEnvLocatorClass;
        Settings.ModuleLocatorClass = SavedModuleLocatorClass;
    }
END_DEFINE_SPEC(FUnLuaNovaSettingsSpec)

void FUnLuaNovaSettingsSpec::Define()
{
    BeforeEach([this]
    {
        const auto& Settings = *GetDefault<UUnLuaNovaSettings>();
        SavedStartupModuleName = Settings.StartupModuleName;
        SavedDeadLoopCheck = Settings.DeadLoopCheck;
        SavedDanglingCheck = Settings.DanglingCheck;
        SavedEnvLocatorClass = Settings.EnvLocatorClass;
        SavedModuleLocatorClass = Settings.ModuleLocatorClass;
    });

    AfterEach(EAsyncExecution::TaskGraphMainThread, [this]
    {
        Restore();
    });

    Describe(TEXT("启动模块名称"), [this]()
    {
        It(TEXT("设置后 Start 会执行那个模块"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The setting is only meaningful if an env created afterwards actually runs the
            // module; asserting the round-trip alone would pass for a setting nobody reads.
            auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
            Settings.StartupModuleName = TEXT("Tests.Specs.Settings.StartupProbe");

            UnLuaNova::FLuaEnv Local;
            Local.Start();

            const auto LocalL = Local.GetMainState();
            lua_getglobal(LocalL, "SpecStartupProbeRan");
            TEST_TRUE(lua_toboolean(LocalL, -1));
            lua_pop(LocalL, 1);
        });

        It(TEXT("为空时 Start 什么都不跑"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
            Settings.StartupModuleName = TEXT("");

            // The other half of the contract: an empty name must mean "skip", not "try to
            // require an empty module name".
            UnLuaNova::FLuaEnv Local;
            Local.Start();

            TEST_TRUE(Local.DoString(TEXT("return 1")));
            TEST_EQUAL((int32)lua_tointeger(Local.GetMainState(), -1), 1);
        });
    });

    Describe(TEXT("诊断开关"), [this]()
    {
        It(TEXT("DeadLoopCheck 可设置且能还原"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
            Settings.DeadLoopCheck = 7;
            TEST_EQUAL(GetDefault<UUnLuaNovaSettings>()->DeadLoopCheck, 7);

            Restore();
            TEST_EQUAL(GetDefault<UUnLuaNovaSettings>()->DeadLoopCheck, SavedDeadLoopCheck);
        });

        It(TEXT("DanglingCheck 可设置且能还原"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
            Settings.DanglingCheck = !SavedDanglingCheck;
            TEST_EQUAL(GetDefault<UUnLuaNovaSettings>()->DanglingCheck, !SavedDanglingCheck);

            Restore();
            TEST_EQUAL(GetDefault<UUnLuaNovaSettings>()->DanglingCheck, SavedDanglingCheck);
        });
    });

    Describe(TEXT("定位器类"), [this]()
    {
        It(TEXT("两个定位器类都能切换并还原"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
            Settings.ModuleLocatorClass = ULuaModuleLocator_ByPackage::StaticClass();
            Settings.EnvLocatorClass = ULuaEnvLocator_ByGameInstance::StaticClass();

            // Compared as raw UClass*: a TSubclassOf against a UClass* leaves the template
            // TestEqual with two candidate ValueTypes and no way to pick one.
            TEST_EQUAL(GetDefault<UUnLuaNovaSettings>()->ModuleLocatorClass.Get(), ULuaModuleLocator_ByPackage::StaticClass());
            TEST_EQUAL(GetDefault<UUnLuaNovaSettings>()->EnvLocatorClass.Get(), ULuaEnvLocator_ByGameInstance::StaticClass());

            Restore();
            TEST_EQUAL(GetDefault<UUnLuaNovaSettings>()->ModuleLocatorClass, SavedModuleLocatorClass);
            TEST_EQUAL(GetDefault<UUnLuaNovaSettings>()->EnvLocatorClass, SavedEnvLocatorClass);
        });
    });
}

#endif
