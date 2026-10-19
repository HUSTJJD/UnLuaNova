// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT

#pragma once

#include "CoreMinimal.h"

#include <atomic>

namespace UnLuaNova
{
    /**
     * Tracks Lua heap usage.
     *
     * Fed from FLuaEnv::DefaultLuaAllocator, which every supported backend routes through
     * (see FLuaEnv::GetLuaAllocator), so this behaves identically on Lua 5.5 and Luau.
     *
     * It is a process-wide singleton rather than per-env on purpose: the allocator is a
     * free function with no env pointer, and the question this answers is usually "how
     * much Lua memory is the process using" - not "which env".
     *
     * Off by default (see bEnabled) because the hooks cost a couple of atomics on every
     * single allocation. Enable it with 'lua.memstats on'.
     */
    class FLuaMemoryStats
    {
    public:
        static FLuaMemoryStats& Get();

        /** Hot path - called for every allocation / reallocation. */
        void TrackAlloc(size_t OldSize, size_t NewSize);

        /** Hot path - called for every free. */
        void TrackFree(size_t Size);

        uint64 GetCurrentBytes() const { return CurrentBytes.load(std::memory_order_relaxed); }
        uint64 GetPeakBytes() const { return PeakBytes.load(std::memory_order_relaxed); }
        uint64 GetAllocCount() const { return AllocCount.load(std::memory_order_relaxed); }
        uint64 GetReallocCount() const { return ReallocCount.load(std::memory_order_relaxed); }
        uint64 GetFreeCount() const { return FreeCount.load(std::memory_order_relaxed); }

        void Reset();

        /** Multi-line summary, sized in MB, for the console. */
        FString BuildReport() const;

        std::atomic<bool> bEnabled{false};

    private:
        std::atomic<int64> CurrentBytes{0};
        std::atomic<int64> PeakBytes{0};
        std::atomic<uint64> AllocCount{0};
        std::atomic<uint64> ReallocCount{0};
        std::atomic<uint64> FreeCount{0};
    };
}
