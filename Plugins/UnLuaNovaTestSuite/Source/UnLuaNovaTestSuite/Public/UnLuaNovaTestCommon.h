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

#include "UnLuaNova.h"
#include "Engine.h"
#include "GameFramework/GameStateBase.h"

#include "Dom/JsonObject.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FUnLuaNovaTestCommand_WaitSeconds, float, Duration);

class FUnLuaNovaTestCommand_WaitOneTick : public IAutomationLatentCommand
{
public:
    FUnLuaNovaTestCommand_WaitOneTick()
        : bAlreadyRun(false)
    {
    }

    virtual bool Update() override;
private:
    bool bAlreadyRun;
};

class FUnLuaNovaTestDelayedCallbackLatentCommand : public IAutomationLatentCommand
{
public:
    FUnLuaNovaTestDelayedCallbackLatentCommand(TFunction<void()>&& InCallback, float InDelay = 0.1f)
        : Callback(MoveTemp(InCallback)), Delay(InDelay)
    {
        //Set the start time
        StartTime = FPlatformTime::Seconds();
    }

    virtual bool Update() override
    {
        const double NewTime = FPlatformTime::Seconds();
        if (NewTime - StartTime >= static_cast<double>(Delay))
        {
            Callback();
            return true;
        }
        return false;
    }

private:
    TFunction<void()> Callback;
    float Delay;
};

class UNLUANOVATESTSUITE_API FUnLuaNovaTestCommand_LoadMap : public IAutomationLatentCommand
{
public:
    FUnLuaNovaTestCommand_LoadMap(FWorldContext* WorldContext, FString MapName)
        : MapName(MapName), WorldContext(WorldContext), bMapLoadingStarted(false), bMapLoaded(false)
    {
    }

    virtual bool Update() override
    {
        if (!bMapLoadingStarted)
        {
            const auto OldWorld = GWorld;
            const FURL URL(*MapName);
            FString Error;
            LoadPackage(nullptr, *URL.Map, LOAD_None);
            FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
            GEngine->LoadMap(*WorldContext, URL, nullptr, Error);
            GWorld = OldWorld;
            bMapLoadingStarted = true;
            return false;
        }

        if (!bMapLoaded)
        {
            if (!bMapLoadingStarted)
                return false;

            const auto TestWorld = WorldContext->World();
            if (!TestWorld)
                return false;

            if (TestWorld && TestWorld->AreActorsInitialized())
            {
                AGameStateBase* GameState = TestWorld->GetGameState();
                if (GameState && GameState->HasMatchStarted())
                {
                    return true;
                }
            }

            return false;
        }

        return false;
    }

private:
    FString MapName;
    FWorldContext* WorldContext;
    bool bMapLoadingStarted;
    bool bMapLoaded;
};

struct UNLUANOVATESTSUITE_API FUnLuaNovaTestBase
{
public:
    virtual ~FUnLuaNovaTestBase()
    {
    }

    virtual bool InstantTest() { return false; }

    virtual bool SetUp();

    virtual bool Update() { return true; }

    virtual void TearDown();

    virtual void SetTestRunner(FAutomationTestBase& AutomationTestInstance) { TestRunner = &AutomationTestInstance; }

    void AddLatent(TFunction<void()>&& Func, float Delay = 0.1f) const;

protected:
    FUnLuaNovaTestBase() : L(nullptr), TestRunner(nullptr), GameInstance(nullptr), WorldContext(nullptr)
    {
    }

    virtual FString GetMapName() { return ""; }

    FAutomationTestBase& GetTestRunner() const
    {
        check(TestRunner);
        return *TestRunner;
    }

    UWorld* GetWorld() const;

    void SimulateTick(float Seconds = SMALL_NUMBER, ELevelTick TickType = LEVELTICK_All) const;

    void LoadMap(FString MapName) const;
    
    lua_State* L;
    FAutomationTestBase* TestRunner;
    UGameInstance* GameInstance;
    FWorldContext* WorldContext;
};

DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(UNLUANOVATESTSUITE_API, FUnLuaNovaTestCommand_SetUpTest, FUnLuaNovaTestBase*, UnLuaNovaTest);

DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(UNLUANOVATESTSUITE_API, FUnLuaNovaTestCommand_PerformTest, FUnLuaNovaTestBase*, UnLuaNovaTest);

DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(UNLUANOVATESTSUITE_API, FUnLuaNovaTestCommand_TearDownTest, FUnLuaNovaTestBase*, UnLuaNovaTest);

/////////////////////////////////////////
// TEST DEFINE MACROS

#define IMPLEMENT_UNLUA_LATENT_TEST(TestClass, PrettyName) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass##_Runner, PrettyName, (EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)) \
bool TestClass##_Runner::RunTest(const FString& Parameters) \
{ \
/* spawn test instance. Setup should be done in test's constructor */ \
TestClass* TestInstance = new TestClass(); \
TestInstance->SetTestRunner(*this); \
/* set up */ \
ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_SetUpTest(TestInstance)); \
/* run latent command to update */ \
ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_PerformTest(TestInstance)); \
/* run latent command to tear down */ \
ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_TearDownTest(TestInstance)); \
return true; \
}

#define IMPLEMENT_UNLUA_INSTANT_TEST(TestClass, PrettyName) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass##Runner, PrettyName, (EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)) \
bool TestClass##Runner::RunTest(const FString& Parameters) \
{ \
bool bSuccess = false; \
/* spawn test instance. */ \
TestClass* TestInstance = new TestClass(); \
TestInstance->SetTestRunner(*this); \
/* set up */ \
if (TestInstance->SetUp()) \
{ \
/* call the instant-test code */ \
bSuccess = TestInstance->InstantTest(); \
/* tear down */ \
TestInstance->TearDown(); \
}\
delete TestInstance; \
return bSuccess; \
}

#define IMPLEMENT_TESTSUITE_TEST(TestClass, PrettyName) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass, PrettyName, (EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter))

#define BEGIN_TESTSUITE(TestClass, PrettyName) \
namespace UnLuaNovaTestSuite { \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass, PrettyName, (EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter))

// A suite that can only run with a real viewport. UMG widgets are not ticked when the editor
// runs with -NullRHI - Slate has nothing to render, so SObjectWidget::Tick never runs - and a
// test that waits for a widget's Tick to happen (Issue434 broadcasts its multicast delegate
// from there) has nothing to wait for; it looks exactly like the delegate binding being
// broken. NonNullRHI is how that is told to the framework: with -NullRHI on the command line
// the test is left out of the run rather than counted as a failure, so RunTests.ps1 reports it
// only when a viewport is available. Screenshots are collected unless -NoScreenshots.
#define BEGIN_VIEWPORT_TESTSUITE(TestClass, PrettyName) \
namespace UnLuaNovaTestSuite { \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass, PrettyName, (EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI))

// Same as BEGIN_TESTSUITE plus ServerContext.
//
// A test that runs *inside a net peer* has to be reachable from whatever that peer is: a
// client is ClientContext, but a dedicated server is ServerContext, and the framework drops
// a test whose flags do not contain the context it is running in - silently, with no
// "test not found" worth the name. A peer test registered with the plain macro therefore
// never runs on the -server side, which shows up as "the peer produced no report" three
// minutes later.
#define BEGIN_TESTSUITE_ANY_CONTEXT(TestClass, PrettyName) \
namespace UnLuaNovaTestSuite { \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass, PrettyName, (EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter))

#define END_TESTSUITE(TestClass) \
}

///////////////////////////////
// RUNNER_TEST MACROS

#define RUNNER_TEST_TRUE(expression)\
    if (!GetTestRunner().TestTrue(TEXT(#expression), (bool)(expression)))\
    {\
        return true;\
    }

#define RUNNER_TEST_FALSE(expression)\
    if (!GetTestRunner().TestFalse(TEXT(#expression), (bool)(expression)))\
    {\
    return true;\
    }

#define RUNNER_TEST_EQUAL(expression, expected)\
    if (!GetTestRunner().TestEqual(TEXT(#expression), expression, expected))\
    {\
    return true;\
    }

#define RUNNER_TEST_NULL(expression)\
    if (!GetTestRunner().TestNull(TEXT(#expression), expression))\
    {\
    return true;\
    }

#define RUNNER_TEST_NOT_NULL(expression)\
    if (!GetTestRunner().TestNotNull(TEXT(#expression), expression))\
    {\
    return true;\
    }

namespace UnLuaNovaTestSuite
{
    void PrintReferenceChain(UObject* Target);

    /**
     * Call an RPC the way a blueprint call would - ProcessEvent, which is the entry that
     * decides whether a FUNC_Net function is executed here or sent to the other peer.
     * Going through CallRemoteFunction instead would test our own idea of when to send
     * rather than the engine's, and the engine's is the one that has to agree on both ends.
     *
     * inline here rather than static in each net test: this module is built with unity
     * files, and a static helper of the same name in two of them is a redefinition the
     * moment they land in the same translation unit.
     */
    inline void InvokeRpc(AActor* Actor, const TCHAR* Name, int32 Arg)
    {
        if (!Actor)
            return;

        UFunction* Function = Actor->FindFunction(FName(Name));
        if (!Function)
            return;

        struct
        {
            int32 Value;
        } Params;
        Params.Value = Arg;
        Actor->ProcessEvent(Function, &Params);
    }

    /** "Name:Id" per RPC - the form that makes a diff readable in a failure message. */
    inline FString DescribeRpcs(const TSharedPtr<FJsonObject>& Report)
    {
        TArray<FString> Parts;
        for (const TSharedPtr<FJsonValue>& Value : Report->GetArrayField(TEXT("Rpcs")))
        {
            const TSharedPtr<FJsonObject> Entry = Value->AsObject();
            Parts.Add(FString::Printf(TEXT("%s:%d"), *Entry->GetStringField(TEXT("Name")),
                (int32)Entry->GetNumberField(TEXT("RpcId"))));
        }
        return FString::Join(Parts, TEXT(", "));
    }
}

#endif //WITH_DEV_AUTOMATION_TESTS
