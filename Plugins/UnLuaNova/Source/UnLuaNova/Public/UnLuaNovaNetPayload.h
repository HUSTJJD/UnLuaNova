// Copyright (c) 2026 UnLuaNova contributors
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

// EXPERIMENTAL API - Lua-declared networking. Interfaces in this header may change
// without a deprecation cycle and are not recommended for production use. See
// Docs/CN/LuaNetworking.md for the supported subset.

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
 * A field also carries a replication condition (UE's own ELifetimeCondition). Conditions are
 * served by giving each supported one its own storage group - see UnLuaNovaNetBuckets.h -
 * so the address of a field is really (condition group, bucket, index).
 *
 * Push-model dirty tracking operates per type for unconditional fields; a conditional group is
 * dirtied as a whole because it is a single property. Legacy/Iris delivery, delta granularity
 * and bandwidth require connected-peer measurements.
 */
namespace UnLuaNova
{
    /**
     * The conditions a Lua field is allowed to declare, as storage groups.
     *
     * Only the conditions that can be honoured by a plain per-class registration are here.
     * COND_Custom and COND_Dynamic are deliberately absent: the engine rejects them on
     * subobjects outright (ActorReplication.cpp: "Custom netconditions do not work with
     * SubObjects"), and Lua replication lives on a component. COND_Never / COND_NetGroup are
     * absent for the same reason - neither describes a per-connection filter this layer can
     * register. The Replay* family is absent because it describes replay visibility, which
     * this layer has no say in and no test for. All of them are recognised by name so a typo
     * and an unsupported condition produce different errors; asking for an unsupported one is
     * an error at declaration time, never a silent downgrade.
     */
    enum class ELuaNetConditionGroup : uint8
    {
        None,
        InitialOnly,
        OwnerOnly,
        SkipOwner,
        SimulatedOnly,
        AutonomousOnly,
        SimulatedOrPhysics,
        InitialOrOwner,

        Max
    };

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
        /**
         * One shared array for every declared UStruct / container field. UPROPERTYs are fixed
         * at class link time, so an arbitrary type cannot get its own typed array; it gets a
         * canonical text payload in this bucket instead (UnLuaNovaNetTypeSpec.h).
         */
        Generic,

        Max
    };

    /** Where one declared field sits in the payload. */
    struct FLuaNetSlot
    {
        FName Name;
        ELuaNetBucket Bucket = ELuaNetBucket::Int;
        /**
         * Index inside the bucket's array, counted within the field's condition group: two
         * OwnerOnly Floats and one unconditional Float each start at index 0 of their own
         * group's Float array.
         */
        int32 Index = 0;
        ELuaNetParamType Type = ELuaNetParamType::Int;
        /** UE's own ELifetimeCondition, or COND_None. */
        uint8 Condition = 0;
        ELuaNetConditionGroup Group = ELuaNetConditionGroup::None;
        /** Normalised type spec, only for Bucket == Generic. */
        FString TypeSpec;
    };

    /** Which bucket a declared type belongs to. */
    UNLUANOVA_API ELuaNetBucket BucketForType(ELuaNetParamType Type);

    /**
     * Translate UE's condition constant into the storage group that serves it.
     *
     * @return false for a condition this layer cannot honour - the caller rejects the
     *         declaration rather than quietly treating it as COND_None.
     */
    UNLUANOVA_API bool ConditionToGroup(uint8 Condition, ELuaNetConditionGroup& OutGroup);

    /** The UE condition a group is registered with. */
    UNLUANOVA_API uint8 GroupToCondition(ELuaNetConditionGroup Group);

    /** Stable name of a group, for logs and for the schema fingerprint's human-readable form. */
    UNLUANOVA_API const TCHAR* GroupName(ELuaNetConditionGroup Group);

    /**
     * Turn declarations into slot addresses. Declaration order decides the index inside a
     * bucket, which is why both peers must declare in the same order - see SchemaVersion.
     *
     * Declarations whose condition has no group are skipped; the caller has already rejected
     * the whole declaration list in that case, so seeing one here means a programming error.
     */
    UNLUANOVA_API void BuildSlots(const TArray<FLuaRepDecl>& Decls, TArray<FLuaNetSlot>& OutSlots);

    /** How many slots each bucket needs, indexed by ELuaNetBucket, across every group. */
    UNLUANOVA_API void CountSlots(const TArray<FLuaNetSlot>& Slots, int32 (&OutCounts)[int32(ELuaNetBucket::Max)]);

    /** How many slots each bucket needs inside each group: [group][bucket]. */
    UNLUANOVA_API void CountSlotsByGroup(const TArray<FLuaNetSlot>& Slots,
        int32 (&OutCounts)[int32(ELuaNetConditionGroup::Max)][int32(ELuaNetBucket::Max)]);

    /**
     * Fingerprint of the layout, sent alongside the data.
     *
     * It covers names, order and types - everything the receiving side uses to map an array
     * element back to a Lua field. A hot reload that changes the declaration list changes
     * this, so the other peer says so instead of writing HP into Mana.
     */
    UNLUANOVA_API uint32 ComputeSchemaVersion(const TArray<FLuaRepDecl>& Decls);
}
