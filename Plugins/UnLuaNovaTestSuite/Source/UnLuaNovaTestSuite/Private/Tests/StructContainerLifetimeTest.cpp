// Licensed under the MIT License. See LICENSE in the repository root.

#include "LuaEnv.h"
#include "UnLuaNovaBase.h"
#include "Misc/AutomationTest.h"
#include "Misc/EngineVersionComparison.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStructContainerLifetimeTest,
    "UnLuaNova.Regression.StructContainerLifetime",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)

bool FStructContainerLifetimeTest::RunTest(const FString& Parameters)
{
    UScriptStruct* ElementType = NewObject<UScriptStruct>(GetTransientPackage());
#if UE_VERSION_OLDER_THAN(5, 3, 0)
    auto* TextProperty = new FStrProperty(ElementType, TEXT("Text"), RF_Transient);
#else
    auto* TextProperty = new FStrProperty(ElementType, TEXT("Text"));
#endif
    ElementType->AddCppProperty(TextProperty);
    ElementType->StaticLink(true);

    TWeakObjectPtr<UScriptStruct> WeakType(ElementType);
    auto Env = MakeUnique<UnLuaNova::FLuaEnv>();
    UnLuaNova::PushUObject(Env->GetMainState(), ElementType);
    lua_setglobal(Env->GetMainState(), "LifetimeType");
    TestTrue(TEXT("Create struct array through Lua"), Env->DoString(R"(
        do
            local value = LifetimeType()
            value.Text = string.rep('x', 4096)
            LifetimeArray = UE.TArray(LifetimeType)
            LifetimeArray:Add(value)
        end
        LifetimeType = nil
        collectgarbage('collect')
    )"));

    CollectGarbage(RF_NoFlags, true);
    TestTrue(TEXT("Element type survives while values need its destructor"), WeakType.IsValid());

    TestTrue(TEXT("Read and finalize struct array after UE GC"), Env->DoString(R"(
        assert(LifetimeArray:Get(1).Text == string.rep('x', 4096))
        LifetimeArray = nil
        collectgarbage('collect')
    )"));
    Env.Reset();
    CollectGarbage(RF_NoFlags, true);
    TestFalse(TEXT("Element type is released after the environment"), WeakType.IsValid());
    return true;
}

#endif
