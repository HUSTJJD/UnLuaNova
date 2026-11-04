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

#pragma once

#include "CoreMinimal.h"
#include "LuaBackend.h"

namespace UnLuaNova
{
    /** Totals recomputed by Tools/ValidateCoverage.py - never take the JSON's word for them. */
    struct UNLUANOVA_API FCoverageSummary
    {
        int32 Functions = 0;
        int32 CoveredFunctions = 0;
        int64 InstrumentedLines = 0;
        int64 HitLines = 0;
    };

    /** Whether this build can export coverage counters at all (Luau backend only). */
    UNLUANOVA_API bool IsLuaCoverageAvailable();

    /** Compiler coverage level currently in effect (0 means chunks were compiled without counters). */
    UNLUANOVA_API int32 GetLuaCoverageLevel();

    /**
     * Export per-source, per-line hit counters from every reachable Lua function in L.
     *
     * Coverage is a *compiler* option (luau.coverage): only chunks loaded after it was set
     * carry counters, and the counters live on the Proto. Nothing is reset here - the
     * export is a snapshot, so successive dumps are cumulative and the gate can merge runs
     * by taking the maximum hit count per line.
     *
     * The JSON is deliberately written without a JSON module dependency: the runtime module
     * only needs Core, and a hand-built document keeps the format in one place
     * (Tools/ValidateCoverage.py is the schema authority).
     *
     * Returns false with a diagnostic on a non-Luau backend, a missing state, an empty VM,
     * or a write failure. An empty export is a failure, not a passing zero-coverage report.
     */
    UNLUANOVA_API bool DumpLuaCoverage(lua_State* L, const FString& FilePath, FString& OutError,
        FCoverageSummary* OutSummary = nullptr);
}
