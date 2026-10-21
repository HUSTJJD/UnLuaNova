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
// This header is also the only file that pulls in a backend's headers, and it does so
// through UnLuaNovaBackend_Lua.h / UnLuaNovaBackend_Luau.h: no other file should include
// lua.h, lualib.h, lauxlib.h or luacode.h, because both backends own those names.

#pragma once

#include <cstdarg> // va_list, for the luaL_error shim below
#include <cmath>   // floor, for UnLuaNovaIsIntegralNumber below
#include <cstdint> // int64_t - this header is also included by the vendored extension
                   // modules, where Unreal's own `int64` typedef is not in scope
#include <cstdlib> // free, for the luaL_loadbufferx shim below
#include <cstring> // strlen, for the luaL_loadstring shim below

#define UNLUANOVA_BACKEND_LUA 0
#define UNLUANOVA_BACKEND_LUAU 1

#ifndef UNLUANOVA_LUA_BACKEND
#define UNLUANOVA_LUA_BACKEND UNLUANOVA_BACKEND_LUAU
#endif

// Both backends ship headers called lua.h / lualib.h, so never ask for one by name:
// include-order decides which you get, and when it picks the wrong one the build still
// compiles far enough to bury the cause under dozens of unrelated errors. These two
// wrappers have unique names and include their backend by path relative to themselves,
// so neither can be satisfied by, or reach into, the other backend.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU

#include "UnLuaNovaBackend_Luau.h"

#else

#include "UnLuaNovaBackend_Lua.h"

#endif

// --- guards against the backends bleeding into each other --------------------
//
// Three independent checks, cheapest first. Any one of them turns "compiles, then
// explodes somewhere else" into a single error that points at the real cause.

// 1. Which backend *modules* are in this build (a PublicDefinitions marker from
// Lua.Build.cs / Luau.Build.cs). Both present means something named the backend that
// was not selected - UnLuaNovaDemo.Build.cs once listed 'Lua' outright while the
// backend was Luau, which put both on the include path.
#if defined(UNLUANOVA_BACKEND_MODULE_LUA) && defined(UNLUANOVA_BACKEND_MODULE_LUAU)
#error "Both the Lua and Luau backend modules are in this build. They export the same lua_* symbols and share header file names, so one is answering the other's includes. Only UnLuaNova.Build.cs may name a backend module - find and remove the direct dependency (grep another .Build.cs for 'Lua' or 'Luau')."
#endif

// 2. The selected module is the one that is actually here. Skipped when no marker is
// visible, which only happens for translation units outside the dependency graph.
#if defined(UNLUANOVA_BACKEND_MODULE_LUA) || defined(UNLUANOVA_BACKEND_MODULE_LUAU)
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#if !defined(UNLUANOVA_BACKEND_MODULE_LUAU)
#error "UNLUANOVA_LUA_BACKEND selects Luau, but the module in this build is Lua. The backend is chosen once in UnLuaNova.Build.cs - check LuaBackend in Config/DefaultUnLuaNovaEditor.ini, and check for a stale Intermediate/ build."
#endif
#else
#if !defined(UNLUANOVA_BACKEND_MODULE_LUA)
#error "UNLUANOVA_LUA_BACKEND selects Lua, but the module in this build is Luau. The backend is chosen once in UnLuaNova.Build.cs - check LuaBackend in Config/DefaultUnLuaNovaEditor.ini, and check for a stale Intermediate/ build."
#endif
#endif
#endif

// 3. Which headers we actually got. LUA_VECTOR_SIZE exists only in Luau's luaconf.h,
// and lua_Integer is 32-bit there versus 64-bit in plain Lua - the width difference is
// the one that silently corrupts values, so it is worth asserting on its own.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#if !defined(LUA_VECTOR_SIZE)
#error "Configured for the Luau backend but these are not Luau's headers. Check that Plugins/UnLuaNova/Source/ThirdParty/Luau is checked out (git submodule update --init --recursive) and that the Luau module is really in the build - see any 'Luau deleted' note in the build log."
#endif
static_assert(sizeof(lua_Integer) == 4, "Luau's lua_Integer is 32-bit; these headers are not Luau's, or Luau changed its integer type - UNLUANOVA_PUSHINT64/TOINT64 need rechecking.");
#else
#if defined(LUA_VECTOR_SIZE)
#error "Configured for the plain Lua backend but these are Luau's headers. Check that Plugins/UnLuaNova/Source/ThirdParty/Lua is checked out and that the Lua module is really in the build."
#endif
static_assert(sizeof(lua_Integer) == 8, "Plain Lua's lua_Integer is 64-bit; these headers are not Lua's, or LUA_INT_TYPE was changed in luaconf.h - UNLUANOVA_PUSHINT64/TOINT64 need rechecking.");
#endif

// --- creating a state ---------------------------------------------------------
// Both backends build a lua_State, but not with the same argument list: plain Lua takes a
// third one, a seed for the string hash (it randomises string hashing so a script cannot
// force collisions), Luau has no such parameter. The spelling that produces a seed the same
// way the reference lauxlib does is luaL_makeseed, which is why this is not simply
// "pass 0" - a fixed seed would undo exactly what the parameter is for.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_NEWSTATE(ALLOC, UD) lua_newstate(ALLOC, UD)
#else
#define UNLUANOVA_NEWSTATE(ALLOC, UD) lua_newstate(ALLOC, UD, luaL_makeseed(nullptr))
#endif

// --- giving a loaded chunk its own environment --------------------------------
// Plain Lua keeps a chunk's environment in upvalue 1 (_ENV); Luau keeps it on the
// LClosure itself - `luaF_newLclosure(L, 0, envt, main)` in lvmload.cpp gives the main
// chunk *zero* upvalues. So `lua_setupvalue(L, Idx, 1)` on Luau is a no-op, and worse, a
// silent one: lapi.cpp only pops the value when it found the upvalue, so the env table
// stays on the stack. Anything that then expects the chunk on top of the stack calls the
// table instead, and the resulting error text ends up being indexed as if it were a
// table - which, outside a protected call, is an abort().
//
// Both calls take the chunk at Idx (negative) and the env table on top of the stack, and
// both pop it on success; plain Lua does not pop when it refuses, so this does.
inline bool UnLuaNovaSetChunkEnv(lua_State* L, int Idx)
{
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
    return lua_setfenv(L, Idx) != 0; // always pops
#else
    const bool Ok = lua_setupvalue(L, Idx, 1) != nullptr; // pops only on success
    if (!Ok)
        lua_pop(L, 1);
    return Ok;
#endif
}

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
// Luau declares lua_error [[noreturn]] (l_noret). The dead `return 0` that follows is
// part of the contract, not a bug - this header mirrors plain Lua, where luaL_error
// returns int - but it still has to compile: see the note at the raise below.
inline int luaL_error(lua_State* L, const char* Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    lua_pushvfstring(L, Format, Args);
    va_end(Args);
    // Gated on L so the `return 0` stays reachable: lua_error is [[noreturn]], so a plain
    // `lua_error(L); return 0;` is dead code - C4702, which BuildSettingsVersion.V7
    // promotes to an error and which neither #pragma warning nor the module's warning
    // level can switch back off. L is never null at a call site, so the longjmp happens
    // exactly as before.
    if (L != nullptr)
    {
        lua_error(L);
    }
    return 0;
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

// Plain Lua's luaL_newmetatable does two things: it files the new table in the registry
// under TypeName, and it stamps that same name onto the table as __name. Luau's only
// does the first - see laux.cpp, where the whole function is four stack operations and
// no __name anywhere.
//
// UnLuaNova reads __name to get from a metatable back to the type it describes:
// Class_Index, Enum_Index, PropertyRegistry and the container checks all start there.
// On Luau the field was simply absent, so every *first* read of a class or enum member -
// the one that is not cached in the metatable yet - reached GetFieldInternal, looked up
// a nil __name and tripped its check(). Nothing noticed while hot reload was broken,
// because that was the only path reaching an uncached member this early: HotReload.lua
// reads UE.UUnLuaNovaFunctionLibrary.GetScriptRootPath() while it is being required.
//
// Teach Luau the plain-Lua spelling rather than patching each reader. Same contract:
// 1 and the new table on the stack, or 0 and the existing one.
inline int UnLuaNovaNewMetatable(lua_State* L, const char* TypeName)
{
    lua_getfield(L, LUA_REGISTRYINDEX, TypeName);
    if (!lua_isnil(L, -1))
        return 0; // already registered - leave the previous table on top, as plain Lua does
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushstring(L, TypeName);
    lua_setfield(L, -2, "__name");
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, TypeName);
    return 1;
}
#define luaL_newmetatable(L, TypeName) UnLuaNovaNewMetatable(L, TypeName)

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

// --- 64-bit integers ---------------------------------------------------------
// lua_Integer is 64-bit on plain Lua but only 32-bit on Luau (typedef int), and Luau
// spells the 64-bit pair *64. Same name, same signature, half the width - so a 64-bit
// value sent through lua_pushinteger is silently truncated on Luau, at compile time
// without a sound.
//
// Use these whenever the value may exceed 32 bits: UE int64/uint64 properties, enum
// values (their underlying type can be 64-bit), hashes, pointers-as-integers. Plain
// lua_pushinteger/lua_tointeger stay correct for indices, counts and small ints.
//
// They also keep the *type* consistent, which is the subtler half: Luau's
// lua_pushinteger pushes a double (LUA_TNUMBER), so lua_isinteger answers false for a
// value Lua 5.5 reports as an integer. lua_pushinteger64 pushes LUA_TINTEGER instead,
// which is what plain Lua does.
//
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_PUSHINT64(L, V) lua_pushinteger64(L, (int64_t)(V))

// --- integers that fit in 32 bits -------------------------------------------
// LUA_TINTEGER is the right tag for a genuine 64-bit value and the wrong one for
// everything smaller. Luau's compiler emits a double (LUA_TNUMBER) for every literal, so a
// LUA_TINTEGER handed to a script makes `Value + 1` an integer plus a double - a hard
// runtime error, "attempt to perform arithmetic (add) on integer and number". Scripts read
// int32 properties and enum values constantly and do arithmetic on them, so those go out as
// doubles and stay usable; only real 64-bit values keep the integer tag, where keeping the
// value exact matters more than being able to add 1 to it in a script.
#define UNLUANOVA_PUSHINT(L, V) lua_pushnumber(L, (lua_Number)(V))

// Luau's lua_tointeger64 reads *only* the 64-bit integer type (LUA_TINTEGER) and hands
// back 0 for anything else - see lapi.cpp: it tests ttisinteger() and returns 0 from the
// else branch. Every ordinary Lua number is a double there, and the compiler emits
// doubles for every literal, so `return 1` read through it gave 0. Anything round-tripped
// through a script - enum values, int64 properties, indices - was silently zero.
//
// Read the double when the value is not already an integer, under the same rule plain
// Lua's lua_tointeger applies: a number without an exact integer representation is not
// an answer, so 1.5 yields 0 rather than 1.
inline int64_t UnLuaNovaToInt64(lua_State* L, int Idx)
{
    int IsInteger = 0;
    const int64_t AsInteger = (int64_t)lua_tointeger64(L, Idx, &IsInteger);
    if (IsInteger)
        return AsInteger;

    if (!lua_isnumber(L, Idx))
        return 0;

    const double AsDouble = lua_tonumber(L, Idx);
    if (AsDouble != AsDouble) // NaN
        return 0;

    const int64_t Truncated = (int64_t)AsDouble;
    return (double)Truncated == AsDouble ? Truncated : 0;
}

#define UNLUANOVA_TOINT64(L, Idx) UnLuaNovaToInt64(L, Idx)
#else
#define UNLUANOVA_PUSHINT64(L, V) lua_pushinteger(L, (lua_Integer)(V))
// Plain Lua already pushes integers of every width as one numeric type, so both macros
// collapse to the same thing here.
#define UNLUANOVA_PUSHINT(L, V) lua_pushinteger(L, (lua_Integer)(V))
#define UNLUANOVA_TOINT64(L, Idx) ((int64)lua_tointeger(L, Idx))
#endif

// --- can this value be read as an integer? -----------------------------------
// Plain Lua has one numeric type and lua_isinteger() separates 1 from 1.5. Luau has a
// second type tag for integers (LUA_TINTEGER) *and* compiles every literal to a double,
// so neither half of that works there: `SetCollisionEnabled(0)` written in a script
// arrives as a double, while a value UnLuaNova pushed with UNLUANOVA_PUSHINT64 arrives as
// LUA_TINTEGER - and lua_isinteger() answers false for both, which is how every integer
// argument of every UFunction started being rejected.
//
// lua_isnumber() is true for either tag, so it is the question that means the same thing
// on both backends. A double still has to actually hold an integer, because that is the
// only case UNLUANOVA_TOINT64 can read back: 1.5 would silently become 0, and saying so
// is better than that.
inline bool UnLuaNovaIsIntegralNumber(lua_State* L, int Idx)
{
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
    // Already an integer, and it may not even survive a round trip through a double.
    if (lua_isinteger64(L, Idx))
        return true;
#endif

    if (!lua_isnumber(L, Idx))
        return false;

    const double Value = lua_tonumber(L, Idx);
    if (Value != Value) // NaN
        return false;

    const double Truncated = (Value < 0.0) ? -std::floor(-Value) : std::floor(Value);
    return Truncated == Value;
}

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
// Source goes in, a chunk comes out - the same on both backends.
//
// Luau's luau_load() takes *bytecode*, not source, which is why it cannot stand in here:
// the call sites hand over script text. luaL_loadbufferx is the shim above that compiles
// first, so this backend gets the plain-Lua spelling after all.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_LOAD(L, ChunkName, Data, Size) luaL_loadbufferx(L, Data, Size, ChunkName, nullptr)
#else
#define UNLUANOVA_LOAD(L, ChunkName, Data, Size) luaL_loadbuffer(L, Data, Size, ChunkName)
#endif

// Luau's lua_gc always takes (L, what, data). Plain Lua's data argument is ignored for
// most ops, and call sites spell it with two arguments - give them that spelling back.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
inline int lua_gc(lua_State* L, int What)
{
    return lua_gc(L, What, 0);
}
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

// --- native vector type -------------------------------------------------------
// Luau carries a vector directly in a TValue: three (or four) floats, no allocation,
// no indirection. That is the whole point - a hot loop over FVector values allocates
// nothing on Luau, where the userdata path pays a GC-tracked allocation per result.
//
// Plain Lua has no such type, so a "vector" there is a table or a userdata and the
// macros below are inert. Call sites must go through UNLUANOVA_HAS_NATIVE_VECTOR
// rather than assuming the type exists.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_HAS_NATIVE_VECTOR 1
#define UNLUANOVA_ISVECTOR(L, Idx) lua_isvector(L, Idx)
/**
 * Components as a raw array. Its length is LUA_VECTOR_SIZE (3 or 4, depending on how
 * Luau was built), so read only the components you need and never index past 2.
 * The pointer belongs to the value on the stack - do not hold on to it.
 */
#define UNLUANOVA_TOVECTOR(L, Idx) lua_tovector(L, Idx)
// Luau's lua_pushvector takes a fourth (w) component when the value is 4-wide.
#if defined(LUA_VECTOR_SIZE) && LUA_VECTOR_SIZE == 4
#define UNLUANOVA_PUSHVECTOR3(L, X, Y, Z) lua_pushvector(L, (LUA_VECTOR_TYPE)(X), (LUA_VECTOR_TYPE)(Y), (LUA_VECTOR_TYPE)(Z), 0)
#else
#define UNLUANOVA_PUSHVECTOR3(L, X, Y, Z) lua_pushvector(L, (LUA_VECTOR_TYPE)(X), (LUA_VECTOR_TYPE)(Y), (LUA_VECTOR_TYPE)(Z))
#endif
// Luau's buffer type: a fixed-size, typed byte block, again without a per-object
// allocation for the common sizes. Plain Lua has no equivalent.
#define UNLUANOVA_HAS_NATIVE_BUFFER 1
#define UNLUANOVA_ISBUFFER(L, Idx) lua_isbuffer(L, Idx)
#else
#define UNLUANOVA_HAS_NATIVE_VECTOR 0
#define UNLUANOVA_ISVECTOR(L, Idx) false
#define UNLUANOVA_TOVECTOR(L, Idx) nullptr
#define UNLUANOVA_PUSHVECTOR3(L, X, Y, Z) ((void)0)
#define UNLUANOVA_HAS_NATIVE_BUFFER 0
#define UNLUANOVA_ISBUFFER(L, Idx) false
#endif

// --- read-only tables ---------------------------------------------------------
// Luau can mark any table read-only; writes then fail instead of silently creating a
// field. Plain Lua has no such flag, so the macros are inert there and callers have to
// branch on UNLUANOVA_HAS_SANDBOX (or accept a no-op).
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_HAS_READONLY_TABLES 1
#define UNLUANOVA_SET_READONLY(L, Idx, Enabled) lua_setreadonly(L, Idx, (Enabled) ? 1 : 0)
#define UNLUANOVA_GET_READONLY(L, Idx) (lua_getreadonly(L, Idx) != 0)
#else
#define UNLUANOVA_HAS_READONLY_TABLES 0
#define UNLUANOVA_SET_READONLY(L, Idx, Enabled) ((void)0)
#define UNLUANOVA_GET_READONLY(L, Idx) false
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
// Luau has no lua_sethook. A debugger claims the two lua_Callbacks slots the VM
// calls on its own: debugbreak (a BREAK instruction, i.e. a breakpoint) and
// debugstep (single-step mode). Both are completely separate from the
// lua_Callbacks::interrupt slot the profiler and the dead-loop watchdog share,
// so installing a debugger disturbs neither of them.
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
// A null hook means "off", and that is the only way off the channel.
//
// lua_callhook() looks like the setter but is not one: it *invokes* a hook on the
// spot, against the current call frame, so it installed nothing and - with a null
// hook and no frame to speak of (L->ci == base_ci, hence L->ci->p == null) -
// dereferenced a proto that does not exist. luau_callhook() dereferences it at
// lvmexecute.cpp:181 before it ever reaches the callback.
#define UNLUANOVA_SET_DEBUG_HOOK(L, Hook, UserData) \
    do \
    { \
        lua_callbacks(L)->debugbreak = (Hook); \
        lua_callbacks(L)->debugstep = (Hook); \
        lua_callbacks(L)->userdata = (UserData); \
    } while (0)
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

/**
 * 0 on Luau: it has no __gc metamethod at all - "TM_GC" does not exist anywhere in
 * Luau/VM/src. Userdata finalizers there go through lua_newuserdatadtor() instead.
 *
 * Note this is about *userdata*: every __gc in this codebase is registered on a
 * userdata metatable (TArray/TMap/TSet/FText/file handles), which is why the flag used
 * to be called HAS_TABLE_GC - that name was wrong and made it look like an unused
 * corner. Luau's substitute, lua_newuserdatadtor(), is NOT wired in yet: every __gc
 * here ends in ContainerRegistry::Remove(), which goes back through FLuaEnv for the
 * main state and then touches the Lua stack, and that is not safe from a GC callback.
 * Closing the gap needs a different cleanup design - explicit release, or a sweep owned
 * by the env - not a rename. Until then Luau simply never finalizes these userdata.
 */
#define UNLUANOVA_HAS_USERDATA_GC 0
/**
 * 0 on Luau: it is recognized by the iteration protocol only through __iter (TM_ITER).
 * __pairs/__ipairs do not exist there, and Luau's own pairs() even insists on a real
 * table, so registering "__pairs" on a userdata is silently dead. Use
 * UNLUANOVA_ITER_ENTRY to register the right name.
 */
#define UNLUANOVA_HAS_ITER_METAMETHOD 1
#else

#define UNLUANOVA_HAS_USERDATA_GC 1
#define UNLUANOVA_HAS_ITER_METAMETHOD 0

#endif

// --- iteration metamethod ----------------------------------------------------
// Luau's FORGPREP looks up TM_ITER ("__iter"), calls it with the object and expects
// the usual three results (generator, state, initial) - which is exactly the protocol
// a Lua __pairs function already implements. So the same C function serves both, and
// only the name it is registered under differs. Drop this into a luaL_Reg table:
//
//     static const luaL_Reg TArrayLib[] = { ..., UNLUANOVA_ITER_ENTRY(TArray_Pairs) };
#if UNLUANOVA_HAS_ITER_METAMETHOD
#define UNLUANOVA_ITER_ENTRY(Fn) {"__iter", (Fn)}
#else
#define UNLUANOVA_ITER_ENTRY(Fn) {"__pairs", (Fn)}
#endif

// --- can this C function yield? ------------------------------------------------
// Both backends answer this, but only Luau ever says no: it refuses lua_yield() once
// another C boundary has been crossed (nCcalls > baseCcalls). Reaching a C function
// straight from Lua is fine - Luau's LOP_CALL does not bump nCcalls - but one reached
// through lua_pcall is not, and Luau reports that as the opaque "attempt to yield across
// metamethod/C-call boundary". So a LatentAction started by script works, while one
// started inside an engine callback does not. Ask before yielding so the failure names
// the real constraint instead of surfacing a VM error.
#define UNLUANOVA_IS_YIELDABLE(L) lua_isyieldable(L)

// --- filling in what Luau leaves out -----------------------------------------
// Luau is a deliberately smaller language than the plain Lua this plugin was written
// against, and two of its omissions change behaviour rather than merely failing to
// compile:
//
//   * its debug library is traceback, info and the profiler hooks and nothing else,
//     while plain Lua 5.5 also answers getupvalue / setupvalue / upvalueid / getlocal /
//     setlocal / getregistry / getuservalue. Anything that hot-reloads Lua - UnLuaNova's
//     own HotReload.lua among them - needs those to move a live closure's state onto a
//     freshly loaded one.
//   * it has no load(), so there is no way for a script to compile source at all.
//
// Both are patched in from the C API Luau does have, so the two backends answer the same
// questions. Where Luau genuinely has no answer - a registry table, uservalues - the
// shim returns nothing instead of raising, which is what lets a script degrade instead
// of dying.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
#define UNLUANOVA_SETUP_SCRIPT_COMPAT(L) UnLuaNova::SetupScriptCompat(L)
#else
#define UNLUANOVA_SETUP_SCRIPT_COMPAT(L) ((void)0)
#endif

namespace UnLuaNova
{
    // Adds the missing debug functions to a Luau state's debug table and installs load().
    // Existing entries are left alone, so this only ever adds.
    void SetupScriptCompat(lua_State* L);
}
