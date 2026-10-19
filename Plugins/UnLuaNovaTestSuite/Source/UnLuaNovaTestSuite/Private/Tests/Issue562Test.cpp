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


#include "LowLevel.h"
#include "UnLuaNovaCompatibility.h"
#include "UnLuaNovaSettings.h"
#include "UnLuaNovaTestCommon.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FUnLuaNovaTest_Issue562 : FUnLuaNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuaNovaTestBase::SetUp();

        // 使用-binnedmalloc2来提高复现概率

        const auto Chunk1 = R"(
            local InterfaceClass = UE.UClass.Load('/UnLuaNovaTestSuite/Tests/Regression/Issue562/BPI_Issue562.BPI_Issue562_C')
            local Array = UE.TArray(InterfaceClass)
        )";
        UnLuaNova::RunChunk(L, Chunk1);

        const auto Before = FindFirstObject<UClass>(TEXT("BPI_Issue562_C"));
        RUNNER_TEST_NOT_NULL(Before);

        CollectGarbage(RF_NoFlags, true);
        const auto After = FindFirstObject<UClass>(TEXT("BPI_Issue562_C"));
        RUNNER_TEST_NULL(After);

        lua_gc(L, LUA_GCCOLLECT);
        lua_gc(L, LUA_GCCOLLECT);

        // LogUObjectBase: Error: Object flags are invalid or either Class or Outer is misaligned

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaNovaTest_Issue562, TEXT("UnLuaNova.Regression.Issue562 容器元素为蓝图类型时，在蓝图Class被提前UE回收后，会导致容器在luagc时报错"))

#endif
