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

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "LuaBackend.h"
#include <atomic>

namespace UnLuaNova
{
    class FLuaEnv;

    /**
     * Watchdog that breaks out of a Lua script that runs forever.
     *
     * One polling thread for the whole process - not one per FLuaEnv.
     *
     * Environments come and go with every PIE session, so a thread per environment is a
     * thread per session (and per test that builds its own world). A full suite creates
     * hundreds of them; one run leaked 453. That matters because the editor build of
     * WindowsPlatformTLS keeps a fixed pool of 512 thread slots
     * (WindowsPlatformTLS_Private::TIndices<512>) and AllocThreadStorage() pops from it with
     * AtomicQueue::pop(), which spins forever on an empty queue - no timeout, no log. Once
     * the pool is gone every thread created afterwards hangs at startup, and the game
     * thread deadlocks waiting for one of them
     * (FOnlineSubsystemNull::Shutdown -> FRunnableThreadWin::Kill -> WaitForSingleObject).
     * The editor then stops ticking and stops logging with no error anywhere.
     *
     * The thread is created on the first guard and stays until Shutdown(), so a run with
     * DeadLoopCheck=0 costs no thread at all.
     */
    class FDeadLoopCheck
    {
    public:
        static int32 Timeout; // in seconds
        
        class FGuard final
        {
        public:
            explicit FGuard(FDeadLoopCheck* Owner);

            ~FGuard();

            void SetTimeout();

            FDeadLoopCheck* GetOwner() const { return Owner; }

        private:

            FDeadLoopCheck* Owner;
        };

        class FRunner final : public FRunnable
        {
        public:
            explicit FRunner();

            virtual uint32 Run() override;

            virtual void Stop() override;

            void GuardEnter(FGuard* Guard);

            void GuardLeave();

            /**
             * Forget a guard that is still armed for an environment which is being destroyed.
             *
             * The runner is shared and outlives any single FLuaEnv, so without this the poll
             * loop can hand a stale guard to SetTimeout() after its env is gone.
             */
            void ClearPending(const FDeadLoopCheck* Owner);

            /**
             * Ask the poll loop to finish and wait for it.
             *
             * The runner is owned by the caller (see FDeadLoopCheck::Shutdown), unlike the
             * old per-env runner which deleted itself from Exit().
             */
            void Shutdown();

        private:
            FThreadSafeBool bRunning;
            FRunnableThread* Thread;
            FThreadSafeCounter GuardCounter;
            FThreadSafeCounter TimeoutCounter;
            std::atomic<FGuard*> TimeoutGuard;
        };
        
        explicit FDeadLoopCheck(FLuaEnv* Env);

        ~FDeadLoopCheck();

        TUniquePtr<FGuard> MakeGuard();

        /**
         * Stop the shared watchdog thread. Called when the module unloads; safe when no
         * thread was ever created.
         */
        static void Shutdown();

    private:
        // Creates the shared runner on first use; nullptr while the watchdog is disabled.
        static FRunner* GetRunner();

        // Same, but never creates one - for destructors, which must not resurrect it.
        static FRunner* FindRunner();

        FLuaEnv* Env;
    };
}
