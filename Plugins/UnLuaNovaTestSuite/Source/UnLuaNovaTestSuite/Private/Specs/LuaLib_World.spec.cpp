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

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "GameFramework/DefaultPawn.h"
#include "Misc/AutomationTest.h"
#include "Engine.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUnLuaNovaLibWorldSpec, "UnLuaNova.API.World", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    lua_State* L;
    UWorld* World;
END_DEFINE_SPEC(FUnLuaNovaLibWorldSpec)

void FUnLuaNovaLibWorldSpec::Define()
{
    BeforeEach([this]
    {
        UnLuaNova::Startup();
        L = UnLuaNova::GetState();

        World = UWorld::CreateWorld(EWorldType::Game, false, "UnLuaNovaTest");
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(World);

        const FURL URL;
        World->InitializeActorsForPlay(URL);
        World->BeginPlay();

        UnLuaNova::PushUObject(L, World);
        lua_setglobal(L, "World");
    });

    Describe(TEXT("SpawnActor"), [this]
    {
        It(TEXT("创建Actor，仅指定Actor类型"), EAsyncExecution::TaskGraphMainThread, [this]
        {
            const auto Chunk = R"(
            local Actor = World:SpawnActor(UE.AActor)
            return Actor
            )";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Actor = (AActor*)UnLuaNova::GetUObject(L, -1);
            TEST_EQUAL(Actor->GetClass(), AActor::StaticClass());
        });

        It(TEXT("创建Actor，指定初始Transform"), EAsyncExecution::TaskGraphMainThread, [this]
        {
            const auto Chunk = R"(
            local Rotation = UE.FQuat(4,3,2,1)
            local Translation = UE.FVector(1,1,1)
            local Scale = UE.FVector(2,2,2)
            local Transform = UE.FTransform(Rotation, Translation, Scale)
            local Actor = World:SpawnActor(UE.ACharacter, Transform)
            return Actor
            )";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Actor = (AActor*)UnLuaNova::GetUObject(L, -1);
            const auto& Transform = Actor->GetActorTransform();
            TEST_QUAT_EQUAL(Transform.GetRotation(), FQuat(4,3,2,1));
            TEST_EQUAL(Transform.GetTranslation(), FVector(1,1,1));
            TEST_EQUAL(Transform.GetScale3D(), FVector(2,2,2));
        });

        It(TEXT("创建Actor，指定初始Transform/CollisionHandling"), EAsyncExecution::TaskGraphMainThread, [this]
        {
            const auto Chunk = R"(
            local Rotation = UE.FQuat(4,3,2,1)
            local Translation = UE.FVector(1,1,1)
            local Scale = UE.FVector(2,2,2)
            local Transform = UE.FTransform(Rotation, Translation, Scale)
            local Actor = World:SpawnActor(UE.ACharacter, Transform, UE.ESpawnActorCollisionHandlingMethod.AlwaysSpawn)
            return Actor
            )";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Actor = (AActor*)UnLuaNova::GetUObject(L, -1);
            const auto& Transform = Actor->GetActorTransform();
            TEST_QUAT_EQUAL(Transform.GetRotation(), FQuat(4,3,2,1));
            TEST_EQUAL(Transform.GetTranslation(), FVector(1,1,1));
            TEST_EQUAL(Transform.GetScale3D(), FVector(2,2,2));
        });

        It(TEXT("创建Actor，指定初始Transform/CollisionHanding/Owner"), EAsyncExecution::TaskGraphMainThread, [this]
        {
            const auto OwnerActor = World->SpawnActor(AActor::StaticClass());
            UnLuaNova::PushUObject(L, OwnerActor);
            lua_setglobal(L, "G_OwnerActor");

            const auto Chunk = R"(
            local Rotation = UE.FQuat(4,3,2,1)
            local Translation = UE.FVector(1,1,1)
            local Scale = UE.FVector(2,2,2)
            local Transform = UE.FTransform(Rotation, Translation, Scale)
            local Actor = World:SpawnActor(UE.ACharacter, Transform, UE.ESpawnActorCollisionHandlingMethod.AlwaysSpawn, G_OwnerActor)
            return Actor
            )";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Actor = (AActor*)UnLuaNova::GetUObject(L, -1);
            const auto& Transform = Actor->GetActorTransform();
            TEST_QUAT_EQUAL(Transform.GetRotation(), FQuat(4,3,2,1));
            TEST_EQUAL(Transform.GetTranslation(), FVector(1,1,1));
            TEST_EQUAL(Transform.GetScale3D(), FVector(2,2,2));
            TEST_EQUAL(Actor->GetOwner(), OwnerActor);
        });

        It(TEXT("创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator"), EAsyncExecution::TaskGraphMainThread, [this]
        {
            const auto OwnerActor = World->SpawnActor(AActor::StaticClass());
            UnLuaNova::PushUObject(L, OwnerActor);
            lua_setglobal(L, "G_OwnerActor");

            const auto InstigatorPawn = (APawn*)World->SpawnActor(ADefaultPawn::StaticClass());
            UnLuaNova::PushUObject(L, InstigatorPawn);
            lua_setglobal(L, "G_InstigatorPawn");

            const auto Chunk = R"(
            local Rotation = UE.FQuat(4,3,2,1)
            local Translation = UE.FVector(1,1,1)
            local Scale = UE.FVector(2,2,2)
            local Transform = UE.FTransform(Rotation, Translation, Scale)
            local Actor = World:SpawnActor(UE.ACharacter, Transform, UE.ESpawnActorCollisionHandlingMethod.AlwaysSpawn, G_OwnerActor, G_InstigatorPawn)
            return Actor
            )";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Actor = (AActor*)UnLuaNova::GetUObject(L, -1);
            const auto& Transform = Actor->GetActorTransform();
            TEST_QUAT_EQUAL(Transform.GetRotation(), FQuat(4,3,2,1));
            TEST_EQUAL(Transform.GetTranslation(), FVector(1,1,1));
            TEST_EQUAL(Transform.GetScale3D(), FVector(2,2,2));
            TEST_EQUAL(Actor->SpawnCollisionHandlingMethod, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            TEST_EQUAL(Actor->GetOwner(), OwnerActor);
            TEST_EQUAL(Actor->GetInstigator(), InstigatorPawn);
        });

        It(TEXT("创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator/LuaModule"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The module name is SpawnActor's seventh argument: the actor is bound to it as
            // it is created, which is the whole point of dynamic binding. The module's
            // Initialize() has run by the time SpawnActor returns, so reading a field it
            // sets is what proves the binding happened - not just that the actor exists.
            const auto Chunk = R"(
            local Transform = UE.FTransform(UE.FQuat(4,3,2,1), UE.FVector(1,1,1), UE.FVector(2,2,2))
            G_Actor = World:SpawnActor(UE.AActor, Transform, UE.ESpawnActorCollisionHandlingMethod.AlwaysSpawn, nil, nil, "Tests.Binding.BP_UnLuaTestActor")
            return G_Actor.InitializeCalled
            )";
            UnLuaNova::RunChunk(L, Chunk);
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("创建Actor，自定义构造参数"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The eighth argument is an initializer table, handed to the module's
            // Initialize() - the only way to give a freshly spawned actor values that are
            // not on the class. Asserted on the value coming back out of Lua, so the case
            // covers the whole trip rather than just the call not raising.
            const auto Chunk = R"(
            local Transform = UE.FTransform()
            G_Actor = World:SpawnActor(UE.AActor, Transform, UE.ESpawnActorCollisionHandlingMethod.AlwaysSpawn, nil, nil, "Tests.Specs.World.SpawnWithInitializer", { Counter = 7 })
            return G_Actor.Initializer.Counter
            )";
            UnLuaNova::RunChunk(L, Chunk);
            TEST_EQUAL((int32)lua_tointeger(L, -1), 7);
        });
    });

    AfterEach([this]
    {
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        UnLuaNova::Shutdown();
    });
}

#endif //WITH_DEV_AUTOMATION_TESTS
