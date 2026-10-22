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

// Covers FStaticBindingRegistry - the lookup layer behind the build-time binding tables.
//
// Why this needed its own test: the tables the registry reads are *generated* by
// UnLuaNovaStaticBindingGenerator and are not committed, so in a normal build the registry is
// empty, the fast call path is never taken, and nothing in the suite ever notices if the
// registry itself breaks. These tests register a table by hand instead of depending on one
// having been generated - which is also why the tables below are mutable (see the note).

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#include "StaticBinding/StaticBindingRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    // Deliberately NOT static constexpr: FStaticBindingRegistry::Resolve() writes the
    // resolved UFunction / FProperty pointers, offsets and flags back into these tables
    // (they are registered from static initialisers, long before UObject exists). Read-only
    // storage would be a crash, not a failed test.
    FStaticPropBinding GSpecProps[] = {
        {TEXT("Counter"), 0, EStaticPropKind::Int, nullptr},
        {TEXT("MapForIssue407"), 0, EStaticPropKind::Map, nullptr},
    };

    FStaticParamBinding GSpecParams[] = {
        {TEXT("In"), 0, EStaticPropKind::Int, nullptr},
    };

    FStaticFuncBinding GSpecFuncs[] = {
        {TEXT("ProbeWithStaticBinding"), 0, 0, 1, FStaticFuncFlags(), GSpecParams, nullptr},
    };

    FStaticTypeBinding GSpecType = {
        TEXT("UUnLuaNovaTestStub"), GSpecProps, UE_ARRAY_COUNT(GSpecProps),
        GSpecFuncs, UE_ARRAY_COUNT(GSpecFuncs), nullptr, false
    };
}

BEGIN_DEFINE_SPEC(FStaticBindingSpec, "UnLuaNova.API.StaticBinding", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
END_DEFINE_SPEC(FStaticBindingSpec)

void FStaticBindingSpec::Define()
{
    BeforeEach([this]
    {
        // Class has to be filled before the first lookup: resolution uses it to find the
        // functions, and the table is a static that outlives every test.
        GSpecType.Class = UUnLuaNovaTestStub::StaticClass();
        FStaticBindingRegistry::Get().Register(&GSpecType);
    });

    AfterEach([this]
    {
        FStaticBindingRegistry::Get().Unregister(&GSpecType);
    });

    Describe(TEXT("注册表"), [this]()
    {
        It(TEXT("注册后能按 USTRUCT 和名字查到同一个表"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Registry = FStaticBindingRegistry::Get();

            // Note the asymmetry this pins down: Find(FName) does not drain pending
            // registrations, only the UStruct overload (and FindProp / FindFunc) does.
            // Going through the UStruct overload first is what makes the name lookup work.
            const FStaticTypeBinding* ByStruct = Registry.Find(UUnLuaNovaTestStub::StaticClass());
            TEST_TRUE(ByStruct != nullptr);
            TEST_TRUE(ByStruct->bResolved);

            const FStaticTypeBinding* ByName = Registry.Find(FName(TEXT("UUnLuaNovaTestStub")));
            TEST_TRUE(ByName == ByStruct);
        });

        It(TEXT("取消注册后查不到"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Registry = FStaticBindingRegistry::Get();
            Registry.Find(UUnLuaNovaTestStub::StaticClass()); // force resolution

            Registry.Unregister(&GSpecType);
            TEST_TRUE(Registry.Find(UUnLuaNovaTestStub::StaticClass()) == nullptr);
        });
    });

    Describe(TEXT("解析"), [this]()
    {
        It(TEXT("属性解析到真实的 FProperty"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Registry = FStaticBindingRegistry::Get();
            const FStaticTypeBinding* Type = Registry.Find(UUnLuaNovaTestStub::StaticClass());
            TEST_TRUE(Type != nullptr);

            const FStaticPropBinding* Prop = Registry.FindProp(Type, TEXT("Counter"));
            TEST_TRUE(Prop != nullptr);

            const FProperty* Expected = UUnLuaNovaTestStub::StaticClass()->FindPropertyByName(TEXT("Counter"));
            TEST_TRUE(Prop->Property == Expected);
            TEST_TRUE(Prop->Kind == EStaticPropKind::Int);
            TEST_TRUE(Prop->Offset == Expected->GetOffset_ForGC());
        });

        It(TEXT("函数解析到真实的 UFunction 并填好 flag"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Registry = FStaticBindingRegistry::Get();
            const FStaticTypeBinding* Type = Registry.Find(UUnLuaNovaTestStub::StaticClass());
            TEST_TRUE(Type != nullptr);

            const FStaticFuncBinding* Func = Registry.FindFunc(Type, TEXT("ProbeWithStaticBinding"));
            TEST_TRUE(Func != nullptr);

            UFunction* Expected = UUnLuaNovaTestStub::StaticClass()->FindFunctionByName(TEXT("ProbeWithStaticBinding"));
            TEST_TRUE(Func->Function == Expected);
            TEST_TRUE(Func->ParamsSize == Expected->ParmsSize);

            // These three are what the call path branches on (see FunctionDesc::CallUE).
            TEST_FALSE(Func->Flags.bIsNet);
            TEST_FALSE(Func->Flags.bIsLatent);
            TEST_FALSE(Func->Flags.bInterface);
        });

        It(TEXT("表里写了不存在的成员时跳过而不是崩"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Registry = FStaticBindingRegistry::Get();
            const FStaticTypeBinding* Type = Registry.Find(UUnLuaNovaTestStub::StaticClass());
            TEST_TRUE(Type != nullptr);
            TEST_TRUE(Registry.FindProp(Type, TEXT("NoSuchProperty")) == nullptr);
            TEST_TRUE(Registry.FindFunc(Type, TEXT("NoSuchFunction")) == nullptr);
        });
    });

    Describe(TEXT("属性类型分类"), [this]()
    {
        It(TEXT("常见 UProperty 映射到对应的 Kind"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const UClass* Class = UUnLuaNovaTestStub::StaticClass();

            const FProperty* Counter = Class->FindPropertyByName(TEXT("Counter"));
            const FProperty* Map = Class->FindPropertyByName(TEXT("MapForIssue407"));
            const FProperty* Handler = Class->FindPropertyByName(TEXT("SimpleHandler"));
            const FProperty* Event = Class->FindPropertyByName(TEXT("SimpleEvent"));

            TEST_TRUE(Counter != nullptr && Map != nullptr && Handler != nullptr && Event != nullptr);

            TEST_TRUE(FStaticBindingRegistry::GetPropertyKind(Counter) == EStaticPropKind::Int);
            TEST_TRUE(FStaticBindingRegistry::GetPropertyKind(Map) == EStaticPropKind::Map);
            TEST_TRUE(FStaticBindingRegistry::GetPropertyKind(Handler) == EStaticPropKind::Delegate);
            TEST_TRUE(FStaticBindingRegistry::GetPropertyKind(Event) == EStaticPropKind::MulticastDelegate);
            TEST_TRUE(FStaticBindingRegistry::GetPropertyKind(nullptr) == EStaticPropKind::Unknown);
        });
    });
}

#endif
