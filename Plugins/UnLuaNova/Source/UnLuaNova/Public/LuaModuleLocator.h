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
#include "LuaModuleLocator.generated.h"

UCLASS()
class UNLUANOVA_API ULuaModuleLocator : public UObject
{
    GENERATED_BODY()
public:
    virtual FString Locate(const UObject* Object);

    // 注意：这里**没有**"能不能给未实现接口的对象绑"这类开关。
    // 那是项目策略，属于配置（UUnLuaNovaSettings::bDataDrivenBinding），
    // 不是某个 locator 子类在 C++ 里声明的属性 —— locator 只负责"给得出模块名吗"。
};

UCLASS()
class UNLUANOVA_API ULuaModuleLocator_ByPackage : public ULuaModuleLocator
{
    GENERATED_BODY()
public:
    virtual FString Locate(const UObject* Object) override;

private:
    TMap<FName, FString> Cache;
};

/**
 * 数据驱动定位器（默认）：按"接口 → 配置映射 → 绑定组件"的顺序找模块名。
 *
 * 前两级保证与旧行为逐字一致（没配任何东西时结果仍是空串），第三级才是新增能力：
 *   ① IUnLuaNovaInterface::GetModuleName（老路子，C++ 或蓝图实现接口）
 *   ② DefaultUnLuaNovaSettings.ini 里的 ClassModuleBindings（改配置就能绑，连蓝图都不用开）
 *   ③ Actor 上的 UUnLuaNovaBindingComponent（BP-only 团队最省事的一条路）
 *
 * 它给出模块名之后**是否真的绑上去**，由 UUnLuaNovaSettings::bDataDrivenBinding 决定
 * （默认关）：本类不掌握那个策略，只回答"这个对象我能给出模块名吗"。
 *
 */
UCLASS()
class UNLUANOVA_API ULuaModuleLocator_ByBinding : public ULuaModuleLocator
{
    GENERATED_BODY()
public:
    virtual FString Locate(const UObject* Object) override;

private:
    FString LocateByConfig(const UClass* Class) const;
    static FString LocateByComponent(const UObject* Object);

    TMap<FName, FString> Cache;
    TSet<FName> Misses;
};

