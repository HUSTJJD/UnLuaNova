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
        // GC is non-zero on Luau when the interrupt came from the collector. Listeners
        // that can raise a Lua error must skip those (a longjmp out of the GC is not
        // safe); the profiler needs the flag to attribute collector time correctly.
        // It is always 0 on plain Lua, where lua_sethook has no such channel.
        using FListener = TFunction<void(lua_State* L, lua_Debug* Ar, int GC)>;

        /**
         * Called when a (co)routine is created or destroyed.
         * Parent is nullptr on destruction. Luau only - see UNLUANOVA_HAS_THREAD_CALLBACK.
         */
        using FThreadListener = TFunction<void(lua_State* Parent, lua_State* Thread)>;

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
            Slot->ThreadListeners.Empty();
            Refresh(L, *Slot);
            RefreshThreads(L, *Slot);
        }

#if UNLUANOVA_HAS_THREAD_CALLBACK
        /** Register for thread creation/destruction. No-op on plain Lua. */
        void AddThreadHook(lua_State* L, const void* Owner, FThreadListener Fn)
        {
            FSlot& Slot = Slots.FindOrAdd(L);
            Slot.ThreadListeners.Add({Owner, MoveTemp(Fn)});
            RefreshThreads(L, Slot);
        }

        void RemoveThreadOwner(lua_State* L, const void* Owner)
        {
            FSlot* Slot = Slots.Find(L);
            if (!Slot)
                return;

            Slot->ThreadListeners.RemoveAll([Owner](const FThreadEntry& E) { return E.Owner == Owner; });
            RefreshThreads(L, *Slot);
        }
#else
        void AddThreadHook(lua_State* L, const void* Owner, FThreadListener Fn) {}
        void RemoveThreadOwner(lua_State* L, const void* Owner) {}
#endif

    private:
        struct FEntry
        {
            int Mask;
            const void* Owner;
            FListener Fn;
        };

        struct FThreadEntry
        {
            const void* Owner;
            FThreadListener Fn;
        };

        struct FSlot
        {
            TArray<FEntry> Listeners;
            TArray<FThreadEntry> ThreadListeners;
        };

        static void Dispatch(lua_State* L, lua_Debug* Ar, int GC)
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
                E.Fn(L, Ar, GC);
            }
        }

        /** lua_sethook's callback has no GC flag; plain Lua always reports 0. */
        static void OnHook(lua_State* L, lua_Debug* Ar)
        {
            Dispatch(L, Ar, 0);
        }

        void Refresh(lua_State* L, FSlot& Slot)
        {
            const bool bAny = Slot.Listeners.Num() > 0;

#if UNLUANOVA_HAS_HOOK_EVENT
            int Mask = 0;
            for (const FEntry& E : Slot.Listeners)
                Mask |= E.Mask;

            // LUA_MASKCOUNT never fires with a zero count. A listener that asks for it
            // (the execution budget) needs count=1 to see every instruction; Dispatch
            // filters by event bit, so the extra calls do not become extra samples for
            // the line/call listeners sharing the slot.
            const int Count = (Mask & LUA_MASKCOUNT) != 0 ? 1 : 0;
            lua_sethook(L, bAny ? &OnHook : nullptr, Mask, Count);
#else
            lua_callbacks(L)->interrupt = bAny ? &OnInterrupt : nullptr;
#endif
        }

#if !UNLUANOVA_HAS_HOOK_EVENT
        static void OnInterrupt(lua_State* L, int GC)
        {
            // GC is passed through rather than filtered here: the profiler uses it to
            // separate collector time from script time. Listeners that may raise a Lua
            // error (watchdog, execution budget) skip GC events themselves - a longjmp
            // out of the collector is not safe.
            Dispatch(L, nullptr, GC);
        }
#endif

#if UNLUANOVA_HAS_THREAD_CALLBACK
        void RefreshThreads(lua_State* L, FSlot& Slot)
        {
            UNLUANOVA_SET_THREAD_CALLBACK(L, Slot.ThreadListeners.Num() > 0 ? &OnUserThread : nullptr);
        }

        static void OnUserThread(lua_State* Parent, lua_State* Thread)
        {
            FSlot* Slot = Get().Slots.Find(Parent ? Parent : Thread);
            if (!Slot)
                return;

            for (const FThreadEntry& E : Slot->ThreadListeners)
                E.Fn(Parent, Thread);
        }
#else
        void RefreshThreads(lua_State* L, FSlot& Slot) {}
#endif

        TMap<lua_State*, FSlot> Slots;
    };
}
