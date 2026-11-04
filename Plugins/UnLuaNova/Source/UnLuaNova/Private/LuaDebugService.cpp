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

#include "LuaDebugService.h"

#include "LuaVmHooks.h"
#include "UnLuaNovaDebugBase.h"

#include "HAL/PlatformProcess.h"
#include "Logging/LogMacros.h"
#include "Misc/ScopeLock.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogLuaDebug, Log, All);

namespace UnLuaNova
{
    namespace
    {
        // Lua prefixes file-backed chunks with '@' and literal sources with '='; Luau may
        // hand back either. Strip it so a breakpoint can be spelled the way the script was
        // actually loaded rather than the way the VM names it internally.
        FString NormalizeSource(const char* Source)
        {
            if (!Source)
                return FString();

            if (*Source == '@' || *Source == '=')
                ++Source;

            return FString(ANSI_TO_TCHAR(Source));
        }

        FString SafeString(const char* In)
        {
            return In ? FString(ANSI_TO_TCHAR(In)) : FString();
        }

        bool FillFrame(lua_State* L, int32 Level, lua_Debug& Out)
        {
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            // Luau spells the source option 's' and folds the stack walk into this call.
            return lua_getinfo(L, Level, "sln", &Out) != 0;
#else
            // Plain Lua spells it 'S', and an option lua_getinfo does not know makes it
            // answer 0 - which reads as "no frame here", so the service would never stop
            // and never report a stack. Same spelling the profiler and lua.dbg use.
            return lua_getstack(L, Level, &Out) != 0 && lua_getinfo(L, "nSl", &Out) != 0;
#endif
        }
    }

    FLuaDebugService& FLuaDebugService::Get()
    {
        static FLuaDebugService Instance;
        return Instance;
    }

    void FLuaDebugService::Attach(lua_State* L)
    {
        if (!L || Attached == L)
            return;

        Detach();
        Attached = L;

#if UNLUANOVA_HAS_BREAKPOINT
        // Luau keeps the debug hook on a channel of its own, so installing it leaves the
        // profiler / watchdog safe-point callback untouched.
        UNLUANOVA_SET_DEBUG_HOOK(L, &DebugHook, nullptr);
#else
        // Plain Lua has no second channel: lua_sethook is the only hook and FLuaVmHooks
        // owns it, so register as a listener instead of claiming the slot.
        FLuaVmHooks::Get().Add(L, LUA_MASKLINE, this,
            [](lua_State* InL, lua_Debug* Ar, int) { DebugHook(InL, Ar); });
#endif

        // A service that is attached with breakpoints already registered - the console
        // lets you set one before anything is running, and a DAP client sends
        // setBreakpoints before attach - has to arm them now, not at the next AddBreakpoint.
        SyncVmBreakpoints();

        // DAP `modules` needs a first real snapshot even if the VM never stops.
        RefreshLoadedModules(L);
    }

    void FLuaDebugService::SyncVmBreakpoints()
    {
#if UNLUANOVA_HAS_BREAKPOINT
        if (!Attached)
            return;

        // Luau's debug hook runs only at a LOP_BREAK instruction or a single step
        // (lvmexecute.cpp:3338); there is no per-line callback to match source:line
        // against. Its real breakpoint, lua_breakpoint(), takes the function *on the
        // stack* and patches that one Proto - and a (source, line) pair does not identify
        // a Proto until something is executing it, so it cannot be armed from here.
        //
        // So while any breakpoint exists we run in single step and do the line matching
        // ourselves, exactly like plain Lua. That is the honest trade: breakpoints work on
        // the default backend instead of silently never firing, at the cost of a hook per
        // line while a breakpoint session is armed.
        // Function breakpoints are matched by the same line hook, so they keep it armed
        // exactly like source:line breakpoints do.
        UNLUANOVA_SINGLESTEP(Attached,
            (bSingleStep || Breakpoints.Num() > 0 || FunctionBreakpoints.Num() > 0) ? 1 : 0);
#endif
    }

    void FLuaDebugService::Detach()
    {
        if (!Attached)
            return;

#if UNLUANOVA_HAS_BREAKPOINT
        UNLUANOVA_SET_DEBUG_HOOK(Attached, nullptr, nullptr);
        UNLUANOVA_SINGLESTEP(Attached, 0);
#else
        FLuaVmHooks::Get().RemoveOwner(Attached, this);
#endif

        // Release every luaL_ref captured for expandable variables before the state goes
        // away; they are only meaningful while this VM is attached and paused.
        ClearVariableRefs();
        Attached = nullptr;
        bPaused = false;
        bSingleStep = false;
        StepMode = EStepMode::None;
        StackTrace.Empty();
        Locals.Empty();
        FunctionBreakpoints.Empty();
        {
            FScopeLock Lock(&ModulesLock);
            LoadedModules.Empty();
        }
    }

    void FLuaDebugService::SetFunctionBreakpoints(const TArray<FString>& Names)
    {
        FunctionBreakpoints.Empty();
        for (const FString& Name : Names)
        {
            if (!Name.IsEmpty())
                FunctionBreakpoints.Add(Name);
        }
        SyncVmBreakpoints();
    }

    bool FLuaDebugService::MatchesFunctionBreakpoint(const FString& FunctionName) const
    {
        if (FunctionName.IsEmpty() || FunctionBreakpoints.Num() == 0)
            return false;
        if (FunctionBreakpoints.Contains(FunctionName))
            return true;

        // "Foo" also matches "Table.Foo" and "Table:Foo", but not "NotFoo".
        for (const FString& Name : FunctionBreakpoints)
        {
            const int32 Separator = FunctionName.Len() - Name.Len() - 1;
            if (Separator >= 0 && FunctionName.EndsWith(Name))
            {
                const TCHAR Char = FunctionName[Separator];
                if (Char == TEXT('.') || Char == TEXT(':'))
                    return true;
            }
        }
        return false;
    }

    void FLuaDebugService::RefreshLoadedModules(lua_State* L)
    {
        if (!L)
            return;

        // Only called on the VM thread (Attach / OnStop). Read-only walk of package.loaded;
        // a backend without a package table yields an empty snapshot, not an error.
        const int32 Top = lua_gettop(L);
        TArray<FString> Names;

        lua_getglobal(L, "package");
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, "loaded");
            if (lua_istable(L, -1))
            {
                lua_pushnil(L);
                while (lua_next(L, -2) != 0)
                {
                    if (lua_type(L, -2) == LUA_TSTRING)
                        Names.Add(FString(ANSI_TO_TCHAR(lua_tostring(L, -2))));
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 2); // loaded (or the non-table) and package
        }
        else
        {
            lua_pop(L, 1); // package
        }
        lua_settop(L, Top);

        Names.Sort();
        FScopeLock Lock(&ModulesLock);
        LoadedModules = MoveTemp(Names);
    }

    TArray<FString> FLuaDebugService::GetLoadedModules() const
    {
        FScopeLock Lock(&ModulesLock);
        return LoadedModules;
    }

    bool FLuaDebugService::AddBreakpoint(const FString& Source, int32 Line)
    {
        const FBreakpoint Breakpoint{Source, Line};
        if (Breakpoints.Contains(Breakpoint))
            return false;

        Breakpoints.Add(Breakpoint);
        SyncVmBreakpoints();
        return true;
    }

    bool FLuaDebugService::RemoveBreakpoint(const FString& Source, int32 Line)
    {
        const bool bRemoved = Breakpoints.Remove(FBreakpoint{Source, Line}) > 0;
        if (bRemoved)
            SyncVmBreakpoints();
        return bRemoved;
    }

    void FLuaDebugService::ClearBreakpoints()
    {
        Breakpoints.Empty();
        SyncVmBreakpoints();
    }

    void FLuaDebugService::SetSingleStep(bool bInEnabled)
    {
        bSingleStep = bInEnabled;
        // continue/disconnect end any in-flight DAP step; the console `step` switch is
        // persistent by design and never carries a StepMode in the first place.
        if (!bInEnabled)
            StepMode = EStepMode::None;
        SyncVmBreakpoints();
    }

    void FLuaDebugService::BeginStep(EStepMode Mode)
    {
        if (!Attached || Mode == EStepMode::None)
            return;

        // StackTrace was captured at the stop that the step resumes from.
        StepTargetDepth = StackTrace.Num() > 0 ? StackTrace.Num() : 1;
        StepMode = Mode;
        bSingleStep = true;
        SyncVmBreakpoints();
        Continue();
    }

    void FLuaDebugService::DebugHook(lua_State* L, lua_Debug* Ar)
    {
#if UNLUANOVA_HAS_BREAKPOINT
        // Only valid inside a debug callback, which is exactly where we are.
        const bool bAtBreakpoint = UNLUANOVA_ATBREAKPOINT(L) != 0;
#else
        const bool bAtBreakpoint = false;
#endif
        Get().OnStop(L, Ar, bAtBreakpoint);
    }

    void FLuaDebugService::OnStop(lua_State* L, lua_Debug* Ar, bool bAtBreakpoint)
    {
        // Ignore nested stops - inspecting state can itself run Lua.
        if (bPaused)
            return;

        // Handles from the previous stop point into that frame's stack; drop them before
        // capturing this one, otherwise a stale reference could expand a dead table.
        ClearVariableRefs();

        lua_Debug Info;
        if (!FillFrame(L, 0, Info))
            return;

        const FString Source = NormalizeSource(Info.source);
        const int32 Line = Info.currentline;
        const FString Function = SafeString(Info.name);
        const bool bAtFunctionBreakpoint = MatchesFunctionBreakpoint(Function);

        // Trust the VM when it says we are on a breakpoint; otherwise fall back to
        // matching source:line / function name ourselves, which is the only mechanism
        // plain Lua has. bPauseRequested is the other way in: `pause` asked for the next
        // line to stop.
        if (!bAtBreakpoint && !bPauseRequested && !bSingleStep && !bExceptionStopPending
            && !bAtFunctionBreakpoint && !MatchesBreakpoint(Source, Line))
            return;

        // A pending DAP step must not stop on every line: `next` stays at or above the
        // current frame, `stepOut` waits for the stack to unwind, `stepIn` stops on the
        // very next line. A real breakpoint always outranks the step filter.
        if (StepMode != EStepMode::None)
        {
            int32 Depth = 0;
            for (int32 Level = 0; Level < 128; ++Level)
            {
                lua_Debug Frame;
                if (!FillFrame(L, Level, Frame))
                    break;
                ++Depth;
            }

            bool bStepSatisfied = true;
            if (StepMode == EStepMode::Over)
                bStepSatisfied = Depth <= StepTargetDepth;
            else if (StepMode == EStepMode::Out)
                bStepSatisfied = Depth < StepTargetDepth;

            // A function breakpoint outranks the step filter just like a line breakpoint.
            if (!bStepSatisfied && !bAtBreakpoint && !bPauseRequested && !bExceptionStopPending
                && !bAtFunctionBreakpoint && !MatchesBreakpoint(Source, Line))
                return;

            StepMode = EStepMode::None;
            bSingleStep = false;
            // Leave the per-line hook armed only if some breakpoint still needs it.
            SyncVmBreakpoints();
        }

        ++HitCount;

        // The DAP `stopped` reason is decided here, on the VM thread, where the cause is
        // still unambiguous. The adapter only polls IsPaused() and must not guess.
        if (bExceptionStopPending)
        {
            StopReason = TEXT("exception");
            bExceptionStopPending = false;
#if UNLUANOVA_HAS_BREAKPOINT
            // RecordException armed a bare single step to get us here. Re-arm only what
            // is still needed, or an exception stop would silently disable breakpoints.
            SyncVmBreakpoints();
#endif
        }
        else if (bAtBreakpoint || bAtFunctionBreakpoint || MatchesBreakpoint(Source, Line))
            StopReason = TEXT("breakpoint");
        else if (bPauseRequested)
            StopReason = TEXT("pause");
        else
            StopReason = TEXT("step");

        if (bPauseRequested)
        {
            bPauseRequested = false;
#if UNLUANOVA_HAS_BREAKPOINT
            // The request armed a single step to get here. Re-arm exactly what is still
            // needed, or answering `pause` would silently disable every breakpoint.
            SyncVmBreakpoints();
#endif
        }

        CaptureStackTrace(L);
        CaptureLocals(L);
        RefreshLoadedModules(L);
        bPaused = true;

        UE_LOG(LogLuaDebug, Display, TEXT("Stopped at %s:%d%s%s"),
            *Source, Line,
            Info.name ? TEXT(" in ") : TEXT(""),
            *SafeString(Info.name));

        if (bBlocking)
        {
            // Opt-in only: something outside this thread must call Continue(), so leaving
            // it on while running on the game thread would hang the editor.
            while (bPaused)
                FPlatformProcess::Sleep(0.005f);
        }
    }

    bool FLuaDebugService::MatchesBreakpoint(const FString& Source, int32 Line) const
    {
        for (const FBreakpoint& Breakpoint : Breakpoints)
        {
            if (Breakpoint.Line != Line)
                continue;

            // Accept a suffix match so "Foo.lua" also matches ".../Scripts/Foo.lua".
            if (Breakpoint.Source == Source || Source.EndsWith(Breakpoint.Source))
                return true;
        }

        return false;
    }

    void FLuaDebugService::CaptureStackTrace(lua_State* L)
    {
        StackTrace.Empty();

        for (int32 Level = 0; Level < 128; ++Level)
        {
            lua_Debug Info;
            if (!FillFrame(L, Level, Info))
                break;

            FLuaDebugFrame Frame;
            Frame.Source = NormalizeSource(Info.source);
            Frame.Function = SafeString(Info.name);
            Frame.Line = Info.currentline;
            Frame.Depth = Level;
            StackTrace.Add(MoveTemp(Frame));
        }
    }

    void FLuaDebugService::CaptureLocals(lua_State* L)
    {
        Locals.Empty();

        for (int32 Index = 1; Index < 64; ++Index)
        {
            const char* Name = nullptr;

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            Name = lua_getlocal(L, 0, Index);
#else
            lua_Debug Ar;
            if (!lua_getstack(L, 0, &Ar))
                break;
            Name = lua_getlocal(L, &Ar, Index);
#endif
            if (!Name)
                break;

            const int Type = lua_type(L, -1);

            FLuaDebugVariable Var;
            Var.Name = ANSI_TO_TCHAR(Name);
            Var.Type = ANSI_TO_TCHAR(lua_typename(L, Type));

            bool bValueConsumed = false;
            if (Type == LUA_TNUMBER)
                Var.Value = FString::Printf(TEXT("%g"), lua_tonumber(L, -1));
            else if (Type == LUA_TSTRING)
                Var.Value = FString::Printf(TEXT("\"%s\""), ANSI_TO_TCHAR(lua_tostring(L, -1)));
            else if (Type == LUA_TBOOLEAN)
                Var.Value = lua_toboolean(L, -1) ? TEXT("true") : TEXT("false");
            else
            {
                Var.Value = FString::Printf(TEXT("%p"), lua_topointer(L, -1));
                if (Type == LUA_TTABLE)
                {
                    // Keep the table alive in the registry so DAP can expand it with a
                    // `variables` request while the VM is still paused. luaL_ref pops the
                    // value, which is why the caller must not lua_pop it again.
                    const int32 LuaRef = luaL_ref(L, LUA_REGISTRYINDEX);
                    const int32 Handle = NextVariableReference++;
                    VariableRefs.Add(Handle, LuaRef);
                    VariableDepths.Add(Handle, 0);
                    Var.VariablesReference = Handle;
                    bValueConsumed = true;
                }
            }

            // lua_getlocal pushed the value; drop it before asking for the next one.
            if (!bValueConsumed)
                lua_pop(L, 1);
            Locals.Add(MoveTemp(Var));
        }
    }

    void FLuaDebugService::ClearVariableRefs()
    {
        if (Attached)
        {
            for (const auto& Pair : VariableRefs)
                luaL_unref(Attached, LUA_REGISTRYINDEX, Pair.Value);
        }
        VariableRefs.Reset();
        VariableDepths.Reset();
        NextVariableReference = 2;
    }

    bool FLuaDebugService::GetChildren(int32 ReferenceId, TArray<FLuaDebugVariable>& Out,
        FString& OutError)
    {
        // Bound the expansion so a deeply nested or cyclic table cannot stall the editor
        // or grow the registry without limit while an IDE expands the watch tree.
        constexpr int32 MaxExpandDepth = 4;
        constexpr int32 MaxChildren = 100;

        Out.Reset();
        if (!bPaused || !Attached)
        {
            OutError = TEXT("no paused frame; nested variables are only valid while stopped");
            return false;
        }

        const int32* LuaRef = VariableRefs.Find(ReferenceId);
        if (!LuaRef)
        {
            OutError = FString::Printf(TEXT("unknown variablesReference %d"), ReferenceId);
            return false;
        }

        const int32 Depth = VariableDepths.FindRef(ReferenceId);
        lua_State* L = Attached;
        const int Top = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, *LuaRef);
        if (lua_type(L, -1) != LUA_TTABLE)
        {
            lua_settop(L, Top);
            return true;
        }

        TSet<const void*> Chain;
        Chain.Add(lua_topointer(L, -1));

        auto AddEntry = [&](const FString& Name, int32 ValueIndex, int32 ChildDepth)
        {
            const int Type = lua_type(L, ValueIndex);
            FLuaDebugVariable Var;
            Var.Name = Name;
            Var.Type = ANSI_TO_TCHAR(lua_typename(L, Type));
            Var.Depth = ChildDepth;

            if (Type == LUA_TNUMBER)
                Var.Value = FString::Printf(TEXT("%g"), lua_tonumber(L, ValueIndex));
            else if (Type == LUA_TSTRING)
                Var.Value = FString::Printf(TEXT("\"%s\""), ANSI_TO_TCHAR(lua_tostring(L, ValueIndex)));
            else if (Type == LUA_TBOOLEAN)
                Var.Value = lua_toboolean(L, ValueIndex) ? TEXT("true") : TEXT("false");
            else
            {
                const void* Pointer = lua_topointer(L, ValueIndex);
                Var.Value = FString::Printf(TEXT("%p"), Pointer);
                if (Type == LUA_TTABLE && ChildDepth < MaxExpandDepth && !Chain.Contains(Pointer))
                {
                    lua_pushvalue(L, ValueIndex);
                    const int32 ChildRef = luaL_ref(L, LUA_REGISTRYINDEX);
                    const int32 Handle = NextVariableReference++;
                    VariableRefs.Add(Handle, ChildRef);
                    VariableDepths.Add(Handle, ChildDepth);
                    Var.VariablesReference = Handle;
                }
            }
            Out.Add(MoveTemp(Var));
        };

        lua_pushnil(L);
        int32 Count = 0;
        while (lua_next(L, -2) != 0 && Count < MaxChildren)
        {
            // Copy-free key naming: lua_tostring on a number key would rewrite the key on
            // the stack and break lua_next, so read numbers with lua_tonumber instead.
            FString Name;
            const int KeyType = lua_type(L, -2);
            if (KeyType == LUA_TSTRING)
                Name = ANSI_TO_TCHAR(lua_tostring(L, -2));
            else if (KeyType == LUA_TNUMBER)
                Name = FString::Printf(TEXT("[%g]"), lua_tonumber(L, -2));
            else
                Name = FString::Printf(TEXT("[%s]"), ANSI_TO_TCHAR(lua_typename(L, KeyType)));

            AddEntry(Name, lua_gettop(L), Depth + 1);
            ++Count;
            lua_pop(L, 1);          // drop the value, keep the key for lua_next
        }

        lua_settop(L, Top);
        return true;
    }

    void FLuaDebugService::SetExceptionFilters(const TArray<FString>& Filters)
    {
        ExceptionFilters.Reset();
        for (const FString& Filter : Filters)
        {
            if (Filter == TEXT("raised") || Filter == TEXT("uncaught"))
                ExceptionFilters.Add(Filter);
        }
        if (ExceptionFilters.Num() == 0)
            bExceptionStopPending = false;
    }

    void FLuaDebugService::RecordException(lua_State* L)
    {
        if (!L)
            return;

        const char* Message = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : nullptr;
        RecordException(Message ? FString(UTF8_TO_TCHAR(Message))
            : FString(TEXT("Lua error (non-string error object)")));

#if UNLUANOVA_HAS_BREAKPOINT
        // The shared error handler runs on the VM thread. Arm the line hook so the next
        // line becomes an exception stop; a filter that is off never touches the hook.
        if (Attached == L && bExceptionStopPending)
            UNLUANOVA_SINGLESTEP(L, 1);
#endif
    }

    void FLuaDebugService::RecordException(const FString& Message)
    {
        LastException = Message;
        if (ExceptionFilters.Num() > 0)
            bExceptionStopPending = true;
    }

    void FLuaDebugService::ClearException()
    {
        LastException.Empty();
        bExceptionStopPending = false;
        StopReason.Empty();
    }

    void FLuaDebugService::Continue()
    {
        bPaused = false;
    }

    void FLuaDebugService::RequestPause()
    {
        bPauseRequested = true;

#if UNLUANOVA_HAS_BREAKPOINT
        // Luau's debug hook fires only at breakpoints and single steps - unlike plain
        // Lua's line hook, which fires constantly - so the only way to make the *next*
        // line a stop is to arm a single step. OnStop disarms it the moment it lands.
        if (Attached)
            UNLUANOVA_SINGLESTEP(Attached, 1);
#endif
        // Plain Lua needs nothing: FLuaVmHooks already installed a LUA_MASKLINE hook,
        // so OnStop runs on the next line and sees the flag.
    }

    /**
     * Push the function that owns the given stack level, so its upvalues can be walked.
     *
     * Indexed by level rather than by a lua_Debug the caller holds: Luau addresses a frame
     * by level and plain Lua by the descriptor, and UNLUANOVA_GETINFO already hides that.
     */
    static bool PushFrameFunction(lua_State* L, int32 Level)
    {
        lua_Debug Ar;
        return UNLUANOVA_GETINFO(L, Level, "f", &Ar) != 0;
    }

    bool FLuaDebugService::Evaluate(const FString& Expression, int32 FrameIndex, FString& OutValue, FString& OutError)
    {
        OutValue.Empty();
        OutError.Empty();

        if (!Attached)
        {
            OutError = TEXT("debug service is not attached");
            return false;
        }

        lua_State* L = Attached;
        const int32 Top = lua_gettop(L);

        // Locals of the requested frame become the parameters of a wrapper function, so
        // the expression can name them. Two passes: names first (the VM pops each value
        // as it hands it over), then the values in the same order left on the stack.
        TArray<FString> Names;
        // lua_getupvalue() indices of the names added after the locals - the values have to
        // be pushed in exactly the order the names were collected, or the wrapper binds them
        // to the wrong parameters and the expression silently reads the wrong variable.
        TArray<int32> UpvalueIndices;
        int32 NumLocals = 0;

        if (bPaused && FrameIndex >= 0)
        {
            const int32 Level = FMath::Min(FrameIndex, 127);

            for (int32 Index = 1; Index < 64; ++Index)
            {
                const char* Name = nullptr;
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
                Name = lua_getlocal(L, Level, Index);
#else
                lua_Debug Ar;
                if (!lua_getstack(L, Level, &Ar))
                    break;
                Name = lua_getlocal(L, &Ar, Index);
#endif
                if (!Name)
                    break;

                Names.Add(ANSI_TO_TCHAR(Name));
                lua_pop(L, 1); // the value it pushed; the name is what we wanted
            }
            NumLocals = Names.Num();

            // ---- upvalues ---------------------------------------------------
            // Locals alone are not enough to evaluate what someone actually types into a
            // watch window. Lua keeps most of its state in upvalues: anything closed over,
            // and - through the way HotReload rebinds a module - most module-level `local`s
            // too. Without this a variable that is plainly in scope answers "attempt to
            // index a nil value", which reads as "it is gone" and sends you looking in the
            // wrong place.
            if (PushFrameFunction(L, Level))
            {
                const int32 FunctionIndex = lua_gettop(L);

                for (int32 Index = 1; Index < 64; ++Index)
                {
                    const char* Name = lua_getupvalue(L, FunctionIndex, Index);
                    if (!Name)
                        break;

                    const FString UpvalueName = ANSI_TO_TCHAR(Name);
                    lua_pop(L, 1); // the value; the name is what we wanted

                    // _ENV is every function's first upvalue - binding it as a parameter
                    // would shadow the globals an expression legitimately names. A local of
                    // the same name already won: it is the nearer binding, so it wins here
                    // too rather than making the generated wrapper fail to compile.
                    if (UpvalueName == TEXT("_ENV") || Names.Contains(UpvalueName))
                        continue;

                    Names.Add(UpvalueName);
                    UpvalueIndices.Add(Index);
                }

                lua_pop(L, 1); // the function
            }

            for (int32 Index = 0; Index < NumLocals; ++Index)
            {
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
                lua_getlocal(L, Level, Index + 1);
#else
                lua_Debug Ar;
                if (!lua_getstack(L, Level, &Ar))
                    break;
                lua_getlocal(L, &Ar, Index + 1);
#endif
            }

            if (UpvalueIndices.Num() > 0 && PushFrameFunction(L, Level))
            {
                const int32 FunctionIndex = lua_gettop(L);
                for (const int32 Index : UpvalueIndices)
                {
                    lua_getupvalue(L, FunctionIndex, Index);
                }
                // Drop the function and slide the values down, so what is left above Top is
                // exactly one value per name, in order.
                lua_remove(L, FunctionIndex);
            }
        }

        FString Source = TEXT("return (function(");
        for (int32 Index = 0; Index < Names.Num(); ++Index)
        {
            if (Index > 0)
                Source += TEXT(", ");
            Source += Names[Index];
        }
        Source += TEXT(") return ");
        Source += Expression;
        Source += TEXT(" end)(...)");

        const FTCHARToUTF8 Bytes(*Source);
        if (UNLUANOVA_LOAD(L, "=evaluate", Bytes.Get(), Bytes.Length()) != LUA_OK)
        {
            OutError = FString::Printf(TEXT("compile error: %s"), ANSI_TO_TCHAR(lua_tostring(L, -1)));
            lua_settop(L, Top);
            return false;
        }

        // [args...][chunk] -> [chunk][args...]
        lua_insert(L, Top + 1);

        if (lua_pcall(L, Names.Num(), 1, 0) != LUA_OK)
        {
            OutError = FString::Printf(TEXT("runtime error: %s"), ANSI_TO_TCHAR(lua_tostring(L, -1)));
            lua_settop(L, Top);
            return false;
        }

        FLuaDebugValue Value;
        Value.Build(L, -1, 1);
        OutValue = Value.ToString();

        lua_settop(L, Top);
        return true;
    }

    bool FLuaDebugService::SetVariable(const FString& Name, const FString& Value,
        int32 FrameIndex, FString& OutValue, FString& OutError)
    {
        OutValue.Empty();
        OutError.Empty();

        if (!Attached || !bPaused)
        {
            OutError = TEXT("setVariable requires a paused frame");
            return false;
        }

        lua_State* L = Attached;
        const int32 Top = lua_gettop(L);
        const int32 Level = FMath::Max(0, FMath::Min(FrameIndex, 127));
#if UNLUANOVA_LUA_BACKEND != UNLUANOVA_BACKEND_LUAU
        lua_Debug Ar;
#endif

        int32 Slot = INDEX_NONE;
        for (int32 Index = 1; Index < 64; ++Index)
        {
            const char* LocalName = nullptr;
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            LocalName = lua_getlocal(L, Level, Index);
#else
            if (!lua_getstack(L, Level, &Ar))
                break;
            LocalName = lua_getlocal(L, &Ar, Index);
#endif
            if (!LocalName)
                break;
            const bool bMatch = Name == ANSI_TO_TCHAR(LocalName);
            lua_pop(L, 1); // lua_getlocal pushes the value along with the name
            if (bMatch)
            {
                Slot = Index;
                break;
            }
        }

        if (Slot == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("'%s' is not a local of frame %d"), *Name, Level);
            lua_settop(L, Top);
            return false;
        }

        // The DAP value is a Lua expression (a debugger sends "1", "'text'", "true"...).
        const FString Source = FString::Printf(TEXT("return (%s)"), *Value);
        const FTCHARToUTF8 Bytes(*Source);
        if (UNLUANOVA_LOAD(L, "=setVariable", Bytes.Get(), Bytes.Length()) != LUA_OK)
        {
            OutError = FString::Printf(TEXT("compile error: %s"), ANSI_TO_TCHAR(lua_tostring(L, -1)));
            lua_settop(L, Top);
            return false;
        }
        if (lua_pcall(L, 0, 1, 0) != LUA_OK)
        {
            OutError = FString::Printf(TEXT("runtime error: %s"), ANSI_TO_TCHAR(lua_tostring(L, -1)));
            lua_settop(L, Top);
            return false;
        }

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
        const char* Changed = lua_setlocal(L, Level, Slot);
#else
        if (!lua_getstack(L, Level, &Ar))
        {
            OutError = TEXT("frame is no longer available");
            lua_settop(L, Top);
            return false;
        }
        const char* Changed = lua_setlocal(L, &Ar, Slot);
#endif
        if (!Changed)
        {
            OutError = FString::Printf(TEXT("could not assign '%s'"), *Name);
            lua_settop(L, Top);
            return false;
        }

#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
        lua_getlocal(L, Level, Slot);
#else
        if (lua_getstack(L, Level, &Ar))
            lua_getlocal(L, &Ar, Slot);
        else
            lua_pushnil(L);
#endif
        FLuaDebugValue NewValue;
        NewValue.Build(L, -1, 1);
        OutValue = NewValue.ToString();
        lua_settop(L, Top);
        return true;
    }
}
