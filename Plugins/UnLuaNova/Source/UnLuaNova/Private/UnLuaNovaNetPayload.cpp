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
        }

        return ELuaNetBucket::Int;
    }

    void BuildSlots(const TArray<FLuaRepDecl>& Decls, TArray<FLuaNetSlot>& OutSlots)
    {
        int32 Next[int32(ELuaNetBucket::Max)];
        FMemory::Memzero(Next);

        OutSlots.Reset(Decls.Num());
        for (const FLuaRepDecl& Decl : Decls)
        {
            FLuaNetSlot Slot;
            Slot.Name = Decl.Name;
            Slot.Type = Decl.Type;
            Slot.Condition = Decl.Condition;
            Slot.Bucket = BucketForType(Decl.Type);
            Slot.Index = Next[int32(Slot.Bucket)]++;
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

    uint32 ComputeSchemaVersion(const TArray<FLuaRepDecl>& Decls)
    {
        // Order matters as much as content: the index inside a bucket is the position in the
        // declaration list, so a reordering is a different layout even with the same fields.
        FString Text;
        Text.Reserve(Decls.Num() * 24);

        for (const FLuaRepDecl& Decl : Decls)
            Text += FString::Printf(TEXT("%s:%d:%d;"), *Decl.Name.ToString(), int32(Decl.Type), int32(Decl.Condition));

        const FTCHARToUTF8 Utf8(*Text);
        return FCrc::MemCrc32(Utf8.Get(), Utf8.Length());
    }
}
