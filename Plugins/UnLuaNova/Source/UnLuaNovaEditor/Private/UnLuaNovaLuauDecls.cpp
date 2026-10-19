// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT

#include "UnLuaNovaLuauDecls.h"

namespace UnLuaNova
{
    namespace LuauDecl
    {
        /** Integers of every width collapse to Luau's single number type. */
        static bool IsIntegerProperty(const FProperty* Property)
        {
            return CastField<FByteProperty>(Property)
                || CastField<FInt8Property>(Property)
                || CastField<FInt16Property>(Property)
                || CastField<FIntProperty>(Property)
                || CastField<FInt64Property>(Property)
                || CastField<FUInt16Property>(Property)
                || CastField<FUInt32Property>(Property)
                || CastField<FUInt64Property>(Property);
        }

        static bool IsStringProperty(const FProperty* Property)
        {
            return CastField<FStrProperty>(Property)
                || CastField<FNameProperty>(Property)
                || CastField<FTextProperty>(Property);
        }

        FString TypeName(const FProperty* Property)
        {
            if (!Property)
            {
                return TEXT("any");
            }

            if (CastField<FBoolProperty>(Property))
            {
                return TEXT("boolean");
            }

            // Luau has no integer subtype - int32/int64/uint32 are all just numbers.
            if (IsIntegerProperty(Property)
                || CastField<FFloatProperty>(Property)
                || CastField<FDoubleProperty>(Property))
            {
                return TEXT("number");
            }

            if (IsStringProperty(Property))
            {
                return TEXT("string");
            }

            if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
            {
                return FString::Printf(TEXT("{%s}"), *TypeName(Array->Inner));
            }

            if (const FMapProperty* Map = CastField<FMapProperty>(Property))
            {
                return FString::Printf(TEXT("{[%s]: %s}"), *TypeName(Map->KeyProp), *TypeName(Map->ValueProp));
            }

            if (const FSetProperty* Set = CastField<FSetProperty>(Property))
            {
                return FString::Printf(TEXT("{[%s]: boolean}"), *TypeName(Set->ElementProp));
            }

            if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
            {
                return Struct->Struct ? Struct->Struct->GetName() : TEXT("any");
            }

            if (const FClassProperty* Class = CastField<FClassProperty>(Property))
            {
                return Class->MetaClass ? Class->MetaClass->GetName() : TEXT("any");
            }

            if (const FObjectProperty* Object = CastField<FObjectProperty>(Property))
            {
                return Object->PropertyClass ? Object->PropertyClass->GetName() : TEXT("any");
            }

            if (const FInterfaceProperty* Interface = CastField<FInterfaceProperty>(Property))
            {
                return Interface->InterfaceClass ? Interface->InterfaceClass->GetName() : TEXT("any");
            }

            // Delegates, weak/soft/lazy object pointers and anything exotic.
            return TEXT("any");
        }

        static FString FunctionDecl(const UFunction* Function, const UStruct* Owner)
        {
            if (!Function)
            {
                return FString();
            }

            // Parameters: Luau methods take self explicitly in `declare class` bodies.
            FString Params = FString::Printf(TEXT("self: %s"), Owner ? *Owner->GetName() : TEXT("any"));

            FString ReturnType = TEXT("()");
            bool bSawOutParam = false;

            for (TFieldIterator<FProperty> It(Function); It; ++It)
            {
                const FProperty* Param = *It;
                if (!Param)
                {
                    continue;
                }

                const bool bIsReturn = (Param->PropertyFlags & CPF_ReturnParm) != 0;
                const bool bIsOut = (Param->PropertyFlags & CPF_OutParm) != 0 && !bIsReturn;

                if (bIsReturn)
                {
                    ReturnType = TypeName(Param);
                    continue;
                }

                Params += FString::Printf(TEXT(", %s: %s"), *Param->GetName(), *TypeName(Param));

                // UnLua returns out-params alongside the return value, so the effective
                // signature is a tuple rather than just ReturnType. Marking it `any...`
                // keeps luau-analyze from rejecting the call while we do not model tuples.
                if (bIsOut)
                {
                    bSawOutParam = true;
                }
            }

            if (bSawOutParam && ReturnType != TEXT("()"))
            {
                ReturnType += TEXT(", any...");
            }
            else if (bSawOutParam)
            {
                ReturnType = TEXT("any...");
            }

            return FString::Printf(TEXT("\tfunction %s(%s): %s\n"), *Function->GetName(), *Params, *ReturnType);
        }

        FString StructDecl(const UStruct* Struct)
        {
            if (!Struct)
            {
                return FString();
            }

            FString Out = FString::Printf(TEXT("declare class %s\n"), *Struct->GetName());

            for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::IncludeSuper); It; ++It)
            {
                const FProperty* Property = *It;
                if (Property)
                {
                    Out += FString::Printf(TEXT("\t%s: %s\n"), *Property->GetName(), *TypeName(Property));
                }
            }

            for (TFieldIterator<UFunction> It(Struct, EFieldIteratorFlags::IncludeSuper); It; ++It)
            {
                const UFunction* Function = *It;
                // Skip the UObject-generated script stubs and internal callbacks.
                if (Function && (Function->FunctionFlags & FUNC_BlueprintCallable))
                {
                    Out += FunctionDecl(Function, Struct);
                }
            }

            Out += TEXT("end\n\n");
            return Out;
        }

        FString EnumDecl(const UEnum* Enum)
        {
            if (!Enum)
            {
                return FString();
            }

            // Luau has no enum construct; a union of string literals is the closest
            // thing luau-analyze can actually check against.
            FString Values;
            const int32 Num = Enum->NumEnums();
            for (int32 Index = 0; Index < Num; ++Index)
            {
                const FString Name = Enum->GetNameStringByIndex(Index);
                if (Name.IsEmpty())
                {
                    continue;
                }
                Values += Values.IsEmpty()
                    ? FString::Printf(TEXT("\"%s\""), *Name)
                    : FString::Printf(TEXT(" | \"%s\""), *Name);
            }

            return Values.IsEmpty()
                ? FString::Printf(TEXT("type %s = any\n\n"), *Enum->GetName())
                : FString::Printf(TEXT("type %s = %s\n\n"), *Enum->GetName(), *Values);
        }

        FString Generate(const TArray<const UField*>& Types)
        {
            FString Out;
            Out += TEXT("-- Generated by UnLuaNova. Do not edit by hand.\n");
            Out += TEXT("-- Luau type declarations for the UE API exposed to scripts.\n");
            Out += TEXT("-- Consumed by luau-analyze; run it in CI to catch UE API misuse\n");
            Out += TEXT("-- before the editor ever starts.\n\n");

            for (const UField* Field : Types)
            {
                if (!Field)
                {
                    continue;
                }

                if (const UEnum* Enum = Cast<UEnum>(Field))
                {
                    Out += EnumDecl(Enum);
                }
                else if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Field))
                {
                    Out += StructDecl(ScriptStruct);
                }
                else if (const UClass* Class = Cast<UClass>(Field))
                {
                    Out += StructDecl(Class);
                }
            }

            return Out;
        }
    }
}
