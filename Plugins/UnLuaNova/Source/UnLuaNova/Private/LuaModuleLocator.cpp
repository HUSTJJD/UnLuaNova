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

#include "LuaModuleLocator.h"
#include "UnLuaNovaInterface.h"
#include "UnLuaNovaSettings.h"
#include "UnLuaNovaBindingComponent.h"
#include "GameFramework/Actor.h"

FString ULuaModuleLocator::Locate(const UObject* Object)
{
    if (!Object)
    {
        return "";
    }

    const UObject* CDO;
    if (Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
    {
        CDO = Object;
    }
    else
    {
        const auto Class = Cast<UClass>(Object);
        const UClass* ObjectClass = Class ? Class : Object->GetClass();
        if (!ObjectClass)
        {
            return "";
        }

        CDO = ObjectClass->GetDefaultObject();
    }

    // GetDefaultObject() 不保证非空：还在加载中、还没走完构造的类（异步加载会成批喂进来
    // 这种对象）就没有 CDO。以前直接往下走，于是一处空指针崩溃。
    // 拿不到 CDO 就当"不是脚本绑定目标"，跳过。
    if (!CDO)
    {
        return "";
    }

    if (CDO->HasAnyFlags(RF_NeedInitialization))
    {
        // CDO还没有初始化完成
        return "";
    }

    if (!CDO->GetClass()->ImplementsInterface(UUnLuaNovaInterface::StaticClass()))
    {
        return "";
    }

    return IUnLuaNovaInterface::Execute_GetModuleName(CDO);
}

FString ULuaModuleLocator_ByBinding::Locate(const UObject* Object)
{
    // ① 老规矩优先：实现了接口就听接口的，一级不差地保留旧行为
    const FString FromInterface = Super::Locate(Object);
    if (!FromInterface.IsEmpty())
        return FromInterface;

    // Super 已经挡掉了空 Object；这里再挡一次，因为下面要直接解引用。
    // TryBind 在异步加载 flush 时会成批喂对象进来，防御性判空比崩溃划算。
    if (!Object)
        return FString();

    const UClass* Class = Object->IsA<UClass>() ? static_cast<const UClass*>(Object) : Object->GetClass();
    if (!Class)
        return FString();

    const FName Key = Class->GetFName();
    if (const FString* Cached = Cache.Find(Key))
        return *Cached;
    if (Misses.Contains(Key))
        return FString();

    // ③ ini 里的 Class → Module 映射
    FString ModuleName = LocateByConfig(Class);

    // ④ Actor 上的绑定组件
    if (ModuleName.IsEmpty())
        ModuleName = LocateByComponent(Object);

    if (ModuleName.IsEmpty())
    {
#if !WITH_EDITOR
        // 打包后缓存"没绑"：TryBind 对每个新建对象都会问一次，别每次都去翻组件。
        // 编辑器里不缓存 —— 改完蓝图上的组件要立刻生效，不能等到重启。
        Misses.Add(Key);
#endif
        return FString();
    }

    Cache.Add(Key, ModuleName);
    return ModuleName;
}

FString ULuaModuleLocator_ByBinding::LocateByConfig(const UClass* Class) const
{
    const UUnLuaNovaSettings* Settings = GetDefault<UUnLuaNovaSettings>();
    if (!Settings || Settings->ClassModuleBindings.Num() == 0)
        return FString();

    // 构建时就要避免"为了匹配而加载资产"：TryBind 是在 UObject 创建过程中调的，
    // 那里面 LoadObject 是自找麻烦。所以只比字符串，不解析 Class。
    const FString PathName = Class->GetPathName();

    // ① 完整路径精确匹配（/Game/Blueprints/BP_Foo.BP_Foo_C）
    for (const FUnLuaNovaClassModuleBinding& Binding : Settings->ClassModuleBindings)
    {
        if (Binding.ModuleName.IsEmpty() || !Binding.Class.IsValid())
            continue;
        if (Binding.Class.ToString() == PathName)
            return Binding.ModuleName;
    }

    // ② 类名匹配，并沿继承链向上找：绑"AKDemoCharacter"就能覆盖它的蓝图子类
    //    （BP_DemoCharacter 的类名不是 AKDemoCharacter，只能靠父类链命中）
    for (const UClass* Current = Class; Current; Current = Current->GetSuperClass())
    {
        const FString Name = Current->GetName();
        for (const FUnLuaNovaClassModuleBinding& Binding : Settings->ClassModuleBindings)
        {
            if (Binding.ModuleName.IsEmpty())
                continue;

            // 既收裸类名（"AKDemoCharacter"）也收完整路径里的对象名
            // （"/Script/UAKDemo.AKDemoCharacter" → GetAssetName() = "AKDemoCharacter"）
            if (Binding.ClassName == Current->GetFName() || Binding.Class.GetAssetName() == Name)
                return Binding.ModuleName;
        }
    }

    return FString();
}

FString ULuaModuleLocator_ByBinding::LocateByComponent(const UObject* Object)
{
    auto ReadFrom = [](const UObject* Candidate) -> FString
    {
        const AActor* Actor = Cast<AActor>(Candidate);
        if (!Actor)
            return FString();

        // TryBind 对每个新建对象都会问一次，所以这里用 inline allocator：
        // 常见情况（组件数 < 8）不产生堆分配，别把开销加在 Actor 创建路径上
        TArray<UUnLuaNovaBindingComponent*, TInlineAllocator<8>> Components;
        // GetComponents 在部分版本里不是 const，这里统一 const_cast，两种签名都能编过
        const_cast<AActor*>(Actor)->GetComponents(Components);
        for (const UUnLuaNovaBindingComponent* Component : Components)
        {
            if (Component && Component->bEnabled && !Component->ModuleName.IsEmpty())
                return Component->ModuleName;
        }
        return FString();
    };

    // 实例优先：TryBind 是在对象创建时调的，这时候组件已经在对象上了
    const FString FromInstance = ReadFrom(Object);
    if (!FromInstance.IsEmpty())
        return FromInstance;

    // 拿到的是 UClass（启动预绑 / PreBindClasses）时，读 CDO 上的组件模板
    if (const UClass* Class = Cast<UClass>(Object))
        return ReadFrom(Class->GetDefaultObject());

    return FString();
}

FString ULuaModuleLocator_ByPackage::Locate(const UObject* Object)
{
    const auto Class = Object->IsA<UClass>() ? static_cast<const UClass*>(Object) : Object->GetClass();
    const auto Key = Class->GetFName();
    const auto Cached = Cache.Find(Key);
    if (Cached)
        return *Cached;

    FString ModuleName;
    if (Class->IsNative())
    {
        ModuleName = Class->GetName();
    }
    else
    {
        ModuleName = Object->GetOutermost()->GetName();
        const auto ChopCount = ModuleName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 1) + 1;
        ModuleName = ModuleName.Replace(TEXT("/"), TEXT(".")).RightChop(ChopCount);
    }
    Cache.Add(Key, ModuleName);
    return ModuleName;
}
