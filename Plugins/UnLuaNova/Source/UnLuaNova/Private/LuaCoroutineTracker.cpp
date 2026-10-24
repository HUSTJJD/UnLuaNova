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

#include "LuaCoroutineTracker.h"

#include "LuaBackend.h"
#include "LuaCore.h"
#include "UnLuaNovaBase.h"

namespace UnLuaNova
{
    FLuaCoroutineTracker& FLuaCoroutineTracker::Get()
    {
        static FLuaCoroutineTracker Instance;
        return Instance;
    }

    void FLuaCoroutineTracker::Attach(lua_State* L)
    {
        if (Attached == L)
            return;
        if (Attached)
            Detach(Attached);
        Attached = L;
    }

    void FLuaCoroutineTracker::Detach(lua_State* L)
    {
        if (Attached != L)
            return;
        SetEnabled(false);
        Attached = nullptr;
    }

    /**
     * The wrapped `coroutine.wrap` cannot just forward to the original: the original returns
     * a function, and the thread it created - the thing worth tracking - is invisible from
     * outside. So it is rebuilt here out of the primitives: create the thread with the
     * original constructor (which registers it), then return a closure that resumes it.
     *
     * The closure has to reproduce two behaviours that make `wrap` what it is, because a
     * script cannot tell that it got a substitute:
     *   - success: only the resumed values come back, not the leading `true`;
     *   - failure: the error is re-raised at the call site, not returned as a value.
     */
    static int WrappedCoroutineCall(lua_State* L)
    {
        // upvalue 1: the coroutine, 2: the original coroutine.resume, 3: error
        lua_pushvalue(L, lua_upvalueindex(2));                  // resume
        lua_insert(L, 1);
        lua_pushvalue(L, lua_upvalueindex(1));                  // co
        lua_insert(L, 2);

        const int32 NumArgs = lua_gettop(L) - 2;
        lua_call(L, NumArgs + 1, LUA_MULTRET);

        if (lua_toboolean(L, 1))
        {
            // Drop the leading true: wrap returns the resumed values only.
            return lua_gettop(L) - 1;
        }

        // Re-raise. level 2 points at the caller of the wrapper, which is where a script
        // expects the error to appear to come from.
        lua_pushvalue(L, lua_upvalueindex(3));                  // error
        lua_pushvalue(L, 2);                                    // message
        lua_call(L, 1, 0);
        return 0;
    }

    namespace
    {
        /**
         * "file:line" of the Lua frame that called into a C function.
         *
         * Level 1 from inside a C closure is that closure's caller - the line that said
         * `coroutine.create(...)`. Forgetting where a coroutine came from is the whole
         * reason a leaked one is hard to find, so this is recorded at creation, not at scan
         * time: by then the frame is gone.
         */
        FString CallerSite(lua_State* L, int32 Level)
        {
            lua_Debug Ar;
            if (!UNLUANOVA_GETINFO(L, Level, "Sl", &Ar))
                return TEXT("?");

            const FString Source = Ar.source
                ? FPaths::GetBaseFilename(FString(ANSI_TO_TCHAR(Ar.source)))
                : TEXT("?");
            return FString::Printf(TEXT("%s:%d"), *Source, Ar.currentline);
        }

        FString Where(lua_State* Co)
        {
            if (!Co)
                return TEXT("?");

            lua_Debug Ar;
            if (!UNLUANOVA_GETINFO(Co, 0, "Sl", &Ar))
                return TEXT("?");

            const FString Source = Ar.source
                ? FPaths::GetBaseFilename(FString(ANSI_TO_TCHAR(Ar.source)))
                : TEXT("?");
            return FString::Printf(TEXT("%s:%d"), *Source, Ar.currentline);
        }
    }

    void FLuaCoroutineTracker::RegisterThread(lua_State* L, int32 Index)
    {
        if (!Attached || LiveRef < 0 || !bEnabled)
            return;
        if (!lua_isthread(L, Index))
            return;

        const int32 Co = lua_absindex(L, Index);
        ++Created;

        // [table][co][site] -> rawset. Weak keys on the table: once nothing else holds the
        // coroutine the entry goes away by itself, so scan time shows what is *held*, not
        // what merely existed.
        lua_rawgeti(L, LUA_REGISTRYINDEX, LiveRef);
        lua_pushvalue(L, Co);
        {
            // Called from Create/Wrap, so level 1 is the script line that created it.
            const FString Site = CallerSite(L, 1);
            const FTCHARToUTF8 Bytes(*Site);
            lua_pushlstring(L, Bytes.Get(), Bytes.Length());
        }
        lua_rawset(L, -3);
        lua_pop(L, 1);
    }

    int FLuaCoroutineTracker::Create(lua_State* L)
    {
        // upvalue 1: the original coroutine.create
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, 1);
        lua_call(L, lua_gettop(L) - 1, 1);

        FLuaCoroutineTracker::Get().RegisterThread(L, -1);
        return 1;
    }

    int FLuaCoroutineTracker::Wrap(lua_State* L)
    {
        // upvalue 1: original coroutine.create, 2: original coroutine.resume, 3: error
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, 1);
        lua_call(L, lua_gettop(L) - 1, 1);
        FLuaCoroutineTracker::Get().RegisterThread(L, -1);

        lua_pushvalue(L, -1);                       // co
        lua_pushvalue(L, lua_upvalueindex(2));      // resume
        lua_pushvalue(L, lua_upvalueindex(3));      // error
        lua_pushcclosure(L, WrappedCoroutineCall, 3);
        return 1;
    }

    void FLuaCoroutineTracker::ApplyPatch()
    {
        lua_State* L = Attached;
        if (!L)
            return;

        const int32 Top = lua_gettop(L);

        lua_getglobal(L, "coroutine");
        if (!lua_istable(L, -1))
        {
            lua_settop(L, Top);
            return;
        }

        // The live set: weak keys, so a coroutine nobody references disappears by itself.
        if (LiveRef < 0)
        {
            lua_newtable(L);
            lua_newtable(L);
            lua_pushstring(L, "k");
            lua_setfield(L, -2, "__mode");
            lua_setmetatable(L, -2);
            LiveRef = luaL_ref(L, LUA_REGISTRYINDEX);   // pops the table
        }

        // Grab the originals as registry refs rather than juggling stack positions: three
        // values with three different lifetimes, and an off-by-one here silently installs a
        // closure over the wrong one.
        lua_getfield(L, -1, "create");
        const int32 CreateRef = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_getfield(L, -1, "resume");
        const int32 ResumeRef = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_getfield(L, -1, "error");
        const int32 ErrorRef = luaL_ref(L, LUA_REGISTRYINDEX);

        lua_rawgeti(L, LUA_REGISTRYINDEX, CreateRef);
        lua_pushcclosure(L, Create, 1);
        lua_setfield(L, -2, "create");

        lua_rawgeti(L, LUA_REGISTRYINDEX, CreateRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ResumeRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ErrorRef);
        lua_pushcclosure(L, Wrap, 3);
        lua_setfield(L, -2, "wrap");

        lua_settop(L, Top);
        bInstalled = true;
    }

    void FLuaCoroutineTracker::SetEnabled(bool bInEnabled)
    {
        if (bEnabled == bInEnabled)
            return;

        bEnabled = bInEnabled;

        if (bEnabled && !bInstalled)
            ApplyPatch();

        // Turning it off stops recording; it does not put the original constructors back.
        // They survive only inside the upvalues of the closures installed above, so there is
        // nothing to restore them from. A tracker that claims to uninstall but leaves the
        // wrappers in place would be lying about the state, so it says so instead - and the
        // cost of leaving them is one table write per coroutine.
    }

    TArray<FLuaCoroutineTracker::FLiveCoroutine> FLuaCoroutineTracker::Snapshot()
    {
        TArray<FLiveCoroutine> Out;
        if (!Attached || LiveRef < 0)
            return Out;

        lua_State* L = Attached;
        const int32 Top = lua_gettop(L);

        // Collect first. A coroutine that nothing references but that has not been collected
        // yet would otherwise be reported as live, and "it shows up in the list" is exactly
        // the signal a leak hunt depends on - it has to mean "something still holds this".
        lua_gc(L, LUA_GCCOLLECT, 0);

        lua_rawgeti(L, LUA_REGISTRYINDEX, LiveRef);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            lua_State* Co = lua_tothread(L, -2);
            const FString CreatedAt = lua_isstring(L, -1)
                ? FString(UTF8_TO_TCHAR(lua_tostring(L, -1)))
                : TEXT("?");

            if (Co)
            {
                FLiveCoroutine Live;
                Live.CreatedAt = CreatedAt;
                Live.SuspendedAt = Where(Co);
                Live.Status = (lua_status(Co) != LUA_OK) ? TEXT("suspended") : TEXT("dead");
                Out.Add(MoveTemp(Live));
            }

            lua_pop(L, 1);   // value; keep the key for lua_next
        }

        lua_settop(L, Top);
        return Out;
    }
}
