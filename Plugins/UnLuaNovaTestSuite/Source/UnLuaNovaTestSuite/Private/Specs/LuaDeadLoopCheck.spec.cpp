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

#include "LuaDeadLoopCheck.h"
#include "LuaEnv.h"
#include "HAL/IConsoleManager.h"
#include "UnLuaNovaTestHelpers.h"
#include "UnLuaNovaModule.h"
#include "UnLuaNovaSettings.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FLuaDeadLoopCheckSpec, "UnLuaNova.Settings", EAutomationTestFlags::ProductFilter | ApplicationContextMask)

    virtual bool SuppressLogWarnings() override
    {
        return true;
    }

END_DEFINE_SPEC(FLuaDeadLoopCheckSpec)

void FLuaDeadLoopCheckSpec::Define()
{
    Describe(TEXT("DeadLoopCheck"), [this]()
    {
        AfterEach(EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
            Settings.DeadLoopCheck = 0;
        });

        It(TEXT("设置防止无限循环的超时时间"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // a preceding PIE-based test deactivates UnLuaNova on a deferred tick, so the module
            // may still be active here; Startup() would then early-return without applying
            // DeadLoopCheck to FDeadLoopCheck::Timeout and the chunk below never times out
            UnLuaNova::Shutdown();

            auto& Settings = *GetMutableDefault<UUnLuaNovaSettings>();
            Settings.DeadLoopCheck = 1;

            UnLuaNova::Startup();

            const auto Env = IUnLuaNovaModule::Get().GetEnv();
            AddExpectedError(TEXT("timeout"), EAutomationExpectedErrorFlags::Contains);
            const auto Chunk = R"(
                local count = 0
                while true do
                    count = count + 1
                end
            )";
            const int32 Before = UnLuaNova::FDeadLoopCheck::GetTimeoutCount();
            Env->DoString(Chunk);
            // Detection alone is not evidence of cancellation: the hook must have fired.
            TestTrue(TEXT("watchdog recorded a real cancellation"),
                UnLuaNova::FDeadLoopCheck::GetTimeoutCount() > Before);

            UnLuaNova::Shutdown();
        });

        It(TEXT("lua.maxsafepoints 打断死循环，且不污染下一次调用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Constructing a guard registers the CVar. Use a private env so the case does
            // not depend on whichever PIE / module state the previous test left behind.
            auto LocalEnv = MakeShared<UnLuaNova::FLuaEnv>();
            LocalEnv->DoString(TEXT("return 1"));
            auto* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.maxsafepoints"));
            if (!Var)
                return;

            const int32 Saved = Var->GetInt();
            Var->Set(64, ECVF_SetByCode);

            const int32 Before = UnLuaNova::FDeadLoopCheck::FGuard::GetSafePointInterruptCount();
            AddExpectedError(TEXT("maxsafepoints"), EAutomationExpectedErrorFlags::Contains);
            // Bounded on purpose. This still crosses the 64-safe-point budget by a wide
            // margin, but if cancellation ever regresses the chunk simply finishes and
            // the assertions below fail instead of wedging the editor in a genuine
            // infinite loop - which takes the whole Release run down with it.
            const bool bCancelled = !LocalEnv->DoString(
                TEXT("local n = 0 for i = 1, 1000000 do n = n + 1 end return n"));
            const bool bInterrupted =
                UnLuaNova::FDeadLoopCheck::FGuard::GetSafePointInterruptCount() > Before;
            const bool bShort = LocalEnv->DoString(TEXT("return 1 + 1"));

            Var->Set(Saved, ECVF_SetByCode);
            TestTrue(TEXT("the budget cancelled the long-running chunk"), bCancelled);
            TestTrue(TEXT("budget recorded a real interruption"), bInterrupted);
            TestTrue(TEXT("the next call is not capped by a stale counter"), bShort);
        });
    });
}

#endif
