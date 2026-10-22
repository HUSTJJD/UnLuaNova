// Tencent is pleased to support the open source community by making UNLUANOVA available.
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

#include "UNLUANOVABase.h"
#include "UNLUANOVATemplate.h"
#include "Misc/AutomationTest.h"
#include "UNLUANOVATestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUnLuaNovaEnumSpec, "UnLuaNova.API.Enum", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    lua_State* L;
END_DEFINE_SPEC(FUnLuaNovaEnumSpec)

void FUnLuaNovaEnumSpec::Define()
{
    BeforeEach([this]
    {
        UnLuaNova::Startup();
        L = UnLuaNova::GetState();
    });

    Describe(TEXT("GetNameStringByValue"), [this]()
    {
        It(TEXT("获取枚举的名称"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            return UE.ECollisionResponse:GetNameStringByValue(UE.ECollisionResponse.ECR_Overlap)\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Actual = lua_tostring(L, -1);
            const auto Expected = UEnum::GetValueAsString(ECR_Overlap);
            TEST_EQUAL(Actual, Expected);
        });

        It(TEXT("不存在的枚举值返回nil"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::Push(L, static_cast<int16>(0x7FFF));
            TEST_TRUE(UnLuaNova::IsType(L, -1, UnLuaNova::TType<int16>()));
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 0x7FFFLL);
        });
    });

    Describe(TEXT("GetDisplayNameTextByValue"), [this]()
    {
        It(TEXT("获取枚举的本地化文本"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = R"(
            return UE.ECollisionResponse:GetDisplayNameTextByValue(UE.ECollisionResponse.ECR_Overlap)
            )";
            UnLuaNova::RunChunk(L, Chunk);
#if UNLUANOVA_ENABLE_FTEXT
            const auto Actual = *(FText*)GetCppInstanceFast(L, -1);
            const auto Expected = UEnum::GetDisplayValueAsText(ECR_Overlap);
            TEST_TRUE(Expected.EqualTo(Actual))
#else
            const auto Actual = FUTF8ToTCHAR(lua_tostring(L, -1));
            const auto Expected = UEnum::GetDisplayValueAsText(ECR_Overlap).ToString();
            TEST_EQUAL(Actual.Get(), Expected);
#endif
        });

        It(TEXT("不存在的枚举值返回nil"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::Push(L, static_cast<int16>(0x7FFF));
            TEST_TRUE(UnLuaNova::IsType(L, -1, UnLuaNova::TType<int16>()));
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 0x7FFFLL);
        });
    });

    Describe(TEXT("GetMaxValue"), [this]
    {
        It(TEXT("获取枚举类型的最大值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = R"(
            return UE.ECollisionResponse:GetMaxValue()
            )";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Actual = (int32)lua_tointeger(L, -1);
            const auto Expected = (int32)ECR_MAX;
            TEST_EQUAL(Actual, Expected);
        });
    });

    AfterEach([this]
    {
        UnLuaNova::Shutdown();
    });
}

#endif
