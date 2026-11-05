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
#include "LuaVmHooks.h"
#include "HAL/IConsoleManager.h"
#include "HAL/RunnableThread.h"
#include "UnLuaNovaModule.h"

namespace UnLuaNova
{
    int32 FDeadLoopCheck::Timeout = 0;

    // Process-wide: a timeout is a real cancellation, and "detection exists" is not the
    // same claim as "we can show that scripts were actually interrupted".
    static FThreadSafeCounter GTimeoutCount;
    static FThreadSafeCounter GSafePointInterruptions;

    int32 FDeadLoopCheck::GetTimeoutCount()
    {
        return GTimeoutCount.GetValue();
    }

    int32 FDeadLoopCheck::FGuard::GetSafePointInterruptCount()
    {
        return GSafePointInterruptions.GetValue();
    }

    /**
     * Hard cap on Lua execution per host call, counted in VM safe points. On Luau that is
     * the interrupt callback (loop back-edges and calls); on plain Lua it is the count
     * hook, i.e. actual instructions. 0 disables it.
     *
     * This is deliberately not called an instruction quota on Luau: that VM offers no
     * per-instruction hook, and claiming one would be a stronger guarantee than the code
     * can make. It is still deterministic (unlike the wall-clock watchdog) and it covers
     * tight loops that never yield to the OS.
     */
    static int32 MaxSafePoints()
    {
        static auto CVar = IConsoleManager::Get().RegisterConsoleVariable(
            TEXT("lua.maxsafepoints"),
            0,
            TEXT("Maximum Lua safe points per host call (Luau interrupt / plain Lua "
                 "instruction count). 0 = off (default). Exceeding it raises a Lua error. "
                 "Independent of the time-based DeadLoopCheck."),
            ECVF_Default);
        return CVar->GetInt();
    }

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
        // The safe-point budget is independent of the polling thread: a project may want
        // a deterministic execution cap with DeadLoopCheck=0. Returning an empty pointer
        // whenever the watchdog is off used to make that budget impossible to attach.
        // With both disabled a guard costs one CVar read and no hook registration.
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
        {
            // Nested call. If the outer guard already left and disarmed itself, the
            // inner one has to take over the slot or a long inner call runs unguarded.
            FGuard* Expected = nullptr;
            TimeoutGuard.compare_exchange_strong(Expected, Guard);
            return;
        }
        TimeoutCounter.Set(0);
        TimeoutGuard.store(Guard);
    }

    void FDeadLoopCheck::FRunner::GuardLeave(FGuard* Guard)
    {
        // A guard that finishes normally must disarm itself. The old code only
        // decremented a counter: with nested calls the armed pointer still named the
        // outer guard after its destructor ran, so a later timeout called SetTimeout()
        // on freed memory instead of cancelling the script that was actually running.
        FGuard* Expected = Guard;
        TimeoutGuard.compare_exchange_strong(Expected, nullptr);

        if (GuardCounter.Decrement() == 0)
        {
            TimeoutCounter.Set(0);
            TimeoutGuard.store(nullptr);
        }
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
        if (FRunner* Runner = Owner->GetRunner())
        {
            Runner->GuardEnter(this);
            bRegisteredWithRunner = true;
        }

        const int32 Limit = MaxSafePoints();
        if (Limit <= 0 || !Owner->Env)
            return;

        lua_State* L = Owner->Env->GetMainState();
        if (!L)
            return;

        SafePoints.store(0);
#if UNLUANOVA_HAS_HOOK_EVENT
        const int Mask = LUA_MASKCOUNT; // every instruction on plain Lua
#else
        const int Mask = 0;             // Luau: an interrupt is already a safe point
#endif
        FLuaVmHooks::Get().Add(L, Mask, &BudgetOwner,
            [this, Limit](lua_State* InL, lua_Debug*, int GC)
            {
                // A GC interrupt executes no script instruction, and raising an error
                // from inside the collector would longjmp out of it. Luau reports an
                // ordinary VM safe point as -1, NOT 0: `if (GC)` used to discard every
                // real safe point and left `while true do end` running forever.
                if (FLuaVmHooks::IsGcInterrupt(GC))
                    return;
                if (SafePoints.fetch_add(1) + 1 > Limit)
                {
                    GSafePointInterruptions.Increment();
                    luaL_error(InL, "lua script exceeded lua.maxsafepoints (%d)", Limit);
                }
            });
        bBudgetInstalled = true;
    }

    FDeadLoopCheck::FGuard::~FGuard()
    {
        // Withdraw both listeners. A Lua error raised by either unwinds through lua_pcall,
        // which is inside this guard's scope, so this destructor does run - and leaving
        // the budget listener installed would cap the NEXT call with a stale counter.
        // SetTimeout() registers under `this`; if the guard dies between arming that hook
        // and its firing, the listener would otherwise hold a dangling capture.
        if (Owner->Env)
        {
            if (lua_State* L = Owner->Env->GetMainState())
            {
                FLuaVmHooks::Get().RemoveOwner(L, this);
                if (bBudgetInstalled)
                    FLuaVmHooks::Get().RemoveOwner(L, &BudgetOwner);
            }
        }

        // FindRunner, not GetRunner: a guard being destroyed must not start the thread back
        // up after Shutdown() joined it.
        if (bRegisteredWithRunner)
        {
            if (const auto Runner = Owner->FindRunner())
                Runner->GuardLeave(this);
        }
    }

    void FDeadLoopCheck::FGuard::SetTimeout()
    {
        // Count the cancellation before installing the hook: the hook fires on the VM
        // thread and luaL_error longjmps out of it, so this is the last safe point.
        GTimeoutCount.Increment();

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
            [this](lua_State* InL, lua_Debug*, int GC)
            {
                if (FLuaVmHooks::IsGcInterrupt(GC))
                    return; // never longjmp out of the collector
                FLuaVmHooks::Get().RemoveOwner(InL, this);
                luaL_error(InL, "lua script exec timeout");
            });
    }
}
