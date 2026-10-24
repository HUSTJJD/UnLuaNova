// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"

#include <atomic>

struct lua_State;
struct lua_Debug;

namespace UnLuaNova
{
    /** One Lua function's aggregated cost, for the self-time report. */
    struct FLuaProfilerFuncStat
    {
        FString Name;
        FString Source;
        int32 Line = 0;

        /** Number of completed calls (Luau: number of safe points hit, see below). */
        uint64 Hits = 0;

        /**
         * Time spent in this function excluding its callees, in milliseconds.
         *
         * Measured exactly on the Lua backend, where calls and returns pair up. On Luau it
         * is *attributed*, not measured: a safe point is neither a call nor a return, so
         * there is no span to subtract callees from. Each interval between two safe points
         * is charged to whatever was running when it started, which is the usual sampling
         * approximation - good enough to rank functions, and not a substitute for Insights
         * when you need the exact split. FLuaProfiler::IsSelfTimeSampled() says which one
         * you are looking at.
         */
        double SelfMs = 0.0;

        /** Samples that landed on this function. Lua backend: 0 (it counts real calls). */
        uint64 Samples = 0;

        /**
         * Net Lua heap change while this function ran, in bytes. Positive means the heap
         * grew and was not given back - the signature of "this function allocates".
         *
         * Same caveat as SelfMs: on the Lua backend it is the heap delta over a real call,
         * on Luau it is the delta between two safe points attributed to whatever was
         * running. Neither is a byte-exact allocation count - the GC can run inside the
         * span and give memory back, and a span that allocates and frees in equal measure
         * nets to zero. What it is good at is answering "which of my functions is the heap
         * growing in", which is the question you actually have when it grows.
         */
        int64 NetBytes = 0;
    };

    /**
     * Lua CPU profiler, feeding Unreal Insights.
     *
     * Replaces the old inline hook in LuaEnv.cpp, which rebuilt an FString on every
     * single call/return - survivable for a few frames, ruinous as anything you might
     * leave on. This registers each Lua function with Insights once via
     * FCpuProfilerTrace::OutputEventType() and afterwards only emits the cached spec id,
     * so the steady-state cost is an integer lookup instead of a heap allocation.
     *
     * The backend dictates the mechanism (see LuaBackend.h):
     *   UNLUANOVA_HAS_HOOK_EVENT=1 (official Lua) - lua_Debug carries `event`, so calls
     *     and returns pair up into proper begin/end scopes.
     *   UNLUANOVA_HAS_HOOK_EVENT=0 (Luau) - lua_Debug has no `event` field, so pairing is
     *     impossible; instead the interrupt callback samples whatever is currently
     *     running and emits a point event. Coarser, but Insights still shows where the
     *     time clusters.
     *
     * Off by default. Toggle at runtime with `lua.profiler on|off`. The hook is claimed
     * through FLuaVmHooks, so FDeadLoopCheck stays active alongside it - the two used to
     * overwrite each other, which is exactly what the dispatcher exists to prevent.
     */
    // Exported (as is everything the console commands and the automation specs drive):
    // another module reaches this across the module boundary, and a modular build will
    // not let it link an unexported symbol.
    class UNLUANOVA_API FLuaProfiler
    {
    public:
        static FLuaProfiler& Get();

        /** Remember the state so the runtime toggle can install/remove the hook. */
        void Attach(lua_State* L);
        void Detach(lua_State* L);

        void SetEnabled(bool bInEnabled);
        bool IsEnabled() const { return bEnabled.load(std::memory_order_relaxed); }

        /** Number of distinct Lua functions registered so far. */
        int32 NumTrackedFunctions() const { return SpecIds.Num(); }

        /**
         * Per-function aggregation, for the self-time report.
         *
         * Insights is the right tool for a timeline and the wrong one for "which of my
         * 400 Lua functions is actually hot" - it has no bottom-up view. This is that
         * view, and it is off by default because it costs a stack push/pop per call.
         */
        void SetAggregateEnabled(bool bInEnabled);
        bool IsAggregateEnabled() const { return bAggregate.load(std::memory_order_relaxed); }
        void ResetAggregate();
        int32 NumAggregatedFunctions() const { return FuncStats.Num(); }

        /** Bottom-up table, hottest first, as CSV. */
        FString BuildReport(int32 MaxRows = 40) const;

        /** Bottom-up table by net heap growth, biggest first, as CSV. */
        FString BuildAllocReport(int32 MaxRows = 40) const;

        /** Write BuildReport() to disk so a CI run can diff two commits. */
        bool DumpToFile(const FString& Path, int32 MaxRows = INDEX_NONE) const;

        /**
         * Sampling trace in Luau's own profile.out format.
         *
         * Insights is the tool for a timeline, and the bottom-up report is the tool for
         * ranking functions. This is the third thing: a plain file of
         *
         *     <microseconds>  <source>,<name>,<linedefined>;<source>,<name>,...;
         *
         * which is exactly what Luau's tools/perfgraph.py consumes, flamegraph included.
         * Writing it means a profile taken inside the editor can be opened with upstream's
         * own tooling - no conversion step, no private format - and it is the only output
         * here that survives without Unreal Insights running.
         *
         * Off by default and decimated to ~1 kHz, because a full stack walk per safe point
         * is not free. Toggle with `lua.profiler trace on|off|dump [path]`.
         */
        /** Hard ceiling on recorded samples: a runaway trace must not be what exhausts memory. */
        static constexpr int32 MaxTraceSamples = 200000;

        void SetTraceEnabled(bool bInEnabled);
        bool IsTraceEnabled() const { return bTrace.load(std::memory_order_relaxed); }
        int32 NumTraceSamples() const { return TraceSamples.Num(); }
        bool DumpTraceToFile(const FString& Path) const;
        void ResetTrace();

        /** Entry point used when the backend can distinguish call from return. */
        static void Hook(lua_State* L, lua_Debug* Ar);

        /** Entry point used when it cannot (Luau interrupt callback). */
        static void Interrupt(lua_State* L, int GC);

        /**
         * Whether SelfMs in this build is sampled rather than measured.
         *
         * It matters because the two answer different questions. A measured self time is
         * "this function burned this much CPU"; a sampled one is "this fraction of the
         * samples landed here", and a leaf function that yields at every loop iteration
         * will look cheaper than it is. Reporting a sampled number as if it were measured
         * is how a profile ends up pointing at the wrong function.
         */
        static bool IsSelfTimeSampled();

    private:
        FLuaProfiler() = default;

        /** Insights spec id for a function, registering it on first sight. */
        uint32 ResolveSpecId(lua_State* L, lua_Debug* Ar);

        /** Record one sample for the profile.out trace, if the decimation lets it through. */
        void MaybeTraceSample(lua_State* L, int GC);

        /**
         * Drop call-stack frames left behind by a thread that just died.
         *
         * A coroutine that is abandoned mid-call never returns, so its frame would sit on
         * the stack forever and the next unrelated return would pop it - charging one
         * function's time to another. Luau is the backend that can tell us about thread
         * destruction at all (lua_Callbacks::userthread), so this only helps there; on the
         * Lua backend the call/return pairing is exact and there is nothing to prune.
         */
        void PruneThread(lua_State* Thread);

        std::atomic<bool> bEnabled{false};
        lua_State* Attached = nullptr;

        void RecordEnter(lua_State* L, lua_Debug* Ar, uint64 Key, uint64 NowCycles);
        void RecordLeave(uint64 NowCycles);
        void RecordSample(lua_State* L, lua_Debug* Ar, uint64 Key, uint64 NowCycles);

        /** Lua heap in bytes right now, for attributing growth to a function. */
        static int64 HeapBytes(lua_State* L);

        /**
         * Charge one sampled interval to the function that was running when it began.
         *
         * Intervals are attributed one sample late on purpose: at safe point N we know when
         * N-1 happened and what was running then, and that is the only interval whose owner
         * is fully known. Attributing the *upcoming* interval instead would credit a
         * function with time spent after it returned.
         */
        void AttributeSample(uint64 Key, uint64 ElapsedCycles, int64 DeltaBytes);

        /** Function id (see UNLUANOVA_DEBUG_PROTOID) -> Insights spec id. */
        TMap<uint64, uint32> SpecIds;

        /** Same key -> aggregated cost. Only populated while aggregation is on. */
        TMap<uint64, FLuaProfilerFuncStat> FuncStats;

        struct FFrame
        {
            uint64 Key;
            uint64 StartCycles;
            uint64 ChildCycles;
            /** Which thread this frame belongs to, so a dead coroutine's frames can go. */
            lua_State* Thread = nullptr;
            /** Heap reading at entry, so the growth over this call can be attributed. */
            int64 StartBytes = 0;
        };
        TArray<FFrame> CallStack;

        struct FTraceSample
        {
            uint64 Us = 0;
            FString Stack;
        };
        TArray<FTraceSample> TraceSamples;
        std::atomic<bool> bTrace{false};
        uint64 LastTraceCycles = 0;

        // Sampled self time (Luau) needs one interval of history: what was running at the
        // previous safe point, and when that was. Memory attribution is sampled the same
        // way, so it needs the heap reading from that same moment.
        uint64 LastSampleKey = 0;
        uint64 LastSampleCycles = 0;
        int64 LastSampleBytes = 0;

        std::atomic<bool> bAggregate{false};
    };
}
