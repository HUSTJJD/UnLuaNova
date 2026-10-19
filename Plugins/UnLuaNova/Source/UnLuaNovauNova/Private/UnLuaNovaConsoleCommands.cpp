#include "UnLuaNovaConsoleCommands.h"

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
}

#undef LOCTEXT_NAMESPACE
