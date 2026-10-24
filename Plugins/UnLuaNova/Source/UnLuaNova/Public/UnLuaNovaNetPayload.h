// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

#pragma once

#include "CoreMinimal.h"
#include "UnLuaNovaNetDecl.h"

/**
 * The wire layout of Lua-declared replicated fields.
 *
 * Fields are grouped into one replicated UPROPERTY array per type:
 *
 *     Floats  = [ HP, Mana ]      // two Float declarations, in declaration order
 *     Strings = [ NickName ]
 *
 * A field is addressed as (bucket, index), and the layout is derived locally on both peers
 * from the same Lua declaration, so nothing about it travels on the wire. The one thing that
 * does travel is a 32-bit fingerprint of the layout (SchemaVersion) so a peer running a
 * different build of the script reports a mismatch instead of silently decoding garbage.
 *
 * Push-model dirty tracking operates per type. The layout is fixed at class level and
 * currently accepts only COND_None; conditional declarations are rejected. Legacy/Iris
 * delivery, delta granularity and bandwidth require connected-peer measurements.
 */
namespace UnLuaNova
{
    /** The replicated array a declared field lives in. */
    enum class ELuaNetBucket : uint8
    {
        Bool,
        Int,
        Int64,
        Float,
        Double,
        String,
        Name,
        Vector,
        Rotator,
        Transform,
        Object,

        Max
    };

    /** Where one declared field sits in the payload. */
    struct FLuaNetSlot
    {
        FName Name;
        ELuaNetBucket Bucket = ELuaNetBucket::Int;
        /** Index inside the bucket's array. */
        int32 Index = 0;
        ELuaNetParamType Type = ELuaNetParamType::Int;
        /** UE's own ELifetimeCondition, or COND_None. */
        uint8 Condition = 0;
    };

    /** Which bucket a declared type belongs to. */
    UNLUANOVA_API ELuaNetBucket BucketForType(ELuaNetParamType Type);

    /**
     * Turn declarations into slot addresses. Declaration order decides the index inside a
     * bucket, which is why both peers must declare in the same order - see SchemaVersion.
     */
    UNLUANOVA_API void BuildSlots(const TArray<FLuaRepDecl>& Decls, TArray<FLuaNetSlot>& OutSlots);

    /** How many slots each bucket needs, indexed by ELuaNetBucket. */
    UNLUANOVA_API void CountSlots(const TArray<FLuaNetSlot>& Slots, int32 (&OutCounts)[int32(ELuaNetBucket::Max)]);

    /**
     * Fingerprint of the layout, sent alongside the data.
     *
     * It covers names, order and types - everything the receiving side uses to map an array
     * element back to a Lua field. A hot reload that changes the declaration list changes
     * this, so the other peer says so instead of writing HP into Mana.
     */
    UNLUANOVA_API uint32 ComputeSchemaVersion(const TArray<FLuaRepDecl>& Decls);
}
