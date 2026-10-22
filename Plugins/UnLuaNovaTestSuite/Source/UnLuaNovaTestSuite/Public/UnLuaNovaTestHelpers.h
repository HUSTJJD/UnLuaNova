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

#pragma once

#include "Tickable.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "UnLuaNova.h"
#include "UnLuaNovaInterface.h"
#include "LuaEnvLocator.h"
#include "LuaModuleLocator.h"
#include "UnLuaNovaTestHelpers.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUnLuaNovaTestSimpleEvent);

DECLARE_DYNAMIC_DELEGATE(FUnLuaNovaTestSimpleHandler);

DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(int32, FUnLuaNovaTestComplexHandler, FString&, Name);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIssue304Event, TArray<FString>, Array);

DECLARE_DYNAMIC_DELEGATE_OneParam(FIssue362Delegate, TArray<int32>&, Array);

#if UE_VERSION_NEWER_THAN(5,5,0)
constexpr EAutomationTestFlags ApplicationContextMask = EAutomationTestFlags_ApplicationContextMask;
#else
constexpr EAutomationTestFlags ApplicationContextMask = EAutomationTestFlags::ApplicationContextMask;
#endif

UENUM()
enum EEnumForIssue331
{
    RECORD_NONE = 0,
    RECORD_TO_FILE = 1 + 2,
    RECORD_TO_LOG = 4,
};

namespace EScopedEnum
{
    enum Type
    {
        Value1 = 1,
        Value2 = 2,
    };
}

enum class EEnumClass
{
    Value1 = 1,
    Value2 = 2
};

UENUM(BlueprintType)
enum class EUnLuaTestEnum : uint8
{
    None = 0 UMETA(DisplayName = "无"),
    Value1 = 1 UMETA(DisplayName = "数值1"),
    Value2 = 2 UMETA(DisplayName = "数值2"),
};

UCLASS(Blueprintable)
class UNLUANOVATESTSUITE_API UUnLuaNovaTestStub : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FUnLuaNovaTestSimpleEvent SimpleEvent;

    UPROPERTY()
    FUnLuaNovaTestSimpleHandler SimpleHandler;

    UPROPERTY()
    FUnLuaNovaTestComplexHandler ComplexHandler;

    UPROPERTY()
    FIssue304Event Issue304Event;

    UPROPERTY()
    FIssue362Delegate Issue362Delegate;

    UPROPERTY()
    int32 Counter;

    EScopedEnum::Type ScopedEnum;

    EEnumClass EnumClass;

    UFUNCTION(BlueprintCallable)
    void AddCount() { Counter++; }

    UPROPERTY()
    TMap<int32, int32> MapForIssue407;

    UUnLuaNovaTestStub()
    {
        Counter = 0;
        ScopedEnum = EScopedEnum::Value1;
        EnumClass = EEnumClass::Value1;
        MapForIssue407.Add(1, 1);
        MapForIssue407.Add(2, 2);
    }

    UFUNCTION(BlueprintCallable)
    int32 TestForIssue407(TArray<int32> Array);

    // Two identical functions for UnLuaNova.API.CallUEFast: only the first one ever gets a
    // static binding registered, so the pre-resolved fast path and the reflective path can
    // be exercised separately and compared. They exist as two functions rather than one
    // because FFunctionDesc captures whether a binding exists when *it* is first built -
    // one function cannot be called through both paths in the same process.
    UFUNCTION(BlueprintCallable)
    int32 ProbeWithStaticBinding(int32 In) { return In + 1; }

    UFUNCTION(BlueprintCallable)
    int32 ProbeWithoutStaticBinding(int32 In) { return In + 1; }
};

UCLASS()
class UNLUANOVATESTSUITE_API UUnLuaNovaTestStubForIssue446 : public UObject, public FTickableGameObject, public IUnLuaNovaInterface
{
    GENERATED_BODY()

public:
    virtual FString GetModuleName_Implementation() const override
    {
        return TEXT("Tests.Regression.Issue446.TestStub");
    }
    
    UFUNCTION(BlueprintImplementableEvent)
    void Test();

    virtual void Tick(float DeltaTime) override
    {
        Test();
    }
    
    virtual TStatId GetStatId() const override
    {
        return GetStatID();
    }
};

// A dedicated stub for the override spec.
//
// Overriding needs an object that actually binds: ULuaModuleLocator::Locate() returns an empty
// module name for a class that does not implement IUnLuaNovaInterface, so neither
// UUnLuaNovaTestStub nor AUnLuaTestActor can be bound and the dispatch behaviour - Lua
// implementation, original implementation, suspend/resume - has nothing to observe.
//
// It is deliberately its own class rather than an extra member on UUnLuaNovaTestStub: adding
// IUnLuaNovaInterface there would make roughly twenty existing NewObject calls try to bind,
// which is a much larger blast radius than one new type.
UCLASS()
class UNLUANOVATESTSUITE_API UUnLuaNovaTestStubForOverrides : public UObject, public IUnLuaNovaInterface
{
    GENERATED_BODY()

public:
    UPROPERTY()
    int32 Counter;

    UUnLuaNovaTestStubForOverrides()
    {
        Counter = 0;
    }

    virtual FString GetModuleName_Implementation() const override
    {
        return TEXT("Tests.Specs.Overrides.OverridesStub");
    }

    // An event: overridable from Lua.
    UFUNCTION(BlueprintImplementableEvent)
    int32 Overridable();

    // A plain callable: not an event, so overriding must reject it.
    UFUNCTION(BlueprintCallable)
    int32 NotOverridable() { return 7; }
};

UCLASS()
class UNLUANOVATESTSUITE_API AUnLuaTestActor : public AActor
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
    int32 TestForIssue300();

    UFUNCTION(BlueprintImplementableEvent)
    bool TestForIssue328();

    UFUNCTION(BlueprintImplementableEvent)
    TSubclassOf<UUserWidget> TestForIssue445(int32 Index);
};

// Locator subclasses for the locators spec. They have to live in a header: UHT does not
// pick up a UCLASS declared inside a .cpp.
// Opts out of static binding generation, so the generator spec can assert that the opt-out
// tag is honoured. Without a type that actually carries the metadata, "excluded types are
// excluded" is not observable.
UCLASS(meta = (UnLuaNovaNoExport))
class UNLUANOVATESTSUITE_API USpecNoExportType : public UObject
{
    GENERATED_BODY()
};

UCLASS()
class UNLUANOVATESTSUITE_API USpecModuleLocator : public ULuaModuleLocator
{
    GENERATED_BODY()

public:
    virtual FString Locate(const UObject* Object) override
    {
        return TEXT("Spec.Module.FromCustomLocator");
    }
};

UCLASS()
class UNLUANOVATESTSUITE_API USpecEnvLocator : public ULuaEnvLocator
{
    GENERATED_BODY()

public:
    virtual UnLuaNova::FLuaEnv* Locate(const UObject* Object) override
    {
        return Env.Get();
    }
};

USTRUCT(BlueprintType)
struct UNLUANOVATESTSUITE_API FUnLuaNovaTestTableRow : public FTableRowBase
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Level;

    FUnLuaNovaTestTableRow() : Level(0)
    {
    }
};

struct UNLUANOVATESTSUITE_API FUnLuaNovaTestLib
{
    static void TestForBaseSpec1(int32 A, int32& B, const int32& C, FString& D)
    {
        B++;
        D += "Flag";
    }

    static bool TestForBaseSpec2(int32 A, int32& B, int32& C)
    {
        B++;
        C++;
        return true;
    }
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FIssule294Event, int32, Value1, UObject*, Value2);

UCLASS()
class UNLUANOVATESTSUITE_API UUnLuaNovaTestFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

    UFUNCTION(BlueprintCallable)
    static void TestForBaseSpec1(int32 A, int32& B, const int32& C, FString& D)
    {
        B++;
        D += "Flag";
    }

    UFUNCTION(BlueprintCallable)
    static bool TestForBaseSpec2(int32 A, int32& B, int32& C)
    {
        B++;
        C++;
        return true;
    }

    UFUNCTION(BlueprintCallable)
    static int32 TestForIssue293(const FString& A, int32 B, const TArray<FColor>& C) { return C.Num(); }

    UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "Event, Array"))
    static int32 TestForIssue294(const FString& A, int32 B, const FIssule294Event& Event, const TArray<FColor>& Array)
    {
        Event.ExecuteIfBound(1, StaticClass());
        return Array.Num();
    }

    UFUNCTION(BlueprintCallable)
    static bool TestForIssue323(FVector Location = FVector::ZeroVector,
                                FRotator Rotation = FRotator::ZeroRotator,
                                FVector2D Vector2D = FVector2D::ZeroVector,
                                FLinearColor LinearColor = FLinearColor::Green,
                                FColor Color = FColor::Blue)
    {
        return Location == FVector::ZeroVector
            && Rotation == FRotator::ZeroRotator
            && Vector2D == FVector2D::ZeroVector
            && LinearColor == FLinearColor::Green
            && Color == FColor::Blue;
    }

    UFUNCTION(BlueprintCallable)
    static bool TestForIssue331(EEnumForIssue331 InEnum = EEnumForIssue331::RECORD_TO_FILE)
    {
        return InEnum == EEnumForIssue331::RECORD_TO_FILE;
    }

    UFUNCTION(BlueprintCallable)
    static void TestForIssue376(UPARAM(ref) FUnLuaNovaTestTableRow& Struct)
    {
        Struct.Level = 100;
    }
};

#if WITH_DEV_AUTOMATION_TESTS

#define TEST_TRUE(expression) \
EPIC_TEST_BOOLEAN_(TEXT(#expression), expression, true)

#define TEST_FALSE(expression) \
EPIC_TEST_BOOLEAN_(TEXT(#expression), expression, false)

#define TEST_EQUAL(expression, expected) \
EPIC_TEST_BOOLEAN_(TEXT(#expression), expression, expected)

#define TEST_QUAT_EQUAL(expression, expected) \
EPIC_TEST_BOOLEAN_(TEXT(#expression), FQuat::ErrorAutoNormalize(expression, expected) < 0.0001, true)

#define EPIC_TEST_BOOLEAN_(text, expression, expected) \
TestEqual(text, expression, expected);

#endif
