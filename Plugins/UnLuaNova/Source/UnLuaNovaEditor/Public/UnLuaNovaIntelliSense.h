// Tencent is pleased to support the open source community by making UnLuaNova available.
// 
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#pragma once

#include "CoreMinimal.h"

namespace UnLuaNova
{
    namespace IntelliSense
    {
        UnLuaNovaEDITOR_API FString Get(const UBlueprint* Blueprint);

        UnLuaNovaEDITOR_API FString Get(const UField* Field);

        UnLuaNovaEDITOR_API FString Get(const UEnum* Enum);
        
        UnLuaNovaEDITOR_API FString Get(const UStruct* Struct);

        UnLuaNovaEDITOR_API FString Get(const UScriptStruct* ScriptStruct);
        
        UnLuaNovaEDITOR_API FString Get(const UClass* Class);
        
        UnLuaNovaEDITOR_API FString Get(const UFunction* Function, FString ForceClassName = "");

        UnLuaNovaEDITOR_API FString Get(const FProperty* Property);

        UnLuaNovaEDITOR_API FString GetUE(const TArray<const UField*> AllTypes);
        
        UnLuaNovaEDITOR_API FString GetTypeName(const UObject* Field);

        UnLuaNovaEDITOR_API FString GetTypeName(const FProperty* Property);

        UnLuaNovaEDITOR_API FString EscapeComments(const FString Comments, bool bSingleLine);

        UnLuaNovaEDITOR_API FString EscapeSymbolName(const FString InName);

        UnLuaNovaEDITOR_API bool IsValid(const UFunction* Function);

        UnLuaNovaEDITOR_API bool IsValidFunctionName(const FString Name);
    }
}
