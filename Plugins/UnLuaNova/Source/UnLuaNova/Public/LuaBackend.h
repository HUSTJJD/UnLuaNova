// Single place where the two supported Lua backends differ.
//
// Officially supported backends:
//   UNLUANOVA_BACKEND_LUA  (0) - official Lua (https://github.com/lua/lua, submodule)
//   UNLUANOVA_BACKEND_LUAU (1) - Luau        (https://github.com/luau-lang/luau, submodule)  <-- default
//
// The ids are named after the backend, not the version: upgrading Lua means moving the
// submodule pointer, not touching these names.
//
// The two backends cannot coexist in one build - they export the same lua_* symbols, so
// this is a compile-time choice, not a runtime one. It is made by UNLUANOVA_LUA_BACKEND,
// injected by UnLuaNova.Build.cs from its `LuaBackend` setting; the default below only
// exists so a stray translation unit still compiles.
//
// Everything else in UnLuaNova must go through the UNLUANOVA_* macros below instead of
// calling the VM directly, so that neither backend leaks into the rest of the codebase.

#pragma once

#include <cstdarg> // va_list, for the luaL_error shim below
#include <cstdlib> // free, for the luaL_loadbufferx shim below
#include <cstring> // strlen, for the luaL_loadstring shim below

#define UNLUANOVA_BACKEND_LUA 0
#define UNLUANOVA_BACKEND_LUAU 1

#ifndef UNLUANOVA_LUA_BACKEND
#define UNLUANOVA_LUA_BACKEND UNLUANOVA_BACKEND_LUAU
#endif

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU

#include "lua.h"
#include "lualib.h"
#include "luacode.h"

#else

#include "lua.hpp"

#endif

// --- plain-Lua spellings Luau does not provide -------------------------------
//
// The rest of UnLuaNova was written against plain Lua. Rather than touch every call
// site, Luau is taught the same spellings here - this header is the one place where
// the backends are allowed to differ, and everything below is a faithful equivalent
// of the lauxlib/lua 5.x behaviour the call sites expect.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU

// Luau declares luaL_error as noreturn void (it is a macro over luaL_errorL); plain
// Lua returns int, and a great many call sites are written `return luaL_error(...)`.
// Raise the same error - longjmp, so the value is never observed - with the Lua type.
// Declared first: the shims below raise errors themselves.
#undef luaL_error
inline int luaL_error(lua_State* L, const char* Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    lua_pushvfstring(L, Format, Args);
    va_end(Args);
#ifdef _MSC_VER
    __pragma(warning(push))
    __pragma(warning(disable: 4702)) // unreachable: lua_error longjmps out
#endif
    lua_error(L);
    return 0;
#ifdef _MSC_VER
    __pragma(warning(pop))
#endif
}

// Luau's lua_pushcfunction/lua_pushcclosure are macros that also take a debug name,
// and take it *before* nup. Re-expose them as the plain-Lua two/three-argument
// functions so existing call sites compile unchanged; the debug name is optional.
#undef lua_pushcfunction
inline void lua_pushcfunction(lua_State* L, lua_CFunction Fn, const char* DebugName = "UnLuaNova")
{
    lua_pushcclosurek(L, Fn, DebugName, 0, nullptr);
}

#undef lua_pushcclosure
inline void lua_pushcclosure(lua_State* L, lua_CFunction Fn, int NUp, const char* DebugName = "UnLuaNova")
{
    lua_pushcclosurek(L, Fn, DebugName, NUp, nullptr);
}

// Luau spells the integer type test lua_isinteger64 (LUA_TINTEGER).
inline int lua_isinteger(lua_State* L, int Index)
{
    return lua_isinteger64(L, Index);
}

// Luau kept the 5.1 name for "raw length". UNLUANOVA_UDATA_LEN is the userdata spelling.
inline int lua_rawlen(lua_State* L, int Index)
{
    return lua_objlen(L, Index);
}

// Lua 5.2+ only. pushvalue + replace is the documented emulation.
inline void lua_copy(lua_State* L, int FromIndex, int ToIndex)
{
    lua_pushvalue(L, FromIndex);
    lua_replace(L, ToIndex);
}

// Luau's lua_ref/lua_unref address the value by stack index and do not pop it, while
// luaL_ref pops the value on top of the stack. Match the lauxlib behaviour so callers
// can keep writing `Ref = luaL_ref(L, LUA_REGISTRYINDEX)` after pushing a value.
inline int luaL_ref(lua_State* L, int Table)
{
    (void)Table; // Luau only ever refs into the registry
    const int Ref = lua_ref(L, -1);
    lua_pop(L, 1);
    return Ref;
}

inline void luaL_unref(lua_State* L, int Table, int Ref)
{
    (void)Table;
    lua_unref(L, Ref);
}

inline void luaL_setmetatable(lua_State* L, const char* TableName)
{
    luaL_getmetatable(L, TableName);
    lua_setmetatable(L, -2);
}

// Luau has no text loader: source is compiled to bytecode up front and then handed to
// luau_load. These are the lauxlib spellings the rest of the plugin is written against.
inline int luaL_loadbufferx(lua_State* L, const char* Buff, size_t Size, const char* Name, const char* Mode)
{
    (void)Mode; // Luau always compiles source; there is no "binary chunk" mode to honour
    lua_CompileOptions Options = {};
    Options.optimizationLevel = 1; // baseline - keeps line info, unlike level 2
    Options.debugLevel = 1;        // line info + function names: enough for backtraces

    size_t BytecodeSize = 0;
    char* Bytecode = luau_compile(Buff, Size, &Options, &BytecodeSize);
    if (!Bytecode)
    {
        lua_pushliteral(L, "Luau compilation failed");
        return LUA_ERRSYNTAX;
    }

    const int Result = luau_load(L, Name, Bytecode, BytecodeSize, 0);
    free(Bytecode); // luau_load has already decoded the bytecode into a Proto
    return Result;
}

#define luaL_loadbuffer(L, s, sz, n) luaL_loadbufferx(L, s, sz, n, nullptr)

inline int luaL_loadstring(lua_State* L, const char* Source)
{
    return luaL_loadbufferx(L, Source, strlen(Source), Source, nullptr);
}

#define luaL_dostring(L, s) (luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))

// Luau has no luaL_setfuncs. Register each entry as a *named* C closure - which is
// exactly what the debug name argument of lua_pushcclosure is for.
inline void luaL_setfuncs(lua_State* L, const luaL_Reg* Reg, int NUp)
{
    for (; Reg->name != nullptr; Reg++)
    {
        for (int i = 0; i < NUp; i++)
        {
            lua_pushvalue(L, -NUp);
        }
        lua_pushcclosure(L, Reg->func, NUp, Reg->name);
        lua_setfield(L, -(NUp + 2), Reg->name);
    }
    lua_pop(L, NUp);
}

// Luau has no luaL_requiref either. This is the uncached form: the module table is
// left on the stack, and published as a global when Glb is set, which is all the
// call sites rely on.
inline void luaL_requiref(lua_State* L, const char* ModuleName, lua_CFunction OpenF, int Glb)
{
    lua_pushcfunction(L, OpenF, ModuleName);
    lua_pushstring(L, ModuleName);
    lua_call(L, 1, 1);
    if (Glb)
    {
        lua_pushvalue(L, -1);
        lua_setglobal(L, ModuleName);
    }
}

// Lua 5.2+ folded lua_equal/lua_lessthan into lua_compare. Luau kept the 5.1 pair, so
// rebuild the comparison out of them. Raising an error on an unknown op matches Lua.
#ifndef LUA_OPEQ
#define LUA_OPEQ 0
#endif
#ifndef LUA_OPLT
#define LUA_OPLT 1
#endif
#ifndef LUA_OPLE
#define LUA_OPLE 2
#endif

inline int lua_compare(lua_State* L, int Index1, int Index2, int Op)
{
    switch (Op)
    {
    case LUA_OPEQ:
        return lua_equal(L, Index1, Index2);
    case LUA_OPLT:
        return lua_lessthan(L, Index1, Index2);
    case LUA_OPLE:
        return lua_lessthan(L, Index1, Index2) || lua_equal(L, Index1, Index2);
    default:
        return luaL_error(L, "invalid option for lua_compare");
    }
}

// Luau has no lua_register (it only offers lua_registeruserdatadirect*). Plain Lua's
// form pushes the C function and assigns it to a global of the same name.
#undef lua_register
inline void lua_register(lua_State* L, const char* Name, lua_CFunction Fn)
{
    lua_pushcfunction(L, Fn, Name);
    lua_setglobal(L, Name);
}

// lauxlib.h defines this on plain Lua; Luau never did.
#ifndef LUA_GNAME
#define LUA_GNAME "_G"
#endif

#endif // UNLUANOVA_BACKEND_LUAU

// --- userdata allocation ----------------------------------------------------
// Lua 5.5: lua_newuserdatauv(L, size, nuvalue)
// Luau:    lua_newuserdatatagged(L, size, tag) - 0 is the "untagged" value, and
//          lua_newuserdata() is defined as exactly that, so the plain name is shared.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_NEWDATA(L, Size) lua_newuserdatatagged(L, Size, 0)
#else
#define UNLUANOVA_NEWDATA(L, Size) lua_newuserdatauv(L, Size, 0)
#endif

// --- size of a userdata payload ---------------------------------------------
// Lua 5.3+ replaced lua_objlen with lua_rawlen; Luau kept the 5.1 name.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_UDATA_LEN(L, Idx) lua_objlen(L, Idx)
#else
#define UNLUANOVA_UDATA_LEN(L, Idx) lua_rawlen(L, Idx)
#endif

// --- compiling a chunk -------------------------------------------------------
// Luau exposes luau_load() and has no bytecode loader/dumper at all.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_LOAD(L, ChunkName, Data, Size) luau_load(L, ChunkName, Data, Size, 0)
#else
#define UNLUANOVA_LOAD(L, ChunkName, Data, Size) luaL_loadbuffer(L, Data, Size, ChunkName)
#endif

// --- garbage collector --------------------------------------------------------
// Lua 5.5 has a generational mode (LUA_GCGEN). Luau does not - it tunes the
// incremental collector through the goal / step multiplier instead.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_SETUP_GC(L)              \
	do {                                   \
		lua_gc(L, LUA_GCSETGOAL, 100);      \
		lua_gc(L, LUA_GCSETSTEPMUL, 100);   \
	} while (false)
#else
#define UNLUANOVA_SETUP_GC(L)              \
	do {                                   \
		lua_gc(L, LUA_GCGEN, 0, 0);         \
	} while (false)
#endif

// --- coroutine resume ---------------------------------------------------------
// Lua 5.4/5.5 report the number of results through an out param; Luau's lua_resume
// takes only three arguments. Callers pass &NResults and it is simply ignored on Luau.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_RESUME(L, From, NArgs, NResults) lua_resume(L, From, NArgs)
#else
#define UNLUANOVA_RESUME(L, From, NArgs, NResults) lua_resume(L, From, NArgs, NResults)
#endif

// --- main thread ---------------------------------------------------------------
// Luau exposes lua_mainthread() directly. Plain Lua keeps the main thread in the
// registry instead, so fetch it from there - that is public API, and avoids the
// G(L)->mainthread shortcut that would otherwise drag in lstate.h.
inline lua_State* UnLuaNovaGetMainState(lua_State* L)
{
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
    return lua_mainthread(L);
#else
    const int Top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    lua_State* MainThread = lua_tothread(L, -1);
    lua_settop(L, Top);
    return MainThread;
#endif
}

// --- debug introspection -------------------------------------------------------
// Two real API differences the profiler has to live with:
//
// 1. lua_getinfo takes an extra `level` argument on Luau.
// 2. Luau's lua_Debug has no `event` field, so a hook callback cannot tell a call from
//    a return. Pairing begin/end events (what Unreal Insights wants) is therefore only
//    possible on plain Lua; on Luau the profiler samples from the interrupt callback
//    instead. See UNLUANOVA_HAS_HOOK_EVENT below.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_GETINFO(L, Level, What, Ar) lua_getinfo(L, Level, What, Ar)
#define UNLUANOVA_HAS_HOOK_EVENT 0
#else
#define UNLUANOVA_GETINFO(L, Level, What, Ar) lua_getinfo(L, What, Ar)
#define UNLUANOVA_HAS_HOOK_EVENT 1
#endif

// --- sandboxing ---------------------------------------------------------------
// Luau hardens the global table in a single call: after luaL_sandbox the globals are
// read-only and the env is flagged safe, so a hotfixed / downloaded script can no
// longer clobber engine bindings by accident. Plain Lua 5.5 has no equivalent (5.5
// adds global *declarations*, not a sandbox), so that backend opts out entirely —
// callers must test UNLUANOVA_HAS_SANDBOX instead of assuming it took effect.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_HAS_SANDBOX 1
#define UNLUANOVA_SANDBOX(L) luaL_sandbox(L)
#define UNLUANOVA_SANDBOXTHREAD(L) luaL_sandboxthread(L)
#else
#define UNLUANOVA_HAS_SANDBOX 0
#define UNLUANOVA_SANDBOX(L) ((void)0)
#define UNLUANOVA_SANDBOXTHREAD(L) ((void)0)
#endif

// --- debug channel ------------------------------------------------------------
// Luau keeps the debug hook on a channel of its own (lua_callhook), completely
// separate from the lua_Callbacks::interrupt slot the profiler and the dead-loop
// watchdog share - so installing a debugger disturbs neither of them.
//
// Plain Lua has no second channel: lua_sethook is the only hook and FLuaVmHooks
// already owns it, so a debugger must register there as a listener rather than
// call lua_sethook itself.
//
// The breakpoint primitives - ask the VM to stop on a given line of a given
// function - have no equivalent in plain Lua. lua_sethook can *report* every
// line, but cannot make the VM stop on one. Those are Luau-only.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_HAS_BREAKPOINT 1
#define UNLUANOVA_SET_DEBUG_HOOK(L, Hook, UserData) lua_callhook(L, Hook, UserData)
#define UNLUANOVA_BREAKPOINT(L, FuncIndex, Line, Enabled) lua_breakpoint(L, FuncIndex, Line, Enabled)
#define UNLUANOVA_SINGLESTEP(L, Enabled) lua_singlestep(L, Enabled)
#define UNLUANOVA_ATBREAKPOINT(L) lua_atbreakpoint(L)
#define UNLUANOVA_STACKDEPTH(L) lua_stackdepth(L)
#else
#define UNLUANOVA_HAS_BREAKPOINT 0
#define UNLUANOVA_SET_DEBUG_HOOK(L, Hook, UserData) ((void)0) // register via FLuaVmHooks
#define UNLUANOVA_BREAKPOINT(L, FuncIndex, Line, Enabled) 0
#define UNLUANOVA_SINGLESTEP(L, Enabled) ((void)0)
#define UNLUANOVA_ATBREAKPOINT(L) 0
#define UNLUANOVA_STACKDEPTH(L) 0
#endif

// --- allocation callbacks -----------------------------------------------------
// Luau reports every allocation through lua_Callbacks, and crucially passes the
// memory category, the Lua type and the userdata tag *as arguments* - so a breakdown
// by "what kind of memory" costs no stack walk at all. Plain Lua has no such thing;
// its allocator hook only ever sees a size, which is all the port FLuaMemoryStats
// already uses.
//
// Note that onfree carries no size, so the categorized view counts allocation
// *volume* (churn) rather than live bytes. Live totals come from the allocator hook.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_HAS_ALLOC_CALLBACKS 1
#define UNLUANOVA_SET_ALLOC_CALLBACKS(L, Alloc, Free) \
    do \
    { \
        lua_callbacks(L)->onallocate = (Alloc); \
        lua_callbacks(L)->onfree = (Free); \
    } while (0)
#else
#define UNLUANOVA_HAS_ALLOC_CALLBACKS 0
#define UNLUANOVA_SET_ALLOC_CALLBACKS(L, Alloc, Free) ((void)0)
#endif

// --- per-function id, for profiler keys ----------------------------------------
// Luau exposes a VM-wide unique proto id on lua_Debug, which makes an ideal cache key.
// Plain Lua has no such field, so key on (source pointer, line the function was defined
// on) instead - still O(1), still stable for the lifetime of the loaded chunk.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_DEBUG_PROTOID(Ar) ((uint64)((Ar)->protoid))
#else
#define UNLUANOVA_DEBUG_PROTOID(Ar) \
    ((uint64)(((uintptr_t)(Ar)->source) << 32) | (uint32)((Ar)->linedefined))
#endif

// --- capability flags ----------------------------------------------------------
// These are the semantics that actually change behaviour, so call sites must branch
// on them rather than on the backend id.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU

/** Luau has no table __gc; finalizers only exist for userdata (lua_newuserdatadtor). */
#define UNLUANOVA_HAS_TABLE_GC 0
/** Luau numbers are doubles only - there is no integer subtype. */
#define UNLUANOVA_HAS_INTEGER_SUBTYPE 0
/** Luau drops io/os/loadfile/dofile for sandboxing and has no string.dump bytecode. */
#define UNLUANOVA_HAS_BYTECODE_DUMP 0
/** Luau uses __iter instead of __pairs/__ipairs. */
#define UNLUANOVA_HAS_ITER_METAMETHOD 1

#else

#define UNLUANOVA_HAS_TABLE_GC 1
#define UNLUANOVA_HAS_INTEGER_SUBTYPE 1
#define UNLUANOVA_HAS_BYTECODE_DUMP 1
#define UNLUANOVA_HAS_ITER_METAMETHOD 0

#endif
