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


#include "Issue561Test.h"
#include "LowLevel.h"
#include "UnLuauNovaSettings.h"
#include "UnLuauNovaTestCommon.h"
#include "UnLuauNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FUnLuauNovaTest_Issue561 : FUnLuauNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuauNovaTestBase::SetUp();

        const auto Chunk = R"(
            local Struct1 = UE.FIssue561Struct()
            local Struct2 = UE.FIssue561Struct()
            Struct1.OnMouseEvent = Struct2.OnMouseEvent
        )";
        UnLuauNova::RunChunk(L, Chunk);

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuauNovaTest_Issue561, TEXT("UnLuauNova.Regression.Issue561 访问UStruct内部的委托会check"))

#endif
