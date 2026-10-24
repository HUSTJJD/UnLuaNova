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
#include "LuaVmHooks.h"
#include "UnLuaNovaBase.h"

#if CPUPROFILERTRACE_ENABLED
#include "ProfilingDebugging/CpuProfilerTrace.h"
#endif

#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

        // Claim the slot through FLuaVmHooks instead of calling lua_sethook directly:
        // the dead-loop watchdog needs the same one, and whichever feature claimed it
        // second used to silently disable the other.
        if (bInEnabled)
        {
#if UNLUANOVA_HAS_HOOK_EVENT
            FLuaVmHooks::Get().Add(Attached, LUA_MASKCALL | LUA_MASKRET, this,
                [](lua_State* L, lua_Debug* Ar) { FLuaProfiler::Hook(L, Ar); });
#else
            // Luau routes safe points through lua_Callbacks rather than lua_sethook.
            FLuaVmHooks::Get().Add(Attached, 0, this,
                [](lua_State* L, lua_Debug*) { FLuaProfiler::Interrupt(L, 0); });
#endif
        }
        else
        {
            FLuaVmHooks::Get().RemoveOwner(Attached, this);
        }
    }

    void FLuaProfiler::SetAggregateEnabled(bool bInEnabled)
    {
        if (IsAggregateEnabled() == bInEnabled)
            return;

        bAggregate.store(bInEnabled, std::memory_order_relaxed);

        // Turning it off mid-run would otherwise leave a half-built stack whose next
        // return attributes the wrong elapsed time to the wrong function.
        CallStack.Empty();
    }

    void FLuaProfiler::ResetAggregate()
    {
        FuncStats.Empty();
        CallStack.Empty();
    }

    void FLuaProfiler::RecordEnter(lua_State* L, lua_Debug* Ar, uint64 Key, uint64 NowCycles)
    {
        if (!IsAggregateEnabled())
            return;

        if (!FuncStats.Contains(Key))
        {
            FLuaProfilerFuncStat& Stat = FuncStats.Add(Key);
            Stat.Name = Ar->name ? FString(ANSI_TO_TCHAR(Ar->name)) : TEXT("N/A");
            Stat.Source = Ar->source ? FPaths::GetBaseFilename(FString(ANSI_TO_TCHAR(Ar->source))) : TEXT("?");
            Stat.Line = Ar->linedefined;
        }

        CallStack.Add(FFrame{Key, NowCycles, 0});
    }

    void FLuaProfiler::RecordLeave(uint64 NowCycles)
    {
        if (!IsAggregateEnabled() || CallStack.Num() == 0)
            return;

        const FFrame Frame = CallStack.Pop();
        const uint64 Elapsed = NowCycles - Frame.StartCycles;

        if (FLuaProfilerFuncStat* Stat = FuncStats.Find(Frame.Key))
        {
            Stat->Hits++;
            // Self time: everything this call took, minus what its callees took.
            Stat->SelfMs += FPlatformTime::ToMilliseconds64(Elapsed > Frame.ChildCycles ? Elapsed - Frame.ChildCycles : 0);
        }

        // Charge the whole span to the caller, so its own self time excludes it.
        if (CallStack.Num() > 0)
            CallStack.Last().ChildCycles += Elapsed;
    }

    void FLuaProfiler::RecordSample(lua_State* L, lua_Debug* Ar, uint64 Key)
    {
        if (!IsAggregateEnabled())
            return;

        if (!FuncStats.Contains(Key))
        {
            FLuaProfilerFuncStat& Stat = FuncStats.Add(Key);
            Stat.Name = Ar->name ? FString(ANSI_TO_TCHAR(Ar->name)) : TEXT("N/A");
            Stat.Source = Ar->source ? FPaths::GetBaseFilename(FString(ANSI_TO_TCHAR(Ar->source))) : TEXT("?");
            Stat.Line = Ar->linedefined;
        }

        FuncStats.Find(Key)->Hits++;
    }

    FString FLuaProfiler::BuildReport(int32 MaxRows) const
    {
        TArray<FLuaProfilerFuncStat> Rows;
        Rows.Reserve(FuncStats.Num());
        for (const TPair<uint64, FLuaProfilerFuncStat>& Pair : FuncStats)
            Rows.Add(Pair.Value);

        Rows.Sort([](const FLuaProfilerFuncStat& A, const FLuaProfilerFuncStat& B)
        {
            // Self time first; on the Luau backend, where self time is always 0, this
            // degenerates into "most sampled first", which is the right order there too.
            if (A.SelfMs != B.SelfMs)
                return A.SelfMs > B.SelfMs;
            return A.Hits > B.Hits;
        });

        FString Out = TEXT("Function,Source,Line,Hits,SelfMs\n");

        const int32 Count = MaxRows > 0 ? FMath::Min(MaxRows, Rows.Num()) : Rows.Num();
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Out += FString::Printf(TEXT("%s,%s,%d,%llu,%.3f\n"),
                *Rows[Index].Name, *Rows[Index].Source, Rows[Index].Line,
                Rows[Index].Hits, Rows[Index].SelfMs);
        }

        return Out;
    }

    bool FLuaProfiler::DumpToFile(const FString& Path, int32 MaxRows) const
    {
        const FString FullPath = FPaths::IsRelative(Path)
            ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Profiler"), Path)
            : Path;

        const FString Report = BuildReport(MaxRows);
        if (!FFileHelper::SaveStringToFile(Report, *FullPath))
            return false;

        UE_LOG(LogUnLuaNova, Log, TEXT("Lua profiler report written to %s"), *FullPath);
        return true;
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

        const uint64 Key = UNLUANOVA_DEBUG_PROTOID(Ar);
        const uint32 Spec = Self.ResolveSpecId(L, Ar);
        if (Ar->event == LUA_HOOKCALL || Ar->event == LUA_HOOKTAILCALL)
        {
            FCpuProfilerTrace::OutputBeginEvent(Spec);
            Self.RecordEnter(L, Ar, Key, FPlatformTime::Cycles64());
        }
        else
        {
            FCpuProfilerTrace::OutputEndEvent();
            Self.RecordLeave(FPlatformTime::Cycles64());
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

        // Sampling, not timing: without a return event there is no span to subtract
        // callees from, so self time stays 0 and Hits counts safe points instead.
        Self.RecordSample(L, &Ar, UNLUANOVA_DEBUG_PROTOID(&Ar));
    }

#endif
}
