// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT

#include "LuaProfiler.h"

#include "LuaBackend.h"

#if CPUPROFILERTRACE_ENABLED
#include "ProfilingDebugging/CpuProfilerTrace.h"
#endif

namespace UnLuaNova
{
    /** Is this frame worth emitting an event for? Only Lua functions have a source line. */
    static bool IsLuaFunction(const lua_Debug* Ar)
    {
        return Ar->what != nullptr && FCStringAnsi::Strcmp(Ar->what, "Lua") == 0;
    }

    FLuaProfiler& FLuaProfiler::Get()
    {
        static FLuaProfiler Instance;
        return Instance;
    }

    void FLuaProfiler::Attach(lua_State* L)
    {
        Attached = L;
    }

    void FLuaProfiler::Detach(lua_State* L)
    {
        if (Attached != L)
        {
            return;
        }
        SetEnabled(false);
        Attached = nullptr;
    }

    void FLuaProfiler::SetEnabled(bool bInEnabled)
    {
        if (IsEnabled() == bInEnabled)
        {
            return;
        }

        bEnabled.store(bInEnabled, std::memory_order_relaxed);

        if (!Attached)
        {
            return;
        }

#if UNLUANOVA_HAS_HOOK_EVENT
        lua_sethook(Attached,
                    bInEnabled ? &Hook : nullptr,
                    bInEnabled ? (LUA_MASKCALL | LUA_MASKRET) : 0,
                    0);
#else
        // Luau routes safe points through lua_Callbacks rather than lua_sethook.
        lua_callbacks(Attached)->interrupt = bInEnabled ? &Interrupt : nullptr;
#endif
    }

    uint32 FLuaProfiler::ResolveSpecId(lua_State* L, lua_Debug* Ar)
    {
        const uint64 Key = UNLUANOVA_DEBUG_PROTOID(Ar);

        if (const uint32* Found = SpecIds.Find(Key))
        {
            return *Found;
        }

        const ANSICHAR* Name = Ar->name ? Ar->name : "N/A";
        const FString EventName = FString::Printf(TEXT("%s [%s:%d]"),
                                                 ANSI_TO_TCHAR(Name),
                                                 *FPaths::GetBaseFilename(FString(ANSI_TO_TCHAR(Ar->source))),
                                                 Ar->linedefined);

        // Registered once per function; every later hit reuses the returned id.
        const uint32 Spec = FCpuProfilerTrace::OutputEventType(*EventName);
        SpecIds.Add(Key, Spec);
        return Spec;
    }

#if UNLUANOVA_HAS_HOOK_EVENT

    void FLuaProfiler::Hook(lua_State* L, lua_Debug* Ar)
    {
        FLuaProfiler& Self = Get();
        if (!Self.IsEnabled())
        {
            return;
        }

        UNLUANOVA_GETINFO(L, 0, "nSl", Ar);
        if (!IsLuaFunction(Ar))
        {
            return;
        }

        const uint32 Spec = Self.ResolveSpecId(L, Ar);
        if (Ar->event == LUA_HOOKCALL || Ar->event == LUA_HOOKTAILCALL)
        {
            FCpuProfilerTrace::OutputBeginEvent(Spec);
        }
        else
        {
            FCpuProfilerTrace::OutputEndEvent();
        }
    }

#else

    void FLuaProfiler::Interrupt(lua_State* L, int GC)
    {
        FLuaProfiler& Self = Get();
        if (!Self.IsEnabled())
        {
            return;
        }

        lua_Debug Ar;
        if (!UNLUANOVA_GETINFO(L, 0, "np", &Ar) || !IsLuaFunction(&Ar))
        {
            return;
        }

        // No call/return pairing on this backend, so each safe point is reported as a
        // zero-length event. Insights shows the density of hits per function.
        const uint32 Spec = Self.ResolveSpecId(L, &Ar);
        FCpuProfilerTrace::OutputBeginEvent(Spec);
        FCpuProfilerTrace::OutputEndEvent();
    }

#endif
}
