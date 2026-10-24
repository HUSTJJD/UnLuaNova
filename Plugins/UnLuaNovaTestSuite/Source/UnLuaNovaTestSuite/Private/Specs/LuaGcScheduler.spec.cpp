// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

#include "UnLuaNovaBase.h"
#include "LuaBackend.h"
#include "LuaEnv.h"
#include "LuaGcScheduler.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FGcSchedulerSpec, "UnLuaNova.API.GcScheduler", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FGcSchedulerSpec)

void FGcSchedulerSpec::Define()
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

    Describe(TEXT("模式"), [this]()
    {
        It(TEXT("默认是 Automatic，切换后可切回"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TEST_TRUE(Env->GetGCMode() == UnLuaNova::ELuaGcMode::Automatic);

            Env->SetGCMode(UnLuaNova::ELuaGcMode::TimeSliced);
            TEST_TRUE(Env->GetGCMode() == UnLuaNova::ELuaGcMode::TimeSliced);

            Env->SetGCMode(UnLuaNova::ELuaGcMode::Automatic);
            TEST_TRUE(Env->GetGCMode() == UnLuaNova::ELuaGcMode::Automatic);
        });

        It(TEXT("Automatic 模式下 Tick 不做任何步进"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            for (int32 i = 0; i < 30; ++i)
                Env->TickGC();

            TEST_EQUAL(Env->GetGCStats().SteppedFrames, 0);
            TEST_EQUAL(Env->GetGCStats().FullCollects, 0);
        });
    });

    Describe(TEXT("帧分摊"), [this]()
    {
        It(TEXT("TimeSliced 下分帧步进能回收掉先建后弃的垃圾"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The case this scheduler exists for: build a lot, drop it, then allocate
            // nothing more. A per-frame allocation model books zero debt here and never
            // collects; measuring against the last completed cycle does collect.
            Env->DoString(TEXT("local t = {} for i = 1, 20000 do t[i] = { i, i + 1, i + 2 } end SpecGarbage = t"));
            Env->DoString(TEXT("SpecGarbage = nil"));

            Env->SetGCMode(UnLuaNova::ELuaGcMode::TimeSliced);
            const int64 Before = Env->GetGCStats().HeapBytes;

            for (int32 i = 0; i < 400; ++i)
                Env->TickGC();

            const int64 After = Env->GetGCStats().HeapBytes;

            TestTrue(TEXT("堆在分帧步进后变小"), After < Before);
            TestTrue(TEXT("确实发生了步进"), Env->GetGCStats().SteppedFrames > 0);
        });

        It(TEXT("每帧预算会限制单次步进的量"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            Env->DoString(TEXT("local t = {} for i = 1, 20000 do t[i] = { i } end SpecGarbage = t"));
            Env->DoString(TEXT("SpecGarbage = nil"));

            Env->SetGCMode(UnLuaNova::ELuaGcMode::TimeSliced);

            // 1 KB per frame against megabytes of garbage: many frames, each tiny. The
            // point is not the count, it is that no single Tick does the whole cycle.
            Env->SetGCStepBudget(1024);
            Env->TickGC();

            const int64 AfterOneFrame = Env->GetGCStats().BytesSteppedTotal;
            TestTrue(TEXT("第一帧只步进了一个预算的量"), AfterOneFrame <= 1024);

            Env->SetGCStepBudget(0); // uncapped: one frame does the rest
            Env->TickGC();
            TestTrue(TEXT("解除上限后一帧步进更多"), Env->GetGCStats().BytesSteppedTotal > AfterOneFrame);
        });

        It(TEXT("堆超过硬上限时改为整轮回收"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            Env->SetGCMode(UnLuaNova::ELuaGcMode::TimeSliced);
            Env->SetGCHardLimit(1); // 1 byte: everything is over the limit

            const int32 Before = Env->GetGCStats().FullCollects;
            Env->TickGC();
            TestTrue(TEXT("触发了整轮回收"), Env->GetGCStats().FullCollects > Before);
        });
    });

    Describe(TEXT("统计"), [this]()
    {
        It(TEXT("帧数与峰值堆随 Tick 增长"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            Env->SetGCMode(UnLuaNova::ELuaGcMode::TimeSliced);
            Env->DoString(TEXT("SpecLive = {} for i = 1, 1000 do SpecLive[i] = i end"));

            for (int32 i = 0; i < 5; ++i)
                Env->TickGC();

            TEST_EQUAL(Env->GetGCStats().Frames, 5);
            TestTrue(TEXT("峰值堆非零"), Env->GetGCStats().PeakHeapBytes > 0);
        });
    });
}

#endif //WITH_DEV_AUTOMATION_TESTS
