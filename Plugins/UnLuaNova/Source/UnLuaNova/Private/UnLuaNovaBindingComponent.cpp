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

#include "UnLuaNovaBindingComponent.h"

UUnLuaNovaBindingComponent::UUnLuaNovaBindingComponent()
{
    // 纯配置载体：不 Tick、不需要 InitializeComponent，挂上去只是为了让 Locator 能读到模块名
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    bWantsInitializeComponent = false;
    SetIsReplicatedByDefault(false);
}
