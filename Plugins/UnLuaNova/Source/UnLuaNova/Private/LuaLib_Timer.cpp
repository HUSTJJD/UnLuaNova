// Copyright (c) 2026 UnLuaNova contributors
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

#include "LuaLib_Timer.h"

#include "LuaBackend.h"
#include "LuaEnv.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaTimerSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

namespace UnLuaNova
{
    namespace LuaTimerLib
    {
        static UUnLuaNovaTimerSubsystem* GetSubsystem(lua_State* L, int32 ContextIndex)
        {
            UWorld* World = nullptr;
            if (!lua_isnoneornil(L, ContextIndex))
            {
                UObject* Context = GetUObject(L, ContextIndex);
                World = Context ? Context->GetWorld() : nullptr;
                if (!World || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE))
                {
                    luaL_error(L, "UnLuaNova.Timer: expected a live Game/PIE world context");
                    return nullptr;
                }
            }
            else if (GEngine)
            {
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                {
                    if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.World())
                    {
                        if (World && World != Context.World())
                        {
                            luaL_error(L, "UnLuaNova.Timer: multiple worlds; pass an explicit world context");
                            return nullptr;
                        }
                        World = Context.World();
                    }
                }
            }

            return World ? World->GetSubsystem<UUnLuaNovaTimerSubsystem>() : nullptr;
        }

        static int32 GetHandle(lua_State* L)
        {
            if (lua_type(L, 1) != LUA_TNUMBER)
                return INDEX_NONE;
            const double Value = lua_tonumber(L, 1);
            return FMath::IsFinite(Value) && Value > 0 && Value <= MAX_int32 && FMath::FloorToDouble(Value) == Value
                ? static_cast<int32>(Value) : INDEX_NONE;
        }

        /**
         * Capture the Lua frame that called Timer.Set (level 1) at REGISTRATION time.
         * Doing it when a callback fails is too late: the stack then belongs to the
         * callback, not to whoever scheduled it. This is diagnostics, not a control path -
         * a missing frame degrades to "?" and never fails the registration.
         */
        static FUnLuaNovaTimerRegisterSite CaptureSite(lua_State* L)
        {
            FUnLuaNovaTimerRegisterSite Site;
            Site.EnvLabel = DescribeEnv(L);

            lua_Debug Ar = {};
            bool bHasFrame = false;
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            bHasFrame = UNLUANOVA_GETINFO(L, 1, "sl", &Ar) != 0;
#else
            // plain Lua selects the frame with lua_getstack first; Luau folds the level
            // into lua_getinfo. Same shape as LuaProfiler.cpp's GetStackFrame().
            bHasFrame = lua_getstack(L, 1, &Ar) != 0;
            if (bHasFrame)
                UNLUANOVA_GETINFO(L, 1, "sl", &Ar);
#endif
            if (bHasFrame)
            {
                Site.Source = Ar.source ? UTF8_TO_TCHAR(Ar.source) : TEXT("?");
                Site.Line = Ar.currentline;
            }
            return Site;
        }

        static int Set(lua_State* L)
        {
            if (lua_type(L, 1) != LUA_TFUNCTION)
                return luaL_error(L, "UnLuaNova.Timer.Set: 第 1 个参数必须是函数");

            if ((!lua_isnoneornil(L, 2) && lua_type(L, 2) != LUA_TNUMBER)
                || (!lua_isnoneornil(L, 4) && lua_type(L, 4) != LUA_TNUMBER))
                return luaL_error(L, "UnLuaNova.Timer.Set: interval and delay must be numbers");
            const double Interval = lua_isnoneornil(L, 2) ? 0.0 : lua_tonumber(L, 2);
            const bool bLoop = lua_toboolean(L, 3) != 0;
            const double Delay = lua_isnoneornil(L, 4) ? 0.0 : lua_tonumber(L, 4);
            const bool bUnscaled = lua_toboolean(L, 5) != 0;

            if (!FMath::IsFinite(Interval) || !FMath::IsFinite(Delay)
                || Interval < 0 || Delay < 0 || Interval > MAX_flt || Delay > MAX_flt)
                return luaL_error(L, "UnLuaNova.Timer.Set: interval and delay must be finite nonnegative floats");

            auto* Subsystem = GetSubsystem(L, 6);
            if (!Subsystem)
                return luaL_error(L, "UnLuaNova.Timer.Set: 拿不到世界（引擎还没起来？）");

            // Take the registration site before luaL_ref changes the stack. Level 1 is
            // the script frame that called Timer.Set.
            const FUnLuaNovaTimerRegisterSite Site = CaptureSite(L);

            // 回调交给注册表持有：脚本侧不用自己保活，移除时由子系统 unref
            lua_pushvalue(L, 1);
            const int32 CallbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

            const int32 Handle = Subsystem->Add(L, CallbackRef, static_cast<float>(Interval), bLoop,
                                                static_cast<float>(Delay), bUnscaled, Site);
            if (Handle == INDEX_NONE)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, CallbackRef);
                return luaL_error(L, "UnLuaNova.Timer.Set: 注册失败");
            }

            // 句柄是小整数，但 pushed 整数在 Luau 上是 double，走 house macro 保持一致
            UNLUANOVA_PUSHINT(L, Handle);
            return 1;
        }

        static int Clear(lua_State* L)
        {
            const int32 Handle = GetHandle(L);
            auto* Subsystem = UUnLuaNovaTimerSubsystem::Find(L, Handle);
            const bool bRemoved = Subsystem && Subsystem->Remove(Handle);
            lua_pushboolean(L, bRemoved ? 1 : 0);
            return 1;
        }

        static int Pause(lua_State* L)
        {
            const int32 Handle = GetHandle(L);
            const bool bPaused = lua_toboolean(L, 2) != 0;
            auto* Subsystem = UUnLuaNovaTimerSubsystem::Find(L, Handle);
            const bool bOk = Subsystem && Subsystem->SetPaused(Handle, bPaused);
            lua_pushboolean(L, bOk ? 1 : 0);
            return 1;
        }

        static int IsValid(lua_State* L)
        {
            const int32 Handle = GetHandle(L);
            auto* Subsystem = UUnLuaNovaTimerSubsystem::Find(L, Handle);
            lua_pushboolean(L, (Subsystem && Subsystem->IsValid(Handle)) ? 1 : 0);
            return 1;
        }

        static int ClearAll(lua_State* L)
        {
            if (auto* Subsystem = GetSubsystem(L, 1))
                Subsystem->RemoveForState(FLuaEnv::FindEnvChecked(L).GetMainState());
            return 0;
        }

        /** 当前世界时间：暂停与时间膨胀算不算进去由参数决定。写脚本时拿它对表很有用。 */
        static int Time(lua_State* L)
        {
            const bool bUnscaled = lua_toboolean(L, 1) != 0;
            auto* Subsystem = GetSubsystem(L, 2);
            lua_pushnumber(L, Subsystem ? Subsystem->Now(bUnscaled) : 0.0);
            return 1;
        }

        static int Count(lua_State* L)
        {
            auto* Subsystem = GetSubsystem(L, 1);
            UNLUANOVA_PUSHINT(L, Subsystem ? Subsystem->NumForState(FLuaEnv::FindEnvChecked(L).GetMainState()) : 0);
            return 1;
        }

        /** One-line registration site for a handle, or nil for a handle that is not ours. */
        static int Describe(lua_State* L)
        {
            const int32 Handle = GetHandle(L);
            auto* Subsystem = UUnLuaNovaTimerSubsystem::Find(L, Handle);
            if (!Subsystem)
            {
                lua_pushnil(L);
                return 1;
            }

            const FString Text = Subsystem->DescribeEntry(Handle);
            if (Text.IsEmpty())
            {
                lua_pushnil(L);
                return 1;
            }
            lua_pushstring(L, TCHAR_TO_UTF8(*Text));
            return 1;
        }

        /**
         * Table keyed by handle for the CALLING env only. Cross-Env handles are left out
         * on purpose: Find() already refuses them for Clear/Pause, so exposing them here
         * would give a script a view of timers it cannot control.
         */
        static int Diagnostics(lua_State* L)
        {
            lua_newtable(L);                                  // [out]
            auto* Subsystem = GetSubsystem(L, 1);
            const auto Env = FLuaEnv::FindEnv(L);
            if (!Subsystem || !Env)
                return 1;

            for (const FUnLuaNovaTimerDiagnostic& D : Subsystem->GetDiagnostics(Env->GetMainState()))
            {
                UNLUANOVA_PUSHINT(L, D.Handle);               // [out, key]
                lua_newtable(L);                              // [out, key, value]

                lua_pushliteral(L, "handle");
                UNLUANOVA_PUSHINT(L, D.Handle);
                lua_rawset(L, -3);

                lua_pushliteral(L, "source");
                lua_pushstring(L, TCHAR_TO_UTF8(*D.Source));
                lua_rawset(L, -3);

                lua_pushliteral(L, "line");
                UNLUANOVA_PUSHINT(L, D.Line);
                lua_rawset(L, -3);

                lua_pushliteral(L, "env");
                lua_pushstring(L, TCHAR_TO_UTF8(*D.EnvLabel));
                lua_rawset(L, -3);

                lua_pushliteral(L, "loop");
                lua_pushboolean(L, D.bLoop ? 1 : 0);
                lua_rawset(L, -3);

                lua_pushliteral(L, "unscaled");
                lua_pushboolean(L, D.bUnscaled ? 1 : 0);
                lua_rawset(L, -3);

                lua_pushliteral(L, "paused");
                lua_pushboolean(L, D.bPaused ? 1 : 0);
                lua_rawset(L, -3);

                lua_pushliteral(L, "remaining");
                lua_pushnumber(L, D.RemainingSeconds);
                lua_rawset(L, -3);

                lua_rawset(L, -3);                            // out[key] = value
            }
            return 1;
        }

        static const luaL_Reg Functions[] = {
            {"Set", Set},
            {"Clear", Clear},
            {"Pause", Pause},
            {"IsValid", IsValid},
            {"ClearAll", ClearAll},
            {"Time", Time},
            {"Count", Count},
            {"Describe", Describe},
            {"Diagnostics", Diagnostics},
            {nullptr, nullptr}
        };

        int Open(lua_State* L)
        {
            lua_newtable(L);
            luaL_setfuncs(L, Functions, 0);
            return 1;
        }
    }
}
