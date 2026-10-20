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

#include "LuaDebugService.h"

#include "LuaVmHooks.h"
#include "UnLuaNovaDebugBase.h"

#include "HAL/PlatformProcess.h"
#include "Logging/LogMacros.h"
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
            [](lua_State* InL, lua_Debug* Ar) { DebugHook(InL, Ar); });
#endif

        // A service that is attached with breakpoints already registered - the console
        // lets you set one before anything is running, and a DAP client sends
        // setBreakpoints before attach - has to arm them now, not at the next AddBreakpoint.
        SyncVmBreakpoints();
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
        UNLUANOVA_SINGLESTEP(Attached, (bSingleStep || Breakpoints.Num() > 0) ? 1 : 0);
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

        Attached = nullptr;
        bPaused = false;
        bSingleStep = false;
        StackTrace.Empty();
        Locals.Empty();
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
        SyncVmBreakpoints();
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

        lua_Debug Info;
        if (!FillFrame(L, 0, Info))
            return;

        const FString Source = NormalizeSource(Info.source);
        const int32 Line = Info.currentline;

        // Trust the VM when it says we are on a breakpoint; otherwise fall back to
        // matching source:line ourselves, which is the only mechanism plain Lua has.
        // bPauseRequested is the fourth way in: `pause` asked for the next line to stop.
        if (!bAtBreakpoint && !bPauseRequested && !bSingleStep && !MatchesBreakpoint(Source, Line))
            return;

        ++HitCount;

        if (bPauseRequested)
        {
            bPauseRequested = false;
#if UNLUANOVA_HAS_BREAKPOINT
            // The request armed a single step to get here. Disarm it - but only when
            // nothing else needs the per-line hook, or answering `pause` would silently
            // disable every breakpoint the user has set.
            if (!bSingleStep && Breakpoints.Num() == 0)
                UNLUANOVA_SINGLESTEP(L, 0);
#endif
        }

        CaptureStackTrace(L);
        CaptureLocals(L);
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

            if (Type == LUA_TNUMBER)
                Var.Value = FString::Printf(TEXT("%g"), lua_tonumber(L, -1));
            else if (Type == LUA_TSTRING)
                Var.Value = FString::Printf(TEXT("\"%s\""), ANSI_TO_TCHAR(lua_tostring(L, -1)));
            else if (Type == LUA_TBOOLEAN)
                Var.Value = lua_toboolean(L, -1) ? TEXT("true") : TEXT("false");
            else
                Var.Value = FString::Printf(TEXT("%p"), lua_topointer(L, -1));

            // lua_getlocal pushed the value; drop it before asking for the next one.
            lua_pop(L, 1);
            Locals.Add(MoveTemp(Var));
        }
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
}
