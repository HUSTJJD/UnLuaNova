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

// The observability features - CPU profiler, memory stats, headless debug service - had no
// test of their own: only the hook plumbing underneath them was covered, so a change that
// made 'lua.profiler' stop installing its hook, or 'lua.memstats' stop counting, would have
// gone green. These drive the same classes the console commands drive.
//
// All three are process-wide singletons, so each test leaves them the way it found them:
// the suite runs in one editor process and a test that leaves the profiler attached would
// silently change what the next one measures.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#include "LuaProfiler.h"
#include "LuaMemoryStats.h"
#include "LuaDebugService.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FObservabilitySpec, "UnLuaNova.API.Observability", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FObservabilitySpec)

void FObservabilitySpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
    });

    AfterEach([this]
    {
        // Detach in reverse order of the Describes below: the debug service and the
        // profiler both want a hook, and whichever is left behind becomes the next test's
        // problem.
        UnLuaNova::FLuaDebugService::Get().Detach();
        UnLuaNova::FLuaProfiler::Get().Detach(L);
        UnLuaNova::FLuaMemoryStats::Get().bEnabled = false;
        Env.Reset();
        L = nullptr;
    });

    Describe(TEXT("CPU 性能分析"), [this]()
    {
        It(TEXT("开关可切换， detach 之后关闭"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Profiler = UnLuaNova::FLuaProfiler::Get();
            TEST_FALSE(Profiler.IsEnabled());

            Profiler.Attach(L);
            Profiler.SetEnabled(true);
            TEST_TRUE(Profiler.IsEnabled());

            // Idempotent: the runtime toggle is a console command, and typing it twice
            // must not install the hook twice.
            Profiler.SetEnabled(true);
            TEST_TRUE(Profiler.IsEnabled());

            Profiler.SetEnabled(false);
            TEST_FALSE(Profiler.IsEnabled());

            Profiler.SetEnabled(true);
            Profiler.Detach(L);
            TEST_FALSE(Profiler.IsEnabled());
        });

        It(TEXT("开启后脚本仍能正常执行"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Profiler = UnLuaNova::FLuaProfiler::Get();
            Profiler.Attach(L);
            Profiler.SetEnabled(true);

            UnLuaNova::RunChunk(L, "local Sum = 0 for i = 1, 100 do Sum = Sum + i end return Sum");
            TEST_EQUAL((int32)lua_tointeger(L, -1), 5050);

            Profiler.SetEnabled(false);
            Profiler.Detach(L);
        });
    });

    Describe(TEXT("内存统计"), [this]()
    {
        It(TEXT("开启后统计分配次数与字节数"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Stats = UnLuaNova::FLuaMemoryStats::Get();
            Stats.Reset();
            Stats.bEnabled = true;

            Env->DoString(TEXT("local T = {} for i = 1, 1000 do T[i] = tostring(i) end return #T"));

            TEST_TRUE(Stats.GetAllocCount() > 0);
            TEST_TRUE(Stats.GetCurrentBytes() > 0);
            TEST_TRUE(Stats.GetPeakBytes() >= Stats.GetCurrentBytes());
            TEST_FALSE(Stats.BuildReport().IsEmpty());
        });

        It(TEXT("关闭后不再记账"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Stats = UnLuaNova::FLuaMemoryStats::Get();
            Stats.Reset();
            Stats.bEnabled = false;

            Env->DoString(TEXT("local T = {} for i = 1, 1000 do T[i] = tostring(i) end return #T"));

            // The allocator hook costs a couple of atomics per allocation, which is why it
            // is opt-in - and why "off" has to mean off.
            TEST_EQUAL((int64)Stats.GetAllocCount(), (int64)0);
        });

        It(TEXT("Reset 清零"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Stats = UnLuaNova::FLuaMemoryStats::Get();
            Stats.bEnabled = true;
            Env->DoString(TEXT("return {}"));
            Stats.bEnabled = false;

            Stats.Reset();
            TEST_EQUAL((int64)Stats.GetCurrentBytes(), (int64)0);
            TEST_EQUAL((int64)Stats.GetAllocCount(), (int64)0);
            TEST_EQUAL((int64)Stats.GetFreeCount(), (int64)0);
        });
    });

    Describe(TEXT("调试服务"), [this]()
    {
        It(TEXT("断点登记"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();
            Debug.ClearBreakpoints();

            TEST_TRUE(Debug.AddBreakpoint(TEXT("Tests.Specs.Observability"), 2));
            // Adding the same one twice is rejected: the list is a set, and a duplicate
            // would make 'clear' and 'remove' ambiguous.
            TEST_FALSE(Debug.AddBreakpoint(TEXT("Tests.Specs.Observability"), 2));
            TEST_EQUAL(Debug.GetBreakpoints().Num(), 1);

            TEST_TRUE(Debug.RemoveBreakpoint(TEXT("Tests.Specs.Observability"), 2));
            TEST_FALSE(Debug.RemoveBreakpoint(TEXT("Tests.Specs.Observability"), 2));
            TEST_EQUAL(Debug.GetBreakpoints().Num(), 0);

            Debug.AddBreakpoint(TEXT("A"), 1);
            Debug.AddBreakpoint(TEXT("B"), 2);
            Debug.ClearBreakpoints();
            TEST_EQUAL(Debug.GetBreakpoints().Num(), 0);
        });

        It(TEXT("attach / detach"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();
            TEST_FALSE(Debug.IsAttached());

            Debug.Attach(L);
            TEST_TRUE(Debug.IsAttached());

            Debug.Detach();
            TEST_FALSE(Debug.IsAttached());
        });

        It(TEXT("单步能停下来并抓到调用栈"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Debug = UnLuaNova::FLuaDebugService::Get();
            Debug.Detach();
            Debug.ClearBreakpoints();
            Debug.Attach(L);
            Debug.SetSingleStep(true);

            // Four lines, so a line/step hook has something to stop on. Nothing here
            // blocks: bBlocking is off by default, so the chunk runs to completion and
            // HitCount tells us how many stops were taken.
            const FString Chunk = TEXT("local A = 1\nlocal B = 2\nlocal C = 3\nreturn A + B + C");
            Env->DoString(Chunk, TEXT("Tests.Specs.Observability.单步"));

            TEST_TRUE(Debug.GetHitCount() > 0);
            TEST_TRUE(Debug.GetStackTrace().Num() > 0);

            Debug.SetSingleStep(false);
            Debug.Detach();
        });
    });
}

#endif
