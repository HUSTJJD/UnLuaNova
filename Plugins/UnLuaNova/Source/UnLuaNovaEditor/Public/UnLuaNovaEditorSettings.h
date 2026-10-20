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
#include "UObject/Object.h"
#include "UnLuaNovaEditorSettings.generated.h"

#define LOCTEXT_NAMESPACE "UnLuaNovaEditorSettings"

UENUM()
enum class EUpdateMode : uint8
{
    Start,
    Manual,
    None,
};

UENUM()
enum class EHotReloadMode : uint8
{
    Manual,
    Auto,
    Never
};

/** 静态绑定生成覆盖哪些类型。 */
UENUM()
enum class EUnLuaNovaStaticBindingSelection : uint8
{
    /** 插件默认静态导出的那批（UObject / UClass / UWorld + 数学结构）。
     *  清单从运行时注册表读，不是硬编码。默认就是这个。 */
    ExportedReflected UMETA(DisplayName = "默认静态导出的类型"),

    /** StaticBindingModules 列出的包里的全部类型。 */
    Modules UMETA(DisplayName = "指定模块"),

    /** 全部类型，只排除 meta=(UnLuaNovaNoExport)。量大，编译会明显变慢。 */
    All UMETA(DisplayName = "全部（opt-out）"),
};

// config=UnLuaNova：和运行时设置类指向**同一份** Config/DefaultUnLuaNova.ini（见上同款注释）。
// 两个 section 同处一个文件：运行时与编辑器分属两个模块，但配置只该有一个入口。
UCLASS(config=UnLuaNova, defaultconfig, meta=(DisplayName="UnLuaNovaEditor"))
class UNLUANOVAEDITOR_API UUnLuaNovaEditorSettings : public UObject
{
    GENERATED_BODY()

public:
    /** Pick a method for lua hot-reload. */
    UPROPERTY(config, EditAnywhere, Category = "Coding", meta = (defaultValue = 0))
    EHotReloadMode HotReloadMode;

    /** Whether or not generate intellisense files for lua. */
    UPROPERTY(config, EditAnywhere, Category = "Coding")
    bool bGenerateIntelliSense = true;

    /**
     * 产物目录还不存在时，编辑器启动就自动导出一次 IntelliSense。
     *
     * 省掉"装完插件还得知道要去工具栏点一下"这一步 —— 这是新用户最常卡住的地方：
     * 没产物就没有补全，而"没有补全"往往被误判成"这个方案不好用"。
     * 只在**首次**跑（目录已存在就交给工具栏 / 目录监听做增量），不会拖慢每次启动。
     */
    UPROPERTY(config, EditAnywhere, Category = "Coding")
    bool bAutoGenerateIntelliSenseOnStartup = true;

    /** Whether or not startup UnLuaNova module on game start. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    bool bAutoStartup = true;

    /** Enable UnLuaNova debug routine codes. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    bool bEnableDebug = false;

    // 特性开关**不在这里**：统一在 UUnLuaNovaSettings（Config/DefaultUnLuaNovaSettings.ini）。
    // 这个文件只放编辑器行为与构建配置（热重载模式、智能提示、后端选择…）。

    /** Enable persistent buffer for UFunction's parameters. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    bool bEnablePersistentParamBuffer = true;

    /** Enable type checking at lua runtime. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    bool bEnableTypeChecking = true;

    /** Enable 'Overridden' support at lua runtime. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    bool bEnableCallOverriddenFunction = true;

    // ---- 静态绑定 ----------------------------------------------------------
    //
    // 构建期生成的静态绑定表（属性按 Offset+Kind 直读直写、函数走编译期定型的 thunk）。
    // 生成是**自动**的：开着 bStaticBinding，编辑器启动时会比对产物指纹，缺了或过期
    // 就重新生成，不需要人去跑 Commandlet。Commandlet 保留给 CI / 构建机。
    //
    // 默认开启：这是插件的核心性能特性，不该依赖用户想起来才生效。默认范围只有十来个
    // 类型（见 StaticBindingSelection），所以"生成 -> 重编译一次"这条链路的代价很轻。
    // 真正需要关掉的情况只有一个：把范围切到 All 之后，类型增删会频繁触发大重编译，
    // 那时改由 CI 生成更合适。

    /**
     * 静态绑定总开关。开启后编辑器启动会自动检查并生成产物。
     *
     * 注意"生效"分两步：先自动生成（本次启动就写盘），再重编译一次（thunk 才进
     * 二进制）。日志会明确说当前处在哪一步。
     */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    bool bStaticBinding = true;

    /** 产物缺失或类型集合变化时自动重新生成。关掉就只在有产物时用，永不生成。 */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    bool bAutoGenerateStaticBinding = true;

    /**
     * 生成覆盖哪些类型。默认 ExportedReflected —— 只生成插件本来就静态导出的那批
     * （UObject / UClass / UWorld + FVector 等数学结构），十来个类型。
     *
     * /Script/Engine 一个包就是几千个类型，全量生成的产物会让编译明显变慢，而真正被
     * 脚本在循环里反复摸的就是这十来个 —— 所以默认范围就停在它们身上。要更大的覆盖面
     * 再切成 Modules 或 All。
     */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    EUnLuaNovaStaticBindingSelection StaticBindingSelection = EUnLuaNovaStaticBindingSelection::ExportedReflected;

    /** StaticBindingSelection=Modules 时生效：只生成这些包里的类型。 */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    TArray<FString> StaticBindingModules;

    /**
     * 产物目录。留空 = UnLuaNova 插件自己的模块目录（<UnLuaNova>/Source/UnLuaNova/Generated）
     * —— 只有落在模块目录里才会被 UBT 编译。
     *
     * 默认不给宿主工程目录：工程名并不等于主模块目录名（宿主 LyraStarterGame 的主模块是
     * Source/LyraGame），按工程名推会把产物写进没有 Build.cs 的目录，UBT 永远看不到它。
     * 插件自己的模块一定存在、且本来就依赖 UnLuaNova，产物放这儿必然被编译。
     */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    FString StaticBindingOutputDir;

    /** Enable FText support at lua runtime which will no longer be treated as a string. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    bool bEnableFText = true;

    /** Whether or not compile lua module as c++ code. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    bool bLuaCompileAsCpp = false;

    /** Use the specified lua version. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Build")
    FString LuaVersion = TEXT("lua-5.4.3");

    /** Create UE4 global table on lua env. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Legacy")
    bool bWithUE4Namespace = true;

    /** Place out parameters before return value. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Legacy")
    bool bLegacyReturnOrder = false;

    /** Auto append '_C' to blueprint class path. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Legacy")
    bool bLegacyBlueprintPath = false;

    /** Allow lua file with UTF-8 BOM header. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Legacy")
    bool bLegacyAllowUTF8WithBOM = false;

    /** Arguments are passed to lua by pointer when called from UE. (Requires restart to take effect) */
    UPROPERTY(config, EditAnywhere, Category = "Legacy")
    bool bLegacyArgsPassing = true;

    UPROPERTY(config, EditAnywhere, Category = "System", meta = (defaultValue = 0))
    EUpdateMode UpdateMode;
};

#undef LOCTEXT_NAMESPACE
