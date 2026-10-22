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

// Drives UnLuaNovaStaticBindingGenerator end to end.
//
// It is the only build-time tool with a test, and it had none: the generated tables are not
// committed, so a generator that stopped emitting anything would leave the registry empty and
// the fast call path dead, with the suite still green.
//
// Output goes to a scratch directory - never to Source/<Module>/Generated, which is where it
// would land by default and where it would become real, compiled code with test-shaped
// contents.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Commandlets/UnLuaNovaStaticBindingGeneratorCommandlet.h"

BEGIN_DEFINE_SPEC(FStaticBindingGeneratorSpec, "UnLuaNova.Editor.StaticBindingGenerator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FStaticBindingGeneratorSpec)

void FStaticBindingGeneratorSpec::Define()
{
    It(TEXT("生成静态绑定表到 -OutputDir 指定的目录"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        const FString OutputDir = FPaths::ProjectSavedDir() / TEXT("StaticBindingGeneratorSpec");
        IFileManager::Get().DeleteDirectory(*OutputDir, false, true);

        UUnLuaNovaStaticBindingGeneratorCommandlet* Commandlet = NewObject<UUnLuaNovaStaticBindingGeneratorCommandlet>();
        const FString Params = FString::Printf(TEXT("-OutputDir=%s"), *OutputDir);
        const int32 Result = Commandlet->Main(Params);
        TEST_EQUAL(Result, 0);

        const FString Generated = OutputDir / TEXT("UnLuaNovaStaticBinding_Generated.cpp");
        TestTrue(*Generated, IFileManager::Get().FileExists(*Generated));

        FString Content;
        FFileHelper::LoadFileToString(Content, *Generated);

        // Without the registration macro the file compiles into nothing that the registry
        // ever sees, which is exactly the failure that is otherwise invisible.
        TestTrue(*Generated, Content.Contains(TEXT("UNLUANOVA_REGISTER_STATIC_BINDING(")));
        TestTrue(*Generated, Content.Contains(TEXT("FStaticTypeBinding")));

        // The opt-out rule: internal / deprecated / transient names must not be selected.
        TestFalse(*Generated, Content.Contains(TEXT("SKEL_")));
        TestFalse(*Generated, Content.Contains(TEXT("REINST_")));

        IFileManager::Get().DeleteDirectory(*OutputDir, false, true);
    });
}

#endif
