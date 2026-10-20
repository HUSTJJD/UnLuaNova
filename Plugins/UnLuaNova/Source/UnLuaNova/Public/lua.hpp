// Tencent is pleased to support the open source community by making UnLua available.
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

// What the vendored C modules under Plugins/UnLuaNovaExtensions get when they ask for
// "lua.hpp".
//
// LuaSocket / LuaProtobuf / LuaRapidjson all include it expecting plain Lua's headers.
// Plain Lua and Luau both ship lua.h/lualib.h, so that include used to be answered by
// whichever backend directory happened to come first on the include path - which is not
// necessarily the one being built. It is also the wrong tool in the first place: lua.hpp
// exists to add extern "C", which is right for plain Lua (built as C) and wrong for
// Luau (built as C++, whose symbols are as mangled as anyone else's).
//
// So this routes through LuaBackend.h, which already owns that choice and reaches the
// backend's headers by path rather than by name, and then adds the handful of lauxlib
// spellings Luau lacks that these three vendors actually call.

#pragma once

// --- rapidjson's SizeType vs Unreal's ---------------------------------------------
//
// LuaRapidjson's sources say `using namespace rapidjson;` at global scope, which puts
// rapidjson's SizeType into the global lookup set - and Unreal's TArray, TMap, ... all
// declare a member `SizeType` of their own. Every Unreal container template instantiated
// after that line therefore hides a global declaration: C4459, which is an error from
// BuildSettingsVersion.V2 on. Neither #pragma warning nor the module's warning level
// suppresses it, because the hiding declaration arrives from the shared PCH carrying the
// engine's stricter warning state.
//
// Rename rapidjson's spelling while its headers are read instead. Every source in that
// module includes this header before any rapidjson header, and nothing Unreal owns is
// parsed afterwards in those translation units, so the macro cannot reach engine code.
// The name is a typedef, so nothing about layout or ABI changes.
#define SizeType UnLuaNovaRapidjsonSizeType

#include "LuaBackend.h"

#include <cerrno>  // errno, for the luaL_fileresult shim below
#include <cstring> // strerror_s / memset, for the shims below

// Vendored modules declare their own luaopen_* entry points with LUALIB_API, and those
// live in *this* module, not in the Lua DLL. On the plain-Lua backend LUALIB_API
// expands to __declspec(dllimport) (LUA_BUILD_AS_DLL), which makes MSVC reject the
// definition outright (C2491). Plain `extern` is what those definitions need, on both
// backends.
#undef LUALIB_API
#define LUALIB_API extern

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU

// Luau defines no LUA_VERSION_NUM at all, and every vendored module here branches on it
// to pick between the Lua 5.1 and the 5.2+ spellings. 503 puts them on the modern
// branch, which is the one both backends can satisfy: the 5.1 branch needs
// luaL_register() and luaL_typerror(), and neither exists in plain Lua 5.5 any more, so
// a vendored module compiled that way could never build against the other backend.
//
// It is a compile-time lie about the version, and deliberately so - the alternative is
// forking all three vendors. Luau is a 5.1 derivative; what these modules actually need
// to know is which *spelling* to use, and 503 is the one both backends can answer.
#ifndef LUA_VERSION_NUM
#define LUA_VERSION_NUM 503
#endif

// Luau spells this LUA_BUFFERSIZE.
#ifndef LUAL_BUFFERSIZE
#define LUAL_BUFFERSIZE LUA_BUFFERSIZE
#endif

#define luaL_prepbuffer(B) luaL_prepbuffsize(B, LUAL_BUFFERSIZE)
// Luau's luaL_Strbuf advances `p` instead of carrying a length.
#define luaL_addsize(B, s) ((B)->p += (s))

// Luau's lua_rawgetp/lua_rawsetp are macros over its *ptagged variants, which take a
// non-const void* and return void* - vendored code passes const pointers and, in pb.cpp,
// wants the Lua 5.2 spelling that returns the type it just pushed. Give them that.
#undef lua_rawgetp
#undef lua_rawsetp
inline int lua_rawgetp(lua_State* L, int Idx, const void* P)
{
    lua_pushlightuserdatatagged(L, const_cast<void*>(P), 0);
    lua_rawget(L, Idx < 0 && Idx > LUA_REGISTRYINDEX ? Idx - 1 : Idx);
    return lua_type(L, -1);
}

inline void lua_rawsetp(lua_State* L, int Idx, const void* P)
{
    lua_pushlightuserdatatagged(L, const_cast<void*>(P), 0);
    lua_insert(L, -2);
    lua_rawset(L, Idx < 0 && Idx > LUA_REGISTRYINDEX ? Idx - 1 : Idx);
}

// Luau has no continuations, so no lua_pcallk and no lua_KContext. LuaSocket only uses
// them to pass a continuation it then discards, so the plain lua_pcall spelling is
// exactly equivalent - this is what LuaSocket defines for itself below 5.2. The two
// trailing arguments are dropped rather than cast to void: one of them is a function
// name, and `(void)fn` is C4551.
#define lua_pcallk(L, na, nr, err, ctx, cont) lua_pcall(L, na, nr, err)
typedef int lua_KContext;

// Defined as a macro, not a function, on purpose: pb.cpp wraps its own 5.1-era shims in
// `#ifndef luaL_newlib`. A macro keeps that block closed, which is what stops it from
// redefining lua_tointegerx/lua_tonumberx - both of which Luau declares - with a
// different return type.
#define luaL_newlib(L, l) (lua_createtable(L, 0, 0), luaL_setfuncs(L, l, 0))

inline void* luaL_testudata(lua_State* L, int Idx, const char* TName)
{
    void* P = lua_touserdata(L, Idx);
    if (P != nullptr && lua_getmetatable(L, Idx))
    {
        luaL_getmetatable(L, TName);
        if (!lua_rawequal(L, -1, -2))
            P = nullptr;
        lua_pop(L, 2);
        return P;
    }
    return nullptr;
}

// Luau has no luaL_fileresult, and pb.cpp only defines its own under #ifdef LUAI_BITSINT
// - a luaconf macro neither backend has.
inline int luaL_fileresult(lua_State* L, int Stat, const char* FileName)
{
    const int Err = errno;
    if (Stat)
    {
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushnil(L);
#ifdef _MSC_VER
    char Message[128];
    strerror_s(Message, Err);
    const char* const Description = Message;
#else
    const char* const Description = strerror(Err);
#endif
    if (FileName != nullptr)
        lua_pushfstring(L, "%s: %s", FileName, Description);
    else
        lua_pushstring(L, Description);
    lua_pushinteger(L, Err);
    return 3;
}

// Luau declares luaL_argerror as a macro over luaL_argerrorL, which is l_noret: a call
// site written `return luaL_argerror(...)` - and there are a dozen of them here - does
// not compile. Plain Lua returns int from the same call, so restore that.
#undef luaL_argerror
// Luau's luaL_argcheck is built on that same assumption: it is
// `cond ? (void)0 : luaL_argerror(...)`, which only compiles while luaL_argerror is
// void. Restore it with the call cast back to void.
#undef luaL_argcheck
#define luaL_argcheck(L, cond, arg, extramsg) ((void)((cond) ? (void)0 : (void)luaL_argerror(L, (arg), (extramsg))))

inline int luaL_argerror(lua_State* L, int NArg, const char* ExtraMsg)
{
    lua_Debug Ar;
    memset(&Ar, 0, sizeof(Ar));
    if (lua_getinfo(L, 0, "n", &Ar) != 0 && Ar.name != nullptr)
        return luaL_error(L, "bad argument #%d to '%s' (%s)", NArg, Ar.name, ExtraMsg);
    return luaL_error(L, "bad argument #%d (%s)", NArg, ExtraMsg);
}

// Same story for lua_error: Luau declares it l_noret, and every vendored module writes
// `return lua_error(L)`. Gated on L for the same reason as LuaBackend.h's luaL_error -
// `lua_error(L); return 0;` is dead code (C4702), which BuildSettingsVersion.V7 turns
// into an error. L is never null at a call site, so the longjmp still happens.
inline int UnLuaNovaExtensionRaiseError(lua_State* L)
{
    if (L != nullptr)
        lua_error(L);
    return 0;
}
#define lua_error(L) UnLuaNovaExtensionRaiseError(L)

#endif // UNLUANOVA_BACKEND_LUAU
