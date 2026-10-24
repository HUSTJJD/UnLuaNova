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

#include "HAL/FileManager.h"
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

    /**
     * Walk the current stack into the one-line form Luau's tools/perfgraph.py parses:
     *
     *     <source>,<name>,<linedefined>;<source>,<name>,<linedefined>;
     *
     * innermost frame first, with a synthetic GC frame at the front while the collector
     * is the thing holding the VM up. Keeping the shape byte-identical is the whole point
     * - it is what lets the upstream script open a capture taken from inside the editor.
     */
    static FString CaptureStackTrace(lua_State* L, int GC)
    {
        FString Out;

        if (GC > 0)
            Out += TEXT("GC,GC,0;");

        for (int Level = 0; Level < 128; ++Level)
        {
            lua_Debug Ar;
            if (!lua_getstack(L, Level, &Ar))
                break;

            lua_getinfo(L, "snl", &Ar);

            const TCHAR* Source = Ar.source ? UTF8_TO_TCHAR(Ar.source) : TEXT("");
            const TCHAR* Name = Ar.name ? UTF8_TO_TCHAR(Ar.name) : TEXT("");

            Out += FString::Printf(TEXT("%s,%s,%d;"), Source, Name, Ar.linedefined);
        }

        return Out;
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

            // Coroutines are the one thing a sampled profiler gets wrong by construction:
            // a coroutine that is abandoned mid-call never returns, and its frame would
            // sit on the shared stack until some unrelated return popped it. Luau tells us
            // when a thread dies (LP == nullptr), which is the moment to drop its frames.
            FLuaVmHooks::Get().AddThreadHook(Attached, this,
                [](lua_State* Parent, lua_State* Thread)
                {
                    if (!Parent)
                        FLuaProfiler::Get().PruneThread(Thread);
                });
#endif
        }
        else
        {
            FLuaVmHooks::Get().RemoveOwner(Attached, this);
            FLuaVmHooks::Get().RemoveThreadOwner(Attached, this);
        }
    }

    void FLuaProfiler::SetTraceEnabled(bool bInEnabled)
    {
        if (IsTraceEnabled() == bInEnabled)
            return;

        bTrace.store(bInEnabled, std::memory_order_relaxed);

        // Same reasoning as SetAggregateEnabled: a stale "last sample" would charge one
        // enormous interval - spanning however long the trace was off - to whatever was
        // running when it was switched back on.
        LastTraceCycles = 0;

        if (!bInEnabled)
            return;

        TraceSamples.Empty();
    }

    void FLuaProfiler::ResetTrace()
    {
        TraceSamples.Empty();
        LastTraceCycles = 0;
    }

    bool FLuaProfiler::DumpTraceToFile(const FString& Path) const
    {
        FString Out;
        Out.Reserve(TraceSamples.Num() * 64);

        for (const FTraceSample& Sample : TraceSamples)
            Out += FString::Printf(TEXT("%llu %s\n"), Sample.Us, *Sample.Stack);

        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        if (!FFileHelper::SaveStringToFile(Out, *Path))
            return false;

        UE_LOG(LogUnLuaNova, Log, TEXT("Wrote %d samples to '%s'. Convert with Tools/perfgraph.py (Luau's own tool)."),
            TraceSamples.Num(), *Path);
        return true;
    }

    void FLuaProfiler::MaybeTraceSample(lua_State* L, int GC)
    {
        if (!IsTraceEnabled())
            return;

        // Decimate to roughly 1 kHz. The interrupt fires far more often than that, and a
        // full stack walk per event would be a bigger perturbation than the noise it was
        // meant to measure.
        const uint64 NowCycles = FPlatformTime::Cycles64();
        if (LastTraceCycles != 0 && FPlatformTime::ToMilliseconds64(NowCycles - LastTraceCycles) < 1.0)
            return;

        LastTraceCycles = NowCycles;

        if (TraceSamples.Num() >= MaxTraceSamples)
        {
            bTrace.store(false, std::memory_order_relaxed);
            UE_LOG(LogUnLuaNova, Warning, TEXT("Lua profiler trace hit its %d sample ceiling and stopped; dump it or raise MaxTraceSamples."),
                MaxTraceSamples);
            return;
        }

        FString Stack = CaptureStackTrace(L, GC);
        if (Stack.IsEmpty())
            return;

        TraceSamples.Add(FTraceSample{uint64(FPlatformTime::ToMilliseconds64(NowCycles) * 1000.0), MoveTemp(Stack)});
    }

    void FLuaProfiler::PruneThread(lua_State* Thread)
    {
        if (!Thread)
            return;

        const int32 Before = CallStack.Num();
        CallStack.RemoveAll([Thread](const FFrame& Frame) { return Frame.Thread == Thread; });

        if (CallStack.Num() != Before)
        {
            // Whatever was mid-flight on that thread is gone, so the sampled path must not
            // attribute the next interval to it either.
            LastSampleKey = 0;
            LastSampleCycles = 0;
        }
    }

    void FLuaProfiler::SetAggregateEnabled(bool bInEnabled)
    {
        if (IsAggregateEnabled() == bInEnabled)
            return;

        bAggregate.store(bInEnabled, std::memory_order_relaxed);

        // Turning it off mid-run would otherwise leave a half-built stack whose next
        // return attributes the wrong elapsed time to the wrong function. Same for the
        // sampled path: a stale LastSample* would charge one huge interval spanning the
        // disabled period to whatever happened to be running before.
        CallStack.Empty();
        LastSampleKey = 0;
        LastSampleCycles = 0;
        LastSampleBytes = 0;
    }

    void FLuaProfiler::ResetAggregate()
    {
        FuncStats.Empty();
        CallStack.Empty();
        LastSampleKey = 0;
        LastSampleCycles = 0;
        LastSampleBytes = 0;
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

        CallStack.Add(FFrame{Key, NowCycles, 0, L, HeapBytes(L)});
    }

    void FLuaProfiler::RecordLeave(lua_State* L, uint64 NowCycles)
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

            // Heap growth over this call. Attributed to this frame, not netted against its
            // callees: "which function is the heap growing in" is the question, and a
            // callee's growth is that callee's answer to give.
            Stat->NetBytes += HeapBytes(L) - Frame.StartBytes;
        }

        // Charge the whole span to the caller, so its own self time excludes it.
        if (CallStack.Num() > 0)
            CallStack.Last().ChildCycles += Elapsed;
    }

    void FLuaProfiler::RecordSample(lua_State* L, lua_Debug* Ar, uint64 Key, uint64 NowCycles)
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

        FLuaProfilerFuncStat* Stat = FuncStats.Find(Key);
        Stat->Hits++;       // a safe point, not a call - see the field comment
        Stat->Samples++;

        // Charge the interval that just closed to whatever was running when it opened.
        if (LastSampleCycles != 0 && LastSampleKey != 0)
        {
            AttributeSample(LastSampleKey, NowCycles - LastSampleCycles, HeapBytes(L) - LastSampleBytes);
        }
        LastSampleKey = Key;
        LastSampleCycles = NowCycles;
        LastSampleBytes = HeapBytes(L);
    }

    void FLuaProfiler::AttributeSample(uint64 Key, uint64 ElapsedCycles, int64 DeltaBytes)
    {
        if (FLuaProfilerFuncStat* Stat = FuncStats.Find(Key))
        {
            Stat->SelfMs += FPlatformTime::ToMilliseconds64(ElapsedCycles);
            Stat->NetBytes += DeltaBytes;
        }
    }

    int64 FLuaProfiler::HeapBytes(lua_State* L)
    {
        if (!L)
            return 0;

        // LUA_GCCOUNT is documented in Kbytes; the extra byte count is only available in
        // some builds, so KB is the portable resolution. Fine for attributing growth - a
        // function that moves the needle moves it by more than a Kbyte.
        return static_cast<int64>(lua_gc(L, LUA_GCCOUNT, 0)) * 1024;
    }

    bool FLuaProfiler::IsSelfTimeSampled()
    {
#if UNLUANOVA_HAS_HOOK_EVENT
        return false;
#else
        return true;
#endif
    }

    FString FLuaProfiler::BuildReport(int32 MaxRows) const
    {
        TArray<FLuaProfilerFuncStat> Rows;
        Rows.Reserve(FuncStats.Num());
        for (const TPair<uint64, FLuaProfilerFuncStat>& Pair : FuncStats)
            Rows.Add(Pair.Value);

        Rows.Sort([](const FLuaProfilerFuncStat& A, const FLuaProfilerFuncStat& B)
        {
            if (A.SelfMs != B.SelfMs)
                return A.SelfMs > B.SelfMs;
            return A.Hits > B.Hits;
        });

        // Say which kind of number SelfMs is, in the file itself. A CSV that gets emailed
        // around loses the header comment on the console command that produced it, and a
        // sampled attribution read as a measurement is worse than no number at all.
        const bool bSampled = IsSelfTimeSampled();
        FString Out = FString::Printf(
            TEXT("# Backend=%s SelfMs=%s Hits=%s\n"),
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            TEXT("Luau"),
#else
            TEXT("Lua"),
#endif
            bSampled ? TEXT("sampled") : TEXT("measured"),
            bSampled ? TEXT("safe points") : TEXT("completed calls"));
        Out += TEXT("Function,Source,Line,Hits,SelfMs\n");

        const int32 Count = MaxRows > 0 ? FMath::Min(MaxRows, Rows.Num()) : Rows.Num();
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Out += FString::Printf(TEXT("%s,%s,%d,%llu,%.3f\n"),
                *Rows[Index].Name, *Rows[Index].Source, Rows[Index].Line,
                Rows[Index].Hits, Rows[Index].SelfMs);
        }

        return Out;
    }

    FString FLuaProfiler::BuildAllocReport(int32 MaxRows) const
    {
        TArray<FLuaProfilerFuncStat> Rows;
        Rows.Reserve(FuncStats.Num());
        for (const TPair<uint64, FLuaProfilerFuncStat>& Pair : FuncStats)
        {
            // Only growth is interesting here. A function that nets to zero either frees
            // what it allocates or never allocates, and neither needs investigating.
            if (Pair.Value.NetBytes > 0)
                Rows.Add(Pair.Value);
        }

        Rows.Sort([](const FLuaProfilerFuncStat& A, const FLuaProfilerFuncStat& B)
        {
            return A.NetBytes > B.NetBytes;
        });

        FString Out = FString::Printf(
            TEXT("# NetBytes=%s  (heap delta while the function ran; a GC running inside the span hides part of it)\n"),
            IsSelfTimeSampled() ? TEXT("sampled") : TEXT("per-call"));
        Out += TEXT("Function,Source,Line,Hits,NetKB\n");

        const int32 Count = MaxRows > 0 ? FMath::Min(MaxRows, Rows.Num()) : Rows.Num();
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Out += FString::Printf(TEXT("%s,%s,%d,%llu,%.1f\n"),
                *Rows[Index].Name, *Rows[Index].Source, Rows[Index].Line,
                Rows[Index].Hits, Rows[Index].NetBytes / 1024.0);
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
            // Only on entry: a return would double the sample rate for the same instant.
            Self.MaybeTraceSample(L, 0);
        }
        else
        {
            FCpuProfilerTrace::OutputEndEvent();
            Self.RecordLeave(L, FPlatformTime::Cycles64());
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

        // GC is passed through: time spent collecting is not time spent in the function
        // the stack says we are in, and the format has a place to say so.
        Self.MaybeTraceSample(L, GC);

        // Sampling, not timing: without a return event there is no span to subtract callees
        // from, so Hits counts safe points and self time is attributed from the interval
        // between them rather than measured. IsSelfTimeSampled() reports which it is.
        Self.RecordSample(L, &Ar, UNLUANOVA_DEBUG_PROTOID(&Ar), FPlatformTime::Cycles64());
    }

#endif
}
