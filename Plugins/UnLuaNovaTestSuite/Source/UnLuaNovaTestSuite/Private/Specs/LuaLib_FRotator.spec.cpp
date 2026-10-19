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

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUnLuaNovaLibFRotatorSpec, "UnLuaNova.API.FRotator", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    lua_State* L;
END_DEFINE_SPEC(FUnLuaNovaLibFRotatorSpec)

void FUnLuaNovaLibFRotatorSpec::Define()
{
    BeforeEach([this]
    {
        UnLuaNova::Startup();
        L = UnLuaNova::GetState();
    });

    Describe(TEXT("构造FRotator"), [this]()
    {
        It(TEXT("默认参数"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return UE.FRotator()");
            const auto& Actual = UnLuaNova::Get<FRotator>(L, -1, UnLuaNova::TType<FRotator>());
            const auto& Expected = FRotator(ForceInitToZero);
            TEST_EQUAL(Actual, Expected);
        });

        It(TEXT("同时指定Pitch/Yaw/Roll"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return UE.FRotator(1)");
            const auto& Actual = UnLuaNova::Get<FRotator>(L, -1, UnLuaNova::TType<FRotator>());
            const auto& Expected = FRotator(1);
            TEST_EQUAL(Actual, Expected);
        });

        It(TEXT("分别指定Pitch/Yaw/Roll"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return UE.FRotator(1,2,3)");
            const auto& Actual = UnLuaNova::Get<FRotator>(L, -1, UnLuaNova::TType<FRotator>());
            const auto& Expected = FRotator(1, 2, 3);
            TEST_EQUAL(Actual, Expected);
        });
    });

    Describe(TEXT("Set"), [this]
    {
        It(TEXT("设置Pitch"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Rotator = UE.FRotator()\
            Rotator:Set(1)\
            return Rotator\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto& Actual = UnLuaNova::Get<FRotator>(L, -1, UnLuaNova::TType<FRotator>());
            const auto& Expected = FRotator(1, 0, 0);
            TEST_EQUAL(Actual, Expected);
        });

        It(TEXT("设置Pitch/Yaw"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Rotator = UE.FRotator()\
            Rotator:Set(1,2)\
            return Rotator\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto& Actual = UnLuaNova::Get<FRotator>(L, -1, UnLuaNova::TType<FRotator>());
            const auto& Expected = FRotator(1, 2, 0);
            TEST_EQUAL(Actual, Expected);
        });

        It(TEXT("设置Pitch/Yaw/Roll"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Rotator = UE.FRotator()\
            Rotator:Set(1,2,3)\
            return Rotator\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto& Actual = UnLuaNova::Get<FRotator>(L, -1, UnLuaNova::TType<FRotator>());
            const auto& Expected = FRotator(1, 2, 3);
            TEST_EQUAL(Actual, Expected);
        });
    });

    Describe(TEXT("tostring()"), [this]
    {
        It(TEXT("转为字符串"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return tostring(UE.FRotator(1,2,3))");
            const auto& Actual = lua_tostring(L, -1);
            const auto& Expected = FRotator(1, 2, 3).ToString();
            TEST_EQUAL(Actual, Expected);
        });
    });

    AfterEach([this]
    {
        UnLuaNova::Shutdown();
    });
}

#endif //WITH_DEV_AUTOMATION_TESTS
