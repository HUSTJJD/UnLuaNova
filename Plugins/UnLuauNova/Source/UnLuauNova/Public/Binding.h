// Tencent is pleased to support the open source community by making UnLua available.
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

#include "UnLuaBase.h"

namespace UnLuauNova
{
    UNLUAUNOVA_API void ExportClass(IExportedClass* Class);

    UNLUAUNOVA_API void ExportEnum(IExportedEnum* Enum);

    UNLUAUNOVA_API void ExportFunction(IExportedFunction* Function);

    UNLUAUNOVA_API void AddType(FString Name, TSharedPtr<ITypeInterface> TypeInterface);

    UNLUAUNOVA_API TMap<FString, IExportedClass*> GetExportedReflectedClasses();

    UNLUAUNOVA_API TMap<FString, IExportedClass*> GetExportedNonReflectedClasses();

    UNLUAUNOVA_API TArray<IExportedEnum*> GetExportedEnums();

    UNLUAUNOVA_API TArray<IExportedFunction*> GetExportedFunctions();

    UNLUAUNOVA_API IExportedClass* FindExportedClass(FString Name);

    UNLUAUNOVA_API IExportedClass* FindExportedReflectedClass(FString Name);

    UNLUAUNOVA_API IExportedClass* FindExportedNonReflectedClass(FString Name);

    UNLUAUNOVA_API TSharedPtr<ITypeInterface> FindTypeInterface(FString Name);
}
