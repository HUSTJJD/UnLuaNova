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

#include "UnLuauNovaBase.h"
#include "UnLuauNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuauNovaTestHelpers.h"
#include "GameFramework/GameModeBase.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUnLuauNovaLibClassSpec, "UnLuauNova.API.UClass", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    lua_State* L;
END_DEFINE_SPEC(FUnLuauNovaLibClassSpec)

void FUnLuauNovaLibClassSpec::Define()
{
    BeforeEach([this]
    {
        UnLuauNova::Startup();
        L = UnLuauNova::GetState();
    });

    Describe(TEXT("Load"), [this]()
    {
        It(TEXT("正确加载蓝图类"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Expected = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/Core/Blueprints/BP_Game.BP_Game_C"));
            UnLuauNova::RunChunk(L, "return UE.UClass.Load('/Game/Core/Blueprints/BP_Game.BP_Game_C')");
            const auto Actual = (UClass*)UnLuauNova::GetUObject(L, -1);
            TEST_EQUAL(Actual, Expected);
        });
    });

    Describe(TEXT("IsChildOf"), [this]()
    {
        It(TEXT("正确判断子类"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local GameModeClass = UE.UClass.Load('/Game/Core/Blueprints/BP_Game.BP_Game_C')\
            return GameModeClass:IsChildOf(UE.AGameModeBase)";
            UnLuauNova::RunChunk(L, Chunk);
            TEST_TRUE(!!lua_toboolean(L, -1));
        });
    });

    Describe(TEXT("GetDefaultObject"), [this]()
    {
        It(TEXT("正确获取类默认对象"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Class = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/Core/Blueprints/BP_Game.BP_Game_C"));
            if (!TestNotNull(TEXT("BP_Game class"), Class))
                return;
            const UObject* Expected = Class->GetDefaultObject();
            const char* Chunk = "\
            local GameModeClass = UE.UClass.Load('/Game/Core/Blueprints/BP_Game.BP_Game_C')\
            return GameModeClass:GetDefaultObject()";
            UnLuauNova::RunChunk(L, Chunk);
            // UObject userdata is managed by FObjectRegistry, so it must be read back
            // with GetUObject; GetPointer only understands raw/smart-pointer userdata
            const UObject* Actual = UnLuauNova::GetUObject(L, -1);
            TEST_EQUAL(Actual, Expected);
        });
    });

    AfterEach([this]
    {
        UnLuauNova::Shutdown();
    });
}

#endif //WITH_DEV_AUTOMATION_TESTS
