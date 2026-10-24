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


#include "Tests/Issue517Test.h"

#include "LowLevel.h"
#include "UnLuaNovaSettings.h"
#include "UnLuaNovaTestCommon.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FUnLuaNovaTest_Issue517 : FUnLuaNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
        Settings.DanglingCheck = true;

        FUnLuaNovaTestBase::SetUp();

        const auto World = GetWorld();
        UnLuaNova::PushUObject(L, World);
        lua_setglobal(L, "World");

        const auto Chunk1 = R"(
            Actor = World:SpawnActor(UE.AIssue517Actor)
            StructRef = Actor.Struct
            StructRef.X = 10
            StructRef.Name = "MyStruct"
            print(StructRef, StructRef.X, StructRef.Name)
            print(Actor.Struct, Actor.Struct.X, StructRef.Name)

            ArrayFromActor = Actor.ArrayFromActor
            ArrayFromStruct = Actor.Struct.ArrayFromStruct
            Actor:K2_DestroyActor()
        )";
        UnLuaNova::RunChunk(L, Chunk1);

        CollectGarbage(RF_Standalone, true);

        GetTestRunner().AddExpectedError(TEXT("released struct"), EAutomationExpectedErrorFlags::Contains);
        const auto Chunk2 = R"(
            print(StructRef.X, StructRef.Name)
        )";
        UnLuaNova::RunChunk(L, Chunk2);

        GetTestRunner().AddExpectedError(TEXT("invalid TArray"), EAutomationExpectedErrorFlags::Contains, 2);
        const auto Chunk3 = R"(
            print(ArrayFromActor:Num())
        )";
        UnLuaNova::RunChunk(L, Chunk3);
        const auto Chunk4 = R"(
            print(ArrayFromStruct:Num())
        )";
        UnLuaNova::RunChunk(L, Chunk4);

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaNovaTest_Issue517, TEXT("UnLuaNova.Regression.Issue517 Actor的Struct成员变量在Lua里引用，释放后仍旧可以访问"))

#endif
