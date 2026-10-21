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

struct FUnLuaNovaTest_Issue314 : FUnLuaNovaTestBase
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

        const auto Chunk = "\
        local ParentClass = UE.UClass.Load('/UnLuaNovaTestSuite/Tests/Regression/Issue314/BP_UnLuaNovaTestActor_Issue314_Parent.BP_UnLuaTestActor_Issue314_Parent_C')\
        local ParentActor = G_World:SpawnActor(ParentClass)\
        local ChildClass = UE.UClass.Load('/UnLuaNovaTestSuite/Tests/Regression/Issue314/BP_UnLuaNovaTestActor_Issue314_Child.BP_UnLuaTestActor_Issue314_Child_C')\
        local ChildActor = G_World:SpawnActor(ChildClass)\
        print('access ParentActor.Cube -> ', ParentActor.Cube)\
        Result = ChildActor.Cube ~= nil\
        \
        ";

        UnLuaNova::RunChunk(L, Chunk);

        lua_getglobal(L, "Result");
        const auto Result = lua_toboolean(L, -1);
        RUNNER_TEST_TRUE(Result);

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaNovaTest_Issue314, TEXT("UnLuaNova.Regression.Issue314 访问过父类不存在的字段，后续在子类中也只能取到nil"))

#endif //WITH_DEV_AUTOMATION_TESTS
