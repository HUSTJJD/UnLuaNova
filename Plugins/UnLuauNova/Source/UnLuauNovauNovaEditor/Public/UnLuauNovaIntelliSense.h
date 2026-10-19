// Tencent is pleased to support the open source community by making UnLuauNova available.
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

namespace UnLuauNova
{
    namespace IntelliSense
    {
        UnLuauNovaEDITOR_API FString Get(const UBlueprint* Blueprint);

        UnLuauNovaEDITOR_API FString Get(const UField* Field);

        UnLuauNovaEDITOR_API FString Get(const UEnum* Enum);
        
        UnLuauNovaEDITOR_API FString Get(const UStruct* Struct);

        UnLuauNovaEDITOR_API FString Get(const UScriptStruct* ScriptStruct);
        
        UnLuauNovaEDITOR_API FString Get(const UClass* Class);
        
        UnLuauNovaEDITOR_API FString Get(const UFunction* Function, FString ForceClassName = "");

        UnLuauNovaEDITOR_API FString Get(const FProperty* Property);

        UnLuauNovaEDITOR_API FString GetUE(const TArray<const UField*> AllTypes);
        
        UnLuauNovaEDITOR_API FString GetTypeName(const UObject* Field);

        UnLuauNovaEDITOR_API FString GetTypeName(const FProperty* Property);

        UnLuauNovaEDITOR_API FString EscapeComments(const FString Comments, bool bSingleLine);

        UnLuauNovaEDITOR_API FString EscapeSymbolName(const FString InName);

        UnLuauNovaEDITOR_API bool IsValid(const UFunction* Function);

        UnLuauNovaEDITOR_API bool IsValidFunctionName(const FString Name);
    }
}
