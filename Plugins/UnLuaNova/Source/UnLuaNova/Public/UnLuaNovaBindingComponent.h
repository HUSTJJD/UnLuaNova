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
#include "Components/ActorComponent.h"
#include "UnLuaNovaBindingComponent.generated.h"

/**
 * 数据驱动绑定的载体：往蓝图 Actor 上挂一个，填模块名，就有了 Lua 绑定。
 *
 * 存在的理由：老路子是让 C++ 类实现 IUnLuaNovaInterface 并覆写 GetModuleName，
 * 这对"只是想在蓝图上写点逻辑"的团队是一堵墙 —— 要么改 C++ 重编译，要么放弃。
 * 组件是蓝图能自己加的东西，所以它是覆盖面最大的那条路（配合
 * ULuaModuleLocator_ByBinding 使用，默认已启用）。
 *
 * 注意：绑定发生在对象创建时（见 FLuaEnv::TryBind），模块名在这里是**类级**配置，
 * 运行期改 ModuleName 不会重新绑定，要热更请走 HotReload。
 */
UCLASS(ClassGroup=(UnLuaNova), HideCategories=(ComponentReplication, Activation, Cooking, AssetUserData, Collision),
       meta=(BlueprintSpawnableComponent, DisplayName="UnLuaNova Binding"))
class UNLUANOVA_API UUnLuaNovaBindingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnLuaNovaBindingComponent();

    /** Lua 模块名，相对 Content/Script，点分（"UAKDemoCharacter" → Content/Script/UAKDemoCharacter.lua）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UnLuaNova", meta=(DisplayName="Lua Module"))
    FString ModuleName;

    /** 关掉就等于没挂这个组件（临时摘绑不用删组件）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UnLuaNova")
    bool bEnabled = true;
};
