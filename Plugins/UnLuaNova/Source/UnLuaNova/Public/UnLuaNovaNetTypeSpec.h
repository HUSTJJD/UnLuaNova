// Copyright (c) 2026 UnLuaNova contributors
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/mit-license.php
//
// Unless required by applicable law or agreed to in writing, software distributed under
// the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the License for the specific language governing
// permissions and limitations under the License.

#pragma once

#include "CoreMinimal.h"
#include "UnLuaNovaNetDecl.h"

struct lua_State;
class UFunction;
class FProperty;

/**
 * Type descriptions that go beyond the eleven fixed replicated buckets.
 *
 * The fixed buckets are eleven UPROPERTY arrays on UUnLuaNovaReplicationComponent. A class
 * cannot gain a new UPROPERTY at runtime, so a Lua script that wants an arbitrary UStruct or
 * a container cannot be given one more typed array. Instead such a field is declared with a
 * type *spec* and travels in one shared generic bucket: a canonical, self-describing text
 * payload that both peers decode against the same declaration. The layout (including the
 * spec) is covered by SchemaVersion, so a peer with a different spec cannot misread it.
 *
 * RPC parameters are the opposite case: a UFunction is built at runtime, so an arbitrary
 * FStructProperty / FArrayProperty / FSetProperty / FMapProperty can be constructed and
 * linked into it. UnLua's normal FFunctionDesc marshalling then does the Lua <-> value work.
 *
 * Spec grammar (case-insensitive keyword, ':' separated, no whitespace):
 *
 *     Int | Float | ...                      one of the eleven builtin type names
 *     Struct:/Script/Engine.Vector           any resolvable UScriptStruct path
 *     /Script/Engine.Vector                  shorthand for the same
 *     Array:<spec>                          TArray of <spec>
 *     Set:<spec>                            TSet of <spec>
 *     Map:<keyspec>:<valuespec>             TMap of <keyspec> -> <valuespec>
 *
 * UObject references are deliberately not part of the generic payload: they need an engine
 * NetGUID, which a text payload cannot carry. They keep using the dedicated Object bucket.
 */
namespace UnLuaNova
{
    enum class ELuaNetSpecKind : uint8
    {
        Invalid,
        Builtin,
        Struct,
        Array,
        Set,
        Map,
    };

    struct FLuaNetTypeSpec
    {
        ELuaNetSpecKind Kind = ELuaNetSpecKind::Invalid;
        /** Only meaningful for Kind == Builtin. */
        ELuaNetParamType Builtin = ELuaNetParamType::Int;
        /** Only meaningful for Kind == Struct. */
        FString StructPath;
        /** Array/Set: one element spec. Map: key spec then value spec. */
        TArray<FLuaNetTypeSpec> Children;
        /** Normalised form; folded into SchemaVersion and stored on the declaration. */
        FString Canonical;

        bool IsValid() const { return Kind != ELuaNetSpecKind::Invalid; }
        bool IsGeneric() const { return Kind != ELuaNetSpecKind::Invalid && Kind != ELuaNetSpecKind::Builtin; }
    };

    /**
     * Parse and validate a type spec. Rejects unknown keywords, missing children, unresolvable
     * UScriptStruct paths, non-hashable map keys and excessive nesting, so a bad declaration
     * fails at bind time instead of producing an undecodable payload at runtime.
     */
    UNLUANOVA_API bool ParseLuaNetTypeSpec(const FString& Text, FLuaNetTypeSpec& Out, FString& OutError);

    /**
     * Build the FProperty for one RPC parameter. Outer owns the resulting field chain and must
     * outlive it (a UFunction built for the RPC does). Returns nullptr on an invalid spec.
     */
    UNLUANOVA_API FProperty* BuildLuaNetTypeProperty(UFunction* Outer, FName Name, const FLuaNetTypeSpec& Spec);

    /** Lua value at Idx -> canonical wire text, checked against Spec. Idx may be negative. */
    UNLUANOVA_API bool EncodeLuaNetValue(lua_State* L, int32 Idx, const FLuaNetTypeSpec& Spec,
        FString& OutText, FString& OutError);

    /**
     * Canonical wire text -> one Lua value pushed on the stack. On failure the stack is left
     * exactly as it was. The text is self-describing; Spec is used only to reject a payload
     * whose top-level shape does not match the declaration.
     */
    UNLUANOVA_API bool DecodeLuaNetValue(lua_State* L, const FString& Text, const FLuaNetTypeSpec& Spec,
        FString& OutError);
}
