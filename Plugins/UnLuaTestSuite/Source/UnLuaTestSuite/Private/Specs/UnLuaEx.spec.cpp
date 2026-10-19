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

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUnLuauNovaBindingSpec, "UnLuauNova.Binding", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuauNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FUnLuauNovaBindingSpec)

void FUnLuauNovaBindingSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuauNova::FLuaEnv>();
        L = Env->GetMainState();
    });

    AfterEach([this]
    {
        Env.Reset();
        L = nullptr;
    });

    Describe(TEXT("Scoped Enum"), [this]
    {
        It(TEXT("Get"), EAsyncExecution::TaskGraphMainThread, [this]
        {
            const auto Chunk = R"(
				local Stub = NewObject(UE.UUnLuauNovaTestStub)
				return Stub.ScopedEnum
			)";
            UnLuauNova::RunChunk(L, Chunk);
            const auto Actual = (EScopedEnum::Type)lua_tointeger(L, -1);
            const auto Expected = EScopedEnum::Value1;
            TEST_EQUAL(Expected, Actual);
        });

        It(TEXT("Set"), EAsyncExecution::TaskGraphMainThread, [this]
        {
            const auto Chunk = R"(
				local Stub = NewObject(UE.UUnLuauNovaTestStub)
				Stub.ScopedEnum = UE.EScopedEnum.Value2
				return Stub.ScopedEnum
			)";
            UnLuauNova::RunChunk(L, Chunk);
            const auto Actual = (EScopedEnum::Type)lua_tointeger(L, -1);
            const auto Expected = EScopedEnum::Value2;
            TEST_EQUAL(Expected, Actual);
        });
    });

    Describe(TEXT("Enum Class"), [this]
    {
        It(TEXT("Get"), EAsyncExecution::TaskGraphMainThread, [this]
        {
            const auto Chunk = R"(
				local Stub = NewObject(UE.UUnLuauNovaTestStub)
				return Stub.EnumClass
			)";
            UnLuauNova::RunChunk(L, Chunk);
            const auto Actual = (EEnumClass)lua_tointeger(L, -1);
            const auto Expected = EEnumClass::Value1;
            TEST_EQUAL(Expected, Actual);
        });

        It(TEXT("Set"), EAsyncExecution::TaskGraphMainThread, [this]
        {
            const auto Chunk = R"(
				local Stub = NewObject(UE.UUnLuauNovaTestStub)
				Stub.EnumClass = UE.EEnumClass.Value2
				return Stub.EnumClass
			)";
            UnLuauNova::RunChunk(L, Chunk);
            const auto Actual = (EEnumClass)lua_tointeger(L, -1);
            const auto Expected = EEnumClass::Value2;
            TEST_EQUAL(Expected, Actual);
        });
    });
}

#endif
