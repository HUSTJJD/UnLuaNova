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
