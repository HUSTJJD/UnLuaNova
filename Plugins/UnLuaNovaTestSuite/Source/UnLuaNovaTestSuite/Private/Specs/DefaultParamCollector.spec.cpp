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

// Covers the default-parameter collection.
//
// The collector itself runs inside UnrealHeaderTool (module type Program), so it cannot be
// linked or unit-tested from here - what it produces is `DefaultParamCollection.inl` at build
// time. Two things *are* testable, and neither had a test:
//
//   1. that the collection is populated at all (an empty collection means the UHT side did
//      not run, and the symptom is not a build error - it is every default argument silently
//      arriving as zero), and
//   2. that the runtime actually fills omitted arguments from it.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#include "DefaultParamCollection.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDefaultParamCollectorSpec, "UnLuaNova.API.DefaultParamCollector", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FDefaultParamCollectorSpec)

void FDefaultParamCollectorSpec::Define()
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

    Describe(TEXT("集合"), [this]()
    {
        It(TEXT("启动时已填充，且至少有一个函数带默认值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Idempotent (guarded by a static bool), so calling it here is safe.
            CreateDefaultParamCollection();

            TEST_TRUE(GDefaultParamCollection.Num() > 0);

            bool bAnyParameter = false;
            for (const TPair<FName, FFunctionCollection>& ClassPair : GDefaultParamCollection)
            {
                for (const TPair<FName, FParameterCollection>& FuncPair : ClassPair.Value.Functions)
                {
                    if (FuncPair.Value.Parameters.Num() > 0)
                    {
                        bAnyParameter = true;
                        break;
                    }
                }
                if (bAnyParameter)
                    break;
            }

            TEST_TRUE(bAnyParameter);
        });
    });

    Describe(TEXT("调用时填充默认值"), [this]()
    {
        It(TEXT("省略的 C++ 默认参数照样生效"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // TestForIssue323 takes five parameters that all have C++ defaults and returns
            // true only when each of them arrived with that default value.
            UnLuaNova::RunChunk(L, "return UE.UUnLuaNovaTestFunctionLibrary.TestForIssue323()");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("枚举默认值也生效"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return UE.UUnLuaNovaTestFunctionLibrary.TestForIssue331()");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("显式传参会覆盖默认值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Passing a non-default argument has to win over the collected default,
            // otherwise filling defaults would silently rewrite explicit calls.
            UnLuaNova::RunChunk(L, R"(
                return UE.UUnLuaNovaTestFunctionLibrary.TestForIssue331(UE.EEnumForIssue331.RECORD_NONE)
            )");
            TEST_FALSE(lua_toboolean(L, -1));
        });
    });
}

#endif
