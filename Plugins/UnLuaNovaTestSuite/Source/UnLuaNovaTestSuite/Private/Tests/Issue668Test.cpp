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

#include "UnLuaNovaTestCommon.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FUnLuaNovaTest_Issue668 : FUnLuaNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuaNovaTestBase::SetUp();

        const auto Chunk = R"(
            local StructType = UE.UObject.Load("/UnLuaNovaTestSuite/Tests/Regression/Issue668/Struct_Issue668.Struct_Issue668")
			local Array = UE.TArray(StructType)
            local Struct1 = StructType()
            local Struct2 = StructType()
			Array:Add(Struct1)
            Array:Add(Struct2)
        )";
        UnLuaNova::RunChunk(L, Chunk);

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaNovaTest_Issue668, TEXT("UnLuaNova.Regression.Issue668 在Lua中调用TArray的Add接口时内存对齐引起的问题"))

#endif
