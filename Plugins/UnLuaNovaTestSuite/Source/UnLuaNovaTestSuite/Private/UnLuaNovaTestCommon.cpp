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
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

bool FUnLuaNovaTestCommand_WaitSeconds::Update()
{
    const float NewTime = FPlatformTime::Seconds();
    return NewTime - StartTime >= Duration;
}

bool FUnLuaNovaTestCommand_WaitOneTick::Update()
{
    if (bAlreadyRun == false)
    {
        bAlreadyRun = true;
        return true;
    }
    return false;
}

bool FUnLuaNovaTestCommand_SetUpTest::Update()
{
    return UnLuaNovaTest && UnLuaNovaTest->SetUp();
}

bool FUnLuaNovaTestCommand_PerformTest::Update()
{
    return UnLuaNovaTest == nullptr || UnLuaNovaTest->Update();
}

bool FUnLuaNovaTestCommand_TearDownTest::Update()
{
    if (!UnLuaNovaTest)
        return false;

    UnLuaNovaTest->AddLatent([this]
    {
        UnLuaNovaTest->TearDown();
        delete UnLuaNovaTest;
        UnLuaNovaTest = nullptr;
    });
    return true;
}

bool FUnLuaNovaTestBase::SetUp()
{
    UnLuaNova::Startup();
    L = UnLuaNova::GetState();

    GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->InitializeStandalone();
    WorldContext = GameInstance->GetWorldContext();

    const auto& MapName = GetMapName();
    if (MapName.IsEmpty())
    {
        if (!WorldContext->World())
        {
            const auto World = UWorld::CreateWorld(EWorldType::Game, false, "UnLuaNovaTest");
            World->SetGameInstance(GameInstance);
            WorldContext->SetCurrentWorld(World);
        }
    }
    else
    {
        if (InstantTest())
        {
            const auto OldWorld = GWorld;
            const FURL URL(*MapName);
            FString Error;
            LoadPackage(nullptr, *URL.Map, LOAD_None);
            FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
            GEngine->LoadMap(*WorldContext, URL, nullptr, Error);
            GWorld = OldWorld;    
        }
        else
        {
            AutomationOpenMap(MapName);
        }
    }

    return true;
}

void FUnLuaNovaTestBase::TearDown()
{
    GameInstance->Shutdown();
    if (InstantTest())
    {
        const auto World = GetWorld();
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        UnLuaNova::Shutdown();
    }
    else
    {
        AddLatent([this]
        {
            UnLuaNova::Shutdown();
        });
    }
}

void FUnLuaNovaTestBase::AddLatent(TFunction<void()>&& Func, float Delay) const
{
    ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestDelayedCallbackLatentCommand(MoveTemp(Func), Delay));
}

UWorld* FUnLuaNovaTestBase::GetWorld() const
{
    return WorldContext->World();
}

void FUnLuaNovaTestBase::SimulateTick(float Seconds, ELevelTick TickType) const
{
    constexpr auto Step = 1.0f / 60.0f;
    const auto World = GetWorld();
    while (Seconds > 0)
    {
        World->Tick(TickType, Step);
        Seconds -= Step;
    }
}

void FUnLuaNovaTestBase::LoadMap(FString MapName) const
{
    const auto OldWorld = GWorld;
    const FURL URL(*MapName);
    FString Error;
    LoadPackage(nullptr, *URL.Map, LOAD_None);
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    GEngine->LoadMap(*WorldContext, URL, nullptr, Error);
    GWorld = OldWorld;
}

void UnLuaNovaTestSuite::PrintReferenceChain(UObject* Target)
{
#if ENGINE_MAJOR_VERSION > 4 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26)
    FReferenceChainSearch Search(Target, EReferenceChainSearchMode::PrintAllResults | EReferenceChainSearchMode::FullChain);
#else
    FReferenceChainSearch Search(Target, EReferenceChainSearchMode::PrintAllResults);
#endif
}

#endif
