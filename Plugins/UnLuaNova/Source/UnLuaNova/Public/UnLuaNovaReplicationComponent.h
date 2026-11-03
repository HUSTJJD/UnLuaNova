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

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "UnLuaNovaNetBuckets.h"
#include "UnLuaNovaNetPayload.h"
#include "UnLuaNovaReplicationComponent.generated.h"

struct lua_State;

/**
 * How the server finds out that a Lua field changed.
 *
 * Polling is the fallback: it costs a Lua read per field per interval on every replicating
 * actor, whether or not anything is looking at it and whether or not anything changed.
 * Explicit costs nothing per frame but asks the script to say when it changed something,
 * which is what production code should do.
 */
UENUM(BlueprintType)
enum class ELuaNetUpdateMode : uint8
{
    /** Nothing is scanned; the script calls MarkFieldDirty. No per-frame cost at all. */
    Explicit UMETA(DisplayName = "Explicit (MarkFieldDirty)"),

    /** The Lua table is scanned every PollInterval seconds. */
    Polled UMETA(DisplayName = "Polled"),
};

/**
 * Carries the replicated fields a Lua module declared.
 *
 * Why a component and not a base class: replication needs a real UPROPERTY(Replicated)
 * to hang off, and there is no way to conjure one onto an arbitrary UClass at runtime -
 * that is the one thing the override machinery cannot fake, because properties live in
 * the class's memory layout rather than in its function map. A component is the UE-idiomatic
 * way to add a capability to an actor, it works on a blueprint or a native class alike,
 * and it needs exactly one of them rather than one per actor flavour.
 *
 * Usage: add it to the actor (from a blueprint, from C++, or at runtime), declare the
 * fields in the Lua module, and the component does the rest:
 *
 *     function M:GetLifetimeReplicatedProps()
 *         return { { Name = "HP", Type = "Float" } }
 *     end
 *
 *     function M:Initialize()
 *         self.Replication = self:AddComponent(UE.UUnLuaNovaReplicationComponent)
 *     end
 *
 * ## Networking
 *
 * Every declared field lives in one of eleven typed arrays, one per supported type; a field
 * is addressed as (bucket, index) and the layout is derived from the declaration on both
 * peers. See UnLuaNovaNetPayload.h for why it is shaped that way. The two consequences
 * worth knowing:
 *
 * - **push model**: every bucket is registered `bIsPushBased` and is dirtied individually,
 *   so changing a Float does not make the replication system look at the actor's Strings.
 *   A change that nobody marks dirty is simply not sent - which is why UpdateMode =
 *   Explicit is the mode that actually pays off. This per-type granularity applies to
 *   unconditional fields; a conditional group is a single property and is dirtied as a whole.
 * - **Conditions**: None, InitialOnly, OwnerOnly, SkipOwner, SimulatedOnly, AutonomousOnly,
 *   SimulatedOrPhysics and InitialOrOwner are supported. Each has its own storage group
 *   (see UnLuaNovaNetBuckets.h) because UE decides what to send from the property's own
 *   registration, which is cached per class and cannot depend on a script value.
 *   Custom / Dynamic are rejected: the engine refuses them on subobjects, and this is a
 *   component. Never / NetGroup and the Replay* family are rejected too - none of them has a
 *   storage group here. Their names still parse, so a typo and an unsupported condition
 *   produce two distinct errors rather than one vague "unknown condition".
 *   Legacy, Iris and push-model delivery still require connected-peer acceptance tests.
 */
UCLASS(ClassGroup = (UnLuaNova), meta = (BlueprintSpawnableComponent))
class UNLUANOVA_API UUnLuaNovaReplicationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnLuaNovaReplicationComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * Push one declared field to the wire now, instead of waiting for the next poll.
     *
     * This is the whole point of UpdateMode = Explicit: the script knows when it changed
     * something, so it says so and nothing is scanned. In Polled mode it is still useful -
     * it is how a change gets out before the interval elapses.
     */
    UFUNCTION(BlueprintCallable, Category = "UnLuaNova|Net")
    void MarkFieldDirty(FName FieldName);

    /** Scan every declared field and push whatever changed. */
    UFUNCTION(BlueprintCallable, Category = "UnLuaNova|Net")
    void MarkAllFieldsDirty();

    /** Send what has accumulated this frame instead of waiting for NetUpdateFrequency. */
    UFUNCTION(BlueprintCallable, Category = "UnLuaNova|Net")
    void FlushNetUpdate();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnLuaNova|Net")
    ELuaNetUpdateMode UpdateMode = ELuaNetUpdateMode::Polled;

    /** Seconds between scans in Polled mode. 0 scans every tick. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnLuaNova|Net")
    float PollInterval = 0.1f;

    /**
     * Obsolete component-wide condition, retained only to recognise old assets. A non-None
     * value logs a Warning at BeginPlay and is ignored - it no longer disables replication.
     * Conditions are declared per field in the Lua module instead.
     */
    UPROPERTY(VisibleAnywhere, Category = "UnLuaNova|Net")
    TEnumAsByte<ELifetimeCondition> Condition;

protected:
    UFUNCTION() void OnRep_Bools();
    UFUNCTION() void OnRep_Ints();
    UFUNCTION() void OnRep_Int64s();
    UFUNCTION() void OnRep_Floats();
    UFUNCTION() void OnRep_Doubles();
    UFUNCTION() void OnRep_Strings();
    UFUNCTION() void OnRep_Names();
    UFUNCTION() void OnRep_Vectors();
    UFUNCTION() void OnRep_Rotators();
    UFUNCTION() void OnRep_Transforms();
    UFUNCTION() void OnRep_Objects();
    UFUNCTION() void OnRep_Generics();
    UFUNCTION() void OnRep_SchemaVersion();

    // One RepNotify per conditional group. A group arrives as one property, so its notify
    // pushes every bucket in it rather than a single type.
    UFUNCTION() void OnRep_GroupInitialOnly();
    UFUNCTION() void OnRep_GroupOwnerOnly();
    UFUNCTION() void OnRep_GroupSkipOwner();
    UFUNCTION() void OnRep_GroupSimulatedOnly();
    UFUNCTION() void OnRep_GroupAutonomousOnly();
    UFUNCTION() void OnRep_GroupSimulatedOrPhysics();
    UFUNCTION() void OnRep_GroupInitialOrOwner();

private:
    friend class FNetSpec;
    /** True when the owner's Lua module actually declared any replicated fields. */
    bool bDeclared = false;

    /** Set once a schema mismatch has been reported, so it is not reported per rep. */
    bool bSchemaMismatchReported = false;

    float PollAccumulator = 0.f;

    /** Field -> (bucket, index), derived from the Lua declaration at BeginPlay. */
    TArray<UnLuaNova::FLuaNetSlot> Slots;

    /**
     * The layout both peers have to agree on: written by the authority, compared on the
     * client. See UnLuaNovaNetPayload.h.
     */
    UPROPERTY(ReplicatedUsing = OnRep_SchemaVersion)
    uint32 SchemaVersion = 0;

    uint32 LocalSchemaVersion = 0;

    // --- the payload -------------------------------------------------------
    //
    // One array per declared type. Deliberately not one struct holding all of them: a
    // UPROPERTY per bucket is what gives push model something to dirty per bucket, a
    // condition per bucket, and a RepNotify per bucket.

    UPROPERTY(ReplicatedUsing = OnRep_Bools)
    TArray<uint8> Bools;

    UPROPERTY(ReplicatedUsing = OnRep_Ints)
    TArray<int32> Ints;

    UPROPERTY(ReplicatedUsing = OnRep_Int64s)
    TArray<int64> Int64s;

    UPROPERTY(ReplicatedUsing = OnRep_Floats)
    TArray<float> Floats;

    UPROPERTY(ReplicatedUsing = OnRep_Doubles)
    TArray<double> Doubles;

    UPROPERTY(ReplicatedUsing = OnRep_Strings)
    TArray<FString> Strings;

    UPROPERTY(ReplicatedUsing = OnRep_Names)
    TArray<FName> Names;

    UPROPERTY(ReplicatedUsing = OnRep_Vectors)
    TArray<FVector> Vectors;

    UPROPERTY(ReplicatedUsing = OnRep_Rotators)
    TArray<FRotator> Rotators;

    UPROPERTY(ReplicatedUsing = OnRep_Transforms)
    TArray<FTransform> Transforms;

    UPROPERTY(ReplicatedUsing = OnRep_Objects)
    TArray<TObjectPtr<UObject>> Objects;

    /**
     * One canonical text payload per declared UStruct / container field, in declaration order.
     * A class cannot gain a UPROPERTY at runtime, so all generic fields share this array;
     * UnLuaNovaNetTypeSpec.h defines the codec and SchemaVersion guards the layout.
     */
    UPROPERTY(ReplicatedUsing = OnRep_Generics)
    TArray<FString> Generics;

    // --- conditional groups ------------------------------------------------
    //
    // One group per supported condition. These are structs rather than more arrays so that a
    // condition costs one property registration, one dirty flag and one RepNotify while still
    // serialising natively (Object references included). Unconditional fields stay on the
    // arrays above, where push model keeps its per-type granularity.

    UPROPERTY(ReplicatedUsing = OnRep_GroupInitialOnly)
    FLuaNetBuckets GroupInitialOnly;

    UPROPERTY(ReplicatedUsing = OnRep_GroupOwnerOnly)
    FLuaNetBuckets GroupOwnerOnly;

    UPROPERTY(ReplicatedUsing = OnRep_GroupSkipOwner)
    FLuaNetBuckets GroupSkipOwner;

    UPROPERTY(ReplicatedUsing = OnRep_GroupSimulatedOnly)
    FLuaNetBuckets GroupSimulatedOnly;

    UPROPERTY(ReplicatedUsing = OnRep_GroupAutonomousOnly)
    FLuaNetBuckets GroupAutonomousOnly;

    UPROPERTY(ReplicatedUsing = OnRep_GroupSimulatedOrPhysics)
    FLuaNetBuckets GroupSimulatedOrPhysics;

    UPROPERTY(ReplicatedUsing = OnRep_GroupInitialOrOwner)
    FLuaNetBuckets GroupInitialOrOwner;

    // --- internals ---------------------------------------------------------

    /** Storage for one conditional group. Returns null for Group::None, which uses the arrays above. */
    FLuaNetBuckets* GroupStorage(UnLuaNova::ELuaNetConditionGroup Group);

    /**
     * Where a slot's value lives: the unconditional arrays, or its condition's group.
     *
     * @return false when the slot carries a condition this component has no storage for.
     */
    bool ResolveStorage(const UnLuaNova::FLuaNetSlot& Slot, FLuaNetBucketView& OutView);

    /** Mark one unconditional bucket dirty for push model. No-op when push model is compiled out. */
    void MarkBucketDirty(UnLuaNova::ELuaNetBucket Bucket);

    /** Mark one whole conditional group dirty. No-op for Group::None and when compiled out. */
    void MarkGroupDirty(UnLuaNova::ELuaNetConditionGroup Group);

    /** Dirty whatever a slot lives in: its bucket for unconditional fields, its group otherwise. */
    void MarkSlotDirty(const UnLuaNova::FLuaNetSlot& Slot);

    /** Read one declared field out of the Lua table; true when the wire value changed. */
    bool SyncSlot(lua_State* L, int32 TableIdx, const UnLuaNova::FLuaNetSlot& Slot);

    bool PullSlot(int32 SlotIndex);
    void PullAllSlots();

    /** Write one unconditional bucket's fields into the Lua table and fire the RepNotifies. */
    void PushBucket(UnLuaNova::ELuaNetBucket Bucket);

    /** Write every bucket of one conditional group into the Lua table and fire the RepNotifies. */
    void PushGroup(UnLuaNova::ELuaNetConditionGroup Group);

    /** Shared push path. Bucket == nullptr means every bucket of the group. */
    void PushSlots(UnLuaNova::ELuaNetConditionGroup Group, const UnLuaNova::ELuaNetBucket* Bucket);

    /** Write one received value into the Lua table; true when it differed. */
    bool ApplySlot(lua_State* L, int32 TableIdx, const UnLuaNova::FLuaNetSlot& Slot);
};
