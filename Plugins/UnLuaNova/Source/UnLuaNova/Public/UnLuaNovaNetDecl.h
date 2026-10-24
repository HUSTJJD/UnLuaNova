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

/**
 * What a Lua module may declare about networking, in plain C++ terms.
 *
 * Deliberately NOT UENUM/USTRUCT: nothing here needs to be visible to the engine, and
 * keeping it out of UHT means the declaration layer has no generated header, no
 * reflection dependency and no risk of tripping over a blueprint rebuild. The Lua side
 * spells every one of these as a **string** ("Int", "OwnerOnly", ...) for the same
 * reason - it does not depend on any enum having been registered with the Lua registry,
 * so it works before anything else is set up and cannot break when a name changes.
 *
 * The gap this fills, stated precisely: ULuaFunction::IsOverridable() is
 *   FUNC_BlueprintEvent || (FUNC_Native && FUNC_Event && !FUNC_Net)
 * so a UFunction carrying FUNC_Net is never overridden - Lua can only re-implement an
 * RPC the engine already declared. These declarations let Lua declare its own.
 */
namespace UnLuaNova
{
    /** Parameter types a Lua-declared RPC or replicated field may carry. */
    enum class ELuaNetParamType : uint8
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
    };

    /** Which way a Lua-declared RPC travels. */
    enum class ELuaRpcKind : uint8
    {
        Server,
        Client,
        Multicast,
    };

    /** One parameter of a Lua-declared RPC. */
    struct FLuaRpcParam
    {
        FName Name;
        ELuaNetParamType Type = ELuaNetParamType::Int;
    };

    /** A Lua-declared RPC. */
    struct FLuaRpcDecl
    {
        FName Name;
        ELuaRpcKind Kind = ELuaRpcKind::Server;
        bool bReliable = false;
        TArray<FLuaRpcParam> Params;
    };

    /** A Lua-declared replicated field. */
    struct FLuaRepDecl
    {
        FName Name;
        ELuaNetParamType Type = ELuaNetParamType::Int;
        /** UE's own ELifetimeCondition, or COND_None. */
        uint8 Condition = 0;
    };

    /**
     * Parse "Int" / "Vector" / "OwnerOnly" and friends. Case-insensitive, because these
     * come from a script and a typo should not be a silent no-op.
     *
     * @return false when the string is not a known name; OutType is left alone.
     */
    UNLUANOVA_API bool ParseLuaNetParamType(const FString& Text, ELuaNetParamType& OutType);

    /**
     * Parse UE's lifetime condition by name - "None", "InitialOnly", "OwnerOnly",
     * "SkipOwner", "SimulatedOnly", "AutonomousOnly", "SimulatedOrPhysics",
     * "InitialOrOwner", "Custom", "ReplayOrOwner", "ReplayOnly", "SimulatedOnlyNoReplay",
     * "SimulatedOrPhysicsNoReplay", "SkipReplay".
     *
     * The values are UE's own ELifetimeCondition constants, which is why this takes a
     * uint8 rather than an enum of ours: there is no reason to invent a parallel enum for
     * something the engine already owns.
     */
    UNLUANOVA_API bool ParseLifetimeCondition(const FString& Text, uint8& OutCondition);
}
