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

#include "Blueprint/UserWidget.h"
#include "UnLuaNovaTestCommon.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FUnLuaNovaTest_Issue445 : FUnLuaNovaTestBase
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
        World->SetBegunPlay(true);

        const auto ActorClass = LoadClass<AUnLuaTestActor>(World, TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue445/BP_UnLuaTestActor_Issue445.BP_UnLuaTestActor_Issue445_C"));
        const auto Actor = (AUnLuaTestActor*)World->SpawnActor(ActorClass);
        const auto Result = Actor->TestForIssue445(1);
        RUNNER_TEST_TRUE(Result!= nullptr)
        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuaNovaTest_Issue445, TEXT("UnLuaNova.Regression.Issue445 TSubclassOf作为函数返回值，从lua传回C++为空"))

#endif
