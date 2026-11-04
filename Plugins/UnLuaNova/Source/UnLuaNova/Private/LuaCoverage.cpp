// Copyright (c) 2026 UnLuaNova contributors
//
// Licensed under the MIT License (the "License");
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under
// the License.

#include "LuaCoverage.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnLuaNova
{
    namespace
    {
        struct FCoverageLine
        {
            int32 Line = 0;
            int32 Hits = 0;
        };

        struct FCoverageFunction
        {
            FString Name;
            int32 LineDefined = 0;
            int32 Depth = 0;
            TArray<FCoverageLine> Lines;
        };

        struct FCoverageContext
        {
            FString CurrentSource;
            TMap<FString, TArray<FCoverageFunction>> Sources;
            FCoverageSummary Summary;
        };

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
        void CoverageCallback(void* Context, const char* Function, int LineDefined, int Depth,
            const int* Hits, size_t Size)
        {
            FCoverageContext& Ctx = *static_cast<FCoverageContext*>(Context);

            FCoverageFunction Fn;
            Fn.Name = Function ? UTF8_TO_TCHAR(Function) : TEXT("<anonymous>");
            Fn.LineDefined = LineDefined;
            Fn.Depth = Depth;

            int32 TotalLines = 0;
            int32 HitLines = 0;
            for (size_t Index = 0; Index < Size; ++Index)
            {
                // -1 is Luau's "no LOP_COVERAGE instruction on this line" marker. Keeping
                // only real counters also keeps the exported denominator honest: it counts
                // instrumented lines, not every line number the debug info happens to span.
                if (Hits[Index] < 0)
                    continue;

                FCoverageLine Line;
                Line.Line = static_cast<int32>(Index);
                Line.Hits = Hits[Index];
                Fn.Lines.Add(Line);
                ++TotalLines;
                if (Hits[Index] > 0)
                    ++HitLines;
            }

            ++Ctx.Summary.Functions;
            if (HitLines > 0)
                ++Ctx.Summary.CoveredFunctions;
            Ctx.Summary.InstrumentedLines += TotalLines;
            Ctx.Summary.HitLines += HitLines;

            Ctx.Sources.FindOrAdd(Ctx.CurrentSource).Add(MoveTemp(Fn));
        }

        void WalkTable(lua_State* L, int TableIndex, const FString& Prefix, int32 Depth,
            TSet<const void*>& Visited, FCoverageContext& Ctx)
        {
            if (Depth > 8 || !lua_checkstack(L, 8))
                return;

            const void* Identity = lua_topointer(L, TableIndex);
            if (!Identity || Visited.Contains(Identity))
                return;
            Visited.Add(Identity);

            const int Top = lua_gettop(L);
            const int AbsoluteIndex = TableIndex > 0 ? TableIndex : Top + TableIndex + 1;

            lua_pushnil(L);
            int32 UnnamedKeys = 0;
            while (lua_next(L, AbsoluteIndex) != 0)
            {
                // Stack: [key][value]. Never call lua_tostring on a non-string key: it
                // converts the key in place, which corrupts lua_next's iteration.
                FString Key;
                if (lua_type(L, -2) == LUA_TSTRING)
                    Key = UTF8_TO_TCHAR(lua_tostring(L, -2));
                else
                    Key = FString::Printf(TEXT("[%d]"), ++UnnamedKeys);

                const FString Path = Prefix.IsEmpty() ? Key : Prefix + TEXT(".") + Key;
                const int ValueType = lua_type(L, -1);

                if (ValueType == LUA_TFUNCTION)
                {
                    // lua_getcoverage asserts on C closures; those have no Proto and no
                    // counters, and the binding's C functions are not script coverage.
                    if (!lua_iscfunction(L, -1))
                    {
                        Ctx.CurrentSource = Path;
                        lua_getcoverage(L, -1, &Ctx, &CoverageCallback);
                    }
                }
                else if (ValueType == LUA_TTABLE)
                {
                    lua_pushvalue(L, -1);
                    WalkTable(L, lua_gettop(L), Path, Depth + 1, Visited, Ctx);
                    lua_pop(L, 1);
                }

                lua_pop(L, 1); // value; key stays for the next lua_next
            }
            lua_settop(L, Top);
        }
#endif

        FString JsonEscape(const FString& Value)
        {
            FString Out;
            Out.Reserve(Value.Len() + 8);
            for (int32 Index = 0; Index < Value.Len(); ++Index)
            {
                const TCHAR Ch = Value[Index];
                switch (Ch)
                {
                case TEXT('\\'): Out += TEXT("\\\\"); break;
                case TEXT('"'): Out += TEXT("\\\""); break;
                case TEXT('\n'): Out += TEXT("\\n"); break;
                case TEXT('\r'): Out += TEXT("\\r"); break;
                case TEXT('\t'): Out += TEXT("\\t"); break;
                default:
                    if (Ch < 0x20)
                        Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Ch));
                    else
                        Out.AppendChar(Ch);
                    break;
                }
            }
            return Out;
        }

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
        FString BuildJson(const FCoverageContext& Ctx, int32 CoverageLevel)
        {
            FString Json;
            Json += TEXT("{\n");
            Json += TEXT("  \"schema_version\": 1,\n");
            Json += FString::Printf(TEXT("  \"backend\": \"%s\",\n"), TEXT("Luau"));
            Json += FString::Printf(TEXT("  \"coverage_level\": %d,\n"), CoverageLevel);
            Json += FString::Printf(TEXT("  \"generated_utc\": \"%s\",\n"),
                *JsonEscape(FDateTime::UtcNow().ToIso8601()));
            // Declared, not silently applied: ValidateCoverage.py refuses a report that
            // does not name its exclusions, so a gate can never quietly shrink its own
            // denominator. These two prefixes are not script modules in this VM.
            Json += TEXT("  \"exclusions\": [\"IntelliSense\", \"<generated>\"],\n");
            Json += TEXT("  \"sources\": [\n");

            bool bFirstSource = true;
            for (const auto& Pair : Ctx.Sources)
            {
                if (!bFirstSource)
                    Json += TEXT(",\n");
                bFirstSource = false;

                Json += TEXT("    {\n");
                Json += FString::Printf(TEXT("      \"source\": \"%s\",\n"), *JsonEscape(Pair.Key));
                Json += TEXT("      \"functions\": [\n");
                bool bFirstFunction = true;
                for (const FCoverageFunction& Fn : Pair.Value)
                {
                    if (!bFirstFunction)
                        Json += TEXT(",\n");
                    bFirstFunction = false;

                    Json += TEXT("        {\n");
                    Json += FString::Printf(TEXT("          \"name\": \"%s\",\n"), *JsonEscape(Fn.Name));
                    Json += FString::Printf(TEXT("          \"linedefined\": %d,\n"), Fn.LineDefined);
                    Json += FString::Printf(TEXT("          \"depth\": %d,\n"), Fn.Depth);
                    Json += TEXT("          \"lines\": [");
                    for (int32 Index = 0; Index < Fn.Lines.Num(); ++Index)
                    {
                        if (Index > 0)
                            Json += TEXT(", ");
                        Json += FString::Printf(TEXT("{\"line\": %d, \"hits\": %d}"),
                            Fn.Lines[Index].Line, Fn.Lines[Index].Hits);
                    }
                    Json += TEXT("]\n");
                    Json += TEXT("        }");
                }
                Json += TEXT("\n      ]\n");
                Json += TEXT("    }");
            }

            Json += TEXT("\n  ],\n");
            Json += TEXT("  \"totals\": {\n");
            Json += FString::Printf(TEXT("    \"functions\": %d,\n"), Ctx.Summary.Functions);
            Json += FString::Printf(TEXT("    \"covered_functions\": %d,\n"), Ctx.Summary.CoveredFunctions);
            Json += FString::Printf(TEXT("    \"instrumented_lines\": %lld,\n"),
                static_cast<int64>(Ctx.Summary.InstrumentedLines));
            Json += FString::Printf(TEXT("    \"hit_lines\": %lld\n"),
                static_cast<int64>(Ctx.Summary.HitLines));
            Json += TEXT("  }\n");
            Json += TEXT("}\n");
            return Json;
        }
#endif
    }

    bool IsLuaCoverageAvailable()
    {
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
        return true;
#else
        return false;
#endif
    }

    int32 GetLuaCoverageLevel()
    {
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
        const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("luau.coverage"));
        return CVar ? FMath::Clamp(CVar->GetInt(), 0, 2) : 0;
#else
        return 0;
#endif
    }

    bool DumpLuaCoverage(lua_State* L, const FString& FilePath, FString& OutError,
        FCoverageSummary* OutSummary)
    {
        if (!IsLuaCoverageAvailable())
        {
            OutError = TEXT("Coverage export requires the Luau backend; plain Lua has no lua_getcoverage.");
            return false;
        }
        if (!L)
        {
            OutError = TEXT("No Lua state is attached.");
            return false;
        }

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
        FCoverageContext Context;
        TSet<const void*> Visited;

        const int Top = lua_gettop(L);

        // _G holds globals and (through nested class/module tables) most script modules.
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        WalkTable(L, lua_gettop(L), TEXT("_G"), 0, Visited, Context);
        lua_pop(L, 1);

        // package.loaded is optional: the host loader does not have to install an upstream
        // Require table. If it exists, its keys give the most useful per-module source names.
        lua_getglobal(L, "package");
        if (lua_type(L, -1) == LUA_TTABLE)
        {
            lua_getfield(L, -1, "loaded");
            if (lua_type(L, -1) == LUA_TTABLE)
                WalkTable(L, lua_gettop(L), TEXT("package.loaded"), 0, Visited, Context);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        lua_settop(L, Top);

        if (Context.Summary.Functions == 0)
        {
            OutError = TEXT("No Lua functions with coverage data are reachable; refusing to write an empty report.");
            return false;
        }

        const FString Json = BuildJson(Context, GetLuaCoverageLevel());
        const FString Directory = FPaths::GetPath(FilePath);
        if (!Directory.IsEmpty())
            IFileManager::Get().MakeDirectory(*Directory, true);

        if (!FFileHelper::SaveStringToFile(Json, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutError = FString::Printf(TEXT("Could not write coverage report to %s"), *FilePath);
            return false;
        }

        if (OutSummary)
            *OutSummary = Context.Summary;
        return true;
#else
        OutError = TEXT("Coverage export is not compiled into this backend.");
        return false;
#endif
    }
}
