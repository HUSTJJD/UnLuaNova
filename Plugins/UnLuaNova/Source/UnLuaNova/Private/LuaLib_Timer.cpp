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

#include "LuaLib_Timer.h"

#include "LuaBackend.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaTimerSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

namespace UnLuaNova
{
    namespace LuaTimerLib
    {
        /**
         * 取计时器所在的子系统。
         *
         * 脚本侧写 `UnLuaNova.Timer.Set(fn, 1)` 时不该被要求传 World，所以这里替它挑：
         * 先 PIE / Game，再退回任意有效世界。多 PIE 同时跑时取的是第一个 —— 这是当前
         * 唯一的已知限制，需要按世界区分时把 World 作为参数传进来（TODO，等有真实需求再加）。
         */
        static UUnLuaNovaTimerSubsystem* GetSubsystem()
        {
            if (!GEngine)
                return nullptr;

            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
                {
                    World = Context.World();
                    break;
                }
            }

            if (!World)
            {
                for (const FWorldContext& Context : GEngine->GetWorldContexts())
                {
                    if (Context.World())
                    {
                        World = Context.World();
                        break;
                    }
                }
            }

            return World ? World->GetSubsystem<UUnLuaNovaTimerSubsystem>() : nullptr;
        }

        static int Set(lua_State* L)
        {
            if (lua_type(L, 1) != LUA_TFUNCTION)
                return luaL_error(L, "UnLuaNova.Timer.Set: 第 1 个参数必须是函数");

            const double Interval = lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 0.0;
            const bool bLoop = lua_toboolean(L, 3) != 0;
            const double Delay = lua_isnumber(L, 4) ? lua_tonumber(L, 4) : 0.0;
            const bool bUnscaled = lua_toboolean(L, 5) != 0;

            auto* Subsystem = GetSubsystem();
            if (!Subsystem)
                return luaL_error(L, "UnLuaNova.Timer.Set: 拿不到世界（引擎还没起来？）");

            // 回调交给注册表持有：脚本侧不用自己保活，移除时由子系统 unref
            lua_pushvalue(L, 1);
            const int32 CallbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

            const int32 Handle = Subsystem->Add(L, CallbackRef, static_cast<float>(Interval), bLoop,
                                                static_cast<float>(Delay), bUnscaled);
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
            const int32 Handle = static_cast<int32>(UNLUANOVA_TOINT64(L, 1));
            auto* Subsystem = GetSubsystem();
            const bool bRemoved = Subsystem && Subsystem->Remove(Handle);
            lua_pushboolean(L, bRemoved ? 1 : 0);
            return 1;
        }

        static int Pause(lua_State* L)
        {
            const int32 Handle = static_cast<int32>(UNLUANOVA_TOINT64(L, 1));
            const bool bPaused = lua_toboolean(L, 2) != 0;
            auto* Subsystem = GetSubsystem();
            const bool bOk = Subsystem && Subsystem->SetPaused(Handle, bPaused);
            lua_pushboolean(L, bOk ? 1 : 0);
            return 1;
        }

        static int IsValid(lua_State* L)
        {
            const int32 Handle = static_cast<int32>(UNLUANOVA_TOINT64(L, 1));
            auto* Subsystem = GetSubsystem();
            lua_pushboolean(L, (Subsystem && Subsystem->IsValid(Handle)) ? 1 : 0);
            return 1;
        }

        static int ClearAll(lua_State* L)
        {
            if (auto* Subsystem = GetSubsystem())
                Subsystem->RemoveAll();
            return 0;
        }

        /** 当前世界时间：暂停与时间膨胀算不算进去由参数决定。写脚本时拿它对表很有用。 */
        static int Time(lua_State* L)
        {
            const bool bUnscaled = lua_toboolean(L, 1) != 0;
            auto* Subsystem = GetSubsystem();
            lua_pushnumber(L, Subsystem ? Subsystem->Now(bUnscaled) : 0.0);
            return 1;
        }

        static int Count(lua_State* L)
        {
            auto* Subsystem = GetSubsystem();
            UNLUANOVA_PUSHINT(L, Subsystem ? Subsystem->Num() : 0);
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
