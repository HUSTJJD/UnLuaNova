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

#pragma once

#include "LuaBackend.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Templates/Function.h"
#include "Templates/UnrealTemplate.h"

namespace UnLuaNova
{
    // A Lua VM exposes exactly one hook slot: lua_sethook() on plain Lua,
    // lua_Callbacks::interrupt on Luau. Two features that both need it - today the CPU
    // profiler and the dead-loop watchdog - would otherwise overwrite each other's
    // callback, and the loser silently stops working. That used to be real: the watchdog
    // checked lua_gethook() first and quietly did nothing whenever the profiler had
    // already claimed the slot.
    //
    // Both features register here instead; this installs the single native hook and fans
    // it out. Listeners are tagged with an opaque owner pointer so a feature can withdraw
    // itself without having to stash a handle.
    //
    // Ar is nullptr on the Luau backend: interrupts fire at safe points and carry no
    // lua_Debug. Listeners that need a frame must call UNLUANOVA_GETINFO themselves.
    class FLuaVmHooks
    {
    public:
        using FListener = TFunction<void(lua_State* L, lua_Debug* Ar)>;

        static FLuaVmHooks& Get()
        {
            static FLuaVmHooks Instance;
            return Instance;
        }

        // Mask is a LUA_MASK* combination and is only honoured on plain Lua. Pass 0 on
        // Luau, where every listener sees every safe point.
        void Add(lua_State* L, int Mask, const void* Owner, FListener Fn)
        {
            FSlot& Slot = Slots.FindOrAdd(L);
            Slot.Listeners.Add({Mask, Owner, MoveTemp(Fn)});
            Refresh(L, Slot);
        }

        void RemoveOwner(lua_State* L, const void* Owner)
        {
            FSlot* Slot = Slots.Find(L);
            if (!Slot)
                return;

            Slot->Listeners.RemoveAll([Owner](const FEntry& E) { return E.Owner == Owner; });
            Refresh(L, *Slot);
        }

        void RemoveAll(lua_State* L)
        {
            FSlot* Slot = Slots.Find(L);
            if (!Slot)
                return;

            Slot->Listeners.Empty();
            Refresh(L, *Slot);
        }

    private:
        struct FEntry
        {
            int Mask;
            const void* Owner;
            FListener Fn;
        };

        struct FSlot
        {
            TArray<FEntry> Listeners;
        };

        static void Dispatch(lua_State* L, lua_Debug* Ar)
        {
            FSlot* Slot = Get().Slots.Find(L);
            if (!Slot)
                return;

#if UNLUANOVA_HAS_HOOK_EVENT
            // LUA_MASK* is defined as 1 << LUA_HOOK*, so shift the event back into
            // mask space before testing it.
            const int Bit = Ar ? (1 << Ar->event) : 0;
#endif

            for (const FEntry& E : Slot->Listeners)
            {
#if UNLUANOVA_HAS_HOOK_EVENT
                if (Bit != 0 && (E.Mask & Bit) == 0)
                    continue;
#endif
                E.Fn(L, Ar);
            }
        }

        void Refresh(lua_State* L, FSlot& Slot)
        {
            const bool bAny = Slot.Listeners.Num() > 0;

#if UNLUANOVA_HAS_HOOK_EVENT
            int Mask = 0;
            for (const FEntry& E : Slot.Listeners)
                Mask |= E.Mask;

            lua_sethook(L, bAny ? &Dispatch : nullptr, Mask, 0);
#else
            lua_callbacks(L)->interrupt = bAny ? &OnInterrupt : nullptr;
#endif
        }

#if !UNLUANOVA_HAS_HOOK_EVENT
        static void OnInterrupt(lua_State* L, int GC)
        {
            (void)GC;
            Dispatch(L, nullptr);
        }
#endif

        TMap<lua_State*, FSlot> Slots;
    };
}
