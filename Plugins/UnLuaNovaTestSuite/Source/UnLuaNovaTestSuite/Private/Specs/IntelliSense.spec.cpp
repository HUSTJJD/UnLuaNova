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

// IntelliSense: the declarations the editor writes so that LuaLS / luau-analyze can see UE
// types.
//
// These cases test the string generator rather than the file-writing workflow. That is a
// deliberate split: the workflow needs an asset registry and a place on disk to write to,
// while the generator is the part that actually decides what a script sees - and if it
// starts dropping functions, the symptom is "the editor's completion is wrong", which no
// other test can observe.
//
// Assertions are on names this suite owns (UUnLuaNovaTestStub and its members), so a rename
// in the plugin cannot make them pass or fail for the wrong reason.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "UnLuaNovaIntelliSense.h"

BEGIN_DEFINE_SPEC(FIntelliSenseSpec, "UnLuaNova.Editor.IntelliSense", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FIntelliSenseSpec)

void FIntelliSenseSpec::Define()
{
    Describe(TEXT("类型声明生成"), [this]()
    {
        It(TEXT("UCLASS 的声明包含它的名字"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const FString Decl = UnLuaNova::IntelliSense::Get(UUnLuaNovaTestStub::StaticClass());
            TestTrue(TEXT("声明非空"), !Decl.IsEmpty());
            TestTrue(TEXT("含类型名"), Decl.Contains(TEXT("UUnLuaNovaTestStub")));
        });

        It(TEXT("声明包含真实的 UFUNCTION 与 UPROPERTY"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // A declaration missing a member is worse than none at all: the editor then
            // flags correct code as wrong.
            const FString Decl = UnLuaNova::IntelliSense::Get(UUnLuaNovaTestStub::StaticClass());
            TestTrue(TEXT("含 AddCount"), Decl.Contains(TEXT("AddCount")));
            TestTrue(TEXT("含 Counter"), Decl.Contains(TEXT("Counter")));
            TestTrue(TEXT("含 ProbeWithStaticBinding"), Decl.Contains(TEXT("ProbeWithStaticBinding")));
        });

        It(TEXT("不同类得到不同的声明"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Guards against a generator that caches or stamps one type's declaration onto
            // every request.
            const FString A = UnLuaNova::IntelliSense::Get(UUnLuaNovaTestStub::StaticClass());
            const FString B = UnLuaNova::IntelliSense::Get(UUnLuaNovaTestStubForOverrides::StaticClass());

            TEST_TRUE(A != B);
            TestTrue(TEXT("第二个类也拿到了自己的名字"), B.Contains(TEXT("UUnLuaNovaTestStubForOverrides")));
        });
    });

    Describe(TEXT("函数级辅助"), [this]()
    {
        It(TEXT("IsValid 区分可导出与不可导出的函数"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const UFunction* Func = UUnLuaNovaTestStub::StaticClass()->FindFunctionByName(TEXT("AddCount"));
            TEST_TRUE(Func != nullptr);
            TEST_TRUE(UnLuaNova::IntelliSense::IsValid(Func));

            // Script-internal names must never reach the declaration file, or completion
            // starts offering things that cannot be called.
            TEST_FALSE(UnLuaNova::IntelliSense::IsValidFunctionName(TEXT("__index")));
        });

        It(TEXT("GetTypeName 对属性给出非空类型名"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const FProperty* Prop = UUnLuaNovaTestStub::StaticClass()->FindPropertyByName(TEXT("Counter"));
            TEST_TRUE(Prop != nullptr);
            TEST_FALSE(UnLuaNova::IntelliSense::GetTypeName(Prop).IsEmpty());
        });
    });
}

#endif
