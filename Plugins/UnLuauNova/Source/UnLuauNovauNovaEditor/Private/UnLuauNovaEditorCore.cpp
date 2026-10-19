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

#include "UnLuauNovaEditorCore.h"
#include "UnLuauNovaInterface.h"
#include "UnLuauNovaPrivate.h"
#include "UnLuauNovaSettings.h"
#include "Engine/Blueprint.h"
#include "Blueprint/UserWidget.h"

ELuaBindingStatus GetBindingStatus(const UBlueprint* Blueprint)
{
    if (!Blueprint)
        return ELuaBindingStatus::NotBound;

    if (Blueprint->Status == EBlueprintStatus::BS_Dirty)
        return ELuaBindingStatus::Unknown;

    const auto Target = Blueprint->GeneratedClass;

    if (!IsValid(Target))
        return ELuaBindingStatus::NotBound;

    if (!Target->ImplementsInterface(UUnLuauNovaInterface::StaticClass()))
        return ELuaBindingStatus::NotBound;

    const auto Settings = GetDefault<UUnLuauNovaSettings>();
    if (!Settings || !Settings->ModuleLocatorClass)
        return ELuaBindingStatus::Unknown;

    const auto ModuleLocator = Cast<ULuaModuleLocator>(Settings->ModuleLocatorClass->GetDefaultObject());
    const auto ModuleName = ModuleLocator->Locate(Target);
    if (ModuleName.IsEmpty())
        return ELuaBindingStatus::Unknown;

    const auto RelativePath = ModuleName.Replace(TEXT("."), TEXT("/")) + TEXT(".lua");
    const auto FullPath = GLuaSrcFullPath + "/" + RelativePath;
    if (!FPaths::FileExists(FullPath))
        return ELuaBindingStatus::BoundButInvalid;

    return ELuaBindingStatus::Bound;
}
