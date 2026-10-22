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

// Module and environment locators: the two extension points that decide which script an
// object gets and which VM it lives in.
//
// The interesting assertion is the empty-string one. Locate() returning "" is what makes an
// object silently unbindable - TryBind bails out before touching Lua, so the symptom is
// "my script is not running" with no error anywhere. Pinning that the *reason* is "the class
// does not implement IUnLuaNovaInterface" is the whole value of the case.
//
// Both settings are process-global and read when an env is built, so every case that touches
// them restores the original value in AfterEach. Getting this wrong does not fail the case
// that did it - it fails an unrelated one twenty cases later.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "LuaEnvLocator.h"
#include "LuaModuleLocator.h"
#include "UnLuaNovaSettings.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FLocatorsSpec, "UnLuaNova.API.Locators", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    TSubclassOf<ULuaModuleLocator> SavedModuleLocatorClass;
    TSubclassOf<ULuaEnvLocator> SavedEnvLocatorClass;
END_DEFINE_SPEC(FLocatorsSpec)

void FLocatorsSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        const auto& Settings = *GetDefault<UUnLuaNovaSettings>();
        SavedModuleLocatorClass = Settings.ModuleLocatorClass;
        SavedEnvLocatorClass = Settings.EnvLocatorClass;
    });

    AfterEach([this]
    {
        auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
        Settings.ModuleLocatorClass = SavedModuleLocatorClass;
        Settings.EnvLocatorClass = SavedEnvLocatorClass;
        Env.Reset();
    });

    Describe(TEXT("模块定位器"), [this]()
    {
        It(TEXT("默认定位器对实现了接口的对象返回 GetModuleName 的结果"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Locator = NewObject<ULuaModuleLocator>();
            const auto Stub = NewObject<UUnLuaNovaTestStubForOverrides>();

            TEST_EQUAL(Locator->Locate(Stub), FString(TEXT("Tests.Specs.Overrides.OverridesStub")));
        });

        It(TEXT("没有实现接口的对象得到空串"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // This is the root cause behind "the object never bound": an empty module name
            // makes TryBind return false before Lua is involved at all.
            const auto Locator = NewObject<ULuaModuleLocator>();
            const auto Stub = NewObject<UUnLuaNovaTestStub>();

            const FString ModuleName = Locator->Locate(Stub);
            TEST_TRUE(ModuleName.IsEmpty());
        });

        It(TEXT("ByPackage 对原生类返回类名，与默认不同"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Default = NewObject<ULuaModuleLocator>();
            const auto ByPackage = NewObject<ULuaModuleLocator_ByPackage>();
            const auto Stub = NewObject<UUnLuaNovaTestStubForOverrides>();

            const FString A = Default->Locate(Stub);
            const FString B = ByPackage->Locate(Stub);

            // A native class has no package path worth using, so ByPackage falls back to
            // the class name. Asserting inequality rather than a literal keeps the case
            // from locking in a name that a rename would legitimately change.
            TEST_FALSE(A.IsEmpty());
            TEST_FALSE(B.IsEmpty());
            TEST_TRUE(A != B);
            TEST_EQUAL(B, FString(TEXT("UnLuaNovaTestStubForOverrides")));
        });

        It(TEXT("换成自定义子类后用的是子类的结果"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The setting is only worth having if a project's subclass is actually
            // consulted; this is the extension point, not just a stored value.
            const auto Locator = NewObject<USpecModuleLocator>();
            const auto Stub = NewObject<UUnLuaNovaTestStubForOverrides>();

            TEST_EQUAL(Locator->Locate(Stub), FString(TEXT("Spec.Module.FromCustomLocator")));
        });

        It(TEXT("设置项切换后可写回原值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
            Settings.ModuleLocatorClass = USpecModuleLocator::StaticClass();
            // Compared as raw UClass*: a TSubclassOf against a UClass* leaves the template
            // TestEqual with two candidate ValueTypes and no way to pick one.
            TEST_EQUAL(Settings.ModuleLocatorClass.Get(), USpecModuleLocator::StaticClass());

            Settings.ModuleLocatorClass = SavedModuleLocatorClass;
            TEST_EQUAL(Settings.ModuleLocatorClass, SavedModuleLocatorClass);
        });
    });

    Describe(TEXT("环境定位器"), [this]()
    {
        It(TEXT("自定义子类把对象定位到指定环境"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Locator = NewObject<USpecEnvLocator>();
            Locator->Env = Env;

            const auto Stub = NewObject<UUnLuaNovaTestStubForOverrides>();
            TEST_EQUAL(Locator->Locate(Stub), Env.Get());
        });

        It(TEXT("默认环境定位器返回的是非空环境"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Locator = NewObject<ULuaEnvLocator>();
            const auto Stub = NewObject<UUnLuaNovaTestStubForOverrides>();

            // Whatever the default policy is, "no environment" would mean every script
            // silently has nowhere to run.
            TEST_TRUE(Locator->Locate(Stub) != nullptr);
        });
    });
}

#endif
