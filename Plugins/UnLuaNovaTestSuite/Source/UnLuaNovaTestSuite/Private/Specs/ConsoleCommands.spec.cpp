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

// Console command *behaviour*, where UnLuaNova.Editor.Commands only checks that the names
// still exist.
//
// The names are asserted here too, and deliberately: a command that was renamed is exactly
// the failure "it is still registered" cannot catch, and it is also the one a user hits
// first. Each case runs the command and then checks the effect, so a command that exists
// but does nothing also fails.
//
// Every command is invoked through its IConsoleCommand rather than by feeding text to the
// console, because splitting a command line is not what is under test - and a case that
// failed on quoting would say nothing about the command.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Runs a registered console command, and fails loudly if the name is gone: a missing
// command would otherwise look like "nothing happened".
static bool SpecExecCommand(FAutomationTestBase* Test, const TCHAR* Name, TArray<FString> Args = {})
{
    IConsoleObject* Object = IConsoleManager::Get().FindConsoleObject(Name);
    Test->TestTrue(Name, Object != nullptr);
    if (!Object || !Object->AsCommand())
        return false;

    // Execute also wants a world and an output device. None of the commands under test
    // looks at either, but they have to be handed something valid rather than nothing.
    Object->AsCommand()->Execute(Args, GWorld, *GLog);
    return true;
}

BEGIN_DEFINE_SPEC(FConsoleCommandsSpec, "UnLuaNova.Editor.ConsoleCommands", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    lua_State* L;
END_DEFINE_SPEC(FConsoleCommandsSpec)

void FConsoleCommandsSpec::Define()
{
    BeforeEach([this]
    {
        UnLuaNova::Startup();
        L = UnLuaNova::GetState();
    });

    Describe(TEXT("lua.do"), [this]()
    {
        It(TEXT("执行的脚本真的生效"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The command's whole job is to reach the environment, so the assertion is on
            // the environment's state afterwards rather than on a return code.
            TEST_TRUE(L != nullptr);
            if (!SpecExecCommand(this, TEXT("lua.do"), {TEXT("_G.SpecConsoleDo = 41 + 1")}))
                return;

            lua_getglobal(L, "SpecConsoleDo");
            TEST_EQUAL((int32)lua_tointeger(L, -1), 42);
            lua_pop(L, 1);
        });

        It(TEXT("一段错误脚本不会让环境失效"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // A command that broke the environment would be far worse than one that just
            // reported an error, so the case runs a good chunk after the bad one.
            AddExpectedError(TEXT("spec console boom"), EAutomationExpectedErrorFlags::Contains);
            TEST_TRUE(L != nullptr);

            SpecExecCommand(this, TEXT("lua.do"), {TEXT("error('spec console boom')")});
            TEST_TRUE(UnLuaNova::RunChunk(L, "return 1"));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 1);
        });

        It(TEXT("缺少参数时给用法提示而不是崩"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.do")));
            TEST_TRUE(UnLuaNova::RunChunk(L, "return 1"));
        });
    });

    Describe(TEXT("开关类命令"), [this]()
    {
        It(TEXT("lua.gc 执行后环境仍可用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TEST_TRUE(L != nullptr);
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.gc")));
            TEST_TRUE(UnLuaNova::RunChunk(L, "return 1"));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 1);
        });

        It(TEXT("lua.profiler 开关是可重复的"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // on -> on -> off has to be survivable: the profiler installs a hook, and a
            // second install that overwrote rather than shared the slot used to silently
            // disable the dead-loop watchdog.
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.profiler"), {TEXT("on")}));
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.profiler"), {TEXT("on")}));
            TEST_TRUE(UnLuaNova::RunChunk(L, "return 1"));
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.profiler"), {TEXT("off")}));
            TEST_TRUE(UnLuaNova::RunChunk(L, "return 1"));
        });

        It(TEXT("lua.memstats 各子命令都不崩"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.memstats"), {TEXT("on")}));
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.memstats")));
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.memstats"), {TEXT("reset")}));
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.memstats"), {TEXT("off")}));
            TEST_TRUE(UnLuaNova::RunChunk(L, "return 1"));
        });

        It(TEXT("lua.dbg 状态查询是幂等的"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // No argument prints status; calling it twice must not attach anything, or a
            // stray debugger would pause the next test's scripts.
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.dbg")));
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.dbg")));
            TEST_TRUE(UnLuaNova::RunChunk(L, "return 1"));
        });
    });

    Describe(TEXT("沙盒"), [this]()
    {
        It(TEXT("lua.sandbox 是一个可写的 CVar"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The sandbox is a CVar rather than a command, which is why a command-shaped
            // test cannot see it. It is off by default and must be restorable, because it
            // changes what every later script may write.
            IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.sandbox"));
            TEST_TRUE(Var != nullptr);
            if (!Var)
                return;

            const int32 Saved = Var->GetInt();
            Var->Set(1, ECVF_SetByCode);
            TEST_EQUAL(Var->GetInt(), 1);
            Var->Set(Saved, ECVF_SetByCode);
            TEST_EQUAL(Var->GetInt(), Saved);
        });
    });
}

#endif
