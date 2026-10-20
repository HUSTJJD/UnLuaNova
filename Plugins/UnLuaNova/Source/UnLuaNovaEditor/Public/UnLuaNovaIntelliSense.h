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
        UNLUANOVAEDITOR_API FString Get(const UBlueprint* Blueprint);

        UNLUANOVAEDITOR_API FString Get(const UField* Field);

        UNLUANOVAEDITOR_API FString Get(const UEnum* Enum);
        
        UNLUANOVAEDITOR_API FString Get(const UStruct* Struct);

        UNLUANOVAEDITOR_API FString Get(const UScriptStruct* ScriptStruct);
        
        UNLUANOVAEDITOR_API FString Get(const UClass* Class);
        
        UNLUANOVAEDITOR_API FString Get(const UFunction* Function, FString ForceClassName = "");

        UNLUANOVAEDITOR_API FString Get(const FProperty* Property);

        UNLUANOVAEDITOR_API FString GetUE(const TArray<const UField*> AllTypes);
        
        UNLUANOVAEDITOR_API FString GetTypeName(const UObject* Field);

        UNLUANOVAEDITOR_API FString GetTypeName(const FProperty* Property);

        UNLUANOVAEDITOR_API FString EscapeComments(const FString Comments, bool bSingleLine);

        UNLUANOVAEDITOR_API FString EscapeSymbolName(const FString InName);

        UNLUANOVAEDITOR_API bool IsValid(const UFunction* Function);

        UNLUANOVAEDITOR_API bool IsValidFunctionName(const FString Name);
    }
}
