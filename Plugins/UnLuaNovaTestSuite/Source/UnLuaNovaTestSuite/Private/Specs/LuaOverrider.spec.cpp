// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

// C++ -> Lua member calls (UnLuaNovaOverrider.h).
//
// The module under test is supplied through AddBuiltInLoader rather than from disk: a
// spec that depends on a .lua file in Content/Script would break the moment someone
// reorganises that folder, and the thing being tested here is the call path, not the
// loader.
//
// Note what is deliberately NOT tested here: there are no ALua* base classes to exercise.
// Shipping a parallel set of Lua-driven C++ classes was the first attempt at this feature
// and it was wrong for this project - ULuaFunction::IsOverridable already accepts any
// UFunction carrying FUNC_BlueprintEvent, including on a native class, so the override
// system was always enough and the extra classes only served to bypass
// self.Overridden.Xxx(self). Binding is covered by the existing locator/binding specs.

#include "UnLuaNovaBase.h"
#include "LuaBackend.h"
#include "LuaEnv.h"
#include "UnLuaNovaManager.h"
#include "UnLuaNovaOverrider.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

static const TCHAR* SpecModuleName = TEXT("Spec.OverriderModule");

namespace
{
    int LuaAnswer(lua_State* L)
    {
        lua_pushinteger(L, 42);
        return 1;
    }

    int LuaLessThan(lua_State* L)
    {
        lua_pushboolean(L, lua_tointeger(L, 1) < lua_tointeger(L, 2));
        return 1;
    }

    int LuaNoReturn(lua_State* L)
    {
        return 0;
    }

    int LuaSum(lua_State* L)
    {
        lua_pushnumber(L, lua_tonumber(L, 1) + lua_tonumber(L, 2));
        return 1;
    }

    int LuaEchoSelf(lua_State* L)
    {
        // The point of the case: self has to arrive as argument 1, not as a global and
        // not as an upvalue. If the marshalling gets the order wrong this returns nil.
        lua_pushvalue(L, 1);
        return 1;
    }

    int SpecModuleLoader(lua_State* L)
    {
        lua_newtable(L);
        lua_pushcfunction(L, LuaAnswer);
        lua_setfield(L, -2, "Answer");
        lua_pushcfunction(L, LuaLessThan);
        lua_setfield(L, -2, "LessThan");
        lua_pushcfunction(L, LuaNoReturn);
        lua_setfield(L, -2, "NoReturn");
        lua_pushcfunction(L, LuaSum);
        lua_setfield(L, -2, "Sum");
        lua_pushcfunction(L, LuaEchoSelf);
        lua_setfield(L, -2, "EchoSelf");
        return 1; // the module table
    }
}

BEGIN_DEFINE_SPEC(FOverriderSpec, "UnLuaNova.API.Overrider", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
    UObject* Bound;

    /** Bind a throwaway UObject to the built-in spec module. */
    UObject* MakeBoundObject()
    {
        UObject* Object = NewObject<UObject>();
        Env->GetManager()->Bind(Object, SpecModuleName);
        return Object;
    }
END_DEFINE_SPEC(FOverriderSpec)

void FOverriderSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
        Env->AddBuiltInLoader(SpecModuleName, SpecModuleLoader);
        Bound = MakeBoundObject();
        Bound->AddToRoot();
    });

    AfterEach([this]
    {
        if (Bound)
        {
            Bound->RemoveFromRoot();
            Bound = nullptr;
        }
        Env.Reset();
        L = nullptr;
    });

    Describe(TEXT("函数查找"), [this]()
    {
        It(TEXT("能找到模块里的函数，找不到不存在的"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TEST_TRUE(UnLuaNova::IsLuaFunctionExist(Bound, TEXT("Answer")));
            TEST_FALSE(UnLuaNova::IsLuaFunctionExist(Bound, TEXT("NotAFunction")));
        });

        It(TEXT("未绑定的对象一律回答“不存在”而不是崩溃"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UObject* Unbound = NewObject<UObject>();
            TEST_FALSE(UnLuaNova::IsLuaFunctionExist(Unbound, TEXT("Answer")));
            TEST_EQUAL(UnLuaNova::CallLuaFunctionIfExist<int32>(Unbound, TEXT("Answer")), 0);
        });
    });

    Describe(TEXT("类型化调用"), [this]()
    {
        It(TEXT("self 作为第一个参数传入"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Reuse the existing UObject -> Lua marshalling to compare identity, so this
            // asserts the argument order without depending on any type conversion.
            const int32 Top = lua_gettop(L);

            // Go through the two-step API so the return value survives long enough to
            // inspect. FLuaRetValues pops on destruction, so the stack must not be
            // truncated while it is still alive - hence no lua_settop here.
            const UnLuaNova::FLuaMemberCall Call = UnLuaNova::BeginLuaMemberCall(Bound, TEXT("EchoSelf"), false);
            TEST_TRUE(Call.IsValid());
            {
                auto Values = UnLuaNova::EndLuaMemberCall(Call, 1);
                TEST_TRUE(Values.IsValid());
                TEST_EQUAL(Values.Num(), 1);
                if (Values.IsValid() && Values.Num() > 0)
                {
                    UObject* Echoed = Values[0].Value<UObject*>();
                    TestTrue(TEXT("第一个参数就是 self"), Echoed == Bound);
                }
            }

            // Everything the call pushed has been popped again.
            TEST_EQUAL(lua_gettop(L), Top);
        });

        It(TEXT("返回值按声明类型取回"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TEST_EQUAL(UnLuaNova::CallLuaFunction<int32>(Bound, TEXT("Answer")), 42);
            // Not TEST_EQUAL: comparing a float for exact equality is a flaky test waiting
            // to happen, and 1.5 + 2.5 is exactly representable only by luck.
            TEST_TRUE(FMath::IsNearlyEqual(UnLuaNova::CallLuaFunction<double>(Bound, TEXT("Sum"), 1.5, 2.5), 4.0));
            TEST_TRUE(UnLuaNova::CallLuaFunction<bool>(Bound, TEXT("LessThan"), 1, 2));
            TEST_FALSE(UnLuaNova::CallLuaFunction<bool>(Bound, TEXT("LessThan"), 2, 1));
        });

        It(TEXT("void 调用不取返回值也不残留栈"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const int32 Before = lua_gettop(L);
            UnLuaNova::CallLuaFunctionVoid(Bound, TEXT("NoReturn"));
            TEST_EQUAL(lua_gettop(L), Before);
        });

        It(TEXT("函数不存在时返回默认值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TEST_EQUAL(UnLuaNova::CallLuaFunctionIfExist<int32>(Bound, TEXT("NotAFunction")), 0);
            TEST_FALSE(UnLuaNova::CallLuaFunctionIfExist<bool>(Bound, TEXT("NotAFunction")));
        });

        It(TEXT("栈在调用后是平衡的"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The real risk with a hand-rolled stack dance is residue on the main state.
            // PushFunction/CallFunction own the layout now, but assert it anyway.
            const int32 Before = lua_gettop(L);
            UnLuaNova::CallLuaFunction<int32>(Bound, TEXT("Answer"));
            UnLuaNova::CallLuaFunction<int32>(Bound, TEXT("NotAFunction"));
            UnLuaNova::CallLuaFunctionVoid(Bound, TEXT("NoReturn"));
            UnLuaNova::CallLuaFunctionIfExist<int32>(Bound, TEXT("NotAFunction"));
            TEST_EQUAL(lua_gettop(L), Before);
        });
    });
}

#endif //WITH_DEV_AUTOMATION_TESTS
