// Tencent is pleased to support the open source community by making UnLuauNova available.
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

#include "Issue595Test.h"
#include "UnLuauNovaTestCommon.h"
#include "UnLuauNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FUnLuauNovaTest_Issue595 : FUnLuauNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuauNovaTestBase::SetUp();

        const auto Chunk = R"(
            local Obj = NewObject(UE.UIssue595Object)
            UE.UIssue595Interface.Test(Obj)
            return Obj:Test()
        )";
        UnLuauNova::RunChunk(L, Chunk);

        const auto Actual = (int32)lua_tointeger(L, -1);
        RUNNER_TEST_EQUAL(1, Actual); 

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuauNovaTest_Issue595, TEXT("UnLuauNova.Regression.Issue595 Lua无法调用Interface中函数"))

#endif
