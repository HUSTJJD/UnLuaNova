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
#include "LuaVmHooks.h"
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

namespace
{
    // Read back by the profiler spec: SpecStackProbe runs one frame up the stack, which is
    // the frame the profiler keys its cache on and lua.dbg names in a stack trace.
    bool bSpecProbeGotFrame = false;
    int32 GSpecProbeLine = 0;
    uint64 GSpecProbeProtoId = 0;

    int SpecStackProbe(lua_State* L)
    {
        lua_Debug Ar;
        FMemory::Memzero(&Ar, sizeof(Ar));

        // Luau folds the stack walk into lua_getinfo - the level is an argument, and a level
        // past the top of the stack simply answers 0. Plain Lua needs lua_getstack first and
        // then hands the lua_Debug it filled to lua_getinfo. Same #if the debugger carries.
        //
        // The 'p' is the part that is easy to get wrong: on Luau protoid is filled only when
        // it is asked for, so a "nSl" read leaves UNLUANOVA_DEBUG_PROTOID reading whatever
        // was in the struct. On plain Lua 'p' is not an option at all and an unknown one
        // makes lua_getinfo answer 0 - which is why the two spellings differ.
        bool bGot = false;
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
        bGot = UNLUANOVA_GETINFO(L, 1, "nSlp", &Ar) != 0;
#else
        bGot = lua_getstack(L, 1, &Ar) != 0 && UNLUANOVA_GETINFO(L, 1, "nSl", &Ar) != 0;
#endif

        bSpecProbeGotFrame = bGot;
        if (bGot)
        {
            GSpecProbeLine = Ar.currentline;
            GSpecProbeProtoId = UNLUANOVA_DEBUG_PROTOID(&Ar);
        }

        lua_pushboolean(L, bGot);
        return 1;
    }
}

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

        It(TEXT("脚本依赖的标准库在两个后端都在"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Luau is a deliberately smaller language: no os, no io, no debug of its own -
            // UnLuaNova fills that last one in, and the gap is covered above. These are the
            // functions UnLuaNova's own scripts and the test content actually call. A missing
            // one does not fail at load time, it fails somewhere inside a chunk that only one
            // backend ever compiles, which is the expensive way to find out.
            Env->DoString(TEXT(
                "local ok = type(string.format) == 'function' and type(string.gsub) == 'function'"
                " and type(string.rep) == 'function' and type(string.sub) == 'function' and type(string.byte) == 'function'"
                " and type(table.concat) == 'function' and type(table.insert) == 'function' and type(table.remove) == 'function'"
                " and type(table.sort) == 'function' and type(table.unpack or unpack) == 'function'"
                " and type(math.floor) == 'function' and type(math.ceil) == 'function' and type(math.max) == 'function'"
                " and type(math.min) == 'function' and type(math.abs) == 'function' and type(math.sqrt) == 'function'"
                " and type(tostring) == 'function' and type(tonumber) == 'function' and type(select) == 'function'"
                " and type(pcall) == 'function' and type(error) == 'function'"
                " and type(setmetatable) == 'function' and type(getmetatable) == 'function'"
                " and type(rawget) == 'function' and type(rawset) == 'function';"
                " return ok"));
            TestTrue(TEXT("标准库表面一致"), lua_toboolean(L, -1) != 0);
            lua_settop(L, Top);
        });

        It(TEXT("luaL_newmetatable 第二次调用返回 0"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Plain Lua returns 0 and leaves the existing table on the stack. Luau has no
            // such API, so the shim has to answer the same way: a caller reading 0 as "already
            // registered" would otherwise re-register the metatable on every lookup.
            TEST_TRUE(luaL_newmetatable(L, "UnLuaNovaSpecMeta") != 0);
            TEST_EQUAL(luaL_newmetatable(L, "UnLuaNovaSpecMeta"), 0);
            lua_settop(L, Top);
        });

        It(TEXT("luaL_ref / luaL_unref 往返一致"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Luau's lua_ref addresses the value by stack index and does not pop it, and it
            // only ever refs into the registry - neither half matches lauxlib, so the shim has
            // to. Everything cached this way (object tables, container caches) silently leaks
            // or loses entries if the two halves disagree.
            lua_newtable(L);
            const int Ref = luaL_ref(L, LUA_REGISTRYINDEX);
            TEST_TRUE(Ref != LUA_NOREF);
            TEST_EQUAL(lua_rawgeti(L, LUA_REGISTRYINDEX, Ref), LUA_TTABLE);
            lua_pop(L, 1);

            luaL_unref(L, LUA_REGISTRYINDEX, Ref);
            TEST_EQUAL(lua_rawgeti(L, LUA_REGISTRYINDEX, Ref), LUA_TNIL);
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

            // The remaining two keys, so the whole table is pinned rather than the three that
            // happened to have a test next to them.
            Env->DoString(TEXT("return UnLuaNova.HasFeature('breakpoint')"));
            TestTrue(TEXT("breakpoint"), (lua_toboolean(L, -1) != 0) == (UNLUANOVA_HAS_BREAKPOINT != 0));
            lua_settop(L, Top);

            Env->DoString(TEXT("return UnLuaNova.HasFeature('allocCallbacks')"));
            TestTrue(TEXT("allocCallbacks"), (lua_toboolean(L, -1) != 0) == (UNLUANOVA_HAS_ALLOC_CALLBACKS != 0));
            lua_settop(L, Top);
        });

        It(TEXT("编译期能力位与后端一致"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Pinned because scripts cannot ask about any of these, and each one is a place
            // where "it still loads" hides "it stopped doing anything":
            //
            // USERDATA_GC - Luau has no __gc at all, so there every container, struct and
            //               smart-pointer finalizer is dead code that never runs.
            // HOOK_EVENT  - only plain Lua's lua_Debug carries an event, so only there can the
            //               profiler pair begin/end events; Luau samples instead.
            // ITER / READONLY - the metamethod name and the table flag each exist on one side.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            TEST_TRUE(UNLUANOVA_HAS_USERDATA_GC == 0);
            TEST_TRUE(UNLUANOVA_HAS_HOOK_EVENT == 0);
            TEST_TRUE(UNLUANOVA_HAS_ITER_METAMETHOD != 0);
            TEST_TRUE(UNLUANOVA_HAS_READONLY_TABLES != 0);
#else
            TEST_TRUE(UNLUANOVA_HAS_USERDATA_GC != 0);
            TEST_TRUE(UNLUANOVA_HAS_HOOK_EVENT != 0);
            TEST_TRUE(UNLUANOVA_HAS_ITER_METAMETHOD == 0);
            TEST_TRUE(UNLUANOVA_HAS_READONLY_TABLES == 0);
#endif
        });

#if UNLUANOVA_HAS_READONLY_TABLES
        It(TEXT("只读表能置位也能读回"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // luaL_sandbox is bulk; this is the single-table primitive underneath it, and
            // RestoreTablesAfterSandbox uses it to un-freeze the UE namespace again. Plain Lua
            // has no such flag, so there the macros are inert - which is why this is compiled
            // out rather than expected to pass.
            lua_newtable(L);
            UNLUANOVA_SET_READONLY(L, -1, true);
            TEST_TRUE(UNLUANOVA_GET_READONLY(L, -1));

            UNLUANOVA_SET_READONLY(L, -1, false);
            TEST_FALSE(UNLUANOVA_GET_READONLY(L, -1));
            lua_settop(L, Top);
        });
#endif

        It(TEXT("迭代元方法注册在后端认识的名字下"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // UNLUANOVA_ITER_ENTRY is __iter on Luau and __pairs on Lua 5.x. Registering both
            // is not an option: the wrong name is silently dead rather than an error, so a
            // container would look iterable and never iterate. All three containers have to
            // register it - TSet did not, and `pairs(Set)` raised on both backends for it.
#if UNLUANOVA_HAS_ITER_METAMETHOD
            const TCHAR* IterKey = TEXT("__iter");
            const TCHAR* OtherKey = TEXT("__pairs");
#else
            const TCHAR* IterKey = TEXT("__pairs");
            const TCHAR* OtherKey = TEXT("__iter");
#endif

            const auto CheckContainer = [&](const TCHAR* Factory) -> bool
            {
                const FString Chunk = FString::Printf(TEXT(
                    "local mt = getmetatable(%s);"
                    "if type(mt) ~= 'table' then return false end;"
                    "if mt['%s'] == nil then return false end;"
                    "if mt['%s'] ~= nil then return false end;"
                    "return true"), Factory, IterKey, OtherKey);
                if (!Env->DoString(Chunk))
                    return false;

                const bool bOk = lua_toboolean(L, -1) != 0;
                lua_settop(L, Top);
                return bOk;
            };

            TestTrue(TEXT("TArray 注册了正确的迭代元方法"), CheckContainer(TEXT("UE.TArray(0)")));
            TestTrue(TEXT("TMap 注册了正确的迭代元方法"), CheckContainer(TEXT("UE.TMap(0, 0)")));
            TestTrue(TEXT("TSet 注册了正确的迭代元方法"), CheckContainer(TEXT("UE.TSet(0)")));

            // And the builtin has to reach it: on Luau pairs() insists on a real table, so
            // LuaPairs routes anything else through that metamethod. Both backends, same loop.
            Env->DoString(TEXT(
                "local a = UE.TArray(0); a:Add(1); a:Add(2);"
                "local m = UE.TMap(0, 0); m:Add(1, 10);"
                "local s = UE.TSet(0); s:Add(7);"
                "local na, nm, ns = 0, 0, 0;"
                "for _ in pairs(a) do na = na + 1 end;"
                "for _ in pairs(m) do nm = nm + 1 end;"
                "for _ in pairs(s) do ns = ns + 1 end;"
                "return na * 100 + nm * 10 + ns"));
            TEST_EQUAL((int64)lua_tonumber(L, -1), 211ll);
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

    // --- integers -----------------------------------------------------------------
    // The seam with the widest blast radius and, until now, no coverage at all: whether a
    // number counts as an integer decides if an int argument is accepted, and which of the
    // two push macros a value arrives through decides whether a script can do arithmetic on
    // it. Both differ per backend, so both need a test that runs on both.
    Describe(TEXT("整数类型"), [this]()
    {
        It(TEXT("非整数读回为 0，而不是被截断"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // The documented contract: UNLUANOVA_TOINT64 answers 0 when the value is not an
            // integer, rather than quietly truncating 1.5 to 1. Every int property is read
            // through it, so a backend that truncates corrupts instead of failing.
            lua_pushnumber(L, 1.5);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 0ll);
            lua_settop(L, Top);

            lua_pushnumber(L, 3.0);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 3ll);
            lua_settop(L, Top);
        });

        It(TEXT("UnLuaNovaIsIntegralNumber 区分整数与非整数"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // This is the gate every integer UFunction argument passes. On Luau a script
            // literal arrives as a double and a value pushed with UNLUANOVA_PUSHINT64 arrives
            // as LUA_TINTEGER - lua_isinteger() answers false for the second, which is how
            // every integer argument of every UFunction started being rejected.
            lua_pushnumber(L, 1.5);
            TEST_FALSE(UnLuaNovaIsIntegralNumber(L, -1));
            lua_settop(L, Top);

            lua_pushnumber(L, 7.0);
            TEST_TRUE(UnLuaNovaIsIntegralNumber(L, -1));
            lua_settop(L, Top);

            UNLUANOVA_PUSHINT64(L, 9007199254740993ll);
            TEST_TRUE(UnLuaNovaIsIntegralNumber(L, -1));
            lua_settop(L, Top);
        });

        It(TEXT("PUSHINT 推入的值能在脚本里参与算术"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Why PUSHINT is not simply PUSHINT64: on Luau an integer plus a number is an
            // error, so anything a script will compute with - an int32 property, an enum
            // value - has to arrive as a plain number. Pushing it as an integer compiles and
            // runs, and only fails the moment a script adds 1 to it.
            Env->DoString(TEXT("function SpecAdd(v) return v + 1 end"));

            lua_getglobal(L, "SpecAdd");
            UNLUANOVA_PUSHINT(L, 41);
            TEST_EQUAL(lua_pcall(L, 1, 1, 0), 0);
            TEST_EQUAL((int64)lua_tonumber(L, -1), 42ll);
            lua_settop(L, Top);
        });

        It(TEXT("PUSHINT64 推入的值被认作整数且能完整读回"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // The other half: PUSHINT64 keeps all 64 bits, which is what an int64 property
            // needs, at the cost of arriving as the backend's integer type.
            const int64 Expected = 9007199254740993ll;
            UNLUANOVA_PUSHINT64(L, Expected);
            TEST_TRUE(UnLuaNovaIsIntegralNumber(L, -1));
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), Expected);
            lua_settop(L, Top);
        });
    });

    // --- coroutines ---------------------------------------------------------------
    Describe(TEXT("协程"), [this]()
    {
        It(TEXT("UNLUANOVA_IS_YIELDABLE 区分主线程与协程"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Latent actions ask this before yielding, so the failure names the real
            // constraint instead of Luau's opaque "attempt to yield across a C-call boundary".
            TEST_TRUE(UNLUANOVA_IS_YIELDABLE(L) == 0);

            const auto Co = lua_newthread(L);
            TEST_TRUE(UNLUANOVA_IS_YIELDABLE(Co) != 0);
            lua_settop(L, Top);
        });

        It(TEXT("UNLUANOVA_RESUME 能恢复协程并带回结果"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Lua 5.4/5.5 report the result count through an out param; Luau's lua_resume
            // takes three arguments and the out param is simply ignored there. Both halves are
            // exercised here - the yield, and the resume that answers it.
            Env->DoString(TEXT("function SpecCoroutine() local v = coroutine.yield(1); return v + 1 end"));

            lua_getglobal(L, "SpecCoroutine");
            const auto Co = lua_newthread(L);
            lua_pushvalue(L, -2);
            lua_xmove(L, Co, 1);

            int NResults = 0;
            TEST_EQUAL(UNLUANOVA_RESUME(Co, L, 0, &NResults), LUA_YIELD);
            TEST_EQUAL((int64)lua_tonumber(Co, -1), 1ll);
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            // No out param on Luau: the count has to come off the coroutine's own stack.
            TEST_EQUAL(lua_gettop(Co), 1);
#else
            TEST_EQUAL(NResults, 1);
#endif
            lua_pop(Co, 1);

            lua_pushnumber(Co, 41.0);
            NResults = 0;
            TEST_EQUAL(UNLUANOVA_RESUME(Co, L, 1, &NResults), LUA_OK);
            TEST_EQUAL((int64)lua_tonumber(Co, -1), 42ll);
            lua_settop(L, Top);
        });
    });

    // --- tearing an env down ------------------------------------------------------
    Describe(TEXT("环境销毁"), [this]()
    {
        It(TEXT("销毁环境会跑完所有 __gc，且不破坏内存"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // lua_close() runs every pending finalizer. On Luau there are none -
            // UNLUANOVA_HAS_USERDATA_GC is 0, because Luau has no __gc at all - so this only
            // exercises anything on the plain-Lua backend. There, static export leaves one
            // "TSharedPtr" userdata per exported property, and its __gc has to reach the
            // payload: NewSmartPointer() lays the userdata out as
            // [FUserdataDesc][padding][TSharedPtr], so lua_touserdata() hands back the
            // descriptor - and Reset() used to tear those bytes down instead of the pointer.
            bool bRan = false;
            {
                UnLuaNova::FLuaEnv LocalEnv;
                bRan = LocalEnv.DoString(TEXT("local v = UE.FVector(1, 2, 3); return v.X + v.Y + v.Z"));
                if (bRan)
                    bRan = lua_tonumber(LocalEnv.GetMainState(), -1) == 6.0;
            }
            TEST_TRUE(bRan);

            // A descriptor smashed by a finalizer, or a smart pointer destroyed twice,
            // corrupts the heap rather than failing at the call - this is the check that the
            // heap is still intact afterwards.
            TEST_TRUE(Env.IsValid());
            Env->DoString(TEXT("return 1 + 1"));
            TEST_EQUAL((int64)lua_tonumber(Env->GetMainState(), -1), 2ll);
        });
    });

    // --- the Luau-only script directory -------------------------------------------
    Describe(TEXT("Luau 示例目录"), [this]()
    {
        It(TEXT("require 入口在两个后端下都安全返回"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // Content/Script/Luau/* is written in Luau-only syntax - continue, compound
            // assignment, type annotations - which is a *compile* error on Lua 5.5, raised the
            // moment it is required. Index.lua guards on GetBackend() so those files are never
            // reached there. GetBackend() alone only proves the guard *can* work; this proves
            // it does, on the backend where the guard is the only thing standing in the way.
            Env->DoString(TEXT(
                "local ok, M = pcall(require, 'Luau.Index');"
                "if not ok or type(M) ~= 'table' then return false end;"
                "if M.Backend ~= UnLuaNova.GetBackend() then return false end;"
                "return M.Available == (UnLuaNova.GetBackend() == 'Luau')"));
            TestTrue(TEXT("require('Luau.Index') 返回与后端一致的结果"), lua_toboolean(L, -1) != 0);
            lua_settop(L, Top);
        });
    });

    // --- profiler and debugger seams ----------------------------------------------
    Describe(TEXT("Profiler 与调试"), [this]()
    {
        It(TEXT("VM hook 能安装、收到事件并按 owner 撤回"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // One hook slot, two features: the CPU profiler and the dead-loop watchdog both
            // register here, and on plain Lua the slot is lua_sethook with a lua_Debug that
            // carries an event, while on Luau it is lua_Callbacks::interrupt - which fires at
            // safe points and carries nothing at all. If fan-out breaks, the loser stops
            // working and nothing says so; that used to be real.
#if UNLUANOVA_HAS_HOOK_EVENT
            const int Mask = LUA_MASKCALL;
#else
            const int Mask = 0; // Luau: every listener sees every safe point
#endif

            int32 Events = 0;
            const void* Owner = &Events;
            UnLuaNova::FLuaVmHooks::Get().Add(L, Mask, Owner,
                [&Events](lua_State*, lua_Debug*) { ++Events; });

            Env->DoString(TEXT("local function f() return 1 end; local s = 0; for i = 1, 16 do s = s + f() end; return s"));
            TEST_EQUAL((int64)lua_tonumber(L, -1), 16ll);
            lua_settop(L, Top);
            TestTrue(TEXT("hook 收到了事件"), Events > 0);

            // Withdraw by owner: the slot has to be released too, or a listener that outlived
            // its feature keeps being called through a closure that is long gone.
            UnLuaNova::FLuaVmHooks::Get().RemoveOwner(L, Owner);
            const int32 Seen = Events;
            Env->DoString(TEXT("local function g() return 1 end; return g() + g()"));
            lua_settop(L, Top);
            TEST_EQUAL(Events, Seen);
        });

        It(TEXT("栈帧能取到源码行，Profiler 键稳定且唯一"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // The profiler keys its function cache on UNLUANOVA_DEBUG_PROTOID and lua.dbg
            // reads the same lua_Debug to name a frame. Luau exposes a proto id on the struct;
            // plain Lua has none, so the macro keys on (source pointer, line the function was
            // defined on) instead - which only works if the two functions differ in one of
            // those. Both closures come from one chunk, on different lines.
            bSpecProbeGotFrame = false;
            GSpecProbeLine = 0;
            GSpecProbeProtoId = 0;

            Env->DoString(TEXT("local function SpecProbeA(probe)\n return probe()\n end\n"
                               "local function SpecProbeB(probe)\n return probe()\n end\n"
                               "return SpecProbeA, SpecProbeB"));

            const auto CallWithProbe = [&](int Index) -> bool
            {
                lua_pushvalue(L, Index);
                lua_pushcfunction(L, SpecStackProbe);
                const int Status = lua_pcall(L, 1, 1, 0);
                const bool bOk = Status == 0 && lua_toboolean(L, -1) != 0;
                lua_pop(L, 1);
                return bOk;
            };

            TEST_TRUE(CallWithProbe(-2));
            TEST_TRUE(bSpecProbeGotFrame);
            TEST_TRUE(GSpecProbeLine > 0);
            const uint64 IdA = GSpecProbeProtoId;
            TEST_TRUE(IdA != 0);

            // Same function, same key - that is what makes it usable as a cache key.
            GSpecProbeProtoId = 0;
            TEST_TRUE(CallWithProbe(-2));
            TEST_EQUAL(GSpecProbeProtoId, IdA);

            // Different function, different key.
            TEST_TRUE(CallWithProbe(-1));
            TEST_TRUE(GSpecProbeProtoId != IdA);

            lua_settop(L, Top);
        });

#if UNLUANOVA_HAS_BREAKPOINT
        It(TEXT("调试通道原语可用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto L = Env->GetMainState();
            const int32 Top = lua_gettop(L);

            // lua.dbg keeps its hook on Luau's own channel (lua_callhook), separate from the
            // lua_Callbacks::interrupt slot the profiler and the watchdog share - so clearing
            // it must disturb neither. Plain Lua has no second channel at all: a debugger
            // registers as a FLuaVmHooks listener instead, which is why this is compiled out
            // there rather than expected to be a no-op.
            TEST_TRUE(UNLUANOVA_ATBREAKPOINT(L) == 0);
            UNLUANOVA_SET_DEBUG_HOOK(L, nullptr, nullptr);

            Env->DoString(TEXT("return 1 + 1"));
            TEST_EQUAL((int64)lua_tonumber(L, -1), 2ll);
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
