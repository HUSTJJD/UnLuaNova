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

// Covers the compatibility layer (Private/LuaScriptCompat.cpp): the globals a script takes
// for granted on Lua 5.x that Luau simply does not have.
//
// These are asserted to exist *and* to behave like plain Lua, because the failure they exist
// to prevent is not "the global is missing" - it is "the script died halfway through on one
// backend with 'attempt to call a nil value'", which is exactly what Issue291 hit with
// collectgarbage. Every assertion here runs on both backends.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FLuaScriptCompatSpec, "UnLuaNova.API.LuaScriptCompat", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FLuaScriptCompatSpec)

void FLuaScriptCompatSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
    });

    AfterEach([this]
    {
        Env.Reset();
        L = nullptr;
    });

    Describe(TEXT("collectgarbage"), [this]()
    {
        It(TEXT("两个后端都存在"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Luau's base library stops at gcinfo(); without the compat layer this is nil
            // and every script that forces a collection dies.
            UnLuaNova::RunChunk(L, "return type(collectgarbage) == 'function'");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("collect 返回 0，与 plain Lua 一致"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return collectgarbage('collect')");
            TEST_EQUAL((int32)lua_tointeger(L, -1), 0);
        });

        It(TEXT("count 返回 KBytes 数字"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return collectgarbage('count')");
            TEST_TRUE(lua_type(L, -1) == LUA_TNUMBER);
            TEST_TRUE(lua_tonumber(L, -1) > 0.0);
        });

        It(TEXT("未知选项报错而不是静默"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Silence here would be worse than the error: a script asking for an option the
            // layer does not implement would carry on as if the collection had happened.
            // pcall also returns the message, and a message is truthy, so the call is
            // parenthesised: only the status is left on the stack, and that is what -1 reads.
            UnLuaNova::RunChunk(L, "return (pcall(collectgarbage, 'nope'))");
            TEST_FALSE(lua_toboolean(L, -1));
        });

        It(TEXT("gc 之后 Lua 值仍然可用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The scenario Issue291 exists for: a binding has to survive a forced collection.
            UnLuaNova::RunChunk(L, R"(
                local T = { Value = 42 }
                collectgarbage('collect')
                return T.Value
            )");
            TEST_EQUAL((int32)lua_tointeger(L, -1), 42);
        });
    });

    Describe(TEXT("load"), [this]()
    {
        It(TEXT("两个后端都能从字符串加载 chunk"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "local F = load('return 7') return F and F() or 0");
            TEST_EQUAL((int32)lua_tointeger(L, -1), 7);
        });
    });
}

#endif
