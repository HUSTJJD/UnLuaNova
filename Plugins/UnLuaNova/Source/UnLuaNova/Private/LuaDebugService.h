// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

#pragma once

#include "LuaBackend.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

namespace UnLuaNova
{
    struct FLuaDebugFrame
    {
        FString Source;
        FString Function;
        int32 Line = 0;
        int32 Depth = 0;
    };

    struct FLuaDebugVariable
    {
        FString Name;
        FString Type;
        FString Value;
    };

    // Headless debug service: no UI of its own, everything is driven through this API so
    // an external front end (a DAP adapter, a console command, an automation test) can
    // attach, set breakpoints and read the captured state.
    //
    // Deliberately does NOT own a socket or a wire protocol - that belongs in the DAP
    // layer, which should talk to this class. Keeping the split means the VM-facing
    // behaviour can be verified on its own.
    class UNLUANOVA_API FLuaDebugService
    {
    public:
        struct FBreakpoint
        {
            FString Source;
            int32 Line = 0;

            bool operator==(const FBreakpoint& Other) const
            {
                return Line == Other.Line && Source == Other.Source;
            }
        };

        static FLuaDebugService& Get();

        void Attach(lua_State* L);
        void Detach();
        bool IsAttached() const { return Attached != nullptr; }

        // Source is matched as the script spells it, e.g. "UnLuaNova.Foo" or a path.
        bool AddBreakpoint(const FString& Source, int32 Line);
        bool RemoveBreakpoint(const FString& Source, int32 Line);
        void ClearBreakpoints();
        const TArray<FBreakpoint>& GetBreakpoints() const { return Breakpoints; }

        void SetSingleStep(bool bInEnabled);

        // Captured at the last stop; only meaningful while IsPaused().
        bool IsPaused() const { return bPaused; }
        const TArray<FLuaDebugFrame>& GetStackTrace() const { return StackTrace; }
        const TArray<FLuaDebugVariable>& GetLocals() const { return Locals; }

        // Release a stopped VM. Safe to call from another thread.
        void Continue();

        // Whether a stop should spin until Continue() is called. Off by default: blocking
        // the game thread while waiting for an external front end would hang the editor,
        // so this is opt-in for out-of-process tooling only.
        void SetBlocking(bool bInBlocking) { bBlocking = bInBlocking; }
        bool IsBlocking() const { return bBlocking; }

        int32 GetHitCount() const { return HitCount; }

    private:
        static void DebugHook(lua_State* L, lua_Debug* Ar);

        void OnStop(lua_State* L, lua_Debug* Ar, bool bAtBreakpoint);
        bool MatchesBreakpoint(const FString& Source, int32 Line) const;
        void CaptureStackTrace(lua_State* L);
        void CaptureLocals(lua_State* L);

        lua_State* Attached = nullptr;
        TArray<FBreakpoint> Breakpoints;
        TArray<FLuaDebugFrame> StackTrace;
        TArray<FLuaDebugVariable> Locals;
        bool bPaused = false;
        bool bSingleStep = false;
        bool bBlocking = false;
        int32 HitCount = 0;
    };
}
