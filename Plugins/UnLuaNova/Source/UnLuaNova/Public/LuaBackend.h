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
