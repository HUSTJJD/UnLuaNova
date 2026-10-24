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
