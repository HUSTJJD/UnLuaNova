#include "UnLuaNovaConsoleCommands.h"
#include "LuaDebugService.h"
#include "LuaEnv.h"
#include "LuaMemoryStats.h"
#include "LuaProfiler.h"

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
              *LOCTEXT("CommandText_CollectGarbage", "Force collect garbage in lua env.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::CollectGarbage)
          ),
          MemStatsCommand(
              TEXT("lua.memstats"),
              *LOCTEXT("CommandText_MemStats", "Lua heap usage. Args: on | off | reset, or none for a report.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::MemStats)
          ),
          ProfilerCommand(
              TEXT("lua.profiler"),
              *LOCTEXT("CommandText_Profiler", "Lua CPU profiling into Unreal Insights. Args: on | off, or none for status.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::Profiler)
          ),
          DebugCommand(
              TEXT("lua.dbg"),
              *LOCTEXT("CommandText_Debug", "Headless lua debugger. Args: bp <src> <line> | step | continue | stack | locals | clear | detach, or none for status.").ToString(),
              FConsoleCommandWithArgsDelegate::CreateRaw(this, &FUnLuaNovaConsoleCommands::Debug)
          ),
          Module(InModule)
    {
    }

    void FUnLuaNovaConsoleCommands::Do(const TArray<FString>& Args) const
    {
        if (Args.Num() == 0)
        {
            UE_LOG(LogUnLuaNova, Log, TEXT("usage: lua.do <your code>"));
            return;
        }

        auto Env = Module->GetEnv();
        if (!Env)
        {
            UE_LOG(LogUnLuaNova, Warning, TEXT("no available lua env found to run code."));
            return;
        }

        const auto Chunk = FString::Join(Args, TEXT(" "));
        Env->DoString(Chunk);
    }

    void FUnLuaNovaConsoleCommands::Debug(const TArray<FString>& Args) const
    {
        auto Env = Module->GetEnv();
        if (!Env)
        {
            UE_LOG(LogUnLuaNova, Warning, TEXT("no available lua env found to debug."));
            return;
        }

        // Attaching is idempotent, so every invocation makes sure the hook is installed
        // before doing anything else - the service only costs anything once armed.
        auto& DebugService = FLuaDebugService::Get();
        DebugService.Attach(Env->GetMainState());

        if (Args.Num() == 0)
        {
            UE_LOG(LogUnLuaNova, Log, TEXT("lua.dbg: attached=%d paused=%d hits=%d breakpoints=%d"),
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
            UE_LOG(LogUnLuaNova, Log, TEXT("lua.dbg: breakpoint toggled at %s:%d"), *Args[1], Line);
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
                UE_LOG(LogUnLuaNova, Log, TEXT("#%d %s:%d %s"),
                    Frame.Depth, *Frame.Source, Frame.Line, *Frame.Function);
            return;
        }

        if (Sub == TEXT("locals"))
        {
            for (const FLuaDebugVariable& Var : DebugService.GetLocals())
                UE_LOG(LogUnLuaNova, Log, TEXT("%s (%s) = %s"),
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

        UE_LOG(LogUnLuaNova, Log, TEXT("usage: lua.dbg [bp <src> <line> | step | continue | stack | locals | clear | detach]"));
    }

    void FUnLuaNovaConsoleCommands::DoFile(const TArray<FString>& Args) const
    {
        if (Args.Num() != 1)
        {
            UE_LOG(LogUnLuaNova, Log, TEXT("usage: lua.dofile <lua.module.path>"));
            return;
        }

        auto Env = Module->GetEnv();
        if (!Env)
        {
            UE_LOG(LogUnLuaNova, Warning, TEXT("no available lua env found to run file."));
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
            UE_LOG(LogUnLuaNova, Warning, TEXT("no available lua env found to collect garbage."));
            return;
        }

        Env->GC();
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

                UE_LOG(LogUnLuaNova, Log, TEXT("Lua memory tracking enabled."));
                return;
            }
            if (Arg.Equals(TEXT("off"), ESearchCase::IgnoreCase))
            {
                Stats.bEnabled.store(false, std::memory_order_relaxed);
                Stats.Detach();
                UE_LOG(LogUnLuaNova, Log, TEXT("Lua memory tracking disabled."));
                return;
            }
            if (Arg.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
            {
                Stats.Reset();
                UE_LOG(LogUnLuaNova, Log, TEXT("Lua memory counters reset."));
                return;
            }

            UE_LOG(LogUnLuaNova, Log, TEXT("usage: lua.memstats [on|off|reset]"));
            return;
        }

        if (!Stats.bEnabled.load(std::memory_order_relaxed))
        {
            UE_LOG(LogUnLuaNova, Log, TEXT("tracking is off - run 'lua.memstats on' first, the numbers below are all zero."));
        }

        UE_LOG(LogUnLuaNova, Log, TEXT("%s"), *Stats.BuildReport());

        // Categorized breakdown is Luau-only; on other backends it reports that and stops.
        // Empty until 'lua.memstats on' has been run, since that is what installs the hooks.
        UE_LOG(LogUnLuaNova, Log, TEXT("%s"), *Stats.BuildCategoryReport());
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
                UE_LOG(LogUnLuaNova, Log, TEXT("Lua profiling on - capture with Unreal Insights. This takes over the VM hook, so DeadLoopCheck is inactive while it runs."));
                return;
            }
            if (Arg.Equals(TEXT("off"), ESearchCase::IgnoreCase))
            {
                Profiler.SetEnabled(false);
                UE_LOG(LogUnLuaNova, Log, TEXT("Lua profiling off."));
                return;
            }

            UE_LOG(LogUnLuaNova, Log, TEXT("usage: lua.profiler [on|off]"));
            return;
        }

        UE_LOG(LogUnLuaNova, Log, TEXT("Lua profiling is %s (%d functions tracked)."),
               Profiler.IsEnabled() ? TEXT("ON") : TEXT("OFF"),
               Profiler.NumTrackedFunctions());
    }
}

#undef LOCTEXT_NAMESPACE
