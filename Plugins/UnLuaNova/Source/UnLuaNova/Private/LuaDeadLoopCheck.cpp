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
#include "LuaVmHooks.h"
#include "HAL/RunnableThread.h"
#include "UnLuaNovaModule.h"

namespace UnLuaNova
{
    int32 FDeadLoopCheck::Timeout = 0;

    FDeadLoopCheck::FDeadLoopCheck(FLuaEnv* Env)
        : Env(Env)
    {
        Runner = new FRunner();
    }

    FDeadLoopCheck::~FDeadLoopCheck()
    {
        // Nothing outside this object ever stopped the runner, so ~FLuaEnv deleting us
        // used to leave the thread running with a dangling Env. Join it here instead.
        if (Runner)
        {
            Runner->Shutdown();
            Runner = nullptr;
        }
    }

    TUniquePtr<FDeadLoopCheck::FGuard> FDeadLoopCheck::MakeGuard()
    {
        if (Timeout <= 0)
            return TUniquePtr<FGuard>();
        return MakeUnique<FGuard>(this);
    }

    FDeadLoopCheck::FRunner::FRunner()
        : bRunning(true),
          GuardCounter(0),
          TimeoutCounter(0),
          TimeoutGuard(nullptr)
    {
        Thread = FRunnableThread::Create(this, TEXT("LuaDeadLoopCheck"), 0, TPri_BelowNormal);
    }

    uint32 FDeadLoopCheck::FRunner::Run()
    {
        while (bRunning)
        {
            FPlatformProcess::Sleep(1.0f);
            if (GuardCounter.GetValue() == 0)
                continue;

            TimeoutCounter.Increment();
            if (TimeoutCounter.GetValue() < Timeout)
                continue;

            const auto Guard = TimeoutGuard.load();
            if (Guard)
            {
                TimeoutGuard.store(nullptr);
                Guard->SetTimeout();
            }
        }
        return 0;
    }

    void FDeadLoopCheck::FRunner::Stop()
    {
        bRunning = false;
        TimeoutGuard.store(nullptr);
    }

    void FDeadLoopCheck::FRunner::Exit()
    {
        delete this;
    }

    void FDeadLoopCheck::FRunner::GuardEnter(FGuard* Guard)
    {
        if (GuardCounter.Increment() > 1)
            return;
        TimeoutCounter.Set(0);
        TimeoutGuard.store(Guard);
    }

    void FDeadLoopCheck::FRunner::GuardLeave()
    {
        GuardCounter.Decrement();
    }

    void FDeadLoopCheck::FRunner::Shutdown()
    {
        Stop();

        if (!Thread)
            return;

        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;

        // Run() has returned by now, and Exit() deletes the runner on the way out.
    }

    FDeadLoopCheck::FGuard::FGuard(FDeadLoopCheck* Owner)
        : Owner(Owner)
    {
        Owner->Runner->GuardEnter(this);
    }

    FDeadLoopCheck::FGuard::~FGuard()
    {
        Owner->Runner->GuardLeave();
    }

    void FDeadLoopCheck::FGuard::SetTimeout()
    {
        const auto L = Owner->Env->GetMainState();

        // Register with FLuaVmHooks rather than calling lua_sethook ourselves. The old
        // lua_gethook() check made the watchdog a silent no-op whenever the profiler had
        // already taken the slot, and on Luau safe points run through
        // lua_Callbacks::interrupt rather than lua_sethook at all.
        FLuaVmHooks::Get().Add(L,
#if UNLUANOVA_HAS_HOOK_EVENT
                               LUA_MASKLINE,
#else
                               0, // Luau: an interrupt is already a safe point, no mask
#endif
                               this,
            [this](lua_State* InL, lua_Debug*)
            {
                FLuaVmHooks::Get().RemoveOwner(InL, this);
                luaL_error(InL, "lua script exec timeout");
            });
    }
}
