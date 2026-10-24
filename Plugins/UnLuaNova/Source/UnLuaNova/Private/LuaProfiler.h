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
         * Zero on the Luau backend: it has no call/return pairing, so there is nothing
         * to subtract. Hits still counts there - it counts samples, not calls, and a
         * function that dominates the samples is the hot one regardless.
         */
        double SelfMs = 0.0;
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
    // these live in UnLuaNova's Private folder, and a modular build will not let another
    // module link them otherwise.
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

        /** Write BuildReport() to disk so a CI run can diff two commits. */
        bool DumpToFile(const FString& Path, int32 MaxRows = INDEX_NONE) const;

        /** Entry point used when the backend can distinguish call from return. */
        static void Hook(lua_State* L, lua_Debug* Ar);

        /** Entry point used when it cannot (Luau interrupt callback). */
        static void Interrupt(lua_State* L, int GC);

    private:
        FLuaProfiler() = default;

        /** Insights spec id for a function, registering it on first sight. */
        uint32 ResolveSpecId(lua_State* L, lua_Debug* Ar);

        std::atomic<bool> bEnabled{false};
        lua_State* Attached = nullptr;

        void RecordEnter(lua_State* L, lua_Debug* Ar, uint64 Key, uint64 NowCycles);
        void RecordLeave(uint64 NowCycles);
        void RecordSample(lua_State* L, lua_Debug* Ar, uint64 Key);

        /** Function id (see UNLUANOVA_DEBUG_PROTOID) -> Insights spec id. */
        TMap<uint64, uint32> SpecIds;

        /** Same key -> aggregated cost. Only populated while aggregation is on. */
        TMap<uint64, FLuaProfilerFuncStat> FuncStats;

        struct FFrame
        {
            uint64 Key;
            uint64 StartCycles;
            uint64 ChildCycles;
        };
        TArray<FFrame> CallStack;

        std::atomic<bool> bAggregate{false};
    };
}
