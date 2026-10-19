// Single place where the Lua VM's API differences are absorbed.
//
// Goal: the rest of UnLuaNova never includes a VM internal header (lstate.h / lobject.h /
// lfunc.h) and never branches on LUA_VERSION_NUM. That is a hard prerequisite for
// swapping Lua 5.4 out for Luau.
//
// UnLuaNovaU_USE_LUAU = 0 -> Lua 5.4.x (current, default)
// UnLuaNovaU_USE_LUAU = 1 -> Luau
//
// The macro is injected by UnLuaNova.Build.cs; this header defaults it to 0 so the tree
// keeps building exactly as it does today.

#pragma once

#ifndef UnLuaNovaU_USE_LUAU
#define UnLuaNovaU_USE_LUAU 0
#endif

#if UnLuaNovaU_USE_LUAU
#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#else
#include "lua.hpp"
#endif

// --- allocation -------------------------------------------------------------
// Lua 5.4.x: lua_newuserdata is a macro over lua_newuserdatauv(L, s, 1).
// Luau:      lua_newuserdata is a macro over lua_newuserdatatagged(L, s, 0).
// Both exist, so the plain name works for either VM.
#define UnLuaNovaU_NEWDATA(L, Size) lua_newuserdata(L, Size)

// --- size of a userdata payload --------------------------------------------
// Lua 5.3+ removed lua_objlen in favour of lua_rawlen; Luau kept the 5.1 name.
#if UnLuaNovaU_USE_LUAU
#define UnLuaNovaU_UDATA_LEN(L, Idx) lua_objlen(L, Idx)
#else
#define UnLuaNovaU_UDATA_LEN(L, Idx) lua_rawlen(L, Idx)
#endif

// --- compiling a chunk -------------------------------------------------------
// Luau exposes luau_load() and has no bytecode loader/dumper at all.
#if UnLuaNovaU_USE_LUAU
#define UnLuaNovaU_LOAD(L, ChunkName, Data, Size) \
	luau_load(L, ChunkName, Data, Size, 0)
#else
#define UnLuaNovaU_LOAD(L, ChunkName, Data, Size) \
	luaL_loadbuffer(L, Data, Size, ChunkName)
#endif

// --- garbage collector --------------------------------------------------------
// Lua 5.4 has a generational mode (LUA_GCGEN); Luau does not - it tunes the incremental
// collector through the goal / step multiplier / step size instead.
#if UnLuaNovaU_USE_LUAU
#define UnLuaNovaU_SETUP_GC(L)                 \
	do {                                   \
		lua_gc(L, LUA_GCSETGOAL, 100);      \
		lua_gc(L, LUA_GCSETSTEPMUL, 100);   \
	} while (false)
#else
#define UnLuaNovaU_SETUP_GC(L)                 \
	do {                                   \
		lua_gc(L, LUA_GCGEN, 0, 0);         \
	} while (false)
#endif

// --- misc ---------------------------------------------------------------------
#if UnLuaNovaU_USE_LUAU
// Luau has no table __gc: finalizers only exist for userdata (lua_newuserdatadtor).
#define UnLuaNovaU_HAS_TABLE_GC 0
// Luau's numbers are doubles only - there is no integer subtype.
#define UnLuaNovaU_HAS_INTEGER_SUBTYPE 0
// Luau drops io/os/loadfile/dofile for sandboxing, and has no string.dump bytecode.
#define UnLuaNovaU_HAS_BYTECODE_DUMP 0
#else
#define UnLuaNovaU_HAS_TABLE_GC 1
#define UnLuaNovaU_HAS_INTEGER_SUBTYPE 1
#define UnLuaNovaU_HAS_BYTECODE_DUMP 1
#endif
