// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT

#pragma once

#include "CoreMinimal.h"

namespace UnLuaNova
{
    namespace LuauDecl
    {
        /**
         * Emit Luau type declarations (`declare class` / `declare function` blocks) for UE
         * reflection types, so that `luau-analyze` can check scripts offline.
         *
         * This is NOT the same output as IntelliSense::Get(), which produces EmmyLua
         * annotations for editors. Luau needs its own mapping for two reasons:
         *
         *   1. Luau has a single number type. The EmmyLua output says `integer` for
         *      int32/int64/uint32..., which luau-analyze rejects outright.
         *   2. Container syntax differs - Luau writes `{T}` and `{[K]: V}` rather than
         *      the `TArray<T>`-style spellings used in annotations.
         *
         * The point of the whole exercise is to move UE API misuse from runtime to
         * "before you even run the editor", which is the strongest argument for the Luau
         * backend in the first place.
         */

        /** UE property -> Luau type name. */
        UNLUANOVAEDITOR_API FString TypeName(const FProperty* Property);

        /** Declaration block for one reflected struct or class. */
        UNLUANOVAEDITOR_API FString StructDecl(const UStruct* Struct);

        /** Declaration block for one enum, as a Luau string union. */
        UNLUANOVAEDITOR_API FString EnumDecl(const UEnum* Enum);

        /** Full .d.lua file for a set of types. */
        UNLUANOVAEDITOR_API FString Generate(const TArray<const UField*>& Types);
    }
}
