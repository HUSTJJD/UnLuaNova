// Tencent is pleased to support the open source community by making UnLuaNova available.
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

#include "LuaBackend.h"

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU

#include <string>
#include <string.h>

// Luau's debug library is a deliberate subset - traceback, info, the profiler hooks -
// because everything else in it is a way to reach past the sandbox, and load() is left
// out for the same reason: a VM that cannot compile source at runtime cannot be made to
// run source that came from somewhere unexpected.
//
// That is a fine default for a game VM and a problem for UnLuaNova, whose hot reload
// works by taking a freshly loaded closure and wiring the old closure's state into it,
// and whose scripts are written against plain Lua. So add both back, out of the C API
// Luau does expose, and let a script ask the same question on either backend. Where Luau
// has no answer at all (a registry table, uservalues) these return nothing rather than
// raising: a caller that gets nil stops, while a caller that gets an error takes the
// process with it, because none of this runs under a protected call.

namespace
{
    // debug.getupvalue(f, n) -> name, value
    int DebugGetUpValue(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        const int N = (int)luaL_checkinteger(L, 2);

        // Pushes the value and hands back its name; NULL when n is past the last one.
        const char* Name = lua_getupvalue(L, 1, N);
        if (Name == nullptr)
            return 0;

        lua_pushstring(L, Name);
        lua_insert(L, -2); // name, value
        return 2;
    }

    // debug.setupvalue(f, n, v) -> name
    int DebugSetUpValue(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        const int N = (int)luaL_checkinteger(L, 2);
        lua_settop(L, 3); // f, n, v

        lua_pushvalue(L, 3); // lua_setupvalue pops the value it takes
        const char* Name = lua_setupvalue(L, 1, N);
        if (Name == nullptr)
        {
            // lapi.cpp only pops when it found the upvalue, so take the copy back.
            lua_pop(L, 1);

            if (N == 1 && lua_istable(L, 3))
            {
                // A Luau main chunk has no upvalue 1 - it has none at all, see
                // luaF_newLclosure(L, 0, envt, main) in lvmload.cpp. On plain Lua this
                // same call is how a script writes _ENV, and setfenv is what that means
                // here, so answer with the environment instead of with nothing.
                lua_pushvalue(L, 3);
                lua_setfenv(L, 1);
                Name = "_ENV";
            }
        }

        if (Name == nullptr)
            return 0;

        lua_pushstring(L, Name);
        return 1;
    }

    // debug.upvalueid(f, n) -> id
    int DebugUpValueId(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        const int N = (int)luaL_checkinteger(L, 2);

        if (lua_getupvalue(L, 1, N) == nullptr)
            return 0;

        // Luau has no lua_upvalueid, and an UpVal address is not reachable from the public
        // API, so the id has to be an approximation.
        //
        // For a reference - a table, a closure, userdata - the pointer of the value itself
        // is the answer plain Lua would give: two closures sharing an upvalue share the
        // value, and that is exactly the case hot reload cares about. For a scalar there is
        // no shared object to point at, so fall back to the slot: the same closure and
        // index always produce the same id, which is stable but not shared.
        //
        // Returning a string in both cases keeps it non-nil, because callers use it as a
        // table key and a nil key would be the error, not the degradation.
        const void* ValuePointer = lua_topointer(L, -1);
        lua_pop(L, 1);

        if (ValuePointer != nullptr)
            lua_pushfstring(L, "%p", ValuePointer);
        else
            lua_pushfstring(L, "%p#%d", lua_topointer(L, 1), N);

        return 1;
    }

    // debug.getlocal([co,] level, n) -> name, value
    int DebugGetLocal(lua_State* L)
    {
        int LevelIdx = 1;
        if (lua_isthread(L, 1))
        {
            // Luau's lua_getlocal only sees the stack of the running thread. Answering a
            // request about some other coroutine with the wrong stack would be worse than
            // answering nothing, so say nothing.
            if (lua_tothread(L, 1) != L)
                return 0;
            LevelIdx = 2;
        }

        const int Level = (int)luaL_checkinteger(L, LevelIdx);
        const int N = (int)luaL_checkinteger(L, LevelIdx + 1);

        const char* Name = lua_getlocal(L, Level, N); // pushes the value
        if (Name == nullptr)
            return 0;

        lua_pushstring(L, Name);
        lua_insert(L, -2);
        return 2;
    }

    // debug.setlocal([co,] level, n, v) -> name
    int DebugSetLocal(lua_State* L)
    {
        int LevelIdx = 1;
        if (lua_isthread(L, 1))
        {
            if (lua_tothread(L, 1) != L)
                return 0;
            LevelIdx = 2;
        }

        const int Level = (int)luaL_checkinteger(L, LevelIdx);
        const int N = (int)luaL_checkinteger(L, LevelIdx + 1);
        lua_settop(L, LevelIdx + 2); // ... , level, n, v

        const char* Name = lua_setlocal(L, Level, N); // pops the value
        if (Name == nullptr)
            return 0;

        lua_pushstring(L, Name);
        return 1;
    }

    // debug.getregistry() -> table
    int DebugGetRegistry(lua_State* L)
    {
        // Luau keeps its registry behind a pseudo-index and never exposes it as a table,
        // so there is nothing to hand over. An empty table is the honest answer: callers
        // walk it looking for stale references and simply find none.
        lua_newtable(L);
        return 1;
    }

    // debug.getuservalue(u) -> value | nil
    int DebugGetUserValue(lua_State* L)
    {
        // No uservalues on Luau: userdata carries a tag and a metatable, nothing else.
        return 0;
    }

    // debug.setuservalue(u, v) -> v
    int DebugSetUserValue(lua_State* L)
    {
        lua_settop(L, 2);
        return 1;
    }

    // load(chunk [, chunkname [, mode [, env]]]) -> chunk | nil, errmsg
    int LuaLoad(lua_State* L)
    {
        // A chunk may contain embedded zero bytes, so the source is collected as bytes
        // with an explicit length rather than as a C string.
        std::string Source;

        if (lua_isstring(L, 1))
        {
            Source.assign(lua_tostring(L, 1), lua_rawlen(L, 1));
        }
        else if (lua_isfunction(L, 1))
        {
            // Lua's reader form: call it until it returns nil or the empty string.
            for (;;)
            {
                lua_pushvalue(L, 1);
                if (lua_pcall(L, 0, 1, 0) != 0)
                    return luaL_error(L, "load: reader function raised an error");

                if (!lua_isstring(L, -1) || lua_rawlen(L, -1) == 0)
                {
                    lua_pop(L, 1);
                    break;
                }

                Source.append(lua_tostring(L, -1), lua_rawlen(L, -1));
                lua_pop(L, 1);
            }
        }
        else
        {
            return luaL_error(L, "load: string or function expected, got %s", luaL_typename(L, 1));
        }

        const char* ChunkName = lua_isstring(L, 2) ? lua_tostring(L, 2) : "=(load)";
        if (luaL_loadbufferx(L, Source.data(), Source.size(), ChunkName, nullptr) != LUA_OK)
        {
            // Exactly what plain Lua's load returns on a syntax error: nil plus the
            // message, with the message last.
            lua_pushnil(L);
            lua_insert(L, -2);
            return 2;
        }

        if (!lua_isnil(L, 4))
        {
            lua_pushvalue(L, 4);
            UnLuaNovaSetChunkEnv(L, -2);
        }

        return 1;
    }

    // pairs(t) -> generator, state, initial
    //
    // Luau's pairs() insists on a real table, so `pairs(TArray)` - the documented way to
    // walk a container - raises "table expected, got userdata" and every script that walks
    // a TArray/TMap/TSet dies. Luau's generic *for* has no such restriction: FORGPREP asks
    // the object's metatable for __iter, which is exactly what UNLUANOVA_ITER_ENTRY
    // registers. So keep the builtin for tables and route everything else through __iter,
    // which returns the same three values a Lua 5.x __pairs function would.
    int LuaPairs(lua_State* L)
    {
        luaL_checkany(L, 1);

        if (lua_istable(L, 1))
        {
            lua_pushvalue(L, lua_upvalueindex(1)); // Luau's own pairs
            lua_pushvalue(L, 1);
            lua_call(L, 1, 3);
            return 3;
        }

        if (lua_getmetatable(L, 1))
        {
            lua_getfield(L, -1, "__iter");
            lua_remove(L, -2); // the metatable
            if (lua_isfunction(L, -1))
            {
                lua_pushvalue(L, 1);
                lua_call(L, 1, 3);
                return 3;
            }
            lua_pop(L, 1);
        }

        return luaL_error(L, "invalid argument #1 to 'pairs' (table expected, got %s)", luaL_typename(L, 1));
    }

    // collectgarbage(option [, arg]) -> result
    //
    // Luau's base library stops at gcinfo(): there is no collectgarbage in it, so a script
    // that forces a collection - which is exactly what the GC-sensitive tests do, to prove
    // a delegate binding survives one - dies with "attempt to call a nil value". The same
    // three options the tests and the hot reload path use are enough here; anything else
    // is answered with plain Lua's error rather than with silence.
    int LuaCollectGarbage(lua_State* L)
    {
        const char* Option = luaL_optstring(L, 1, "collect");

        if (strcmp(Option, "collect") == 0)
        {
            lua_gc(L, LUA_GCCOLLECT, 0);
            lua_pushinteger(L, 0); // what plain Lua returns
            return 1;
        }

        if (strcmp(Option, "step") == 0)
        {
            const int Step = (int)luaL_optinteger(L, 2, 0);
            lua_pushboolean(L, lua_gc(L, LUA_GCSTEP, Step) != 0);
            return 1;
        }

        if (strcmp(Option, "count") == 0)
        {
            // Kbytes, like plain Lua: Luau reports totalbytes >> 10.
            lua_pushnumber(L, (double)lua_gc(L, LUA_GCCOUNT, 0));
            return 1;
        }

        return luaL_error(L, "collectgarbage: invalid option '%s'", Option);
    }

    // Installs a global only when the name is free, so a backend that already provides it
    // keeps its own.
    void AddMissingGlobal(lua_State* L, const char* Name, lua_CFunction Function)
    {
        lua_getglobal(L, Name);
        const bool bAlreadyThere = !lua_isnil(L, -1);
        lua_pop(L, 1);
        if (bAlreadyThere)
            return;

        lua_pushcfunction(L, Function, Name);
        lua_setglobal(L, Name);
    }
}

namespace UnLuaNova
{
    void SetupScriptCompat(lua_State* L)
    {
        lua_getglobal(L, LUA_DBLIBNAME);
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
        }
        else
        {
            static const struct
            {
                const char* Name;
                lua_CFunction Function;
            } Patch[] =
            {
                {"getupvalue", DebugGetUpValue},
                {"setupvalue", DebugSetUpValue},
                {"upvalueid", DebugUpValueId},
                {"getlocal", DebugGetLocal},
                {"setlocal", DebugSetLocal},
                {"getregistry", DebugGetRegistry},
                {"getuservalue", DebugGetUserValue},
                {"setuservalue", DebugSetUserValue},
            };

            for (const auto& Entry : Patch)
            {
                lua_getfield(L, -1, Entry.Name);
                const bool bAlreadyThere = !lua_isnil(L, -1);
                lua_pop(L, 1);
                if (bAlreadyThere)
                    continue;

                // The debug name comes along so a stack trace says "debug.getupvalue" and
                // not "?" - Luau's lua_pushcfunction wants it, unlike plain Lua's.
                lua_pushcfunction(L, Entry.Function, Entry.Name);
                lua_setfield(L, -2, Entry.Name);
            }

            lua_pop(L, 1);
        }

        AddMissingGlobal(L, "load", LuaLoad);

        // Same shape as load: absent on Luau, present on plain Lua, and only installed
        // where it is missing.
        AddMissingGlobal(L, "collectgarbage", LuaCollectGarbage);

        // pairs exists on Luau, so it is not "missing" - it is too narrow. Replace it,
        // keeping the builtin as an upvalue so tables still go through the fast native
        // path rather than through a C boundary.
        lua_getglobal(L, "pairs");
        if (lua_isfunction(L, -1))
        {
            lua_pushcclosure(L, LuaPairs, 1, "pairs");
            lua_setglobal(L, "pairs");
        }
        else
        {
            lua_pop(L, 1);
        }
    }
}

#endif // UNLUANOVA_BACKEND_LUAU
