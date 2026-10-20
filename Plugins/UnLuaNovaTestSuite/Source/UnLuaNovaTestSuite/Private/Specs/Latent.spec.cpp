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

// Latent functions: the entry checks in Class_CallLatentFunction (Private/LuaCore.cpp).
//
// What is worth locking down here is the *wording* of the two refusals. Both are
// diagnostics for a mistake that is otherwise invisible - a latent call that silently does
// nothing, or a Luau VM error ("attempt to yield across metamethod/C-call boundary") that
// names a mechanism the script author never touched. Rewording either is a behaviour
// change, so the cases assert the text.
//
// Deliberately out of scope here: that the action resumes, that its callback fires exactly
// once, and that it still advances while the world is paused. Those need a real world and a
// real latent action manager, which is what UnLuaNova.Regression.Issue276 already is. A
// unit spec that set all that up would duplicate it with less fidelity.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FLatentSpec, "UnLuaNova.API.Latent", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FLatentSpec)

void FLatentSpec::Define()
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

    Describe(TEXT("主线程调用"), [this]()
    {
        It(TEXT("在主线程里调用 latent 函数会报出契约里的那条消息"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // UKismetSystemLibrary.Delay carries the Latent metadata, so it is dispatched
            // through Class_CallLatentFunction. The main-thread refusal happens before any
            // argument is looked at, which is why the world context can be nil here: the
            // point of the case is the refusal, not the action.
            AddExpectedError(TEXT("Can't call latent action in main lua thread!"),
                EAutomationExpectedErrorFlags::Contains);

            Env->DoString(TEXT("UE.UKismetSystemLibrary.Delay(nil, 1)"));

            // The refusal is a logged error, not a raised one: the chunk itself succeeds.
            // Asserting that keeps the case honest - if the check were removed and the
            // call went on to yield from the main thread, this would stop being true.
            TEST_TRUE(Env->DoString(TEXT("return 1")));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 1);
        });

        It(TEXT("报错之后环境仍然可用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // A refused latent call must not leave the coroutine table or the stack in a
            // state that poisons later calls - FindOrAddThread allocated a slot on the way
            // in, and nothing rolls it back.
            AddExpectedError(TEXT("Can't call latent action in main lua thread!"),
                EAutomationExpectedErrorFlags::Contains);

            const int32 TopBefore = lua_gettop(L);
            Env->DoString(TEXT("UE.UKismetSystemLibrary.Delay(nil, 1)"));

            TEST_EQUAL(lua_gettop(L), TopBefore);
            TEST_TRUE(Env->DoString(TEXT("return 42")));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 42);
        });
    });
}

#endif
