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

#include "UnLuauNovaBase.h"

namespace UnLuauNova
{
    UNLUANOVA_API void ExportClass(IExportedClass* Class);

    UNLUANOVA_API void ExportEnum(IExportedEnum* Enum);

    UNLUANOVA_API void ExportFunction(IExportedFunction* Function);

    UNLUANOVA_API void AddType(FString Name, TSharedPtr<ITypeInterface> TypeInterface);

    UNLUANOVA_API TMap<FString, IExportedClass*> GetExportedReflectedClasses();

    UNLUANOVA_API TMap<FString, IExportedClass*> GetExportedNonReflectedClasses();

    UNLUANOVA_API TArray<IExportedEnum*> GetExportedEnums();

    UNLUANOVA_API TArray<IExportedFunction*> GetExportedFunctions();

    UNLUANOVA_API IExportedClass* FindExportedClass(FString Name);

    UNLUANOVA_API IExportedClass* FindExportedReflectedClass(FString Name);

    UNLUANOVA_API IExportedClass* FindExportedNonReflectedClass(FString Name);

    UNLUANOVA_API TSharedPtr<ITypeInterface> FindTypeInterface(FString Name);
}
