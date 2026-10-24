// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

#include "LuaBackend.h"

#include "LuaVmHooks.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaSandbox.h"

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU

#include "HAL/IConsoleManager.h"

namespace
{
    // luau_codegen_create is per-VM and must not be repeated, and the VM gives no way to
    // ask whether it already happened.
    TSet<lua_State*> GCodeGenCreated;
    int32 GCompiledChunks = 0;
    bool bLoggedUnsupported = false;
}

namespace UnLuaNova
{
    lua_CompileOptions GetLuauCompileOptions()
    {
        // Read once per chunk, not once per process: these decide how a chunk is compiled,
        // so flipping one at runtime changes what the *next* require() gets, and does
        // nothing to anything already loaded. That is the same contract lua.sandbox has,
        // and it is why they are console variables rather than settings - a settings class
        // would imply you can change them and have it mean something retroactively.
        static auto OptimizeCVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("luau.optimize"),
            1,
            TEXT("Luau compiler optimization level. 0=none, 1=baseline (default, keeps line info), "
                 "2=aggressive: adds function inlining and loop unrolling. Level 2 makes calls and loops "
                 "invisible to debug.traceback and lua_getlocal, so keep 1 while debugging. "
                 "Affects chunks compiled after the change. Luau backend only."),
            ECVF_Default);

        static auto DebugLevelCVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("luau.debuglevel"),
            1,
            TEXT("Luau debug info level. 0=none (smallest bytecode, no line numbers in errors), "
                 "1=line info + function names (default), 2=also local and upvalue names. "
                 "Affects chunks compiled after the change. Luau backend only."),
            ECVF_Default);

        static auto CoverageCVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("luau.coverage"),
            0,
            TEXT("Luau coverage level. 0=off (default), 1=statement, 2=statement and expression. "
                 "Non-zero costs bytecode size and runtime. Luau backend only."),
            ECVF_Default);

        lua_CompileOptions Options = {};
        Options.optimizationLevel = FMath::Clamp(OptimizeCVar->GetInt(), 0, 2);
        Options.debugLevel = FMath::Clamp(DebugLevelCVar->GetInt(), 0, 2);
        Options.coverageLevel = FMath::Clamp(CoverageCVar->GetInt(), 0, 2);

        return Options;
    }

    void InstallSandboxThreadHardening(lua_State* L)
    {
#if UNLUANOVA_HAS_SANDBOX && UNLUANOVA_HAS_THREAD_CALLBACK
        static auto CVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.sandboxthread"),
            0,
            TEXT("Give every coroutine its own sandboxed global table as it is created. "
                 "1=on, 0=off (default). Reads fall through to the real globals; writes stay in the "
                 "coroutine, so code that sets a global inside a coroutine and reads it outside breaks. "
                 "Read once per environment, at creation. Luau backend only."),
            ECVF_Default);

        if (CVar->GetInt() == 0)
            return;

        // The owner tag only has to be a stable address FLuaVmHooks can compare against.
        static const char Owner[] = "UnLuaNova.SandboxThread";

        FLuaVmHooks::Get().AddThreadHook(L, Owner, [](lua_State* Parent, lua_State* Thread)
        {
            // A null parent is a *destruction* notification; there is nothing to harden.
            if (!Parent)
                return;

            UNLUANOVA_SANDBOXTHREAD(Thread);
        });
#endif
    }

    bool IsNativeCodeGenAvailable()
    {
#if UNLUANOVA_HAS_NATIVE_CODEGEN
        return UNLUANOVA_CODEGEN_SUPPORTED();
#else
        return false;
#endif
    }

    void MaybeCompileNativeChunk(lua_State* L, const char* ChunkName)
    {
#if UNLUANOVA_HAS_NATIVE_CODEGEN
        static auto EnableCVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.codegen"),
            0,
            TEXT("Hand loaded chunks to Luau's native compiler. 0=off (default), 1=on. "
                 "Narrow it to named modules with lua.codegen.modules. "
                 "Only chunks loaded after the change are affected. Luau backend only."),
            ECVF_Default);

        static auto ModulesCVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.codegen.modules"),
            TEXT(""),
            TEXT("Comma separated module names to native-compile; empty means every chunk. "
                 "A chunk is compiled when its name contains one of these. Requires lua.codegen=1. "
                 "Luau backend only."),
            ECVF_Default);

        if (EnableCVar->GetInt() == 0)
            return;

        if (!UNLUANOVA_CODEGEN_SUPPORTED())
        {
            if (!bLoggedUnsupported)
            {
                bLoggedUnsupported = true;
                UE_LOG(LogUnLuaNova, Warning,
                    TEXT("lua.codegen is on but this platform has no Luau native compiler (x64/arm64 only); ignoring."));
            }
            return;
        }

        const FString Filter(ModulesCVar->GetString());
        if (!Filter.IsEmpty())
        {
            TArray<FString> Names;
            Filter.ParseIntoArray(Names, TEXT(","), true);

            const FString Chunk(UTF8_TO_TCHAR(ChunkName));
            bool bMatch = false;
            for (const FString& Name : Names)
            {
                if (Chunk.Contains(Name.TrimStartAndEnd()))
                {
                    bMatch = true;
                    break;
                }
            }

            if (!bMatch)
                return;
        }

        if (!GCodeGenCreated.Contains(L))
        {
            UNLUANOVA_CODEGEN_CREATE(L);
            GCodeGenCreated.Add(L);
        }

        UNLUANOVA_CODEGEN_COMPILE(L, -1);
        ++GCompiledChunks;
#endif
    }

    int32 GetNativeCodeGenChunkCount()
    {
        return GCompiledChunks;
    }

    bool IsClassTableFreezeEnabled()
    {
#if UNLUANOVA_HAS_READONLY_TABLES
        static auto CVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.sandbox.freezeclasstables"),
            0,
            TEXT("Make each class table read-only as it is registered, so a script cannot add to or replace "
                 "class members. 0=off (default), 1=on. Opt-in because it is observable: code that monkey-patches "
                 "a class from Lua stops working. Only affects classes registered after the change. Luau backend only."),
            ECVF_Default);

        return CVar->GetInt() != 0;
#else
        return false;
#endif
    }
}

#else // plain-Lua backend

namespace UnLuaNova
{
    // Exported from the public header, so both backends have to provide it. There is no
    // thread callback and no sandbox to apply it with here, so it is a no-op - same
    // contract as RestoreTablesAfterSandbox.
    void InstallSandboxThreadHardening(lua_State* L)
    {
        (void)L;
    }

    // Plain Lua has no native compiler; these exist so the call sites stay unbranched.
    bool IsNativeCodeGenAvailable()
    {
        return false;
    }

    void MaybeCompileNativeChunk(lua_State* L, const char* ChunkName)
    {
        (void)L;
        (void)ChunkName;
    }

    int32 GetNativeCodeGenChunkCount()
    {
        return 0;
    }

    bool IsClassTableFreezeEnabled()
    {
        return false;
    }
}

#endif // UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
