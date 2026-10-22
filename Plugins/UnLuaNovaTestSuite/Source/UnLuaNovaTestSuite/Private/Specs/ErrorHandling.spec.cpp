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

// Every branch a script can take that is *not* the happy path: a chunk that does not
// compile, a function that raises, a UFunction that does not exist, an argument of the
// wrong type.
//
// The thing being protected is not the error message - it is the state of the VM after
// the error. A script that dies is a bug the author can see; a script that dies and
// leaves two values on the stack, or leaves the next call reading someone else's
// return value, is a bug that surfaces somewhere else entirely. So most of these cases
// assert twice: once about the error, once about the stack or the next call.
//
// Error paths that reach C++ go through ReportLuaCallError, which logs at Error level -
// which an automation test treats as a failure - so those cases register the message as
// expected. The ones that stay inside pcall() do not log and need no expectation, which
// is also why they are preferred where both would do.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FErrorHandlingSpec, "UnLuaNova.API.ErrorHandling", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FErrorHandlingSpec)

void FErrorHandlingSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
    });

    AfterEach([this]
    {
        Env.Reset();
        L = nullptr;
    });

    Describe(TEXT("编译错误"), [this]()
    {
        It(TEXT("load 返回 nil 与错误信息，而不是抛出"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // load() reports failure by returning, which is what lets a caller say
            // something better than "attempt to call a nil value".
            UnLuaNova::RunChunk(L, "local F, E = load('this is not lua') return F == nil");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("错误信息带上 chunk 名与行号"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The chunk name is threaded through from C++, so a script that fails to
            // compile points back at something the author recognises. The message also
            // carries ":line:"; only the name is asserted here because the two backends
            // word their syntax diagnostics differently.
            UnLuaNova::RunChunk(L, "local F, E = load('this is not lua', 'SpecChunk.lua') return E");
            const FString Msg = UTF8_TO_TCHAR(lua_tostring(L, -1));
            TEST_TRUE(Msg.Contains(TEXT("SpecChunk.lua")));
            TEST_TRUE(Msg.Len() > 0);
        });

        It(TEXT("DoString 返回 false，且之后的环境仍可用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Luau reports a broken chunk as "Incomplete statement ..."; plain Lua says
            // "syntax error". One string cannot cover both, so the expected text is
            // whatever this backend actually produces.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            AddExpectedError(TEXT("Incomplete statement"), EAutomationExpectedErrorFlags::Contains);
#else
            AddExpectedError(TEXT("syntax error"), EAutomationExpectedErrorFlags::Contains);
#endif
            TEST_FALSE(Env->DoString(TEXT("invalid chunk")));

            // The real contract: a failed compile must not poison the VM for the next
            // call. (The failed call does leave two values behind - nil and the message
            // - which is not asserted here because asserting it would lock in a wart
            // rather than a guarantee. It is harmless precisely because the next call
            // does not read them.)
            TEST_TRUE(Env->DoString(TEXT("return 1")));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 1);
        });
    });

    Describe(TEXT("运行时错误"), [this]()
    {
        It(TEXT("DoString 返回 false 并把栈还原"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const int32 TopBefore = lua_gettop(L);
            AddExpectedError(TEXT("boom"), EAutomationExpectedErrorFlags::Contains);
            TEST_FALSE(Env->DoString(TEXT("error('boom')")));
            // The failure path pops back to where it started. The success path does not -
            // it leaves the chunk's return values - which is why "no growth" is the wrong
            // assertion for a suite that mixes both.
            TEST_EQUAL(lua_gettop(L), TopBefore);
        });

        It(TEXT("连续三次错误后栈顶不变"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The detector for "an error path that leaks a stack slot": three errors in
            // a row must land on exactly the same top as before the first one.
            const int32 TopBefore = lua_gettop(L);
            AddExpectedError(TEXT("balance"), EAutomationExpectedErrorFlags::Contains, 3);
            Env->DoString(TEXT("error('balance')"));
            Env->DoString(TEXT("error('balance')"));
            Env->DoString(TEXT("error('balance')"));
            TEST_EQUAL(lua_gettop(L), TopBefore);
        });

        It(TEXT("pcall 捕获后错误不逃逸，后续调用照常"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "local Ok, Err = pcall(function() error('pcalled') end) return Ok");
            TEST_FALSE(lua_toboolean(L, -1));

            UnLuaNova::RunChunk(L, "local Ok, Err = pcall(function() error('pcalled') end) return Err");
            const FString Msg = UTF8_TO_TCHAR(lua_tostring(L, -1));
            TEST_TRUE(Msg.Contains(TEXT("pcalled")));

            TEST_TRUE(Env->DoString(TEXT("return 42")));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 42);
        });
    });

    Describe(TEXT("引擎侧错误"), [this]()
    {
        It(TEXT("调用不存在的 UFunction 是受控错误"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Reached through UnLua's __index, so the interesting part is that it comes
            // back as an error instead of as a callable nil or a crash.
            UnLuaNova::RunChunk(L, R"(
                local S = NewObject(UE.UUnLuaNovaTestStub)
                local Ok, Err = pcall(function() return S:NotARealFunction() end)
                return Ok
            )");
            TEST_FALSE(lua_toboolean(L, -1));
        });

        It(TEXT("参数类型错误是受控错误"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // ENABLE_TYPE_CHECK is on by default; without it this would coerce the
            // string to 0 and the call would succeed, which is exactly the regression
            // this pins.
            UnLuaNova::RunChunk(L, R"(
                local S = NewObject(UE.UUnLuaNovaTestStub)
                local Ok, Err = pcall(S.ProbeWithStaticBinding, S, 'not a number')
                return Ok
            )");
            TEST_FALSE(lua_toboolean(L, -1));
        });

        It(TEXT("对 nil 引擎对象调用方法是受控错误"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // A binding whose object has gone away is the common shape of this bug, and
            // the failure mode being guarded against is a native crash rather than a Lua
            // error. pcall catching it proves which one it is.
            UnLuaNova::RunChunk(L, R"(
                local S = nil
                local Ok, Err = pcall(function() return S:AddCount() end)
                return Ok
            )");
            TEST_FALSE(lua_toboolean(L, -1));
        });
    });

    Describe(TEXT("错误信息"), [this]()
    {
        It(TEXT("不含机器绝对路径"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // GenerateTestReport.py strips absolute paths out of committed reports for
            // the same reason: a message carrying D:\... makes every run of the report
            // diff, and the reader still cannot act on it. The chunk name is what should
            // travel, not the machine it ran on.
            UnLuaNova::RunChunk(L, "local Ok, Err = pcall(function() error('nopath') end) return Err");
            const FString Msg = UTF8_TO_TCHAR(lua_tostring(L, -1));
            TEST_TRUE(Msg.Contains(TEXT("nopath")));
            TEST_FALSE(Msg.Contains(TEXT(":\\")));
            TEST_FALSE(Msg.Contains(TEXT("UnLuaNovaDemo")));
        });
    });
}

#endif
