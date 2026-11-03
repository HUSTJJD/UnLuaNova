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

#include "UnLuaNovaNetPayload.h"

#include "Misc/Crc.h"

namespace UnLuaNova
{
    ELuaNetBucket BucketForType(const ELuaNetParamType Type)
    {
        switch (Type)
        {
        case ELuaNetParamType::Bool:      return ELuaNetBucket::Bool;
        case ELuaNetParamType::Int:       return ELuaNetBucket::Int;
        case ELuaNetParamType::Int64:     return ELuaNetBucket::Int64;
        case ELuaNetParamType::Float:     return ELuaNetBucket::Float;
        case ELuaNetParamType::Double:    return ELuaNetBucket::Double;
        case ELuaNetParamType::String:    return ELuaNetBucket::String;
        case ELuaNetParamType::Name:      return ELuaNetBucket::Name;
        case ELuaNetParamType::Vector:    return ELuaNetBucket::Vector;
        case ELuaNetParamType::Rotator:   return ELuaNetBucket::Rotator;
        case ELuaNetParamType::Transform: return ELuaNetBucket::Transform;
        case ELuaNetParamType::Object:    return ELuaNetBucket::Object;
        case ELuaNetParamType::Generic:   return ELuaNetBucket::Generic;
        }

        return ELuaNetBucket::Int;
    }

    bool ConditionToGroup(const uint8 Condition, ELuaNetConditionGroup& OutGroup)
    {
        switch (Condition)
        {
        case COND_None:                OutGroup = ELuaNetConditionGroup::None;               return true;
        case COND_InitialOnly:         OutGroup = ELuaNetConditionGroup::InitialOnly;        return true;
        case COND_OwnerOnly:           OutGroup = ELuaNetConditionGroup::OwnerOnly;          return true;
        case COND_SkipOwner:           OutGroup = ELuaNetConditionGroup::SkipOwner;          return true;
        case COND_SimulatedOnly:       OutGroup = ELuaNetConditionGroup::SimulatedOnly;      return true;
        case COND_AutonomousOnly:      OutGroup = ELuaNetConditionGroup::AutonomousOnly;     return true;
        case COND_SimulatedOrPhysics:  OutGroup = ELuaNetConditionGroup::SimulatedOrPhysics; return true;
        case COND_InitialOrOwner:      OutGroup = ELuaNetConditionGroup::InitialOrOwner;     return true;
        default:                       return false;
        }
    }

    uint8 GroupToCondition(const ELuaNetConditionGroup Group)
    {
        switch (Group)
        {
        case ELuaNetConditionGroup::None:               return COND_None;
        case ELuaNetConditionGroup::InitialOnly:        return COND_InitialOnly;
        case ELuaNetConditionGroup::OwnerOnly:          return COND_OwnerOnly;
        case ELuaNetConditionGroup::SkipOwner:          return COND_SkipOwner;
        case ELuaNetConditionGroup::SimulatedOnly:      return COND_SimulatedOnly;
        case ELuaNetConditionGroup::AutonomousOnly:     return COND_AutonomousOnly;
        case ELuaNetConditionGroup::SimulatedOrPhysics: return COND_SimulatedOrPhysics;
        case ELuaNetConditionGroup::InitialOrOwner:     return COND_InitialOrOwner;
        }

        return COND_None;
    }

    const TCHAR* GroupName(const ELuaNetConditionGroup Group)
    {
        switch (Group)
        {
        case ELuaNetConditionGroup::None:               return TEXT("None");
        case ELuaNetConditionGroup::InitialOnly:        return TEXT("InitialOnly");
        case ELuaNetConditionGroup::OwnerOnly:          return TEXT("OwnerOnly");
        case ELuaNetConditionGroup::SkipOwner:          return TEXT("SkipOwner");
        case ELuaNetConditionGroup::SimulatedOnly:      return TEXT("SimulatedOnly");
        case ELuaNetConditionGroup::AutonomousOnly:     return TEXT("AutonomousOnly");
        case ELuaNetConditionGroup::SimulatedOrPhysics: return TEXT("SimulatedOrPhysics");
        case ELuaNetConditionGroup::InitialOrOwner:     return TEXT("InitialOrOwner");
        }

        return TEXT("Unknown");
    }

    void BuildSlots(const TArray<FLuaRepDecl>& Decls, TArray<FLuaNetSlot>& OutSlots)
    {
        int32 Next[int32(ELuaNetConditionGroup::Max)][int32(ELuaNetBucket::Max)];
        FMemory::Memzero(Next);

        OutSlots.Reset(Decls.Num());
        for (const FLuaRepDecl& Decl : Decls)
        {
            ELuaNetConditionGroup Group = ELuaNetConditionGroup::None;
            if (!ConditionToGroup(Decl.Condition, Group))
                continue;

            FLuaNetSlot Slot;
            Slot.Name = Decl.Name;
            Slot.Type = Decl.Type;
            Slot.Condition = Decl.Condition;
            Slot.Group = Group;
            Slot.TypeSpec = Decl.TypeSpec;
            Slot.Bucket = BucketForType(Decl.Type);
            Slot.Index = Next[int32(Group)][int32(Slot.Bucket)]++;
            OutSlots.Add(Slot);
        }
    }

    void CountSlots(const TArray<FLuaNetSlot>& Slots, int32 (&OutCounts)[int32(ELuaNetBucket::Max)])
    {
        FMemory::Memzero(OutCounts, sizeof(OutCounts));

        for (const FLuaNetSlot& Slot : Slots)
        {
            if (Slot.Bucket >= ELuaNetBucket::Max)
                continue;
            ++OutCounts[int32(Slot.Bucket)];
        }
    }

    void CountSlotsByGroup(const TArray<FLuaNetSlot>& Slots,
        int32 (&OutCounts)[int32(ELuaNetConditionGroup::Max)][int32(ELuaNetBucket::Max)])
    {
        FMemory::Memzero(OutCounts, sizeof(OutCounts));

        for (const FLuaNetSlot& Slot : Slots)
        {
            if (Slot.Group >= ELuaNetConditionGroup::Max || Slot.Bucket >= ELuaNetBucket::Max)
                continue;
            ++OutCounts[int32(Slot.Group)][int32(Slot.Bucket)];
        }
    }

    uint32 ComputeSchemaVersion(const TArray<FLuaRepDecl>& Decls)
    {
        // Order matters as much as content: the index inside a bucket is the position in the
        // declaration list, so a reordering is a different layout even with the same fields.
        FString Text;
        Text.Reserve(Decls.Num() * 24);

        for (const FLuaRepDecl& Decl : Decls)
            Text += FString::Printf(TEXT("%s:%d:%d:%s;"), *Decl.Name.ToString(), int32(Decl.Type),
                int32(Decl.Condition), *Decl.TypeSpec);

        const FTCHARToUTF8 Utf8(*Text);
        return FCrc::MemCrc32(Utf8.Get(), Utf8.Length());
    }
}
