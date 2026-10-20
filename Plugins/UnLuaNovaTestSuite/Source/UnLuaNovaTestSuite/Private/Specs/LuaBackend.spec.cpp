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

    // The backends have to answer the same questions, not just run the same code. These
    // cover the gaps UnLuaNova fills in rather than anything either VM does natively,
    // because that is where a backend quietly stops being interchangeable: everything
    // still loads, and one feature just stops doing anything.
    Describe(TEXT("后端能力对齐"), [this]()
    {
        It(TEXT("package 同时提供 loaded 和 searchers"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Plain Lua's package owns both. Luau has no package at all, so UnLuaNova
            // builds one - and a package without searchers is worse than none, because
            // `ipairs(package.searchers)` stops at a nil and the caller just gets
            // nothing back. That is exactly how hot reload used to die silently.
            Env->DoString(TEXT("return type(package) == 'table' and type(package.loaded) == 'table' and type(package.searchers) == 'table' and #package.searchers > 0"));
            TestTrue(TEXT("package.loaded 与 package.searchers 都在"), lua_toboolean(L, -1) != 0);
            lua_settop(L, Top);
        });

        It(TEXT("debug 提供 upvalue / local / registry 访问"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            Env->DoString(TEXT("local d = debug; return type(d.getupvalue) == 'function' and type(d.setupvalue) == 'function' and type(d.upvalueid) == 'function' and type(d.getlocal) == 'function' and type(d.setlocal) == 'function' and type(d.getregistry) == 'function' and type(d.getuservalue) == 'function'"));
            TestTrue(TEXT("debug 库没有缺口"), lua_toboolean(L, -1) != 0);
            lua_settop(L, Top);
        });

        It(TEXT("debug.setupvalue 能给 chunk 设环境"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // setupvalue(chunk, 1, env) is how Lua 5.x writes _ENV. Luau's main chunk has
            // no upvalue 1 at all, so the same call has to land on setfenv instead - it
            // has to mean the same thing, not just fail quietly.
            Env->DoString(TEXT("local chunk = load('return Marker'); local env = {Marker = 7}; debug.setupvalue(chunk, 1, env); return chunk()"));
            TEST_EQUAL((int64)lua_tonumber(L, -1), 7ll);
            lua_settop(L, Top);
        });

        It(TEXT("debug.getupvalue 能读到闭包捕获的值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            Env->DoString(TEXT("local caught = 5; local f = function() return caught end; local n, v = debug.getupvalue(f, 1); return v"));
            TEST_EQUAL((int64)lua_tonumber(L, -1), 5ll);
            lua_settop(L, Top);
        });

        It(TEXT("luaL_newmetatable 建的元表带 __name"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Plain Lua stamps the type name onto every metatable luaL_newmetatable
            // creates; Luau does not. UnLuaNova reads it back to get from a metatable to
            // the type it describes, so on Luau the first read of a not-yet-cached class
            // or enum member used to find a nil and trip a check().
            Env->DoString(TEXT("local mt = getmetatable(UE.UObject); return mt ~= nil and mt.__name"));
            TestTrue(TEXT("类元表能回溯到类型名"), lua_isstring(L, -1) != 0);
            lua_settop(L, Top);
        });

        It(TEXT("HotReload 能加载模块，而不是静默降级"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // HotReload.load() walks package.searchers and then moves the chunk into its
            // own sandbox with debug.setupvalue. Both halves used to be missing on Luau,
            // so it threw inside a pcall and the whole feature degraded without a word.
            Env->DoString(TEXT("local M = require('UnLuaNova.HotReload'); local chunk = M.load('UnLuaNova.StringEx'); return chunk ~= nil"));
            TestTrue(TEXT("HotReload.load() 返回了 chunk"), lua_toboolean(L, -1) != 0);
            lua_settop(L, Top);
        });
    });

    AfterEach([this]
    {
        Env.Reset();
    });
}

#endif
