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

struct FUnLuaNovaTest_Issue432 : FUnLuaNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuaNovaTestBase::SetUp();

        const auto World = GetWorld();
        UnLuaNova::PushUObject(L, World);
        lua_setglobal(L, "G_World");
        const auto Chunk = R"(
            local ActorClass = UE.UClass.Load("/UnLuaNovaTestSuite/Tests/Regression/Issue432/BP_UnLuaTestActor_Issue432.BP_UnLuaTestActor_Issue432_C")
            local Actor = G_World:SpawnActor(ActorClass)
            Actor:Test()
        )";
        UnLuaNova::RunChunk(L, Chunk);
        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaNovaTest_Issue432, TEXT("UnLuaNova.Regression.Issue432 Lua调用的函数返回蓝图结构体会check"))

#endif
