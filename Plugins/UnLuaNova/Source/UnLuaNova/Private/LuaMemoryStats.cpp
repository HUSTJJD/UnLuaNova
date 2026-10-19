// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT

#include "LuaMemoryStats.h"

namespace UnLuaNova
{
    FLuaMemoryStats& FLuaMemoryStats::Get()
    {
        static FLuaMemoryStats Instance;
        return Instance;
    }

    void FLuaMemoryStats::TrackAlloc(size_t OldSize, size_t NewSize)
    {
        if (!bEnabled.load(std::memory_order_relaxed))
            return;

        const int64 Delta = static_cast<int64>(NewSize) - static_cast<int64>(OldSize);
        const int64 Now = CurrentBytes.fetch_add(Delta, std::memory_order_relaxed) + Delta;

        if (OldSize == 0)
            AllocCount.fetch_add(1, std::memory_order_relaxed);
        else
            ReallocCount.fetch_add(1, std::memory_order_relaxed);

        // Peak feeds reporting only, so a relaxed CAS loop is sufficient - no lock needed
        // on what is otherwise a very hot path.
        int64 Observed = PeakBytes.load(std::memory_order_relaxed);
        while (Observed < Now && !PeakBytes.compare_exchange_weak(Observed, Now, std::memory_order_relaxed))
        {
        }
    }

    void FLuaMemoryStats::TrackFree(size_t Size)
    {
        if (!bEnabled.load(std::memory_order_relaxed))
            return;

        CurrentBytes.fetch_sub(static_cast<int64>(Size), std::memory_order_relaxed);
        FreeCount.fetch_add(1, std::memory_order_relaxed);
    }

    void FLuaMemoryStats::Reset()
    {
        CurrentBytes.store(0, std::memory_order_relaxed);
        PeakBytes.store(0, std::memory_order_relaxed);
        AllocCount.store(0, std::memory_order_relaxed);
        ReallocCount.store(0, std::memory_order_relaxed);
        FreeCount.store(0, std::memory_order_relaxed);
    }

    FString FLuaMemoryStats::BuildReport() const
    {
        constexpr double BytesPerMB = 1024.0 * 1024.0;
        return FString::Printf(TEXT("Lua heap : %.2f MB (peak %.2f MB)\nallocs   : %llu\nreallocs : %llu\nfrees    : %llu"),
                               GetCurrentBytes() / BytesPerMB,
                               GetPeakBytes() / BytesPerMB,
                               GetAllocCount(),
                               GetReallocCount(),
                               GetFreeCount());
    }
}
