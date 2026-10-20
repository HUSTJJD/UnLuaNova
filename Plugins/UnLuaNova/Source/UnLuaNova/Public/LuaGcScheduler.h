// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

#pragma once

#include "CoreMinimal.h"

struct lua_State;

namespace UnLuaNova
{
    /**
     * How the Lua heap is collected.
     *
     * Automatic   - the VM paces itself, which is what UNLUANOVA_SETUP_GC sets up: Lua 5.5
     *               runs generational (LUA_GCGEN), Luau runs incremental with a step size
     *               and goal. **This is the default and the recommended mode.** Nothing
     *               here runs per frame.
     * TimeSliced  - the VM's own pacing is stopped (LUA_GCSTOP) and the collector is driven
     *               from Tick(), one bounded step per frame.
     *
     * Be clear about what TimeSliced is for, because the name oversells it. There is no
     * recurring GC spike in UnLuaNova to fix: nothing calls FLuaEnv::GC() automatically -
     * the only caller is the `lua.gc` console command - and both backends already pace
     * themselves. Switching to TimeSliced therefore *replaces* a tuned generational /
     * incremental collector with a hand-driven stepper, which on Lua 5.5 means giving up
     * the generational mode UNLUANOVA_SETUP_GC deliberately turned on.
     *
     * It earns its keep in exactly one situation: a project that calls GC() at a known
     * safe point - level transition, loading screen - and wants that call to be spread
     * over frames instead of being one synchronous hitch. Measure before reaching for it.
     */
    enum class ELuaGcMode : uint8
    {
        Automatic,
        TimeSliced,
    };

    /**
     * Per-frame, budget-driven Lua garbage collection.
     *
     * Why this exists: FLuaEnv::GC() is two synchronous LUA_GCCOLLECT passes on the game
     * thread. Whoever calls it buys a frame spike proportional to the live heap, and the
     * only way to avoid the spike is to stop doing the work in one go.
     *
     * Why it is not "multithread GC": a Lua heap is a single lua_State, and deciding what
     * is reachable means walking userdata that owns (and is owned by) UObjects - all of
     * which must happen on the game thread. Moving the sweep to a worker thread would mean
     * forking the VM's collector and taking a lock on every allocation. Slicing the same
     * work across frames gets the same latency win at a fraction of the risk, so that is
     * what this does.
     *
     * Why Automatic stays the default: see ELuaGcMode. Both backends already collect
     * incrementally on their own; this class exists to make the *explicit* collect
     * sliceable, not because the implicit one is broken.
     *
     * The accounting: debt is how much the heap has grown since the last *completed* cycle
     * (scaled by StepMultiplier, exactly like Luau's gcstepmul), and each frame spends as
     * much of that debt as the per-frame budget allows. Measuring against the last cycle
     * rather than the previous frame is the whole trick - Lua frees nothing until a cycle
     * runs, so a script that builds 4 MB and drops it has "allocated 0 bytes this frame"
     * and 4 MB of garbage to collect.
     */
    class UNLUANOVA_API FLuaGcScheduler
    {
    public:
        struct FStats
        {
            /** Live heap as of the last Tick()/Collect(). */
            int64 HeapBytes = 0;
            int64 PeakHeapBytes = 0;

            /** Work the collector still owes, in bytes of allocation to process. */
            int64 PendingDebtBytes = 0;

            /** Total bytes the collector has been asked to process. */
            int64 BytesSteppedTotal = 0;

            int32 Frames = 0;
            int32 SteppedFrames = 0;
            int32 FullCollects = 0;

            double TotalStepMs = 0.0;
            double WorstFrameMs = 0.0;
        };

        explicit FLuaGcScheduler(lua_State* InL);

        /**
         * Switch modes. Going to TimeSliced stops the VM's own collector; going back to
         * Automatic restarts it, so leaving this mode never leaves the heap uncollected.
         */
        void SetMode(ELuaGcMode InMode);
        ELuaGcMode GetMode() const { return Mode; }

        /**
         * How fast the collector chases the allocator, in percent. 100 means "process as
         * much as was allocated"; 200 collects twice as fast as the script allocates.
         * Below 100 the heap grows without bound - that is a tuning knob, not a guard.
         */
        void SetStepMultiplier(int32 InPercent);
        int32 GetStepMultiplier() const { return StepMultiplier; }

        /**
         * Upper bound on the work one frame may do, in bytes. 0 means no cap: the frame
         * spends the whole debt, which is precisely the spike this class exists to avoid,
         * so leave it at something unless you are measuring.
         */
        void SetMaxStepBytesPerFrame(int64 InBytes);
        int64 GetMaxStepBytesPerFrame() const { return MaxStepBytesPerFrame; }

        /**
         * Safety valve. Above this heap size the scheduler takes a full synchronous
         * collect rather than let a mis-tuned budget run the process out of memory.
         * 0 disables it.
         */
        void SetHardLimitBytes(int64 InBytes);
        int64 GetHardLimitBytes() const { return HardLimitBytes; }

        /** Drive one frame's worth of collection. Safe to call in Automatic mode too. */
        void Tick(float DeltaSeconds);

        /** One synchronous full cycle, debt cleared. This is the spike; use it sparingly. */
        void Collect();

        /** One explicit step of at most Bytes. Returns true if it finished a cycle. */
        bool Step(int64 Bytes);

        int64 GetHeapBytes() const;

        /** True after a step reports the collection cycle completed. */
        bool IsCycleComplete() const { return bCycleComplete; }

        const FStats& GetStats() const { return Stats; }
        void ResetStats();

    private:
        lua_State* L = nullptr;
        ELuaGcMode Mode = ELuaGcMode::Automatic;

        int32 StepMultiplier = 100;
        int64 MaxStepBytesPerFrame = 0;
        int64 HardLimitBytes = 0;

        /**
         * Heap size at the last completed cycle - the live set. Debt is measured against
         * this rather than against the previous frame, because Lua frees nothing until a
         * cycle runs and per-frame allocation therefore says nothing about garbage that
         * was built earlier and dropped.
         */
        int64 BaselineBytes = 0;
        int64 DebtBytes = 0;
        bool bCycleComplete = false;
        FStats Stats;
    };
}
