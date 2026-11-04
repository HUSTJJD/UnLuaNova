// Copyright (c) 2026 UnLuaNova contributors
//
// Licensed under the MIT License (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

// BIND-INSTANCE: one UClass, several Lua modules.
//
// Overrides are installed on the UClass, so the interesting failure is not "can we install
// them" but "does the call reach the module belonging to *this* object". The class-level
// FFunctionRegistry cache used to keep the first resolved closure, which made instance
// overrides look like they worked until a second object called the same function. These
// cases drive the real dispatch thunk (Stub->Overridable()), not a direct Lua lookup.

#include "UnLuaNovaBase.h"
#include "LuaEnv.h"
#include "LuaFunction.h"
#include "UnLuaNovaManager.h"
#include "UnLuaNovaModule.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

static const TCHAR* SpecInstanceModuleA = TEXT("Spec.InstanceBinding.A");
static const TCHAR* SpecInstanceModuleB = TEXT("Spec.InstanceBinding.B");

namespace
{
    template<int32 Value>
    int InstanceModuleLoader(lua_State* L)
    {
        lua_newtable(L);                                    // M
        lua_pushcfunction(L, [](lua_State* S) -> int
        {
            lua_pushinteger(S, Value);
            return 1;
        });
        lua_setfield(L, -2, "Overridable");
        return 1;
    }

    UUnLuaNovaTestStubForOverrides* NewStub()
    {
        auto* Stub = NewObject<UUnLuaNovaTestStubForOverrides>();
        Stub->AddToRoot();
        return Stub;
    }

    void ReleaseStub(UUnLuaNovaTestStubForOverrides* Stub)
    {
        if (Stub)
            Stub->RemoveFromRoot();
    }
}

BEGIN_DEFINE_SPEC(FUnLuaNovaInstanceBindingSpec, "UnLuaNova.API.InstanceBinding",
    EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    UnLuaNova::FLuaEnv* Env;
    lua_State* L;
END_DEFINE_SPEC(FUnLuaNovaInstanceBindingSpec)

void FUnLuaNovaInstanceBindingSpec::Define()
{
    BeforeEach([this]
    {
        // The dispatch thunk resolves the env through IUnLuaNovaModule::Get().GetEnv(Context).
        // A private FLuaEnv would bind the object where nothing ever calls it.
        UnLuaNova::Startup();
        Env = IUnLuaNovaModule::Get().GetEnv();
        L = Env->GetMainState();
        Env->AddBuiltInLoader(SpecInstanceModuleA, InstanceModuleLoader<101>);
        Env->AddBuiltInLoader(SpecInstanceModuleB, InstanceModuleLoader<202>);
        Env->DoString(TEXT("_G.OverridesStubGlobalCalls = 0"));
    });

    AfterEach([this]
    {
        ULuaFunction::RestoreOverrides(UUnLuaNovaTestStubForOverrides::StaticClass());
        UnLuaNova::Shutdown();
        Env = nullptr;
        L = nullptr;
    });

    Describe(TEXT("同类不同实例"), [this]()
    {
        It(TEXT("实例覆盖与类默认模块在同一类上共存"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* First = NewStub();
            auto* Second = NewStub();

            TEST_TRUE(Env->TryBind(First));
            TEST_TRUE(Env->TryBind(Second));

            FString Error;
            TEST_TRUE(Env->GetManager()->BindInstance(Second, SpecInstanceModuleB, LUA_NOREF, &Error));
            TEST_TRUE(Error.IsEmpty());
            TEST_EQUAL(Env->GetManager()->GetInstanceBoundModuleName(Second), FString(SpecInstanceModuleB));
            TEST_TRUE(Env->GetManager()->GetInstanceBoundModuleName(First).IsEmpty());

            TEST_EQUAL(First->Overridable(), 1);       // class default module
            TEST_EQUAL(Second->Overridable(), 202);    // instance override

            ReleaseStub(First);
            ReleaseStub(Second);
        });

        It(TEXT("两个不同模块交替调用不串线，重绑后立刻生效"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* First = NewStub();
            auto* Second = NewStub();
            TEST_TRUE(Env->TryBind(First));
            TEST_TRUE(Env->TryBind(Second));

            TEST_TRUE(Env->GetManager()->BindInstance(First, SpecInstanceModuleA));
            TEST_TRUE(Env->GetManager()->BindInstance(Second, SpecInstanceModuleB));

            TEST_EQUAL(First->Overridable(), 101);
            TEST_EQUAL(Second->Overridable(), 202);
            TEST_EQUAL(Second->Overridable(), 202);
            TEST_EQUAL(First->Overridable(), 101);

            TEST_TRUE(Env->GetManager()->BindInstance(First, SpecInstanceModuleB));
            TEST_EQUAL(First->Overridable(), 202);
            TEST_EQUAL(Second->Overridable(), 202);

            ReleaseStub(First);
            ReleaseStub(Second);
        });

        It(TEXT("未显式绑定的实例仍回落类默认模块"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Overridden = NewStub();
            auto* Plain = NewStub();
            TEST_TRUE(Env->TryBind(Overridden));
            TEST_TRUE(Env->TryBind(Plain));
            TEST_TRUE(Env->GetManager()->BindInstance(Overridden, SpecInstanceModuleA));

            TEST_EQUAL(Overridden->Overridable(), 101);
            TEST_EQUAL(Plain->Overridable(), 1);
            TEST_EQUAL(Plain->Overridable(), 2);

            ReleaseStub(Overridden);
            ReleaseStub(Plain);
        });

        It(TEXT("解绑后回落到类默认模块且查询 API 复位"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Stub = NewStub();
            TEST_TRUE(Env->TryBind(Stub));
            TEST_TRUE(Env->GetManager()->BindInstance(Stub, SpecInstanceModuleA));

            TEST_TRUE(Env->GetManager()->HasInstanceBinding(Stub));
            TEST_TRUE(Env->GetManager()->GetInstanceBoundRef(Stub) != LUA_NOREF);
            TEST_EQUAL(Stub->Overridable(), 101);

            TEST_TRUE(Env->GetManager()->UnbindInstance(Stub));
            TEST_FALSE(Env->GetManager()->HasInstanceBinding(Stub));
            TEST_EQUAL(Env->GetManager()->GetInstanceBoundRef(Stub), LUA_NOREF);
            TEST_EQUAL(Stub->Overridable(), 1);
            TEST_FALSE(Env->GetManager()->UnbindInstance(Stub));

            ReleaseStub(Stub);
        });

        It(TEXT("非法输入与未建立类级绑定时拒绝而不是静默降级"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            FString Error;
            UClass* ClassObject = UUnLuaNovaTestStubForOverrides::StaticClass();
            TEST_FALSE(Env->GetManager()->BindInstance(ClassObject, SpecInstanceModuleA, LUA_NOREF, &Error));
            TEST_FALSE(Error.IsEmpty());

            auto* Unbound = NewObject<UUnLuaNovaTestStub>();
            Unbound->AddToRoot();
            Error.Reset();
            TEST_FALSE(Env->GetManager()->BindInstance(Unbound, TEXT(""), LUA_NOREF, &Error));
            Error.Reset();
            TEST_FALSE(Env->GetManager()->BindInstance(Unbound, SpecInstanceModuleA, LUA_NOREF, &Error));
            TEST_TRUE(Error.Contains(TEXT("class-level binding")));
            Unbound->RemoveFromRoot();
        });
    });
}

#endif
