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
#include "UnLuaNovaTestHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FUnLuaNovaTest_Issue328 : FUnLuaNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuaNovaTestBase::SetUp();

        const auto World = GetWorld();

        const FURL URL;
        World->InitializeActorsForPlay(URL);
        World->BeginPlay();
        World->bBegunPlay = true;
        
        const auto ActorClass = LoadClass<AUnLuaTestActor>(World, TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue328/BP_UnLuaTestActor_Issue328.BP_UnLuaTestActor_Issue328_C"));
        const auto Actor = (AUnLuaTestActor*)World->SpawnActor(ActorClass);
        const auto Result = Actor->TestForIssue328();
        RUNNER_TEST_TRUE(Result);

        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaNovaTest_Issue328, TEXT("UnLuaNova.Regression.Issue328 C++调用LUA覆写带返回值的函数会崩溃"))

#endif //WITH_DEV_AUTOMATION_TESTS
