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
#include "Internationalization/Regex.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Commandlets/UnLuaNovaStaticBindingGeneratorCommandlet.h"

BEGIN_DEFINE_SPEC(FStaticBindingGeneratorSpec, "UnLuaNova.Editor.StaticBindingGenerator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FStaticBindingGeneratorSpec)

// Runs the commandlet into a scratch directory and hands back the generated text.
// Shared by every case below so that "what the generator produced" is defined once.
static bool SpecGenerate(const FString& OutputDir, FString& OutContent)
{
    IFileManager::Get().DeleteDirectory(*OutputDir, false, true);

    UUnLuaNovaStaticBindingGeneratorCommandlet* Commandlet = NewObject<UUnLuaNovaStaticBindingGeneratorCommandlet>();
    const FString Params = FString::Printf(TEXT("-OutputDir=\"%s\" -Selection=Modules -Modules=/Script/UnLuaNovaTestSuite"), *OutputDir);
    if (Commandlet->Main(Params) != 0)
        return false;

    const FString Generated = OutputDir / TEXT("UnLuaNovaStaticBinding_Generated.cpp");
    return FFileHelper::LoadFileToString(OutContent, *Generated);
}

void FStaticBindingGeneratorSpec::Define()
{
    It(TEXT("Generated thunk parameters use qualified enum values"), [this]()
    {
        const FString OutputDir = FPaths::ProjectSavedDir() / TEXT("Static Binding Generator Spec");
        FString Content;
        TEST_TRUE(SpecGenerate(OutputDir, Content));
        TestTrue(TEXT("Int return and argument are qualified"),
            Content.Contains(TEXT("StaticFuncThunk<EStaticPropKind::Int, EStaticPropKind::Int>")));
        TestTrue(TEXT("Generator format participates in the fingerprint"), Content.Contains(TEXT("fingerprint: v2-")));

        FRegexMatcher Thunks(FRegexPattern(TEXT("StaticFuncThunk<([^>]+)>")), Content);
        int32 Count = 0;
        while (Thunks.FindNext())
        {
            ++Count;
            TArray<FString> Arguments;
            Thunks.GetCaptureGroup(1).ParseIntoArray(Arguments, TEXT(","), true);
            for (FString& Argument : Arguments)
            {
                Argument.TrimStartAndEndInline();
                TestTrue(*Argument, Argument.StartsWith(TEXT("EStaticPropKind::")));
            }
        }
        TestTrue(TEXT("At least one generated thunk was checked"), Count > 0);
        IFileManager::Get().DeleteDirectory(*OutputDir, false, true);
    });

    It(TEXT("生成静态绑定表到 -OutputDir 指定的目录"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        const FString OutputDir = FPaths::ProjectSavedDir() / TEXT("StaticBindingGeneratorSpec");
        IFileManager::Get().DeleteDirectory(*OutputDir, false, true);

        UUnLuaNovaStaticBindingGeneratorCommandlet* Commandlet = NewObject<UUnLuaNovaStaticBindingGeneratorCommandlet>();
        const FString Params = FString::Printf(TEXT("-OutputDir=\"%s\" -Selection=Modules -Modules=/Script/UnLuaNovaTestSuite"), *OutputDir);
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

    It(TEXT("生成的是真实内容，而不只是创建了一个文件"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        // "The file exists" passes for a generator that emits an empty file, which would
        // leave the registry empty and the fast call path dead with a green suite. So the
        // assertion is on the *contents* for a type this suite owns.
        const FString OutputDir = FPaths::ProjectSavedDir() / TEXT("StaticBindingGeneratorSpec");
        FString Content;
        TEST_TRUE(SpecGenerate(OutputDir, Content));

        // UE strips the U prefix: UUnLuaNovaTestStub is written out as UnLuaNovaTestStub,
        // and that is the name the generated symbols carry.
        TestTrue(TEXT("类型被选中"), Content.Contains(TEXT("TEXT(\"UnLuaNovaTestStub\")")));
        TestTrue(TEXT("成员函数被导出"), Content.Contains(TEXT("ProbeWithStaticBinding")));
        TestTrue(TEXT("属性带上了种类"), Content.Contains(TEXT("EStaticPropKind::")));
        TestTrue(TEXT("类型描述符已生成"), Content.Contains(TEXT("TypeBinding_UnLuaNovaTestStub")));

        IFileManager::Get().DeleteDirectory(*OutputDir, false, true);
    });

    It(TEXT("opt-out 的类型不会出现在输出里"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        // Selection is opt-out, which makes this the only rule a project actually writes.
        // If the tag were ignored, every type a project deliberately excluded would be
        // compiled into the binding table - and nothing would fail.
        const FString OutputDir = FPaths::ProjectSavedDir() / TEXT("StaticBindingGeneratorSpec");
        FString Content;
        TEST_TRUE(SpecGenerate(OutputDir, Content));

        TestFalse(TEXT("带 UnLuaNovaNoExport 的类型被排除"), Content.Contains(TEXT("SpecNoExportType")));

        IFileManager::Get().DeleteDirectory(*OutputDir, false, true);
    });

    It(TEXT("连续两次生成结果一致"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        // The generated file is committed in real projects, so a generator that is not
        // deterministic produces a diff on every run and buries real changes.
        const FString First = FPaths::ProjectSavedDir() / TEXT("StaticBindingGeneratorSpecA");
        const FString Second = FPaths::ProjectSavedDir() / TEXT("StaticBindingGeneratorSpecB");

        FString A;
        FString B;
        TEST_TRUE(SpecGenerate(First, A));
        TEST_TRUE(SpecGenerate(Second, B));

        TEST_TRUE(A == B);

        IFileManager::Get().DeleteDirectory(*First, false, true);
        IFileManager::Get().DeleteDirectory(*Second, false, true);
    });
}

#endif
