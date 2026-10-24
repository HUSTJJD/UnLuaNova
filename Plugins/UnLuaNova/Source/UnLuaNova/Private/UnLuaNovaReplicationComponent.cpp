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

#include "UnLuaNovaReplicationComponent.h"

#include "LuaCore.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Registries/ObjectRegistry.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaModule.h"
#include "UnLuaNovaNet.h"
#include "UnLuaNovaNetPayload.h"

/**
 * Push model is a compile-time capability (WITH_PUSH_MODEL) that a project can also switch
 * off at runtime (net.IsPushModelEnabled). Both halves matter here:
 *
 *   - compiled out: MARK_PROPERTY_DIRTY does not exist, so the buckets are ordinary polled
 *     properties and the replication system compares them itself. Everything still works,
 *     it just does not get the "only look at what changed" part.
 *   - compiled in but off at runtime: MARK_PROPERTY_DIRTY is a no-op and the same polling
 *     covers us. There is deliberately no MarkNetDirty()/ForceNetUpdate() in the change
 *     path, because that is the thing push model exists to remove.
 */
#if (defined(WITH_PUSH_MODEL) && WITH_PUSH_MODEL) || (defined(UE_ENABLE_PUSH_MODEL) && UE_ENABLE_PUSH_MODEL)
    #define UNLUANOVA_PUSH_MODEL 1
#else
    #define UNLUANOVA_PUSH_MODEL 0
#endif

namespace
{
    /** Puts the Lua table bound to Owner on the stack, and takes it off again. */
    struct FBoundTable
    {
        lua_State* L = nullptr;
        int32 Top = 0;
        int32 TableIdx = 0;
        bool bOk = false;

        explicit FBoundTable(AActor* Owner)
        {
            if (!Owner)
                return;

            IUnLuaNovaModule& Module = IUnLuaNovaModule::Get();
            if (!Module.IsActive())
                return;

            UnLuaNova::FLuaEnv* Env = Module.GetEnv(Owner);
            if (!Env)
                return;

            L = Env->GetMainState();
            const int32 Ref = Env->GetObjectRegistry()->GetBoundRef(Owner);
            if (Ref == LUA_NOREF)
            {
                L = nullptr;
                return;
            }

            Top = lua_gettop(L);
            lua_rawgeti(L, LUA_REGISTRYINDEX, Ref);
            TableIdx = lua_absindex(L, -1);
            bOk = true;
        }

        ~FBoundTable()
        {
            if (L)
                lua_settop(L, Top);
        }

        FBoundTable(const FBoundTable&) = delete;
        FBoundTable& operator=(const FBoundTable&) = delete;
    };

    // --- equality ----------------------------------------------------------
    //
    // FVector/FQuat do not compare usefully with == for this purpose; everything else does.

    template <typename T>
    bool Same(const T& A, const T& B) { return A == B; }

    inline bool Same(const FVector& A, const FVector& B) { return A.Equals(B); }
    inline bool Same(const FRotator& A, const FRotator& B) { return A.Equals(B); }
    inline bool Same(const FTransform& A, const FTransform& B)
    {
        return A.GetTranslation().Equals(B.GetTranslation())
            && A.GetRotation().Equals(B.GetRotation())
            && A.GetScale3D().Equals(B.GetScale3D());
    }

    // --- reading out of Lua ------------------------------------------------
    //
    // Explicit type checks throughout rather than lua_tonumber/lua_tostring: on both
    // backends those convert in place, which would silently rewrite the script's value.

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, uint8& Out)
    {
        lua_getfield(L, TableIdx, Key);
        const int32 Type = lua_type(L, -1);
        const bool bOk = Type == LUA_TBOOLEAN || Type == LUA_TNUMBER;
        if (bOk)
            Out = Type == LUA_TBOOLEAN ? (lua_toboolean(L, -1) ? 1 : 0) : (lua_tonumber(L, -1) != 0.0 ? 1 : 0);
        lua_pop(L, 1);
        return bOk;
    }

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, int32& Out)
    {
        lua_getfield(L, TableIdx, Key);
        const bool bOk = lua_type(L, -1) == LUA_TNUMBER;
        if (bOk)
            Out = int32(lua_tointeger(L, -1));
        lua_pop(L, 1);
        return bOk;
    }

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, int64& Out)
    {
        lua_getfield(L, TableIdx, Key);
        const bool bOk = lua_type(L, -1) == LUA_TNUMBER;
        if (bOk)
            Out = int64(lua_tointeger(L, -1));
        lua_pop(L, 1);
        return bOk;
    }

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, float& Out)
    {
        lua_getfield(L, TableIdx, Key);
        const bool bOk = lua_type(L, -1) == LUA_TNUMBER;
        if (bOk)
            Out = float(lua_tonumber(L, -1));
        lua_pop(L, 1);
        return bOk;
    }

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, double& Out)
    {
        lua_getfield(L, TableIdx, Key);
        const bool bOk = lua_type(L, -1) == LUA_TNUMBER;
        if (bOk)
            Out = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return bOk;
    }

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, FString& Out)
    {
        lua_getfield(L, TableIdx, Key);
        const bool bOk = lua_type(L, -1) == LUA_TSTRING;
        if (bOk)
            Out = UTF8_TO_TCHAR(lua_tostring(L, -1));
        lua_pop(L, 1);
        return bOk;
    }

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, FName& Out)
    {
        lua_getfield(L, TableIdx, Key);
        const bool bOk = lua_type(L, -1) == LUA_TSTRING;
        if (bOk)
            Out = FName(UTF8_TO_TCHAR(lua_tostring(L, -1)));
        lua_pop(L, 1);
        return bOk;
    }

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, UObject*& Out)
    {
        lua_getfield(L, TableIdx, Key);
        Out = UnLuaNova::GetUObject(L, -1);
        lua_pop(L, 1);
        // A missing field is a legitimate "no object", not a failure to read.
        return true;
    }

    /**
     * Read a math struct out of its userdata.
     *
     * The metatable is compared against the registry entry by name rather than trusting the
     * stack position, so a script handing us some other userdata is reported as "could not
     * read" instead of being reinterpreted as a vector.
     */
    bool TryReadStruct(lua_State* L, int32 TableIdx, const char* Key, const char* MetatableName, void* Out, size_t Size)
    {
        lua_getfield(L, TableIdx, Key);

        bool bOk = false;
        if (lua_type(L, -1) == LUA_TUSERDATA && lua_getmetatable(L, -1) != 0)
        {
            lua_getfield(L, LUA_REGISTRYINDEX, MetatableName);
            bOk = lua_rawequal(L, -1, -2) != 0;
            lua_pop(L, 2); // the registry entry and the metatable
        }

        if (bOk)
        {
            void* Ptr = GetCppInstanceFast(L, -1);
            if (Ptr)
                FMemory::Memcpy(Out, Ptr, Size);
            else
                bOk = false;
        }

        lua_pop(L, 1); // the value
        return bOk;
    }

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, FVector& Out)
    {
        return TryReadStruct(L, TableIdx, Key, "FVector", &Out, sizeof(FVector));
    }

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, FRotator& Out)
    {
        return TryReadStruct(L, TableIdx, Key, "FRotator", &Out, sizeof(FRotator));
    }

    bool TryRead(lua_State* L, int32 TableIdx, const char* Key, FTransform& Out)
    {
        return TryReadStruct(L, TableIdx, Key, "FTransform", &Out, sizeof(FTransform));
    }

    // --- writing into Lua --------------------------------------------------

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, uint8 V)
    {
        lua_pushboolean(L, V != 0);
        lua_setfield(L, TableIdx, Key);
    }

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, int32 V)
    {
        lua_pushinteger(L, lua_Integer(V));
        lua_setfield(L, TableIdx, Key);
    }

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, int64 V)
    {
        lua_pushinteger(L, lua_Integer(V));
        lua_setfield(L, TableIdx, Key);
    }

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, float V)
    {
        lua_pushnumber(L, lua_Number(V));
        lua_setfield(L, TableIdx, Key);
    }

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, double V)
    {
        lua_pushnumber(L, lua_Number(V));
        lua_setfield(L, TableIdx, Key);
    }

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, const FString& V)
    {
        const FTCHARToUTF8 Text(*V);
        lua_pushstring(L, Text.Get());
        lua_setfield(L, TableIdx, Key);
    }

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, const FName& V)
    {
        const FTCHARToUTF8 Text(*V.ToString());
        lua_pushstring(L, Text.Get());
        lua_setfield(L, TableIdx, Key);
    }

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, UObject* V)
    {
        UnLuaNova::PushUObject(L, V);
        lua_setfield(L, TableIdx, Key);
    }

    void WriteStruct(lua_State* L, const char* MetatableName, const void* Src, size_t Size, uint8 Padding)
    {
        void* Ptr = NewUserdataWithPadding(L, int32(Size), MetatableName, Padding);
        if (Ptr)
            FMemory::Memcpy(Ptr, Src, Size);
    }

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, const FVector& V)
    {
        WriteStruct(L, "FVector", &V, sizeof(FVector), CalcUserdataPadding<FVector>());
        lua_setfield(L, TableIdx, Key);
    }

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, const FRotator& V)
    {
        WriteStruct(L, "FRotator", &V, sizeof(FRotator), CalcUserdataPadding<FRotator>());
        lua_setfield(L, TableIdx, Key);
    }

    void WriteField(lua_State* L, int32 TableIdx, const char* Key, const FTransform& V)
    {
        WriteStruct(L, "FTransform", &V, sizeof(FTransform), CalcUserdataPadding<FTransform>());
        lua_setfield(L, TableIdx, Key);
    }

    /** Call `Table[FuncName](Table)` if it exists. Returns false when it errored. */
    bool CallLuaMethod(lua_State* L, int32 TableIdx, const char* FuncName, const TCHAR* Context)
    {
        lua_getfield(L, TableIdx, FuncName);
        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            lua_pop(L, 1);
            return true;
        }

        lua_pushvalue(L, TableIdx); // self
        if (lua_pcall(L, 1, 0, 0) != LUA_OK)
        {
            const char* Message = lua_tostring(L, -1);
            UE_LOG(LogUnLuaNova, Warning, TEXT("%s: '%s' raised %s"),
                Context, UTF8_TO_TCHAR(FuncName), UTF8_TO_TCHAR(Message ? Message : ""));
            lua_pop(L, 1); // the error
            return false;
        }

        return true;
    }
}

UUnLuaNovaReplicationComponent::UUnLuaNovaReplicationComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
    Condition = COND_None;
}

void UUnLuaNovaReplicationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
#if UNLUANOVA_PUSH_MODEL
    Params.bIsPushBased = true;
#endif

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::Bool);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Bools, Params);

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::Int);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Ints, Params);

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::Int64);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Int64s, Params);

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::Float);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Floats, Params);

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::Double);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Doubles, Params);

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::String);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Strings, Params);

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::Name);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Names, Params);

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::Vector);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Vectors, Params);

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::Rotator);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Rotators, Params);

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::Transform);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Transforms, Params);

    Params.Condition = BucketCondition(UnLuaNova::ELuaNetBucket::Object);
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Objects, Params);

    // The fingerprint is not push-based: it changes at most once per hot reload, and it has
    // to be there before the data is interpreted, so it is allowed to be polled.
    FDoRepLifetimeParams MetaParams;
    DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, SchemaVersion, MetaParams);
}

void UUnLuaNovaReplicationComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    const TArray<UnLuaNova::FLuaRepDecl>& Decls = UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Owner->GetClass());
    if (Decls.Num() == 0)
    {
        // Nothing declared means nothing to replicate; leave ticking off so a component
        // someone added "just in case" costs nothing.
        SetComponentTickEnabled(false);
        return;
    }

    bDeclared = true;
    UnLuaNova::BuildSlots(Decls, Slots);

    int32 Counts[int32(UnLuaNova::ELuaNetBucket::Max)];
    UnLuaNova::CountSlots(Slots, Counts);

    Bools.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Bool)]);
    Ints.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Int)]);
    Int64s.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Int64)]);
    Floats.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Float)]);
    Doubles.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Double)]);
    Strings.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::String)]);
    Names.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Name)]);
    Vectors.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Vector)]);
    Rotators.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Rotator)]);
    Transforms.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Transform)]);
    Objects.SetNum(Counts[int32(UnLuaNova::ELuaNetBucket::Object)]);

    LocalSchemaVersion = UnLuaNova::ComputeSchemaVersion(Decls);

    static bool bLoggedPushModel = false;
    if (!bLoggedPushModel)
    {
        bLoggedPushModel = true;
        UE_LOG(LogUnLuaNova, Log, TEXT("Lua replication: push model %s (buckets are marked dirty per type)."),
            UNLUANOVA_PUSH_MODEL ? TEXT("compiled in") : TEXT("NOT compiled in - WITH_PUSH_MODEL=0"));
    }

    // Clients never push; they only receive, so they never need the tick either.
    const bool bAuthority = GetOwnerRole() == ROLE_Authority;
    SetComponentTickEnabled(bAuthority && UpdateMode == ELuaNetUpdateMode::Polled);

    if (bAuthority)
    {
        SchemaVersion = LocalSchemaVersion;
        PullAllSlots();

        // Every bucket has to go out at least once whatever the pull managed to read: with
        // push model a bucket nobody dirties is a bucket nobody ever receives, and if the
        // module was not bound yet at BeginPlay there is no second chance in Explicit mode.
        for (int32 Index = 0; Index < int32(UnLuaNova::ELuaNetBucket::Max); ++Index)
            MarkBucketDirty(UnLuaNova::ELuaNetBucket(Index));

        FlushNetUpdate();
    }
}

void UUnLuaNovaReplicationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bDeclared = false;
    Slots.Empty();
    Super::EndPlay(EndPlayReason);
}

void UUnLuaNovaReplicationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bDeclared || UpdateMode != ELuaNetUpdateMode::Polled)
        return;

    PollAccumulator += DeltaTime;
    if (PollInterval > 0.f && PollAccumulator < PollInterval)
        return;
    PollAccumulator = 0.f;

    const AActor* Owner = GetOwner();
    if (!Owner)
        return;

    // A fully dormant actor is not looked at by the network at all, so scanning its Lua
    // table is pure waste. (Partially dormant actors are still polled: the engine decides
    // per connection, and we cannot know better.)
    // NetDormancy is the configured dormancy level; GetNetDormancy() is the per-connection
    // relevance query (DORM_DormantPartial) and takes a viewer, so it is not what this wants.
    if (Owner->NetDormancy == DORM_DormantAll)
        return;

    PullAllSlots();
}

void UUnLuaNovaReplicationComponent::MarkFieldDirty(FName FieldName)
{
    if (!bDeclared)
        return;

    if (GetOwnerRole() != ROLE_Authority)
        return;

    int32 Found = INDEX_NONE;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        if (Slots[Index].Name == FieldName)
        {
            Found = Index;
            break;
        }
    }

    if (Found == INDEX_NONE)
    {
        UE_LOG(LogUnLuaNova, Warning,
            TEXT("MarkFieldDirty('%s') on '%s': not a field this module declared as replicated."),
            *FieldName.ToString(), *GetNameSafe(GetOwner()));
        return;
    }

    // Dormancy has to be flushed before the write, not after: a dormant actor is not sent
    // anything, so the change would sit here until something else woke it up.
    if (AActor* Owner = GetOwner())
    {
        if (Owner->NetDormancy != DORM_Awake)
            Owner->FlushNetDormancy();
    }

    PullSlot(Found);
}

void UUnLuaNovaReplicationComponent::MarkAllFieldsDirty()
{
    if (!bDeclared)
        return;

    if (GetOwnerRole() != ROLE_Authority)
        return;

    PullAllSlots();
}

void UUnLuaNovaReplicationComponent::FlushNetUpdate()
{
    if (AActor* Owner = GetOwner())
        Owner->ForceNetUpdate();
}

void UUnLuaNovaReplicationComponent::MarkBucketDirty(UnLuaNova::ELuaNetBucket Bucket)
{
#if UNLUANOVA_PUSH_MODEL
    switch (Bucket)
    {
    // MARK_PROPERTY_DIRTY takes an FProperty*, so it cannot be handed a member directly;
    // the _FROM_NAME form resolves the RepIndex at compile time and rejects a name that
    // is not a replicated property of this class.
    case UnLuaNova::ELuaNetBucket::Bool:      MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Bools, this);      break;
    case UnLuaNova::ELuaNetBucket::Int:       MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Ints, this);       break;
    case UnLuaNova::ELuaNetBucket::Int64:     MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Int64s, this);     break;
    case UnLuaNova::ELuaNetBucket::Float:     MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Floats, this);     break;
    case UnLuaNova::ELuaNetBucket::Double:    MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Doubles, this);    break;
    case UnLuaNova::ELuaNetBucket::String:    MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Strings, this);    break;
    case UnLuaNova::ELuaNetBucket::Name:      MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Names, this);      break;
    case UnLuaNova::ELuaNetBucket::Vector:    MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Vectors, this);    break;
    case UnLuaNova::ELuaNetBucket::Rotator:   MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Rotators, this);   break;
    case UnLuaNova::ELuaNetBucket::Transform: MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Transforms, this); break;
    case UnLuaNova::ELuaNetBucket::Object:    MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Objects, this);    break;
    default: break;
    }
#endif
}

ELifetimeCondition UUnLuaNovaReplicationComponent::BucketCondition(UnLuaNova::ELuaNetBucket Bucket) const
{
    ELifetimeCondition Result = Condition.GetValue();

    const AActor* Owner = GetOwner();
    if (!Owner)
        return Result;

    const TArray<UnLuaNova::FLuaRepDecl>& Decls = UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Owner->GetClass());

    uint8 First = COND_None;
    for (const UnLuaNova::FLuaRepDecl& Decl : Decls)
    {
        if (UnLuaNova::BucketForType(Decl.Type) != Bucket || Decl.Condition == COND_None)
            continue;

        if (First == COND_None)
        {
            First = Decl.Condition;
        }
        else if (First != Decl.Condition)
        {
            UE_LOG(LogUnLuaNova, Warning,
                TEXT("'%s' declares conflicting conditions for the fields of one type; '%s' will use the first one. "
                     "Per-field conditions need one UPROPERTY per condition and cannot be added at runtime."),
                *Owner->GetClass()->GetName(), *Decl.Name.ToString());
        }
    }

    return First != COND_None ? ELifetimeCondition(First) : Result;
}

bool UUnLuaNovaReplicationComponent::SyncSlot(lua_State* L, int32 TableIdx, const UnLuaNova::FLuaNetSlot& Slot)
{
    const FTCHARToUTF8 Key(*Slot.Name.ToString());

    switch (Slot.Bucket)
    {
    case UnLuaNova::ELuaNetBucket::Bool:
    {
        uint8 Value;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Bools.IsValidIndex(Slot.Index) || Same(Bools[Slot.Index], Value))
            return false;
        Bools[Slot.Index] = Value;
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Int:
    {
        int32 Value;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Ints.IsValidIndex(Slot.Index) || Same(Ints[Slot.Index], Value))
            return false;
        Ints[Slot.Index] = Value;
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Int64:
    {
        int64 Value;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Int64s.IsValidIndex(Slot.Index) || Same(Int64s[Slot.Index], Value))
            return false;
        Int64s[Slot.Index] = Value;
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Float:
    {
        float Value;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Floats.IsValidIndex(Slot.Index) || Same(Floats[Slot.Index], Value))
            return false;
        Floats[Slot.Index] = Value;
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Double:
    {
        double Value;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Doubles.IsValidIndex(Slot.Index) || Same(Doubles[Slot.Index], Value))
            return false;
        Doubles[Slot.Index] = Value;
        return true;
    }
    case UnLuaNova::ELuaNetBucket::String:
    {
        FString Value;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Strings.IsValidIndex(Slot.Index) || Same(Strings[Slot.Index], Value))
            return false;
        Strings[Slot.Index] = MoveTemp(Value);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Name:
    {
        FName Value;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Names.IsValidIndex(Slot.Index) || Same(Names[Slot.Index], Value))
            return false;
        Names[Slot.Index] = Value;
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Vector:
    {
        FVector Value;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Vectors.IsValidIndex(Slot.Index) || Same(Vectors[Slot.Index], Value))
            return false;
        Vectors[Slot.Index] = Value;
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Rotator:
    {
        FRotator Value;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Rotators.IsValidIndex(Slot.Index) || Same(Rotators[Slot.Index], Value))
            return false;
        Rotators[Slot.Index] = Value;
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Transform:
    {
        FTransform Value;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Transforms.IsValidIndex(Slot.Index) || Same(Transforms[Slot.Index], Value))
            return false;
        Transforms[Slot.Index] = Value;
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Object:
    {
        UObject* Value = nullptr;
        if (!TryRead(L, TableIdx, Key.Get(), Value) || !Objects.IsValidIndex(Slot.Index) || Same(Objects[Slot.Index], Value))
            return false;
        Objects[Slot.Index] = Value;
        return true;
    }
    default:
        return false;
    }
}

bool UUnLuaNovaReplicationComponent::PullSlot(int32 SlotIndex)
{
    if (!Slots.IsValidIndex(SlotIndex))
        return false;

    FBoundTable Table(GetOwner());
    if (!Table.bOk)
        return false;

    if (SyncSlot(Table.L, Table.TableIdx, Slots[SlotIndex]))
    {
        MarkBucketDirty(Slots[SlotIndex].Bucket);
        return true;
    }

    return false;
}

void UUnLuaNovaReplicationComponent::PullAllSlots()
{
    FBoundTable Table(GetOwner());
    if (!Table.bOk)
        return;

    bool bDirty[int32(UnLuaNova::ELuaNetBucket::Max)];
    FMemory::Memzero(bDirty);

    for (const UnLuaNova::FLuaNetSlot& Slot : Slots)
    {
        if (SyncSlot(Table.L, Table.TableIdx, Slot))
            bDirty[int32(Slot.Bucket)] = true;
    }

    for (int32 Index = 0; Index < int32(UnLuaNova::ELuaNetBucket::Max); ++Index)
    {
        if (bDirty[Index])
            MarkBucketDirty(UnLuaNova::ELuaNetBucket(Index));
    }
}

void UUnLuaNovaReplicationComponent::OnRep_SchemaVersion()
{
    if (!bDeclared)
        return;

    if (SchemaVersion == LocalSchemaVersion)
    {
        bSchemaMismatchReported = false;
        return;
    }

    if (bSchemaMismatchReported)
        return;

    bSchemaMismatchReported = true;
    UE_LOG(LogUnLuaNova, Error,
        TEXT("'%s' received replicated fields laid out for a different declaration (0x%08X, local 0x%08X); "
             "they will not be applied. Both peers must run the same script, declaring in the same order."),
        *GetNameSafe(GetOwner()), SchemaVersion, LocalSchemaVersion);
}

void UUnLuaNovaReplicationComponent::PushBucket(UnLuaNova::ELuaNetBucket Bucket)
{
    if (!bDeclared)
        return;

    if (SchemaVersion != LocalSchemaVersion)
    {
        OnRep_SchemaVersion();
        return;
    }

    FBoundTable Table(GetOwner());
    if (!Table.bOk)
        return;

    const FString ContextText = FString::Printf(TEXT("OnRep on '%s'"), *GetNameSafe(GetOwner()));
    const TCHAR* Context = *ContextText;

    TArray<FName> Changed;
    for (const UnLuaNova::FLuaNetSlot& Slot : Slots)
    {
        if (Slot.Bucket != Bucket)
            continue;

        if (!ApplySlot(Table.L, Table.TableIdx, Slot))
            continue;

        Changed.Add(Slot.Name);

        // Per-field RepNotify: the script asks for one by naming it after the field. This
        // costs no bandwidth - the changed set is worked out here by comparing with what the
        // table already holds, not carried on the wire.
        const FString FuncName = FString::Printf(TEXT("OnRep_%s"), *Slot.Name.ToString());
        const FTCHARToUTF8 FuncNameUtf8(*FuncName);
        CallLuaMethod(Table.L, Table.TableIdx, FuncNameUtf8.Get(), Context);
    }

    if (Changed.Num() == 0)
        return;

    // Catch-all: one call with the list of names, for scripts that would rather do their own
    // dispatch than implement a function per field.
    lua_getfield(Table.L, Table.TableIdx, "OnReplicatedProps");
    if (lua_type(Table.L, -1) == LUA_TFUNCTION)
    {
        lua_pushvalue(Table.L, Table.TableIdx); // self, first argument
        lua_newtable(Table.L);                  // the array of changed names, second
        for (int32 Index = 0; Index < Changed.Num(); ++Index)
        {
            const FTCHARToUTF8 Name(*Changed[Index].ToString());
            lua_pushstring(Table.L, Name.Get());
            lua_rawseti(Table.L, -2, Index + 1);
        }

        if (lua_pcall(Table.L, 2, 0, 0) != LUA_OK)
        {
            const char* Message = lua_tostring(Table.L, -1);
            UE_LOG(LogUnLuaNova, Warning, TEXT("%s: OnReplicatedProps raised %s"),
                Context, UTF8_TO_TCHAR(Message ? Message : ""));
            lua_pop(Table.L, 1);
        }
    }
    else
    {
        lua_pop(Table.L, 1);
    }
}

bool UUnLuaNovaReplicationComponent::ApplySlot(lua_State* L, int32 TableIdx, const UnLuaNova::FLuaNetSlot& Slot)
{
    const FTCHARToUTF8 Key(*Slot.Name.ToString());

    switch (Slot.Bucket)
    {
    case UnLuaNova::ELuaNetBucket::Bool:
    {
        if (!Bools.IsValidIndex(Slot.Index))
            return false;
        uint8 Old;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Bools[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Bools[Slot.Index]);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Int:
    {
        if (!Ints.IsValidIndex(Slot.Index))
            return false;
        int32 Old;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Ints[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Ints[Slot.Index]);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Int64:
    {
        if (!Int64s.IsValidIndex(Slot.Index))
            return false;
        int64 Old;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Int64s[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Int64s[Slot.Index]);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Float:
    {
        if (!Floats.IsValidIndex(Slot.Index))
            return false;
        float Old;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Floats[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Floats[Slot.Index]);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Double:
    {
        if (!Doubles.IsValidIndex(Slot.Index))
            return false;
        double Old;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Doubles[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Doubles[Slot.Index]);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::String:
    {
        if (!Strings.IsValidIndex(Slot.Index))
            return false;
        FString Old;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Strings[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Strings[Slot.Index]);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Name:
    {
        if (!Names.IsValidIndex(Slot.Index))
            return false;
        FName Old;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Names[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Names[Slot.Index]);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Vector:
    {
        if (!Vectors.IsValidIndex(Slot.Index))
            return false;
        FVector Old;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Vectors[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Vectors[Slot.Index]);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Rotator:
    {
        if (!Rotators.IsValidIndex(Slot.Index))
            return false;
        FRotator Old;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Rotators[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Rotators[Slot.Index]);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Transform:
    {
        if (!Transforms.IsValidIndex(Slot.Index))
            return false;
        FTransform Old;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Transforms[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Transforms[Slot.Index]);
        return true;
    }
    case UnLuaNova::ELuaNetBucket::Object:
    {
        if (!Objects.IsValidIndex(Slot.Index))
            return false;
        UObject* Old = nullptr;
        if (TryRead(L, TableIdx, Key.Get(), Old) && Same(Old, Objects[Slot.Index]))
            return false;
        WriteField(L, TableIdx, Key.Get(), Objects[Slot.Index]);
        return true;
    }
    default:
        return false;
    }
}

void UUnLuaNovaReplicationComponent::OnRep_Bools()      { PushBucket(UnLuaNova::ELuaNetBucket::Bool); }
void UUnLuaNovaReplicationComponent::OnRep_Ints()       { PushBucket(UnLuaNova::ELuaNetBucket::Int); }
void UUnLuaNovaReplicationComponent::OnRep_Int64s()     { PushBucket(UnLuaNova::ELuaNetBucket::Int64); }
void UUnLuaNovaReplicationComponent::OnRep_Floats()     { PushBucket(UnLuaNova::ELuaNetBucket::Float); }
void UUnLuaNovaReplicationComponent::OnRep_Doubles()    { PushBucket(UnLuaNova::ELuaNetBucket::Double); }
void UUnLuaNovaReplicationComponent::OnRep_Strings()    { PushBucket(UnLuaNova::ELuaNetBucket::String); }
void UUnLuaNovaReplicationComponent::OnRep_Names()      { PushBucket(UnLuaNova::ELuaNetBucket::Name); }
void UUnLuaNovaReplicationComponent::OnRep_Vectors()    { PushBucket(UnLuaNova::ELuaNetBucket::Vector); }
void UUnLuaNovaReplicationComponent::OnRep_Rotators()   { PushBucket(UnLuaNova::ELuaNetBucket::Rotator); }
void UUnLuaNovaReplicationComponent::OnRep_Transforms() { PushBucket(UnLuaNova::ELuaNetBucket::Transform); }
void UUnLuaNovaReplicationComponent::OnRep_Objects()    { PushBucket(UnLuaNova::ELuaNetBucket::Object); }
