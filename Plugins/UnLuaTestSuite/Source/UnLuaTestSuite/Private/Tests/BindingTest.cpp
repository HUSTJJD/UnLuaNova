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

#include "Engine.h"
#include "UnLuauNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

static void Run(TFunction<void(lua_State*, UWorld*)> Test)
{
    // SetUp
    UnLuauNova::Startup();
    const auto L = UnLuauNova::GetState();

    const auto World = UWorld::CreateWorld(EWorldType::Game, false, "UnLuauNovaTest");
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);

    const FURL URL;
    World->InitializeActorsForPlay(URL);
    World->BeginPlay();
    World->SetBegunPlay(true);

    UnLuauNova::PushUObject(L, World);
    lua_setglobal(L, "World");

    // Perform Test
    Test(L, World);

    // TearDown
    World->EndPlay(EEndPlayReason::Destroyed);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    UnLuauNova::Shutdown();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnLuauNovaTest_StaticBinding, TEXT("UnLuauNova.API.Binding.Static 静态绑定，继承UnLuaInterface::GetModuleName指定脚本路径"),
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FUnLuauNovaTest_StaticBinding::RunTest(const FString& Parameters)
{
    Run([this](lua_State* L, UWorld* World)
    {
        const char* Chunk1 = R"(
                    local ActorClass = UE.UClass.Load('/UnLuauNovaTestSuite/Tests/Binding/BP_UnLuaTestActor_StaticBinding.BP_UnLuaTestActor_StaticBinding_C')
                    G_Actor = World:SpawnActor(ActorClass)
                )";
        UnLuauNova::RunChunk(L, Chunk1);


        World->Tick(LEVELTICK_All, SMALL_NUMBER);

        const char* Chunk2 = R"(
                return G_Actor:RunTest()
                )";
        UnLuauNova::RunChunk(L, Chunk2);

        const auto Error = lua_tostring(L, -1);
        TEST_EQUAL(Error, FString(""));
    });

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnLuauNovaTest_DynamicBinding, TEXT("UnLuauNova.API.Binding.Dynamic 动态绑定，SpawnActor时指定脚本路径"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FUnLuauNovaTest_DynamicBinding::RunTest(const FString& Parameters)
{
    Run([this](lua_State* L, UWorld* World)
    {
        const char* Chunk1 = R"(
                    local ActorClass = UE.UClass.Load('/UnLuauNovaTestSuite/Tests/Binding/BP_UnLuaTestActor_DynamicBinding.BP_UnLuaTestActor_DynamicBinding_C')
                    local Transform = UE.FTransform()
                    G_Actor = World:SpawnActor(ActorClass, Transform, UE.ESpawnActorCollisionHandlingMethod.AlwaysSpawn, nil, nil, 'Tests.Binding.BP_UnLuaTestActor')
                )";
        UnLuauNova::RunChunk(L, Chunk1);


        World->Tick(LEVELTICK_All, SMALL_NUMBER);

        const char* Chunk2 = R"(
                return G_Actor:RunTest()
                )";
        UnLuauNova::RunChunk(L, Chunk2);

        const auto Error = lua_tostring(L, -1);
        TEST_EQUAL(Error, FString(""));
    });
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnLuauNovaTest_ConflictedBinding, TEXT("UnLuauNova.API.Binding.Priority 绑定冲突：静态绑定优先于动态绑定"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FUnLuauNovaTest_ConflictedBinding::RunTest(const FString& Parameters)
{
    Run([this](lua_State* L, UWorld* World)
    {
        AddExpectedError(TEXT("conflicts"), EAutomationExpectedErrorFlags::Contains);

        const char* Chunk1 = R"(
                    local ActorClass = UE.UClass.Load('/UnLuauNovaTestSuite/Tests/Binding/BP_UnLuaTestActor_StaticBinding.BP_UnLuaTestActor_StaticBinding_C')
                    local Transform = UE.FTransform()
                    G_Actor = World:SpawnActor(ActorClass, Transform, UE.ESpawnActorCollisionHandlingMethod.AlwaysSpawn, nil, nil, 'Tests.Binding.Foo')
                )";
        UnLuauNova::RunChunk(L, Chunk1);

        World->Tick(LEVELTICK_All, SMALL_NUMBER);

        const char* Chunk2 = R"(
                return G_Actor:RunTest()
                )";
        UnLuauNova::RunChunk(L, Chunk2);

        const auto Error = lua_tostring(L, -1);
        TEST_EQUAL(Error, FString(""));
    });
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnLuauNovaTest_MultipleBinding, TEXT("UnLuauNova.API.Binding.Multiple 多重绑定：同一个Lua脚本绑定到不同类"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FUnLuauNovaTest_MultipleBinding::RunTest(const FString& Parameters)
{
    Run([this](lua_State* L, UWorld* World)
    {
        const char* Chunk1 = R"(
                    local ActorClass = UE.UClass.Load('/UnLuauNovaTestSuite/Tests/Binding/BP_UnLuaTestActor_StaticBindingChild.BP_UnLuaTestActor_StaticBindingChild_C')
                    G_Actor = World:SpawnActor(ActorClass)
                )";
        UnLuauNova::RunChunk(L, Chunk1);

        World->Tick(LEVELTICK_All, SMALL_NUMBER);

        const char* Chunk2 = R"(
                return G_Actor:RunTest()
                )";
        UnLuauNova::RunChunk(L, Chunk2);

        const auto Error = lua_tostring(L, -1);
        TEST_EQUAL(Error, FString(""));
    });

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnLuauNovaTest_Overridden, TEXT("UnLuauNova.API.Binding.Overridden 覆写：同一个Lua脚本绑定到不同类"), EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FUnLuauNovaTest_Overridden::RunTest(const FString& Parameters)
{
    Run([this](lua_State* L, UWorld* World)
    {
        const char* Chunk = R"(
                    local ActorClass = UE.UClass.Load('/UnLuauNovaTestSuite/Tests/Binding/BP_UnLuaTestActor_StaticBindingChild.BP_UnLuaTestActor_StaticBindingChild_C')
                    G_Actor = World:SpawnActor(ActorClass)
                    return G_Actor:Greeting('ABC')
                )";
        UnLuauNova::RunChunk(L, Chunk);

        World->Tick(LEVELTICK_All, SMALL_NUMBER);

        const auto Actual = lua_tostring(L, -1);
        TEST_EQUAL(Actual, FString("BP ABC Lua"));
    });

    return true;
}

#endif
