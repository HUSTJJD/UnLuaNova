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
#include "LuaEnvLocator.h"
#include "LuaModuleLocator.h"
#include "UnLuaNovaSettings.generated.h"


/** 一条"类 → Lua 模块"的映射：给没法（或不想）实现 IUnLuaNovaInterface 的类用。 */
USTRUCT()
struct FUnLuaNovaClassModuleBinding
{
    GENERATED_BODY()

    /** Full class path, e.g. /Game/Blueprints/BP_Foo.BP_Foo_C. */
    UPROPERTY(EditAnywhere, Category="Runtime")
    FSoftClassPath Class;

    /** Optional short class name, also matched against parent classes. */
    UPROPERTY(EditAnywhere, Category="Runtime")
    FName ClassName;

    /** Lua 模块名，相对 Content/Script，点分。 */
    UPROPERTY(EditAnywhere, Category="Runtime")
    FString ModuleName;
};

// Config=UnLuaNova：和编辑器设置类指向**同一份** Config/DefaultUnLuaNova.ini。
// 全插件只有一个配置文件 —— 拆成 DefaultUnLuaNovaSettings / DefaultUnLuaNovaEditor 两份之后，
// "改一个开关该去哪个文件找"本身就成问题。
UCLASS(Config=UnLuaNova, DefaultConfig, Meta=(DisplayName="UnLuaNova"))
class UNLUANOVA_API UUnLuaNovaSettings : public UObject
{
    GENERATED_BODY()

public:
    UUnLuaNovaSettings(const FObjectInitializer& ObjectInitializer);

    /** Entry module name of lua env. Leave it empty to skip execution on startup. */
    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    FString StartupModuleName = TEXT("");

    /** Prevent from infinite loops in lua. Timeout in seconds. */
    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    int32 DeadLoopCheck = 0;

    /** Prevent dangling pointers in lua. */
    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    bool DanglingCheck = false;

    /** Whether to print all Lua env stacks on crash. */
    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    bool bPrintLuaStackOnSystemError = true;

    /** Class of LuaEnvLocator, which handles lua env locating for each UObject. */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", Meta=(AllowAbstract="false"))
    TSubclassOf<ULuaEnvLocator> EnvLocatorClass = ULuaEnvLocator::StaticClass();

    UPROPERTY(Config, EditAnywhere, Category=Runtime, Meta=(AllowAbstract="false", DisplayName="LuaModuleLocator"))
    TSubclassOf<ULuaModuleLocator> ModuleLocatorClass = ULuaModuleLocator_ByBinding::StaticClass();

    /**
     * Class -> Lua module bindings, for classes that cannot implement IUnLuaNovaInterface
     * (pure-blueprint classes, third-party classes). 改 ini 就能绑，不需要动 UE 侧代码。
     *
     * [Script/UnLuaNova.UnLuaNovaSettings]
     * +ClassModuleBindings=(Class="/Game/Blueprints/BP_Foo.BP_Foo_C",ModuleName="Foo")
     */
    UPROPERTY(Config, EditAnywhere, Category=Runtime, Meta=(DisplayName="Class Module Bindings"))
    TArray<FUnLuaNovaClassModuleBinding> ClassModuleBindings;

    // ==== 特性开关（Features） ================================================
    //
    // 全插件**只有这一处**定义特性开关，规则固定三条：
    //   ① 键名一律 `b<CamelCase>`，查询名就是去掉 `b` 的小写形式（`bTimer` → `HasFeature('timer')`）
    //   ② 全部集中在 Config/DefaultUnLuaNovaSettings.ini 的 [/Script/UnLuaNova.UnLuaNovaSettings]
    //   ③ 脚本侧只通过 UnLuaNova.HasFeature('<名>') 问，C++ 侧只通过 UnLuaNovaLib 的那张表问
    //
    // 下面这几个**同时**是编译期开关（UNLUANOVA_HAS_STDLIB_* 由 Build.cs 读同一份 ini 生成），
    // 关掉的代码不进二进制 —— 这是"编译期 vs 运行时"唯一的例外理由，其余全部运行时。
    // 需要重编的在这里标 [需重编]。
    //
    // 后端能力（sandbox / vector / buffer / …）不在这里：它们由后端决定，不可配置，
    // 但对外走同一个 HasFeature 入口，所以脚本侧看起来完全一致。

    /** 内置库 UnLuaNova.Timer：脚本计时器（native，走世界时间）。[需重编] */
    UPROPERTY(Config, EditAnywhere, Category="Features", Meta=(DisplayName="Timer (StdLib)"))
    bool bTimer = true;

    /** 内置库 UnLuaNova.Event：事件总线（纯 Lua）。[需重编] */
    UPROPERTY(Config, EditAnywhere, Category="Features", Meta=(DisplayName="Event (StdLib)"))
    bool bEvent = true;

    /** 内置库 UnLuaNova.OOP：脚本面向对象（纯 Lua）。[需重编] */
    UPROPERTY(Config, EditAnywhere, Category="Features", Meta=(DisplayName="OOP (StdLib)"))
    bool bOOP = true;

    /**
     * 允许给**没有实现 IUnLuaNovaInterface** 的对象绑 Lua 模块（数据驱动绑定）。
     *
     * 默认 false：不配置就绝不自动绑。这是条安全边界 —— 开着的时候，任何能匹配上
     * ClassModuleBindings / 绑定组件的对象都会在它不知情的情况下被绑上脚本；
     * 把它做成配置而不是代码里的常量，是因为"要不要冒这个险"是项目决策，不是库的决策。
     */
    UPROPERTY(Config, EditAnywhere, Category="Features", Meta=(DisplayName="Data-Driven Binding"))
    bool bDataDrivenBinding = false;

    /** lua.dap：把 headless 调试服务接成 DAP 服务，供 IDE attach。 */
    UPROPERTY(Config, EditAnywhere, Category="Features", Meta=(DisplayName="Debug Adapter (lua.dap)"))
    bool bDebugAdapter = true;

    /** lua.dap 默认端口（`lua.dap start <端口>` 可临时改）。 */
    UPROPERTY(Config, EditAnywhere, Category="Features", Meta=(DisplayName="Debug Adapter Port"))
    int32 DebugAdapterPort = 5699;

    /** List of classes to bind on startup. */
    UPROPERTY(config, EditAnywhere, Category=Runtime, meta = (MetaClass="/Script/CoreUObject.Object", AllowAbstract="True", DisplayName = "List of classes to bind on startup"))
    TArray<FSoftClassPath> PreBindClasses;
};
