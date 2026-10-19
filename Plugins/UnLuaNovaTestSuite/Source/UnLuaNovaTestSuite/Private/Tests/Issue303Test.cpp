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

struct FUnLuaNovaTest_Issue303 : FUnLuaNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuaNovaTestBase::SetUp();

        TArray<FString> Array;
        Array.Add("a");
        Array.Add("b");
        Array.Add("c");
        UnLuaNova::Push(L, Array, true);

        lua_setglobal(L, "G_Array");

        const auto Chunk = "\
        Result = G_Array:Get(1) .. G_Array:Get(2) .. G_Array:Get(3)\
        ";
        UnLuaNova::RunChunk(L, Chunk);

        lua_getglobal(L, "Result");
        const auto Result = lua_tostring(L, -1);
        RUNNER_TEST_EQUAL(Result, FString("abc"));

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaNovaTest_Issue303, TEXT("UnLuaNova.Regression.Issue303 部分情况下为TArray创建拷贝至lua时会产生崩溃"))

#endif //WITH_DEV_AUTOMATION_TESTS
