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
#include "LuaBackend.h"

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
    class UNLUANOVA_API FLuaMemoryStats
    {
    public:
        static FLuaMemoryStats& Get();

        FLuaMemoryStats() { Reset(); }

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

        /**
         * Luau only: install / withdraw the categorized allocation callbacks. No-op on
         * plain Lua, which has no equivalent - there the allocator hook is all there is.
         */
        void Attach(lua_State* L);
        void Detach();

        /** Hot path, Luau only. Driven from onallocate; the VM supplies the classification. */
        void TrackCategorizedAlloc(uint8 MemCat, int32 Type, int32 Tag, size_t NewSize);

        /** Where allocation volume is going, per category / type / userdata tag. */
        FString BuildCategoryReport(int32 TopN = 8) const;

        /** One-shot summary of the allocator hook: live / peak bytes and call counts. */
        FString BuildReport() const;

        std::atomic<bool> bEnabled{false};

    private:
        std::atomic<int64> CurrentBytes{0};
        std::atomic<int64> PeakBytes{0};
        std::atomic<uint64> AllocCount{0};
        std::atomic<uint64> ReallocCount{0};
        std::atomic<uint64> FreeCount{0};

        // Categorized breakdown. Sized to the full domain of each dimension so the hot
        // path is an array index rather than a hash lookup: memcat is a uint8, Lua type
        // tags stay well under 32, and LUA_UTAG_LIMIT is 128.
        static constexpr int32 MemCatBuckets = 256;
        static constexpr int32 TypeBuckets = 32;
        static constexpr int32 TagBuckets = 128;

        std::atomic<int64> VolumeByMemCat[MemCatBuckets];
        std::atomic<int64> VolumeByType[TypeBuckets];
        std::atomic<int64> VolumeByTag[TagBuckets];

        lua_State* AttachedState = nullptr;
    };
}
