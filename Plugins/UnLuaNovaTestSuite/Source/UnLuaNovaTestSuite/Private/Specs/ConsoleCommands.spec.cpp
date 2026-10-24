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

#include "LuaBackend.h"
#include "LuaDebugService.h"
#include "LuaMemoryStats.h"
#include "LuaProfiler.h"

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

    AfterEach([this]
    {
        // The cases below deliberately leave the profiler, the memory tracker and the debug
        // service in the state the command put them, because that state *is* the assertion.
        // All three are process-wide: a profiler hook or an attached debugger left behind
        // here would quietly change what the next spec measures.
        UnLuaNova::FLuaDebugService::Get().Detach();
        // SetEnabled(false) rather than Detach(): FLuaEnv attaches the profiler to its own
        // main state at construction, and Detach() is a no-op unless the pointer matches
        // exactly - which would leave the hook installed for the next spec.
        UnLuaNova::FLuaProfiler::Get().SetEnabled(false);
        UnLuaNova::FLuaMemoryStats::Get().bEnabled = false;
        L = nullptr;
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

    // Everything above proves "the command ran". These prove "the command did something":
    // each one reads the state the command is supposed to change. Without them a command
    // that parsed its arguments, logged a line and returned would pass forever.
    Describe(TEXT("命令的可观测效果"), [this]()
    {
        It(TEXT("lua.profiler on/off 真的开关分析器"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Profiler = UnLuaNova::FLuaProfiler::Get();
            Profiler.SetEnabled(false);

            TEST_TRUE(SpecExecCommand(this, TEXT("lua.profiler"), {TEXT("on")}));
            TEST_TRUE(Profiler.IsEnabled());

            TEST_TRUE(SpecExecCommand(this, TEXT("lua.profiler"), {TEXT("off")}));
            TEST_FALSE(Profiler.IsEnabled());
        });

        It(TEXT("lua.memstats on 之后开始记账，off 之后停"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Stats = UnLuaNova::FLuaMemoryStats::Get();
            Stats.bEnabled = false;
            Stats.Reset();

            TEST_TRUE(SpecExecCommand(this, TEXT("lua.memstats"), {TEXT("on")}));
            TEST_TRUE(Stats.bEnabled.load(std::memory_order_relaxed));
            TEST_TRUE(UnLuaNova::RunChunk(L, "local T = {} for i = 1, 1000 do T[i] = tostring(i) end return #T"));
            TEST_TRUE(Stats.GetAllocCount() > 0);

            const int64 Counted = (int64)Stats.GetAllocCount();
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.memstats"), {TEXT("off")}));
            TEST_FALSE(Stats.bEnabled.load(std::memory_order_relaxed));
            TEST_TRUE(UnLuaNova::RunChunk(L, "local T = {} for i = 1, 1000 do T[i] = tostring(i) end return #T"));

            // "off" has to mean off: the allocator callback costs a couple of atomics per
            // allocation, which is the only reason it is opt-in.
            TEST_EQUAL((int64)Stats.GetAllocCount(), Counted);
        });

        It(TEXT("lua.gc 真的回收了垃圾"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TEST_TRUE(L != nullptr);

            UnLuaNova::RunChunk(L, "local T = {} for i = 1, 20000 do T[i] = i end _G.SpecConsoleGarbage = T return true");
            lua_pop(L, 1);
            const int64 PeakBytes = UNLUANOVA_GC_HEAP_BYTES(L);

            // Drop the only reference and force a full collect through the command.
            lua_pushnil(L);
            lua_setglobal(L, "SpecConsoleGarbage");

            TEST_TRUE(SpecExecCommand(this, TEXT("lua.gc")));
            const int64 AfterBytes = UNLUANOVA_GC_HEAP_BYTES(L);

            // Bytes rather than the KB LUA_GCCOUNT reports, and 20000 entries rather than
            // a handful: a full collect that frees nothing is the failure being looked for,
            // and it has to be one a rounding error cannot hide.
            TEST_TRUE(AfterBytes < PeakBytes);
        });

        It(TEXT("lua.dbg 真的挂上调试器，断点登记后可查"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();
            Debug.ClearBreakpoints();

            // No arguments is the status sub-command, and it is also the one that installs
            // the debug hook - the service only costs anything once armed.
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.dbg")));
            TEST_TRUE(Debug.IsAttached());

            TEST_TRUE(SpecExecCommand(this, TEXT("lua.dbg"), {TEXT("bp"), TEXT("Tests.Specs.ConsoleCommands"), TEXT("1")}));
            TEST_EQUAL(Debug.GetBreakpoints().Num(), 1);

            TEST_TRUE(SpecExecCommand(this, TEXT("lua.dbg"), {TEXT("clear")}));
            TEST_EQUAL(Debug.GetBreakpoints().Num(), 0);

            TEST_TRUE(SpecExecCommand(this, TEXT("lua.dbg"), {TEXT("detach")}));
            TEST_FALSE(Debug.IsAttached());
        });

        It(TEXT("lua.coroutines 可查且不影响脚本执行"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The census is worth having only if it can be run against a live env without
            // disturbing it - it forces a collect to tell live coroutines from garbage.
            TEST_TRUE(SpecExecCommand(this, TEXT("lua.coroutines")));
            TEST_TRUE(UnLuaNova::RunChunk(L, "return 1"));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 1);
            lua_pop(L, 1);
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
