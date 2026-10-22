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

    // The one polling thread, shared by every environment. See the note on FDeadLoopCheck:
    // a thread per FLuaEnv is what exhausted the engine's TLS thread pool and hung the
    // editor mid-suite.
    static TUniquePtr<FDeadLoopCheck::FRunner> GDeadLoopRunner;

    FDeadLoopCheck::FRunner* FDeadLoopCheck::FindRunner()
    {
        return GDeadLoopRunner.Get();
    }

    FDeadLoopCheck::FRunner* FDeadLoopCheck::GetRunner()
    {
        if (Timeout <= 0)
            return nullptr; // watchdog off: no thread, and no guard to arm either

        if (!GDeadLoopRunner)
            GDeadLoopRunner = MakeUnique<FRunner>();

        return GDeadLoopRunner.Get();
    }

    void FDeadLoopCheck::Shutdown()
    {
        if (!GDeadLoopRunner)
            return;

        GDeadLoopRunner->Shutdown();
        GDeadLoopRunner.Reset();
    }

    FDeadLoopCheck::FDeadLoopCheck(FLuaEnv* Env)
        : Env(Env)
    {
    }

    FDeadLoopCheck::~FDeadLoopCheck()
    {
        // Nothing to join: the runner belongs to the process, not to this environment.
        // What does have to go is a timeout armed for this environment - the shared runner
        // outlives it, and would otherwise call SetTimeout() on a destroyed FLuaEnv.
        if (const auto Runner = FindRunner())
            Runner->ClearPending(this);
    }

    TUniquePtr<FDeadLoopCheck::FGuard> FDeadLoopCheck::MakeGuard()
    {
        if (!GetRunner())
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

            // Take the guard before firing it, so a second poll cannot fire it again while
            // the hook is being installed.
            if (const auto Guard = TimeoutGuard.exchange(nullptr))
                Guard->SetTimeout();
        }
        return 0;
    }

    void FDeadLoopCheck::FRunner::Stop()
    {
        bRunning = false;
        TimeoutGuard.store(nullptr);
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

    void FDeadLoopCheck::FRunner::ClearPending(const FDeadLoopCheck* Owner)
    {
        auto Guard = TimeoutGuard.load();
        if (Guard && Guard->GetOwner() == Owner)
            TimeoutGuard.compare_exchange_strong(Guard, nullptr);
    }

    void FDeadLoopCheck::FRunner::Shutdown()
    {
        Stop();

        if (!Thread)
            return;

        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;

        // Run() has returned by now, so the runner is idle and safe to delete.
    }

    FDeadLoopCheck::FGuard::FGuard(FDeadLoopCheck* Owner)
        : Owner(Owner)
    {
        Owner->GetRunner()->GuardEnter(this);
    }

    FDeadLoopCheck::FGuard::~FGuard()
    {
        // FindRunner, not GetRunner: a guard being destroyed must not start the thread back
        // up after Shutdown() joined it.
        if (const auto Runner = Owner->FindRunner())
            Runner->GuardLeave();
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
