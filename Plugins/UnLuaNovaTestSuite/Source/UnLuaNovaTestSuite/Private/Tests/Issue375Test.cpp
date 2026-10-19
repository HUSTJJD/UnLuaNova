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

struct FUnLuaNovaTest_Issue375 : FUnLuaNovaTestBase
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
        lua_setglobal(L, "World");

        const auto Chunk = R"(
        local ParentClass = UE.UClass.Load("/UnLuaNovaTestSuite/Tests/Regression/Issue375/BP_ParentActor.BP_ParentActor_C")
        ParentActor = World:SpawnActor(ParentClass)
        
        local ChildClass = UE.UClass.Load("/UnLuaNovaTestSuite/Tests/Regression/Issue375/BP_ChildActor.BP_ChildActor_C")
        local ChildActor = World:SpawnActor(ChildClass)
        ChildActor:K2_DestroyActor()
        return ParentActor
        )";
        UnLuaNova::RunChunk(L, Chunk);
        const auto ParentActor = (AActor*)UnLuaNova::GetUObject(L, -1);

        lua_gc(L, LUA_GCCOLLECT, 0);
        lua_gc(L, LUA_GCCOLLECT, 0);
        CollectGarbage(RF_NoFlags, true);

        const auto GetValue = ParentActor->FindFunction(TEXT("GetValue"));
        struct ParamsStruct
        {
            int32 ReturnValue;
        } Params;
        ParentActor->UObject::ProcessEvent(GetValue, &Params);
        RUNNER_TEST_EQUAL(Params.ReturnValue, 1);
        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaNovaTest_Issue375, TEXT("UnLuaNova.Regression.Issue375 子类被销毁时，可能会令父类的Lua绑定失效"))

#endif //WITH_DEV_AUTOMATION_TESTS
