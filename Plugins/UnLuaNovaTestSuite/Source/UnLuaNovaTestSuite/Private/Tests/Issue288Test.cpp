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

struct FUnLuaNovaTest_Issue288 : FUnLuaNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuaNovaTestBase::SetUp();

        const auto Chunk1 = R"(
        local UMGClass = UE.UClass.Load('/UnLuaNovaTestSuite/Tests/Regression/Issue288/UnLuaNovaTestUMG_Issue288.UnLuaNovaTestUMG_Issue288_C')
        G_UMG = NewObject(UMGClass)
        )";
        UnLuaNova::RunChunk(L, Chunk1);

        auto Texture2D = FindObject<UTexture2D>(nullptr, TEXT("/Game/FPWeapon/Textures/UE4_LOGO_CARD.UE4_LOGO_CARD"));
        RUNNER_TEST_NOT_NULL(Texture2D);

        const auto Chunk2 = R"(
        G_UMG:Release()
        G_UMG = nil
        )";
        UnLuaNova::RunChunk(L, Chunk2);

        lua_gc(L, LUA_GCCOLLECT, 0);
        CollectGarbage(RF_NoFlags, true);
        CollectGarbage(RF_NoFlags, true);

        Texture2D = FindObject<UTexture2D>(nullptr, TEXT("/Game/FPWeapon/Textures/UE4_LOGO_CARD.UE4_LOGO_CARD"));
        if (Texture2D)
            UnLuaNovaTestSuite::PrintReferenceChain(Texture2D);
        RUNNER_TEST_NULL(Texture2D);

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaNovaTest_Issue288, TEXT("UnLuaNova.Regression.Issue288 UMG里Image用到的Texture内存泄漏"))

#endif
