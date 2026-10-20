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
        const int64 Delta = static_cast<int64>(NewSize) - static_cast<int64>(OldSize);
        const int64 Now = CurrentBytes.fetch_add(Delta, std::memory_order_relaxed) + Delta;

        if (bEnabled.load(std::memory_order_relaxed))
        {
            if (OldSize == 0)
                AllocCount.fetch_add(1, std::memory_order_relaxed);
            else
                ReallocCount.fetch_add(1, std::memory_order_relaxed);
        }

        // Peak feeds reporting only, so a relaxed CAS loop is sufficient - no lock needed
        // on what is otherwise a very hot path.
        int64 Observed = PeakBytes.load(std::memory_order_relaxed);
        while (Observed < Now && !PeakBytes.compare_exchange_weak(Observed, Now, std::memory_order_relaxed))
        {
        }
    }

    void FLuaMemoryStats::TrackFree(size_t Size)
    {
        CurrentBytes.fetch_sub(static_cast<int64>(Size), std::memory_order_relaxed);
        if (bEnabled.load(std::memory_order_relaxed))
            FreeCount.fetch_add(1, std::memory_order_relaxed);
    }

    void FLuaMemoryStats::Reset()
    {
        PeakBytes.store(CurrentBytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
        AllocCount.store(0, std::memory_order_relaxed);
        ReallocCount.store(0, std::memory_order_relaxed);
        FreeCount.store(0, std::memory_order_relaxed);

        for (int32 i = 0; i < MemCatBuckets; ++i)
            VolumeByMemCat[i].store(0, std::memory_order_relaxed);
        for (int32 i = 0; i < TypeBuckets; ++i)
            VolumeByType[i].store(0, std::memory_order_relaxed);
        for (int32 i = 0; i < TagBuckets; ++i)
            VolumeByTag[i].store(0, std::memory_order_relaxed);
    }

#if UNLUANOVA_HAS_ALLOC_CALLBACKS
    namespace
    {
        // Signature must match Luau's lua_Callbacks::onallocate exactly.
        void OnLuaAllocate(lua_State* L, void* Block, size_t OldSize, size_t NewSize,
                          uint8_t MemCat, int Type, int Tag)
        {
            (void)L;
            (void)Block;
            (void)OldSize;
            FLuaMemoryStats::Get().TrackCategorizedAlloc(
                static_cast<uint8>(MemCat), Type, Tag, NewSize);
        }

        // Luau hands no size to onfree, which is why the categorized view measures
        // allocation volume rather than live bytes - see BuildCategoryReport.
        void OnLuaFree(lua_State* L, void* Block)
        {
            (void)L;
            (void)Block;
        }
    }
#endif

    void FLuaMemoryStats::Attach(lua_State* L)
    {
#if UNLUANOVA_HAS_ALLOC_CALLBACKS
        if (!L || AttachedState == L)
            return;

        Detach();
        AttachedState = L;
        UNLUANOVA_SET_ALLOC_CALLBACKS(L, &OnLuaAllocate, &OnLuaFree);
#endif
    }

    void FLuaMemoryStats::Detach(lua_State* L)
    {
#if UNLUANOVA_HAS_ALLOC_CALLBACKS
        if (!AttachedState || (L && L != AttachedState))
            return;

        UNLUANOVA_SET_ALLOC_CALLBACKS(AttachedState, nullptr, nullptr);
        AttachedState = nullptr;
#endif
    }

    void FLuaMemoryStats::TrackCategorizedAlloc(const uint8 MemCat, const int32 Type, const int32 Tag, const size_t NewSize)
    {
        if (!bEnabled.load(std::memory_order_relaxed))
            return;

        const int64 Size = static_cast<int64>(NewSize);

        if (MemCat < MemCatBuckets)
            VolumeByMemCat[MemCat].fetch_add(Size, std::memory_order_relaxed);

        if (Type >= 0 && Type < TypeBuckets)
            VolumeByType[Type].fetch_add(Size, std::memory_order_relaxed);

        if (Tag >= 0 && Tag < TagBuckets)
            VolumeByTag[Tag].fetch_add(Size, std::memory_order_relaxed);
    }

    FString FLuaMemoryStats::BuildCategoryReport(const int32 TopN) const
    {
#if !UNLUANOVA_HAS_ALLOC_CALLBACKS
        return FString(TEXT("Categorized breakdown requires the Luau backend."));
#else
        if (AttachedState == nullptr)
            return FString(TEXT("Not attached - run 'lua.memstats on' first."));

        constexpr double BytesPerMB = 1024.0 * 1024.0;

        auto AppendTop = [&](FString& Out, const TCHAR* Title, const std::atomic<int64>* Slots, const int32 Num)
        {
            struct FItem { int32 Index; int64 Bytes; };

            TArray<FItem> Items;
            Items.Reserve(Num);

            int64 Total = 0;
            for (int32 i = 0; i < Num; ++i)
            {
                const int64 Bytes = Slots[i].load(std::memory_order_relaxed);
                if (Bytes > 0)
                {
                    Items.Add({i, Bytes});
                    Total += Bytes;
                }
            }
            Items.Sort([](const FItem& A, const FItem& B) { return A.Bytes > B.Bytes; });

            Out += FString::Printf(TEXT("\n%s (total %.2f MB)\n"), Title, Total / BytesPerMB);

            if (Items.Num() == 0)
            {
                Out += TEXT("  (none)\n");
                return;
            }

            const int32 Count = FMath::Min(TopN, Items.Num());
            for (int32 i = 0; i < Count; ++i)
            {
                Out += FString::Printf(TEXT("  #%-3d %8.2f MB  (%4.1f%%)\n"),
                                       Items[i].Index,
                                       Items[i].Bytes / BytesPerMB,
                                       Total > 0 ? 100.0 * Items[i].Bytes / Total : 0.0);
            }
        };

        FString Out = TEXT("Allocation volume - churn, not live bytes (live/peak come from the allocator hook):");
        AppendTop(Out, TEXT("by memory category"), VolumeByMemCat, MemCatBuckets);
        AppendTop(Out, TEXT("by Lua type"), VolumeByType, TypeBuckets);
        AppendTop(Out, TEXT("by userdata tag"), VolumeByTag, TagBuckets);
        return Out;
#endif
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
