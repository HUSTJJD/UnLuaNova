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

// The debug service - breakpoints, stack, locals, evaluate - had no spec of its own.
// Observability.spec.cpp covers breakpoint *bookkeeping* (add/remove/clear) and that a
// single step produces a hit, which is not the same as "a breakpoint stops where you put
// it": nothing there ever asserted the line that was stopped on, or read a single local.
//
// That blind spot hid a real defect. Luau's debug hook runs only at a LOP_BREAK
// instruction or a single step, and AddBreakpoint() never called lua_breakpoint(), so on
// the default backend `lua.dbg bp <src> <line>` and DAP setBreakpoints never hit at all -
// while the comparison table claimed "real breakpoints ✅" for Luau. The service now arms
// a single step while any breakpoint exists and matches source:line itself.
//
// Two deliberate limits of this file, both explained where they matter below:
//   * The paused-frame path of Evaluate() (binding a frame's locals and upvalues as
//     parameters) cannot be driven from here: a stop is only "live" while the chunk is
//     still on the stack, and by the time DoString() returns the frames are gone. That
//     path is exercised by a blocking DAP session, not by this spec.
//   * Every case runs on both backends, which is the point: the same script must stop,
//     report and evaluate identically on Lua and on Luau.

#include "UnLuaNovaBase.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#include "LuaBackend.h"
#include "LuaDebugService.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDebugServiceSpec, "UnLuaNova.API.DebugService", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;

    // HitCount is a process-wide counter on a singleton and there is no reset, so every
    // case compares a delta instead of an absolute - otherwise the second case to stop
    // would inherit the first one's count and "stopped once" would be untestable.
    int32 HitsBefore = 0;
END_DEFINE_SPEC(FDebugServiceSpec)

void FDebugServiceSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
        HitsBefore = UnLuaNova::FLuaDebugService::Get().GetHitCount();
    });

    AfterEach([this]
    {
        // Leave the singleton the way we found it: it is shared with
        // UnLuaNova.API.Observability and with any editor session that has lua.dbg open,
        // and a breakpoint left armed keeps the VM stepping a hook per instruction.
        auto& Debug = UnLuaNova::FLuaDebugService::Get();
        Debug.Continue();
        Debug.SetSingleStep(false);
        Debug.ClearBreakpoints();
        Debug.Detach();
        Env.Reset();
        L = nullptr;
    });

    Describe(TEXT("断点"), [this]()
    {
        It(TEXT("停在声明的那一行，且只停一次"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();
            Debug.ClearBreakpoints();
            Debug.Attach(L);

            const FString ChunkName = TEXT("Tests.Specs.DebugService.Breakpoint");
            TEST_TRUE(Debug.AddBreakpoint(ChunkName, 3));

            // Line 3 is the third line. A stop that lands anywhere else - or on every
            // line - is invisible to "add/remove/clear" style assertions, which is why
            // this counts stops instead of trusting them.
            Env->DoString(TEXT("local A = 1\nlocal B = 2\nlocal C = A + B\nreturn C"), ChunkName);

            TEST_EQUAL(Debug.GetHitCount() - HitsBefore, (int32)1);
            TEST_TRUE(Debug.IsPaused());
            TEST_EQUAL((int32)lua_tointeger(L, -1), 3);

            Debug.Continue();
            TEST_FALSE(Debug.IsPaused());
        });

        It(TEXT("没声明的行不停"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();
            Debug.ClearBreakpoints();
            Debug.Attach(L);

            const FString ChunkName = TEXT("Tests.Specs.DebugService.NoHit");
            TEST_TRUE(Debug.AddBreakpoint(ChunkName, 99));

            Env->DoString(TEXT("local A = 1\nlocal B = 2\nlocal C = A + B\nreturn C"), ChunkName);

            // The mirror image of the case above: arming the line hook must not by itself
            // produce stops. On Luau that hook is a single step, so "armed" and "stopped"
            // are separate things and only this case tells them apart.
            TEST_EQUAL(Debug.GetHitCount() - HitsBefore, (int32)0);
            TEST_FALSE(Debug.IsPaused());
        });
    });

    Describe(TEXT("调用栈"), [this]()
    {
        It(TEXT("停在嵌套调用里时逐层展开"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();
            Debug.ClearBreakpoints();
            Debug.Attach(L);

            // Line 2 is inside Inner, so at the stop the stack is Inner / Outer / main
            // chunk. A stack that stops at one frame is the failure this catches: reading
            // level 0 of a debug callback is where "which frame am I in" goes wrong.
            const FString ChunkName = TEXT("Tests.Specs.DebugService.Stack");
            TEST_TRUE(Debug.AddBreakpoint(ChunkName, 2));

            const FString Chunk = TEXT("local function Inner()\n    return 1\nend\n"
                                       "local function Outer()\n    return Inner()\nend\n"
                                       "return Outer()");
            Env->DoString(Chunk, ChunkName);

            TEST_TRUE(Debug.IsPaused());

            const TArray<UnLuaNova::FLuaDebugFrame>& Stack = Debug.GetStackTrace();
            TEST_TRUE(Stack.Num() >= 3);
            if (Stack.Num() >= 3)
            {
                TEST_EQUAL(Stack[0].Depth, (int32)0);
                TEST_EQUAL(Stack[1].Depth, (int32)1);
                TEST_EQUAL(Stack[2].Depth, (int32)2);
                TEST_FALSE(Stack[0].Source.IsEmpty());
                // Two different frames, two different lines: a stack that reports the
                // same line at every depth is a level that never advanced.
                TEST_TRUE(Stack[0].Line != Stack[1].Line);
            }
        });
    });

    Describe(TEXT("局部变量"), [this]()
    {
        It(TEXT("停下时读到当前帧的局部变量"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();
            Debug.ClearBreakpoints();
            Debug.Attach(L);

            // Stop on the last line, where all three locals are already assigned.
            const FString ChunkName = TEXT("Tests.Specs.DebugService.Locals");
            TEST_TRUE(Debug.AddBreakpoint(ChunkName, 4));

            const FString Chunk = TEXT("local Answer = 42\nlocal Label = \"hi\"\nlocal Flag = true\nreturn Answer");
            Env->DoString(Chunk, ChunkName);

            TEST_TRUE(Debug.IsPaused());

            const TArray<UnLuaNova::FLuaDebugVariable>& Locals = Debug.GetLocals();
            TEST_TRUE(Locals.Num() >= 3);

            const UnLuaNova::FLuaDebugVariable* Answer = Locals.FindByPredicate(
                [](const UnLuaNova::FLuaDebugVariable& Var) { return Var.Name == TEXT("Answer"); });
            const UnLuaNova::FLuaDebugVariable* Label = Locals.FindByPredicate(
                [](const UnLuaNova::FLuaDebugVariable& Var) { return Var.Name == TEXT("Label"); });
            const UnLuaNova::FLuaDebugVariable* Flag = Locals.FindByPredicate(
                [](const UnLuaNova::FLuaDebugVariable& Var) { return Var.Name == TEXT("Flag"); });

            TEST_TRUE(Answer != nullptr);
            TEST_TRUE(Label != nullptr);
            TEST_TRUE(Flag != nullptr);

            if (Answer && Label && Flag)
            {
                // The captured form, not the raw value: numbers are "%g", strings come
                // back quoted. Asserting on the rendering keeps this honest about what a
                // watch window actually shows.
                TEST_EQUAL(Answer->Value, FString(TEXT("42")));
                TEST_EQUAL(Label->Value, FString(TEXT("\"hi\"")));
                TEST_EQUAL(Flag->Value, FString(TEXT("true")));
            }
        });
    });

    Describe(TEXT("求值"), [this]()
    {
        It(TEXT("没有停的时候按全局求值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();
            Debug.ClearBreakpoints();
            Debug.Attach(L);

            Env->DoString(TEXT("GlobalForDebugSpec = 40 + 2 return true"));
            lua_pop(L, 1);

            FString Value, Error;
            TEST_TRUE(Debug.Evaluate(TEXT("GlobalForDebugSpec == 42"), -1, Value, Error));
            TEST_EQUAL(Value, FString(TEXT("true")));
        });

        It(TEXT("表达式写错时返回 false 并给出原因"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();
            Debug.ClearBreakpoints();
            Debug.Attach(L);

            FString Value, Error;
            TEST_FALSE(Debug.Evaluate(TEXT("1 +"), -1, Value, Error));
            TEST_TRUE(Error.Contains(TEXT("compile error")));
            TEST_TRUE(Value.IsEmpty());

            // runtime error too - a watch window that swallows these looks like it is
            // showing you a value when it is actually showing you nothing
            TEST_FALSE(Debug.Evaluate(TEXT("nil.Field"), -1, Value, Error));
            TEST_TRUE(Error.Contains(TEXT("runtime error")));
        });

        It(TEXT("没有 attach 时求值失败而不是崩溃"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();

            FString Value, Error;
            TEST_FALSE(Debug.Evaluate(TEXT("1 + 1"), -1, Value, Error));
            TEST_TRUE(Error.Contains(TEXT("not attached")));
        });
    });

    Describe(TEXT("后端一致性"), [this]()
    {
        It(TEXT("HasFeature 的回答与编译期常量一致"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            Env->DoString(TEXT("return UnLuaNova.HasFeature('breakpoint')"));

            // Two surfaces claim the same fact: the script-facing query and the compile
            // time macro. They are filled in from different places (UnLuaNovaLib.cpp's
            // table vs LuaBackend.h), so nothing but a test keeps them from drifting -
            // and a script that asks "can I use breakpoints?" deserves the truth.
            const bool bQueried = lua_toboolean(L, -1) != 0;
#if UNLUANOVA_HAS_BREAKPOINT
            TEST_TRUE(bQueried);
#else
            TEST_FALSE(bQueried);
#endif
        });
    });
}

#endif
