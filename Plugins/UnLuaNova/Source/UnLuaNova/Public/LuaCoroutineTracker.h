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

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"

struct lua_State;

namespace UnLuaNova
{
    /**
     * Live-coroutine census, for finding coroutines that never finished.
     *
     * A suspended coroutine is a stack, a closure and everything it captured, held for as
     * long as anything references it - and the thing that references it is usually the
     * Latent action or timer callback that was supposed to resume it. When that resume never
     * comes, nothing is "wrong" in any sense the VM reports: no error, no growth the GC can
     * reclaim, just a slowly rising count of stacks nobody will ever unwind. Lua has no
     * thread dump, so there was no way to see it.
     *
     * How it works: `coroutine.create` and `coroutine.wrap` are replaced by closures that
     * call the original and drop the new thread into a weak-*keyed* table. Weak keys mean a
     * coroutine nobody references disappears on its own, so what is left at scan time is
     * what is genuinely being held - which is the interesting set.
     *
     * What it can and cannot tell you: it says how many coroutines are alive, where each was
     * created and where it is now. It cannot tell a deliberately parked coroutine from a
     * leaked one - that judgment is yours, and the creation site is what makes it possible.
     *
     * Off by default: wrapping the constructors costs a table write per coroutine. Turn it on
     * with `lua.coroutines on` when chasing a leak.
     */
    class FLuaCoroutineTracker
    {
    public:
        static FLuaCoroutineTracker& Get();

        /** Remember the main state so the runtime toggle can install/uninstall. */
        void Attach(lua_State* L);
        void Detach(lua_State* L);

        void SetEnabled(bool bInEnabled);
        bool IsEnabled() const { return bEnabled; }

        struct FLiveCoroutine
        {
            /** Where coroutine.create/wrap was called. */
            FString CreatedAt;
            /** Where the coroutine currently sits - the yield, for a suspended one. */
            FString SuspendedAt;
            /** suspended / running / dead. */
            FString Status;
        };

        /**
         * Snapshot of every coroutine still alive.
         *
         * Collects first, so a coroutine that is merely unreferenced-but-not-yet-collected
         * does not show up as a leak. That is the one thing that must happen inside this
         * call: a full collect is expensive, and doing it here is what makes the answer
         * mean something.
         */
        TArray<FLiveCoroutine> Snapshot();

        /** Total created since tracking began, and how many of those have finished. */
        uint64 NumCreated() const { return Created; }

        /**
         * Record a thread that was just created. Called by the wrapped constructors, and
         * public only so they can reach it from their static context.
         */
        void RegisterThread(lua_State* L, int32 Index);

        /** The two constructors, wrapped. One entry point each so the swap is symmetric. */
        static int Create(lua_State* L);
        static int Wrap(lua_State* L);

    private:
        FLuaCoroutineTracker() = default;

        /** Replace coroutine.create / coroutine.wrap. */
        void ApplyPatch();

        lua_State* Attached = nullptr;
        bool bEnabled = false;
        bool bInstalled = false;

        /** Ref of the weak-keyed table: [thread] = creation site. */
        int LiveRef = -1;

        uint64 Created = 0;
    };
}
