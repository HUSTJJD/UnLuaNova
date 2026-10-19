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

#include <atomic>

struct lua_State;
struct lua_Debug;

namespace UnLuaNova
{
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
     * Off by default. Toggle at runtime with `lua.profiler on|off` - note that it shares
     * the VM hook with FDeadLoopCheck, so enabling this disables that.
     */
    class FLuaProfiler
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

        /** Function id (see UNLUANOVA_DEBUG_PROTOID) -> Insights spec id. */
        TMap<uint64, uint32> SpecIds;
    };
}
