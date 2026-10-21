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
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUELibSpec, "UnLuaNova.API.UE", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FUELibSpec)

void FUELibSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
    });

    AfterEach([this]
    {
        Env.Reset();
        L = nullptr;
    });

    Describe(TEXT("全局UE命名空间"), [this]()
    {
        It(TEXT("访问反射类"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            Env->DoString("return UE.AActor");
            const auto Result = lua_istable(L, -1);
            TEST_TRUE(Result);
        });

        It(TEXT("访问导出类"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            Env->DoString("return UE.FVector");
            const auto Result = lua_istable(L, -1);
            TEST_TRUE(Result);
        });
        
        It(TEXT("访问反射枚举"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Chunk = R"(
                local Enum = UE.EUnLuaTestEnum
                return Enum.None, Enum.Value2
            )";
            Env->DoString(Chunk);
            // UNLUANOVA_TOINT64, not lua_tointeger: a UEnum entry goes out as a 64-bit
            // integer on Luau (LUA_TINTEGER), and lua_tointeger there reads only doubles
            // and hands back 0 for everything else - so every value read this way looked
            // like the 0 member. None looks like a pass precisely because 0 is None.
            const auto Result1 = (EUnLuaTestEnum)UNLUANOVA_TOINT64(L, -2);
            const auto Result2 = (EUnLuaTestEnum)UNLUANOVA_TOINT64(L, -1);
            TEST_EQUAL(Result1, EUnLuaTestEnum::None);
            TEST_EQUAL(Result2, EUnLuaTestEnum::Value2);
        });
    });
}

#endif
