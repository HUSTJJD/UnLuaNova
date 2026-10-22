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

        /**
         * A name that is safe to spell as a member or parameter in a declaration file.
         *
         * `end` is a Lua keyword, and parameters are named by whoever wrote the C++ - so
         * `trace_world(start: Vector, end: Vector)` is a real signature that cannot be written
         * down as-is. Luau has no way to quote a keyword in that position, so it gets a
         * trailing underscore: nothing looks these up by name (calls are positional), the name
         * only has to be unique and readable in the error messages.
         */
        static FString DeclName(const FString& Name)
        {
            static const TSet<FString> Keywords = {
                TEXT("and"), TEXT("break"), TEXT("continue"), TEXT("do"), TEXT("else"), TEXT("elseif"),
                TEXT("end"), TEXT("false"), TEXT("for"), TEXT("function"), TEXT("goto"), TEXT("if"),
                TEXT("in"), TEXT("local"), TEXT("nil"), TEXT("not"), TEXT("or"), TEXT("repeat"),
                TEXT("return"), TEXT("then"), TEXT("true"), TEXT("typeof"), TEXT("until"), TEXT("while")
            };

            return Keywords.Contains(Name) ? Name + TEXT("_") : Name;
        }

        static FString FunctionDecl(const UFunction* Function)
        {
            if (!Function)
            {
                return FString();
            }

            // No `self`. Luau's old `declare class` bodies took it explicitly; `declare extern
            // type` does not - a call like Actor:K2_GetActorLocation() supplies only the
            // declared arguments, so a leading self would put every call one argument short.
            FString Params;

            // UnLua returns a non-const out/ref parameter alongside the return value - that is
            // what FFunctionDesc::OutPropertyIndices is for, and why a caller writes
            // `local ok, hit = Actor:LineTrace(...)`. So they are results, not arguments; a
            // const-ref out parameter is still an argument.
            FString ReturnType;
            int32 NumOutParams = 0;

            for (TFieldIterator<FProperty> It(Function); It; ++It)
            {
                const FProperty* Param = *It;
                if (!Param)
                {
                    continue;
                }

                const bool bIsReturn = (Param->PropertyFlags & CPF_ReturnParm) != 0;
                const bool bIsOut = !bIsReturn
                    && (Param->PropertyFlags & (CPF_OutParm | CPF_ReferenceParm)) != 0
                    && (Param->PropertyFlags & CPF_ConstParm) == 0;

                if (bIsReturn)
                {
                    ReturnType = TypeName(Param);
                    continue;
                }

                if (bIsOut)
                {
                    ++NumOutParams;
                    continue;
                }

                Params += Params.IsEmpty()
                    ? FString::Printf(TEXT("%s: %s"), *DeclName(Param->GetName()), *TypeName(Param))
                    : FString::Printf(TEXT(", %s: %s"), *DeclName(Param->GetName()), *TypeName(Param));
            }

            // The out params come back as `any` - their real types are known at runtime, but
            // modelling "the returned tuple grows with the out params" is all a caller needs.
            FString Results = ReturnType;
            for (int32 Index = 0; Index < NumOutParams; ++Index)
            {
                Results += Results.IsEmpty() ? TEXT("any") : TEXT(", any");
            }

            // A tuple return has to be parenthesised in a function type: `(X) -> (number, string)`
            // and not `(X) -> number, string` - the latter ends the member at the comma. A
            // variadic (`any...`) is not accepted as a return pack by this Luau version either,
            // hence one `any` per out param instead.
            if (Results.IsEmpty())
            {
                Results = TEXT("()");
            }
            else if (Results.Contains(TEXT(",")))
            {
                Results = FString::Printf(TEXT("(%s)"), *Results);
            }

            // A method is a member with a function type, not a `function` statement: that is
            // all `declare extern type ... with` accepts.
            return FString::Printf(TEXT("\t%s: (%s) -> %s\n"), *DeclName(Function->GetName()), *Params, *Results);
        }

        FString StructDecl(const UStruct* Struct)
        {
            if (!Struct)
            {
                return FString();
            }

            // This Luau version dropped `declare class X ... end` (its parser now accepts only
            // `declare function`, `declare extern type` and plain globals), so types are
            // declared as extern types. Properties and methods go in the `with` block.
            //
            // Only a UClass can name a superclass: an extern type declared from a UScriptStruct
            // is a table type to Luau, and using one as a base is "cannot use non-class type X
            // as a superclass". A struct keeps its inherited members inline instead.
            const UStruct* Super = Cast<UClass>(Struct) ? Struct->GetSuperStruct() : nullptr;
            const EFieldIteratorFlags::SuperClassFlags Fields =
                Super ? EFieldIteratorFlags::ExcludeSuper : EFieldIteratorFlags::IncludeSuper;

            FString Out = FString::Printf(TEXT("declare extern type %s"), *Struct->GetName());
            if (Super)
            {
                Out += FString::Printf(TEXT(" extends %s"), *Super->GetName());
            }
            Out += TEXT(" with\n");

            // A property and a function can share a name (a BlueprintCallable getter next to
            // the property it reads). Luau reads the second one as an overload of a
            // non-function member, which is an error, so the first spelling wins.
            TSet<FString> Members;

            for (TFieldIterator<FProperty> It(Struct, Fields); It; ++It)
            {
                const FProperty* Property = *It;
                if (Property && !Members.Contains(DeclName(Property->GetName())))
                {
                    Out += FString::Printf(TEXT("\t%s: %s\n"), *DeclName(Property->GetName()), *TypeName(Property));
                    Members.Add(DeclName(Property->GetName()));
                }
            }

            // Repeating a base class' members next to `extends` is not a redundancy Luau
            // tolerates: it reads the second one as an overload of a non-function member.
            for (TFieldIterator<UFunction> It(Struct, Fields); It; ++It)
            {
                const UFunction* Function = *It;
                // Skip the UObject-generated script stubs and internal callbacks.
                if (Function && (Function->FunctionFlags & FUNC_BlueprintCallable)
                    && !Members.Contains(DeclName(Function->GetName())))
                {
                    Out += FunctionDecl(Function);
                    Members.Add(DeclName(Function->GetName()));
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

            // Two different UObjects can carry the same name (a struct in one module and a
            // class in another, say), and Luau only allows one type per name - the second is
            // "Redefinition of type X". First one wins; the alternative is a file the analyzer
            // refuses to load at all.
            TSet<FString> Declared;

            for (const UField* Field : Types)
            {
                if (!Field)
                {
                    continue;
                }

                if (Declared.Contains(Field->GetName()))
                {
                    continue;
                }

                if (const UEnum* Enum = Cast<UEnum>(Field))
                {
                    Out += EnumDecl(Enum);
                    Declared.Add(Field->GetName());
                }
                else if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Field))
                {
                    Out += StructDecl(ScriptStruct);
                    Declared.Add(Field->GetName());
                }
                else if (const UClass* Class = Cast<UClass>(Field))
                {
                    Out += StructDecl(Class);
                    Declared.Add(Field->GetName());
                }
            }

            // The two globals the runtime injects, neither of which is reflected: UnLuaNova is
            // the script-facing API (see Content/IntelliSense/UnLuaNova.lua) and UE is the
            // namespace the generated types are reached through. Without them every script
            // opens with "Unknown global", and nothing else gets checked at all.
            //
            // They are declared as an opaque table on purpose. Modelling the API surface is its
            // own job, and a wrong declaration is worse than a vague one: it turns real errors
            // into silence. Scripts that want types annotate them - `local V: FVector` - and
            // those are checked against the extern types above.
            Out += TEXT("\n-- Injected by the runtime; see Content/IntelliSense/UnLuaNova.lua for the API.\n");
            Out += TEXT("declare UnLuaNova: { [string]: any }\n");
            Out += TEXT("declare UE: { [string]: any }\n");

            return Out;
        }
    }
}
