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
#include "UnLuaNovaNetPayload.generated.h"

/**
 * One Lua-declared replicated field, in flight.
 *
 * The value rides as a string rather than as a typed variant. That is a deliberate
 * trade: a USTRUCT full of UPROPERTY members replicates with no custom serialiser at
 * all, which means no NetDeltaSerialize implementation, no FNetDeltaSerializeInfo
 * version wrangling, and no per-field change masks to get wrong. What it costs is
 * bandwidth - every field goes every time the payload changes.
 *
 * Given that the alternative is a hand-written delta serializer against an API that
 * changes between engine versions, sending the whole set is the honest choice for a
 * first implementation. See the "已知边界" section of the docs.
 */
USTRUCT()
struct FLuaRepField
{
    GENERATED_BODY()

    UPROPERTY()
    FName Name;

    /** ELuaNetParamType, as an int so the payload needs no dependency on the decl header. */
    UPROPERTY()
    int32 Type = 0;

    UPROPERTY()
    FString Value;
};

USTRUCT()
struct FLuaNetPayload
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FLuaRepField> Fields;
};
