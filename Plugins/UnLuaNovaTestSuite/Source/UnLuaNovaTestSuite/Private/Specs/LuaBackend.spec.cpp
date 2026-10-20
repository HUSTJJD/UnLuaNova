// Tencent is pleased to support the open source community by making UnLuaNova available.
// 
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#include "UnLuaNovaBase.h"
#include "LuaBackend.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Covers the shims in LuaBackend.h rather than UnLuaNova's own behaviour. These are the
// seams between plain Lua and Luau - every one of them is a place where the two backends
// disagree, so a mistake shows up here as a failing spec instead of as a link error or a
// silently wrong number at runtime.
//
// Everything here must compile and pass under both backends; if a test can only hold for
// one of them, it belongs in the backend's own spec, not in this one.

BEGIN_DEFINE_SPEC(FLuaBackendSpec, "UnLuaNova.API.LuaBackend", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
END_DEFINE_SPEC(FLuaBackendSpec)

void FLuaBackendSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
    });

    Describe(TEXT("后端适配层"), [this]()
    {
        It(TEXT("64 位整数读写不截断"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();

            // 2^53 + 1: the first value a double can no longer represent exactly, so a
            // 32-bit or float path anywhere in the stack shows up immediately.
            const int64 Expected = 9007199254740993ll;

            UNLUANOVA_PUSHINT64(L, Expected);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), Expected);
            lua_pop(L, 1);
        });

        It(TEXT("Lua 侧的 64 位字面量能完整取回"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();

            Env->DoString("return 9007199254740993");

            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 9007199254740993ll);
            lua_pop(L, 1);
        });

        It(TEXT("UNLUANOVA_LOAD 能加载并执行 chunk"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const char Code[] = "return 42";

            TEST_EQUAL(UNLUANOVA_LOAD(L, "LuaBackendSpec", Code, sizeof(Code) - 1), 0);
            TEST_EQUAL(lua_pcall(L, 0, 1, 0), 0);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 42ll);
            lua_pop(L, 1);
        });

        It(TEXT("GC 设置可用，且两参数的 lua_gc 能取到数值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();

            UNLUANOVA_SETUP_GC(L);

            // Plain Lua's lua_gc takes (L, what, data); Luau's always takes three. The
            // two-argument spelling is what call sites use, so it has to keep working.
            const auto Kb = lua_gc(L, LUA_GCCOUNT);
            TestTrue(TEXT("lua_gc(LUA_GCCOUNT) 返回非负"), Kb >= 0);
        });

        It(TEXT("两个环境互不影响"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::FLuaEnv Env1;
            UnLuaNova::FLuaEnv Env2;

            Env1.DoString("return 1");
            Env2.DoString("return 2");

            TEST_EQUAL(UNLUANOVA_TOINT64(Env1.GetMainState(), -1), 1ll);
            TEST_EQUAL(UNLUANOVA_TOINT64(Env2.GetMainState(), -1), 2ll);
        });
    });

    AfterEach([this]
    {
        Env.Reset();
    });
}

#endif
