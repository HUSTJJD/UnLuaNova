#include "UnLuaNovaConsoleCommands.h"
#include "UnLuaNovaCompatibility.h"
#include "LuaBackend.h"
#include "LuaCoroutineTracker.h"
#include "LuaCoverage.h"
#include "LuaDebugService.h"
#include "LuaEnv.h"
#include "LuaMemoryStats.h"
#include "LuaProfiler.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "UnLuaNovaConsoleCommands"

namespace UnLuaNova
{
    FUnLuaNovaConsoleCommands::FUnLuaNovaConsoleCommands(IUnLuaNovaModule* InModule)
        : DoCommand(
              TEXT("lua.do"),
              *LOCTEXT("CommandText_Do", "Runs the given string in lua env.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::Do)
          ),
          DoFileCommand(
              TEXT("lua.dofile"),
              *LOCTEXT("CommandText_DoFile", "Runs the given module path in lua env.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::DoFile)
          ),
          CollectGarbageCommand(
              TEXT("lua.gc"),
              *LOCTEXT("CommandText_CollectGarbage", "Lua GC. Args: auto | slice [kb] | budget <kb> | mult <pct> | limit <kb> | stats, or none for a full collect.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::CollectGarbage)
          ),
          MemStatsCommand(
              TEXT("lua.memstats"),
              *LOCTEXT("CommandText_MemStats", "Lua heap usage. Args: on | off | reset, or none for a report.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::MemStats)
          ),
          MemoryQuotaCommand(
              TEXT("lua.memquota"),
              *LOCTEXT("CommandText_MemoryQuota", "Per-VM Lua heap budget. Args: <kb> | off | status | reset. 0/off disables. Distinct from lua.gc limit, which only forces a full collect.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::MemoryQuota)
          ),
          ProfilerCommand(
              TEXT("lua.profiler"),
              *LOCTEXT("CommandText_Profiler", "Lua CPU profiling into Unreal Insights. Args: on | off | agg on|off | reset | top [n] | mem [n] | dump [file], or none for status.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::Profiler)
          ),
          DebugCommand(
              TEXT("lua.dbg"),
              *LOCTEXT("CommandText_Debug", "Headless lua debugger. Args: env [n] | bp <src> <line> | step | continue | stack | locals | clear | detach, or none for status.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::Debug)
          ),
          CoroutinesCommand(
              TEXT("lua.coroutines"),
              *LOCTEXT("CommandText_Coroutines", "Live lua coroutines. Args: on | off, or none for a census of what is still alive and where it was created.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::Coroutines)
          ),
          CodeGenCommand(
              TEXT("lua.codegen"),
              *LOCTEXT("CommandText_CodeGen", "Luau native compiler. Args: on | off | modules <a,b,c> | none for status.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::CodeGen)
          ),
          CoverageCommand(
              TEXT("lua.coverage"),
              *LOCTEXT("CommandText_Coverage", "Luau script coverage. Args: dump [file] | status. Requires luau.coverage=1 before chunks load; judged by Tools/ValidateCoverage.py.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::Coverage)
          ),
          Module(InModule)
    {
    }

    void FUnLuaNovaConsoleCommands::Do(const TArray<FString>& Args) const
    {
        if (Args.Num() == 0)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("usage: lua.do <your code>"));
            return;
        }

        auto Env = Module->GetEnv();
        if (!Env)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("no available lua env found to run code."));
            return;
        }

        const auto Chunk = FString::Join(Args, TEXT(" "));
        Env->DoString(Chunk);
    }

    void FUnLuaNovaConsoleCommands::Debug(const TArray<FString>& Args) const
    {
        // Which env the debugger talks to. One env is the normal case, but
        // ULuaEnvLocator_ByGameInstance makes one per game instance - a listen server with
        // two clients has three - and the debugger used to attach to whichever
        // Module->GetEnv() happened to answer with, which is the one bound to nothing in
        // particular. Breakpoints then silently did nothing in the env you cared about.
        const TArray<FLuaEnv*> Envs = FLuaEnv::GetAllEnvs();
        if (Envs.Num() == 0)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("no available lua env found to debug."));
            return;
        }

        if (Args.Num() >= 2 && Args[0].Equals(TEXT("env"), ESearchCase::IgnoreCase))
        {
            const int32 Wanted = FCString::Atoi(*Args[1]);
            if (!Envs.IsValidIndex(Wanted))
            {
                UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("lua.dbg: no env at index %d - %d env(s) exist. Run 'lua.dbg env' to list them."), Wanted, Envs.Num());
                return;
            }

            DebugEnvIndex = Wanted;
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.dbg: now debugging env %d (main state 0x%p)"), DebugEnvIndex, Envs[DebugEnvIndex]->GetMainState());
            return;
        }

        if (Args.Num() >= 1 && Args[0].Equals(TEXT("env"), ESearchCase::IgnoreCase))
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.dbg: %d env(s), debugging env %d:"), Envs.Num(), DebugEnvIndex);
            for (int32 Index = 0; Index < Envs.Num(); ++Index)
            {
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("  %s%d: %s (main state 0x%p)"),
                    Index == DebugEnvIndex ? TEXT("> ") : TEXT("  "), Index,
                    *Envs[Index]->GetName(), Envs[Index]->GetMainState());
            }
            return;
        }

        if (!Envs.IsValidIndex(DebugEnvIndex))
        {
            // Nothing picked yet, or the picked env died with its world. Prefer the
            // module's env so a single-env session keeps its old behaviour, and only fall
            // back to the first one when that env is not among them.
            DebugEnvIndex = Envs.IndexOfByKey(Module->GetEnv());
            if (DebugEnvIndex == INDEX_NONE)
                DebugEnvIndex = 0;
        }

        // Attaching is idempotent, so every invocation makes sure the hook is installed
        // before doing anything else - the service only costs anything once armed.
        auto& DebugService = FLuaDebugService::Get();
        DebugService.Attach(Envs[DebugEnvIndex]->GetMainState());

        if (Args.Num() == 0)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.dbg: env=%d/%d attached=%d paused=%d hits=%d breakpoints=%d"),
                DebugEnvIndex, Envs.Num(),
                DebugService.IsAttached() ? 1 : 0,
                DebugService.IsPaused() ? 1 : 0,
                DebugService.GetHitCount(),
                DebugService.GetBreakpoints().Num());
            return;
        }

        const FString& Sub = Args[0];

        if (Sub == TEXT("bp") && Args.Num() >= 3)
        {
            const int32 Line = FCString::Atoi(*Args[2]);
            // Toggle: a second invocation on the same site removes it.
            if (!DebugService.RemoveBreakpoint(Args[1], Line))
                DebugService.AddBreakpoint(Args[1], Line);
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.dbg: breakpoint toggled at %s:%d"), *Args[1], Line);
            return;
        }

        if (Sub == TEXT("step"))
        {
            DebugService.SetSingleStep(true);
            return;
        }

        if (Sub == TEXT("continue"))
        {
            DebugService.SetSingleStep(false);
            DebugService.Continue();
            return;
        }

        if (Sub == TEXT("stack"))
        {
            for (const FLuaDebugFrame& Frame : DebugService.GetStackTrace())
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("#%d %s:%d %s"),
                    Frame.Depth, *Frame.Source, Frame.Line, *Frame.Function);
            return;
        }

        if (Sub == TEXT("locals"))
        {
            for (const FLuaDebugVariable& Var : DebugService.GetLocals())
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("%s (%s) = %s"),
                    *Var.Name, *Var.Type, *Var.Value);
            return;
        }

        if (Sub == TEXT("clear"))
        {
            DebugService.ClearBreakpoints();
            return;
        }

        if (Sub == TEXT("detach"))
        {
            DebugService.Detach();
            return;
        }

        UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("usage: lua.dbg [env [n] | bp <src> <line> | step | continue | stack | locals | clear | detach]"));
    }

    void FUnLuaNovaConsoleCommands::DoFile(const TArray<FString>& Args) const
    {
        if (Args.Num() != 1)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("usage: lua.dofile <lua.module.path>"));
            return;
        }

        auto Env = Module->GetEnv();
        if (!Env)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("no available lua env found to run file."));
            return;
        }

        constexpr auto& Format = TEXT(R"(
            local name = "%s"
            package.loaded[name] = nil
            collectgarbage("collect")
            require(name)
        )");
        const auto Chunk = FString::Printf(Format, *Args[0]);
        Env->DoString(Chunk);
    }

    void FUnLuaNovaConsoleCommands::CollectGarbage(const TArray<FString>& Args) const
    {
        auto Env = Module->GetEnv();
        if (!Env)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("no available lua env found to collect garbage."));
            return;
        }

        if (Args.Num() == 0)
        {
            Env->GC();
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.gc: full collect done, heap = %.1f KB"), Env->GetGCStats().HeapBytes / 1024.0);
            return;
        }

        // Sizes on the command line are Kbytes because that is what `lua.memstats` and
        // every other Lua tool reports; the API underneath is in bytes.
        const FString& Sub = Args[0];

        if (Sub == TEXT("auto"))
        {
            Env->SetGCMode(UnLuaNova::ELuaGcMode::Automatic);
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.gc: automatic (VM-paced); the per-frame slice is off."));
            return;
        }

        if (Sub == TEXT("slice"))
        {
            int64 Kb = 0;
            if (Args.Num() >= 2)
            {
                Kb = FCString::Atoi64(*Args[1]);
                Env->SetGCStepBudget(Kb * 1024);
            }
            Env->SetGCMode(UnLuaNova::ELuaGcMode::TimeSliced);
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.gc: time sliced, budget = %lld KB/frame (0 = uncapped)"), Kb);
            return;
        }

        if (Sub == TEXT("budget") && Args.Num() >= 2)
        {
            const int64 Kb = FCString::Atoi64(*Args[1]);
            Env->SetGCStepBudget(Kb * 1024);
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.gc: per-frame budget = %lld KB (0 = uncapped)"), Kb);
            return;
        }

        if (Sub == TEXT("mult") && Args.Num() >= 2)
        {
            const int32 Percent = FCString::Atoi(*Args[1]);
            Env->SetGCStepMultiplier(Percent);
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.gc: step multiplier = %d%% (100 = keep pace with the allocator)"), Percent);
            return;
        }

        if (Sub == TEXT("limit") && Args.Num() >= 2)
        {
            const int64 Kb = FCString::Atoi64(*Args[1]);
            Env->SetGCHardLimit(Kb * 1024);
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.gc: hard limit = %lld KB (0 = disabled)"), Kb);
            return;
        }

        if (Sub == TEXT("stats"))
        {
            const auto& S = Env->GetGCStats();
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.gc: mode=%s heap=%.1f KB peak=%.1f KB debt=%.1f KB"),
                Env->GetGCMode() == UnLuaNova::ELuaGcMode::TimeSliced ? TEXT("time-sliced") : TEXT("automatic"),
                S.HeapBytes / 1024.0, S.PeakHeapBytes / 1024.0, S.PendingDebtBytes / 1024.0);
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.gc: frames=%d stepped=%d fullcollects=%d steppedMB=%.2f total=%.2f ms worst frame=%.3f ms"),
                S.Frames, S.SteppedFrames, S.FullCollects, S.BytesSteppedTotal / (1024.0 * 1024.0), S.TotalStepMs, S.WorstFrameMs);
            return;
        }

        UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("usage: lua.gc [auto | slice [kb] | budget <kb> | mult <pct> | limit <kb> | stats]"));
    }

    void FUnLuaNovaConsoleCommands::MemStats(const TArray<FString>& Args) const
    {
        auto& Stats = FLuaMemoryStats::Get();

        if (Args.Num() > 0)
        {
            const FString& Arg = Args[0];
            if (Arg.Equals(TEXT("on"), ESearchCase::IgnoreCase))
            {
                Stats.bEnabled.store(true, std::memory_order_relaxed);

                // The categorized view needs the VM's allocation callbacks, so it is only
                // wired up while tracking is on - off means literally no hook installed.
                if (auto Env = Module->GetEnv())
                    Stats.Attach(Env->GetMainState());

                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua memory tracking enabled."));
                return;
            }
            if (Arg.Equals(TEXT("off"), ESearchCase::IgnoreCase))
            {
                Stats.bEnabled.store(false, std::memory_order_relaxed);
                Stats.Detach();
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua memory tracking disabled."));
                return;
            }
            if (Arg.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
            {
                Stats.Reset();
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua memory interval counters reset; live allocations preserved."));
                return;
            }

            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("usage: lua.memstats [on|off|reset]"));
            return;
        }

        if (!Stats.bEnabled.load(std::memory_order_relaxed))
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Detailed tracking is off; live bytes remain available. Use 'lua.memstats on' for call counts and categories."));
        }

        UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("%s"), *Stats.BuildReport());

        // Categorized breakdown is Luau-only; on other backends it reports that and stops.
        // Empty until 'lua.memstats on' has been run, since that is what installs the hooks.
        UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("%s"), *Stats.BuildCategoryReport());
    }

    void FUnLuaNovaConsoleCommands::MemoryQuota(const TArray<FString>& Args) const
    {
        const TArray<FLuaEnv*> Envs = FLuaEnv::GetAllEnvs();
        const FString Sub = Args.Num() > 0 ? Args[0] : TEXT("status");

        if (Sub.Equals(TEXT("status"), ESearchCase::IgnoreCase))
        {
            // Force the CVar to exist even on a build where no env has been created yet.
            const int64 CvarBytes = FLuaEnv::ReadMemoryQuotaCvar();
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.memquota: %d env(s); cvar lua.maxmemorykb = %lld KiB for NEW envs"),
                Envs.Num(), CvarBytes / 1024);
            for (int32 Index = 0; Index < Envs.Num(); ++Index)
            {
                FLuaEnv* Env = Envs[Index];
                const int64 Max = Env->GetMemoryQuotaMaxBytes();
                UNLUANOVA_LOGF(LogUnLuaNova, Log,
                    TEXT("  %d: %s limit=%lld B (%s) live=%lld B rejections=%d last_request=%lld B"),
                    Index, *Env->GetName(), Max, Max > 0 ? TEXT("on") : TEXT("off"),
                    Env->GetLiveLuaBytes(), Env->GetMemoryQuotaRejectionCount(),
                    Env->GetMemoryQuota().LastRequestedBytes.load());
            }
            UNLUANOVA_LOGF(LogUnLuaNova, Log,
                TEXT("lua.memquota: a refusal raises a recoverable Lua error; lua.gc limit only forces a collection."));
            return;
        }

        if (Sub.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
        {
            for (FLuaEnv* Env : Envs)
                Env->GetMemoryQuota().Rejections.store(0);
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.memquota: rejection counters cleared for %d env(s); limits unchanged."), Envs.Num());
            return;
        }

        int64 Kb = 0;
        if (Sub.Equals(TEXT("off"), ESearchCase::IgnoreCase))
        {
            Kb = 0;
        }
        else
        {
            Kb = FCString::Atoi64(*Sub);
            if (Kb < 0 || (!Sub.IsNumeric() && Kb == 0))
            {
                UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("usage: lua.memquota <kb> | off | status | reset"));
                return;
            }
        }

        const int64 Bytes = Kb > 0 ? Kb * 1024 : 0;
        (void)FLuaEnv::ReadMemoryQuotaCvar(); // registers the CVar on first use
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.maxmemorykb")))
            CVar->Set((int32)(Bytes / 1024), ECVF_SetByConsole);

        // Existing envs keep their own context; the CVar only seeds envs created later.
        for (FLuaEnv* Env : Envs)
            Env->SetMemoryQuotaMaxBytes(Bytes);

        UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.memquota: limit = %lld B (%lld KiB) for %d live env(s); new envs inherit lua.maxmemorykb. rejections counters kept."),
            Bytes, Bytes / 1024, Envs.Num());
    }

    void FUnLuaNovaConsoleCommands::Profiler(const TArray<FString>& Args) const
    {
        auto& Profiler = FLuaProfiler::Get();

        if (Args.Num() > 0)
        {
            const FString& Arg = Args[0];
            if (Arg.Equals(TEXT("on"), ESearchCase::IgnoreCase))
            {
                Profiler.SetEnabled(true);
                // Not "DeadLoopCheck is inactive": both go through FLuaVmHooks now, which
                // exists precisely so they stop overwriting each other.
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua profiling on - capture with Unreal Insights. DeadLoopCheck keeps its hook (both go through FLuaVmHooks)."));
                return;
            }
            if (Arg.Equals(TEXT("off"), ESearchCase::IgnoreCase))
            {
                Profiler.SetEnabled(false);
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua profiling off."));
                return;
            }

            // Per-function self-time aggregation. Separate switch from on/off because it
            // costs a stack push/pop per call, which the timeline alone does not.
            if (Arg.Equals(TEXT("agg"), ESearchCase::IgnoreCase))
            {
                const bool bOn = !(Args.Num() >= 2 && Args[1].Equals(TEXT("off"), ESearchCase::IgnoreCase));
                Profiler.SetAggregateEnabled(bOn);
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua profiler aggregation %s."), bOn ? TEXT("ON") : TEXT("OFF"));
                return;
            }

            if (Arg.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
            {
                Profiler.ResetAggregate();
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua profiler aggregates reset."));
                return;
            }

            // Sampling trace in Luau's own profile.out format. Enabling it turns profiling
            // on too - there is no other way to get the interrupts it samples from.
            if (Arg.Equals(TEXT("trace"), ESearchCase::IgnoreCase))
            {
                const FString& Sub = Args.Num() >= 2 ? Args[1] : FString();

                if (Sub.Equals(TEXT("off"), ESearchCase::IgnoreCase))
                {
                    Profiler.SetTraceEnabled(false);
                    UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua profiler trace off."));
                    return;
                }

                if (Sub.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
                {
                    Profiler.ResetTrace();
                    UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua profiler trace cleared."));
                    return;
                }

                if (Sub.Equals(TEXT("dump"), ESearchCase::IgnoreCase))
                {
                    const FString Path = Args.Num() >= 3
                        ? Args[2]
                        : FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Profiling"), TEXT("profile.out"));
                    Profiler.DumpTraceToFile(Path);
                    return;
                }

                Profiler.SetEnabled(true);
                Profiler.SetTraceEnabled(true);
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua profiler trace on (~1kHz, %d sample ceiling). 'lua.profiler trace dump [path]' to write it."),
                    FLuaProfiler::MaxTraceSamples);
                return;
            }

            if (Arg.Equals(TEXT("top"), ESearchCase::IgnoreCase))
            {
                const int32 Rows = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : 20;
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("%s"), *Profiler.BuildReport(Rows));
                return;
            }

            // Where the heap grew, not just how big it is. lua.memstats says the heap went
            // from 4 MB to 40 MB; this says which function it happened in. Needs 'agg on'.
            if (Arg.Equals(TEXT("mem"), ESearchCase::IgnoreCase))
            {
                const int32 Rows = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : 20;
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("%s"), *Profiler.BuildAllocReport(Rows));
                return;
            }

            if (Arg.Equals(TEXT("dump"), ESearchCase::IgnoreCase))
            {
                const FString File = Args.Num() >= 2 ? Args[1] : TEXT("lua-profiler.csv");
                if (!Profiler.DumpToFile(File))
                    UNLUANOVA_LOGF(LogUnLuaNova, Error, TEXT("Failed to write the profiler report to '%s'."), *File);
                return;
            }

            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("usage: lua.profiler [on|off|agg on|off|reset|top [n]|mem [n]|dump [file]]"));
            return;
        }

        UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("Lua profiling is %s (%d functions tracked, %d aggregated)."),
               Profiler.IsEnabled() ? TEXT("ON") : TEXT("OFF"),
               Profiler.NumTrackedFunctions(),
               Profiler.NumAggregatedFunctions());
    }

    void FUnLuaNovaConsoleCommands::Coroutines(const TArray<FString>& Args) const
    {
        auto Env = Module->GetEnv();
        if (!Env)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("lua.coroutines: no lua env is active."));
            return;
        }

        auto& Tracker = UnLuaNova::FLuaCoroutineTracker::Get();
        Tracker.Attach(Env->GetMainState());

        if (Args.Num() > 0)
        {
            const FString& Sub = Args[0];

            if (Sub.Equals(TEXT("on"), ESearchCase::IgnoreCase))
            {
                Tracker.SetEnabled(true);
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.coroutines: tracking on. It replaces coroutine.create/wrap, so it costs a table write per coroutine - turn it off when you are done."));
                return;
            }

            if (Sub.Equals(TEXT("off"), ESearchCase::IgnoreCase))
            {
                Tracker.SetEnabled(false);
                // Said out loud rather than left implicit: the originals cannot be put back,
                // they only exist inside the upvalues of the wrappers.
                UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.coroutines: recording off. The wrapped constructors stay installed (there is nothing to restore them from); only new coroutines stop being recorded."));
                return;
            }

            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("usage: lua.coroutines [on|off]"));
            return;
        }

        if (!Tracker.IsEnabled())
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.coroutines: tracking is off - run 'lua.coroutines on' first. A census taken before that is empty, not healthy."));
            return;
        }

        const TArray<UnLuaNova::FLuaCoroutineTracker::FLiveCoroutine> Live = Tracker.Snapshot();

        int32 Suspended = 0;
        for (const UnLuaNova::FLuaCoroutineTracker::FLiveCoroutine& Co : Live)
        {
            if (Co.Status == TEXT("suspended"))
                ++Suspended;
        }

        UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("lua.coroutines: %d live (%d suspended), %llu created since tracking began"),
            Live.Num(), Suspended, Tracker.NumCreated());

        for (const UnLuaNova::FLuaCoroutineTracker::FLiveCoroutine& Co : Live)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("  %-10s created at %s, parked at %s"),
                *Co.Status, *Co.CreatedAt, *Co.SuspendedAt);
        }

        if (Suspended > 0)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Warning,
                TEXT("lua.coroutines: %d suspended coroutine(s), each holding its stack and everything it captured. ")
                TEXT("A deliberately parked one is fine; one nobody will ever resume is a leak - and 'created at' is the only thing that tells you which."),
                Suspended);
        }
    }

    void FUnLuaNovaConsoleCommands::CodeGen(const TArray<FString>& Args) const
    {
        IConsoleVariable* EnableCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.codegen.enabled"));
        IConsoleVariable* ModulesCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.codegen.modules"));

        if (Args.Num() > 0)
        {
            const FString& Sub = Args[0];

            if (Sub.Equals(TEXT("on"), ESearchCase::IgnoreCase) || Sub.Equals(TEXT("off"), ESearchCase::IgnoreCase))
            {
                if (EnableCVar)
                    EnableCVar->Set(Sub.Equals(TEXT("on"), ESearchCase::IgnoreCase) ? 1 : 0, ECVF_SetByConsole);
                return;
            }

            if (Sub.Equals(TEXT("modules"), ESearchCase::IgnoreCase))
            {
                const FString List = Args.Num() > 1 ? FString::Join(TArray<FString>(Args.GetData() + 1, Args.Num() - 1), TEXT(",")) : FString();
                if (ModulesCVar)
                    ModulesCVar->Set(*List, ECVF_SetByConsole);
                return;
            }

            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("usage: lua.codegen [on|off|modules <a,b,c>]"));
            return;
        }

        // Status. "available" is the build/platform question, "enabled" is the switch, and
        // they are different: a build with no native backend must say so rather than
        // reporting the switch as if it were doing something.
        UNLUANOVA_LOGF(LogUnLuaNova, Log,
            TEXT("lua.codegen: available=%d enabled=%d compile_requests=%d modules='%s'"),
            UnLuaNova::IsNativeCodeGenAvailable() ? 1 : 0,
            EnableCVar ? EnableCVar->GetInt() : 0,
            UnLuaNova::GetNativeCodeGenRequestCount(),
            ModulesCVar ? *ModulesCVar->GetString() : TEXT(""));
        UNLUANOVA_LOGF(LogUnLuaNova, Log,
            TEXT("lua.codegen: compile_requests counts chunks handed to Luau's compiler, "
                 "not successful compiles - luau_codegen_compile() exposes no success signal."));

        if (!UnLuaNova::IsNativeCodeGenAvailable())
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Log,
                TEXT("This build cannot emit native code: it needs the Luau backend on x64 or arm64."));
        }
    }

    void FUnLuaNovaConsoleCommands::Coverage(const TArray<FString>& Args) const
    {
        if (!UnLuaNova::IsLuaCoverageAvailable())
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Warning,
                TEXT("lua.coverage: unavailable - coverage counters are a Luau compiler feature, and this build uses the plain Lua backend."));
            return;
        }

        const FString Sub = Args.Num() > 0 ? Args[0] : TEXT("status");

        if (Sub.Equals(TEXT("status"), ESearchCase::IgnoreCase))
        {
            const int32 Level = UnLuaNova::GetLuaCoverageLevel();
            UNLUANOVA_LOGF(LogUnLuaNova, Log,
                TEXT("lua.coverage: available=1 level=%d. Set luau.coverage=1 before the scripts load, then use 'lua.coverage dump'. "
                     "Chunks already loaded have no counters; the gate refuses a level-0 export."),
                Level);
            return;
        }

        if (!Sub.Equals(TEXT("dump"), ESearchCase::IgnoreCase))
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Log, TEXT("usage: lua.coverage [dump [file] | status]"));
            return;
        }

        const FString Path = Args.Num() > 1
            ? Args[1]
            : FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Coverage"), TEXT("coverage-Luau.json"));

        auto* Env = Module->GetEnv();
        if (!Env)
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("lua.coverage: no Lua environment is attached."));
            return;
        }

        FString Error;
        UnLuaNova::FCoverageSummary Summary;
        if (!UnLuaNova::DumpLuaCoverage(Env->GetMainState(), Path, Error, &Summary))
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("lua.coverage: %s"), *Error);
            return;
        }

        UNLUANOVA_LOGF(LogUnLuaNova, Log,
            TEXT("lua.coverage: wrote %s - %d functions, %lld/%lld instrumented lines hit (level=%d). "
                 "Judge it with Tools/ValidateCoverage.py."),
            *Path, Summary.Functions, static_cast<int64>(Summary.HitLines),
            static_cast<int64>(Summary.InstrumentedLines), UnLuaNova::GetLuaCoverageLevel());
    }
}

#undef LOCTEXT_NAMESPACE
