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
        return true;
    }

    bool FLuaDebugService::RemoveBreakpoint(const FString& Source, int32 Line)
    {
        return Breakpoints.Remove(FBreakpoint{Source, Line}) > 0;
    }

    void FLuaDebugService::ClearBreakpoints()
    {
        Breakpoints.Empty();
    }

    void FLuaDebugService::SetSingleStep(bool bInEnabled)
    {
        bSingleStep = bInEnabled;

#if UNLUANOVA_HAS_BREAKPOINT
        if (Attached)
            UNLUANOVA_SINGLESTEP(Attached, bInEnabled ? 1 : 0);
#endif
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
        if (!bAtBreakpoint && !bSingleStep && !MatchesBreakpoint(Source, Line))
            return;

        ++HitCount;
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
}
