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

#include "UnLuauNovaTestCommon.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FUnLuauNovaTest_Issue366 : FUnLuauNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuauNovaTestBase::SetUp();

        const auto Chunk = R"(
                UE.FUnLuauNovaTestLib()
            )";
        UnLuauNova::RunChunk(L, Chunk);

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuauNovaTest_Issue366, TEXT("UnLuauNova.Regression.Issue366 使用ADD_SHARED_PTR_CONSTRUCTOR后启动直接崩溃"))

#endif //WITH_DEV_AUTOMATION_TESTS
