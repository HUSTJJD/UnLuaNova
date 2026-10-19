#include "UnLuaNovaConsoleCommands.h"
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
                UE_LOG(LogUnLuaNova, Log, TEXT("Lua memory tracking enabled."));
                return;
            }
            if (Arg.Equals(TEXT("off"), ESearchCase::IgnoreCase))
            {
                Stats.bEnabled.store(false, std::memory_order_relaxed);
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
