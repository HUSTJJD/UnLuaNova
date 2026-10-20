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
#include "UnLuaNovaSandbox.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#if UNLUANOVA_HAS_ALLOC_CALLBACKS
namespace
{
    // The categorized half of lua.memstats is built entirely on these callbacks - the only
    // place Luau tells you *what kind* of memory an allocation is. Nothing else observes
    // them, so if they stop firing the report still prints, it just quietly says nothing.
    int32 GSpecAllocateEvents = 0;

    void SpecOnAllocate(lua_State* L, void* Block, size_t OldSize, size_t NewSize, uint8_t MemCat, int Type, int Tag)
    {
        (void)L;
        (void)Block;
        (void)OldSize;
        (void)MemCat;
        (void)Type;
        (void)Tag;
        if (NewSize > 0)
            ++GSpecAllocateEvents;
    }

    void SpecOnFree(lua_State* L, void* Block)
    {
        (void)L;
        (void)Block;
    }
}
#endif

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
            // 32-bit or float path anywhere in the stack shows up immediately. Safe here
            // because UNLUANOVA_PUSHINT64 uses the backend's real 64-bit integer type
            // rather than whatever the lexer produces for a literal.
            const int64 Expected = 9007199254740993ll;

            UNLUANOVA_PUSHINT64(L, Expected);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), Expected);
            lua_pop(L, 1);
        });

        It(TEXT("Lua 侧的 64 位字面量能完整取回"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();

            // 2^60, not 2^53 + 1: past 32 bits by a wide margin, and - unlike 2^53 + 1 -
            // exactly representable as a double, which is the only number type Luau has
            // for a literal. A bigger value would be asking about a number the backend
            // cannot even hold, which tests the VM rather than the shim.
            Env->DoString("return 1152921504606846976");

            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 1152921504606846976ll);
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

            // Captured through a parameter, not through a plain local: Luau constant
            // folds a never-reassigned local into its uses, which leaves the closure with
            // no upvalue at all - and then there is nothing for getupvalue to read.
            Env->DoString(TEXT("local function make(v) return function() return v end end; local n, v = debug.getupvalue(make(5), 1); return v"));
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
            Env->DoString(TEXT("local mt = getmetatable(UE.UObject); return mt ~= nil and mt.__name or 'missing'"));
            const FString Found = lua_isstring(L, -1) ? FString(UTF8_TO_TCHAR(lua_tostring(L, -1))) : FString(TEXT("<nil>"));
            TestTrue(TEXT("类元表能回溯到类型名"), Found == TEXT("UObject"));
            lua_settop(L, Top);
        });

        It(TEXT("HotReload 能加载模块，而不是静默降级"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // HotReload's own require walks package.searchers and then moves the chunk
            // into a sandbox with debug.setupvalue. Both halves used to be missing on
            // Luau, so it threw inside a pcall and the whole feature degraded without a
            // word - and every module went through the host require instead.
            Env->DoString(TEXT("local M = require('UnLuaNova.HotReload'); return M.require('UnLuaNova.StringEx') ~= nil"));
            TestTrue(TEXT("HotReload 的 require 加载出了模块"), lua_toboolean(L, -1) != 0);
            lua_settop(L, Top);
        });
    });

    // Capabilities that exist on one backend and not the other. They are compiled out
    // rather than skipped at runtime: a test that is present but never runs reads as
    // covered while covering nothing, which is worse than no test at all.
    Describe(TEXT("Luau 专属能力"), [this]()
    {
        It(TEXT("脚本能问到当前后端"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // This is the guard Content/Script/Luau/Index.lua uses. Without it, requiring
            // a file that uses Luau syntax is a compile error on Lua 5.5 - it fails at
            // require() time, long before any Lua of ours gets a chance to run.
            Env->DoString(TEXT("return UnLuaNova.GetBackend()"));
            const FString Backend = lua_isstring(L, -1) ? FString(UTF8_TO_TCHAR(lua_tostring(L, -1))) : FString();
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            TestTrue(TEXT("GetBackend 报告 Luau"), Backend == TEXT("Luau"));
#else
            TestTrue(TEXT("GetBackend 报告 Lua"), Backend == TEXT("Lua"));
#endif
            lua_settop(L, Top);
        });

        It(TEXT("HasFeature 与编译期能力一致"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Scripts branch on this instead of on the backend name, so the answer has to
            // match the macro the C++ side would have branched on.
            Env->DoString(TEXT("return UnLuaNova.HasFeature('sandbox')"));
            TestTrue(TEXT("sandbox"), (lua_toboolean(L, -1) != 0) == (UNLUANOVA_HAS_SANDBOX != 0));
            lua_settop(L, Top);

            Env->DoString(TEXT("return UnLuaNova.HasFeature('vector')"));
            TestTrue(TEXT("vector"), (lua_toboolean(L, -1) != 0) == (UNLUANOVA_HAS_NATIVE_VECTOR != 0));
            lua_settop(L, Top);

            Env->DoString(TEXT("return UnLuaNova.HasFeature('buffer')"));
            TestTrue(TEXT("buffer"), (lua_toboolean(L, -1) != 0) == (UNLUANOVA_HAS_NATIVE_BUFFER != 0));
            lua_settop(L, Top);
        });

#if UNLUANOVA_HAS_NATIVE_VECTOR
        It(TEXT("Luau 语法能编译并按预期求值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // continue and compound assignment are syntax, not library functions: on Lua
            // 5.5 this chunk never compiles. Compiling it *is* the assertion.
            Env->DoString(TEXT("local s = 0; for i = 1, 10 do if i % 2 == 0 then continue end; s += i end; return s"));
            TEST_EQUAL((int64)lua_tonumber(L, -1), 25ll); // 1 + 3 + 5 + 7 + 9
            lua_settop(L, Top);
        });

        It(TEXT("原生 vector 能进出 FVector"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // vector is carried in the TValue, so UE.FVector(v) builds the FVector with no
            // intermediate userdata - which is the whole reason the type is interesting.
            Env->DoString(TEXT("local v = UE.FVector(vector.create(1, 2, 3)); return v.X + v.Y + v.Z"));
            TEST_EQUAL((int64)lua_tonumber(L, -1), 6ll);
            lua_settop(L, Top);

            Env->DoString(TEXT("local v = UE.FVector(4, 5, 6); return type(v:ToVector())"));
            const FString TypeName = lua_isstring(L, -1) ? FString(UTF8_TO_TCHAR(lua_tostring(L, -1))) : FString();
            TestTrue(TEXT("ToVector 交回原生 vector"), TypeName == TEXT("vector"));
            lua_settop(L, Top);
        });

        It(TEXT("UnLuaNova.Vector 能把原生 vector 转成 FVector"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // The module is the cross-backend entry point: on Lua 5.5 it answers false and
            // nil instead of erroring, so one script runs on both. Here it also proves the
            // vector->FVector path end to end rather than just that the type exists.
            Env->DoString(TEXT("local V = require('UnLuaNova.Vector'); local v = V.ToFVector(V.Create(1, 2, 3)); return tostring(V.HasVector()) .. '|' .. tostring(v.X)"));
            const FString Result = lua_isstring(L, -1) ? FString(UTF8_TO_TCHAR(lua_tostring(L, -1))) : FString();
            TestTrue(TEXT("vector 转 FVector 成功"), Result.StartsWith(TEXT("true|1")));
            lua_settop(L, Top);
        });
#else
        It(TEXT("没有原生 vector 时 ToVector 降级为 nil"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Degrade, do not error: a script written against Luau has to keep running on
            // Lua 5.5, just without the native type.
            Env->DoString(TEXT("local v = UE.FVector(4, 5, 6); return v:ToVector()"));
            TestTrue(TEXT("ToVector 返回 nil"), lua_isnil(L, -1) != 0);
            lua_settop(L, Top);
        });
#endif

#if UNLUANOVA_HAS_NATIVE_BUFFER
        It(TEXT("buffer 能按类型读写"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            Env->DoString(TEXT("local b = buffer.create(4); buffer.writeu8(b, 0, 7); return buffer.readu8(b, 0)"));
            TEST_EQUAL((int64)lua_tonumber(L, -1), 7ll);
            lua_settop(L, Top);
        });
#endif

#if UNLUANOVA_HAS_SANDBOX
        It(TEXT("沙盒开启后全局表只读"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Exactly what UnLuaNova runs at env startup when lua.sandbox=1. After it, a
            // hotfixed or downloaded script can no longer clobber engine bindings - easy to
            // lose if the call ever moves, and invisible until something overwrites UE.
            UNLUANOVA_SANDBOX(L);

            Env->DoString(TEXT("return pcall(function() rawset(_G, 'SandboxProbe', 1) end)"));
            TestTrue(TEXT("向全局表写入被拦截"), lua_toboolean(L, -2) == 0); // -2: pcall 的 ok
            lua_settop(L, Top);
        });

        It(TEXT("沙盒下 UE 命名空间仍能写入：类型注册不会被冻住"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // luaL_sandbox marks every table hanging off the globals read-only - including
            // UE, whose types are registered lazily, on first access. Left alone, the next
            // script to ask for a not-yet-seen type errors out, so the sandbox restores
            // write access to exactly those tables before it is done.
            UNLUANOVA_SANDBOX(L);
            UnLuaNova::RestoreTablesAfterSandbox(L);

            Env->DoString(TEXT("return UE.FVector ~= nil"));
            TestTrue(TEXT("沙盒下仍能访问引擎类型"), lua_toboolean(L, -1) != 0);
            lua_settop(L, Top);
        });
#endif

        It(TEXT("脚本改写 UE 命名空间被拦截"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Not Luau-specific - the namespace's __newindex is installed on both backends -
            // but it is what the sandbox is *for*: engine types are published with
            // lua_rawset, so a script replacing one is always a mistake. luaL_sandbox cannot
            // cover this case, because the namespace has to stay writable to register.
            Env->DoString(TEXT("return pcall(function() UE.FVector = nil end)"));
            TestTrue(TEXT("写入被拦截"), lua_toboolean(L, -2) == 0); // -2: pcall 的 ok
            lua_settop(L, Top);
        });

#if UNLUANOVA_HAS_BREAKPOINT
        It(TEXT("断点原语可用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // lua_breakpoint answers whether the line holds an instruction, so a 0 means
            // "no such line" rather than "no such API" - assert that the calls are accepted
            // and symmetric, which is what lua.dbg is built on.
            Env->DoString(TEXT("return function() local a = 1; local b = 2; return a + b end"));
            const int SetResult = UNLUANOVA_BREAKPOINT(L, -1, 2, 1);
            const int ClearResult = UNLUANOVA_BREAKPOINT(L, -1, 2, 0);
            TestTrue(TEXT("断点设置与清除都被接受"), SetResult >= 0 && ClearResult >= 0);
            TestTrue(TEXT("lua_stackdepth 可用"), UNLUANOVA_STACKDEPTH(L) >= 0);
            lua_settop(L, Top);
        });
#endif

#if UNLUANOVA_HAS_ALLOC_CALLBACKS
        It(TEXT("分配回调能收到分配事件"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            GSpecAllocateEvents = 0;
            UNLUANOVA_SET_ALLOC_CALLBACKS(L, &SpecOnAllocate, &SpecOnFree);
            Env->DoString(TEXT("local t = {}; for i = 1, 64 do t[i] = i end; return #t"));
            UNLUANOVA_SET_ALLOC_CALLBACKS(L, nullptr, nullptr);

            TestTrue(TEXT("分配回调被触发"), GSpecAllocateEvents > 0);
            lua_settop(L, Top);
        });
#endif
    });

    AfterEach([this]
    {
        Env.Reset();
    });
}

#endif
