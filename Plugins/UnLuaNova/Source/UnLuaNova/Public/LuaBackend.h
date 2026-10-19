// Single place where the two supported Lua backends differ.
//
// Officially supported backends:
//   UNLUANOVA_BACKEND_LUA55 (0) - Lua 5.5.x  (https://github.com/lua/lua, vendored as a submodule)
//   UNLUANOVA_BACKEND_LUAU  (1) - Luau       (https://github.com/luau-lang/luau, vendored as a submodule)  <-- default
//
// The backend is chosen by UNLUANOVA_LUA_BACKEND, injected by UnLuaNova.Build.cs from
// the `LuaBackend` setting. This header only supplies a default so a stray translation
// unit still compiles.
//
// Everything else in UnLuaNova must go through the UNLUANOVA_* macros below instead of
// calling the VM directly, so that neither backend leaks into the rest of the codebase.

#pragma once

#define UNLUANOVA_BACKEND_LUA55 0
#define UNLUANOVA_BACKEND_LUAU 1

#ifndef UNLUANOVA_LUA_BACKEND
// Default is Lua 5.5: it compiles straight through UBT (see ThirdParty/Lua55Lib), so
// the tree always builds. Luau is opt-in and additionally needs its CMake step - see
// ThirdParty/LuauLib/Luau.Build.cs.
#define UNLUANOVA_LUA_BACKEND UNLUANOVA_BACKEND_LUA55
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
