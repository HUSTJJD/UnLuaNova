// Copyright (c) 2026 UnLuaNova contributors
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
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"

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
        /** Non-zero when the value is a table that DAP can expand with `variables`. */
        int32 VariablesReference = 0;
        /** Expansion depth of this reference; internal to the service. */
        int32 Depth = 0;
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

        /** DAP next / stepIn / stepOut. One step stops exactly once. */
        enum class EStepMode : uint8 { None, Over, In, Out };

        static FLuaDebugService& Get();

        void Attach(lua_State* L);
        void Detach();
        bool IsAttached() const { return Attached != nullptr; }

        // Source is matched as the script spells it, e.g. "UnLuaNova.Foo" or a path.
        bool AddBreakpoint(const FString& Source, int32 Line);
        bool RemoveBreakpoint(const FString& Source, int32 Line);
        void ClearBreakpoints();
        const TArray<FBreakpoint>& GetBreakpoints() const { return Breakpoints; }

        /**
         * DAP setFunctionBreakpoints. A name matches a frame's function name exactly, or as
         * a '.' / ':' qualified suffix ("Foo" matches "Table.Foo" / "Table:Foo"). Like
         * source:line breakpoints this is matched by the line hook: with any function
         * breakpoint armed the VM runs in single step and stops on the first line of a
         * frame whose name matches. Empty names are rejected by the adapter.
         */
        void SetFunctionBreakpoints(const TArray<FString>& Names);
        const TSet<FString>& GetFunctionBreakpoints() const { return FunctionBreakpoints; }

        /**
         * Keys of package.loaded, snapshotted on the VM thread at Attach and at every
         * stop. DAP `modules` reads this copy so the adapter thread never touches
         * lua_State. Empty means no VM is attached / nothing is loaded - never a fake
         * module invented to make the response look populated.
         */
        TArray<FString> GetLoadedModules() const;

        void SetSingleStep(bool bInEnabled);

        /**
         * Start one DAP step. Over stops at the same or a shallower stack depth, In stops
         * on the next line, Out stops once the stack has unwound. A real breakpoint still
         * wins, and the mode is cleared as soon as the VM stops so stepping never becomes
         * the persistent single-step switch the console command uses.
         */
        void BeginStep(EStepMode Mode);
        EStepMode GetStepMode() const { return StepMode; }

        /**
         * Write a local variable in a paused frame (DAP setVariable). Value is a Lua
         * expression evaluated against globals, then stored with lua_setlocal. Returns
         * false with OutError when there is no paused frame, the name is not a local of
         * that frame, or the expression fails.
         */
        bool SetVariable(const FString& Name, const FString& Value, int32 FrameIndex,
            FString& OutValue, FString& OutError);

        // Captured at the last stop; only meaningful while IsPaused().
        bool IsPaused() const { return bPaused; }
        const TArray<FLuaDebugFrame>& GetStackTrace() const { return StackTrace; }
        const TArray<FLuaDebugVariable>& GetLocals() const { return Locals; }

        /** DAP: 1 is the Locals scope; anything above that is a nested table handle. */
        int32 GetLocalsReferenceId() const { return 1; }

        /**
         * Expand a nested table captured at the last stop. Returns false with OutError
         * when the VM is not paused, the handle is stale, or the expansion budget is
         * exhausted. Handles are only valid until the next stop or Detach().
         */
        bool GetChildren(int32 ReferenceId, TArray<FLuaDebugVariable>& Out, FString& OutError);

        // ── 异常过滤器 / exceptionInfo ──────────────────────────────────────
        /** "raised" and "uncaught" are the two filters the shared error handler knows. */
        void SetExceptionFilters(const TArray<FString>& Filters);
        const TSet<FString>& GetExceptionFilters() const { return ExceptionFilters; }

        /**
         * Record an uncaught Lua error from the shared ReportLuaCallError handler. With an
         * active filter this arms the line hook so the VM stops on the next line and the
         * DAP `stopped` event can report reason "exception" instead of "breakpoint".
         * The message is retained for the `exceptionInfo` request.
         */
        void RecordException(lua_State* L);
        void RecordException(const FString& Message);
        void ClearException();
        FString GetLastException() const { return LastException; }
        FString GetStopReason() const { return StopReason.IsEmpty() ? FString(TEXT("breakpoint")) : StopReason; }

        // Release a stopped VM. Safe to call from another thread.
        void Continue();

        /**
         * Ask for a stop at the next line - the DAP `pause` request.
         *
         * Only meaningful while blocked: there is no way to interrupt a VM that is
         * happily running, so this arms whatever the backend needs for the *next* line
         * to be a stop, and the stop happens shortly after. Call it from the thread that
         * owns the lua_State (AsyncTask(GameThread)) - it touches VM state.
         */
        void RequestPause();
        bool IsPauseRequested() const { return bPauseRequested; }

        /**
         * Evaluate an expression, DAP `evaluate`.
         *
         * While paused, FrameIndex selects whose locals the expression can see: the chunk
         * is compiled as
         *     return (function(<locals...>) return <expr> end)(...)
         * and called with that frame's locals, so `self.HP` and `i + 1` do what they look
         * like they do. Upvalues are not captured - they need the function object on the
         * stack and that is a per-backend spelling nobody here needs yet.
         *
         * Not paused (or FrameIndex < 0) means evaluate against globals only, which is
         * what a watch/repl expression wants when nothing is stopped.
         *
         * Returns false with OutError filled on a compile or runtime error; the Lua stack
         * is restored to where it was either way.
         */
        bool Evaluate(const FString& Expression, int32 FrameIndex, FString& OutValue, FString& OutError);

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
        bool MatchesFunctionBreakpoint(const FString& FunctionName) const;
        void SyncVmBreakpoints();
        void RefreshLoadedModules(lua_State* L);
        void CaptureStackTrace(lua_State* L);
        void CaptureLocals(lua_State* L);
        void ClearVariableRefs();

        lua_State* Attached = nullptr;
        TArray<FBreakpoint> Breakpoints;
        TSet<FString> FunctionBreakpoints;
        TArray<FLuaDebugFrame> StackTrace;
        TArray<FLuaDebugVariable> Locals;

        // Written on the VM thread, read on the DAP adapter thread.
        TArray<FString> LoadedModules;
        mutable FCriticalSection ModulesLock;

        TSet<FString> ExceptionFilters;
        FString LastException;
        FString StopReason;
        bool bExceptionStopPending = false;

        // DAP variablesReference (starting at 2) -> luaL_ref of the captured table.
        TMap<int32, int32> VariableRefs;
        TMap<int32, int32> VariableDepths;
        int32 NextVariableReference = 2;
        bool bPaused = false;
        bool bSingleStep = false;
        bool bBlocking = false;
        bool bPauseRequested = false;
        EStepMode StepMode = EStepMode::None;
        int32 StepTargetDepth = 0;
        int32 HitCount = 0;
    };
}
