// Tencent is pleased to support the open source community by making UNLUANOVA available.
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

using System;
using System.IO;
#if UE_5_0_OR_LATER
using EpicGames.Core;
#else
using Tools.DotNETCommon;
#endif
using UnrealBuildTool;

public class UnLuaNova : ModuleRules
{
    public UnLuaNova(ReadOnlyTargetRules Target) : base(Target)
    {
#if UE_5_2_OR_LATER
        IWYUSupport = IWYUSupport.None;
#else
        bEnforceIWYU = false;
#endif
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[]
            {
            }
        );

        PrivateIncludePaths.AddRange(
            new[]
            {
                "UnLuaNova/Private",
            }
        );

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "InputCore"
            }
        );

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        if (Target.bBuildEditor)
        {
            OptimizeCode = CodeOptimization.Never;
            PrivateDependencyModuleNames.Add("UnrealEd");
        }

        var projectDir = Target.ProjectFile.Directory;
        var configFilePath = projectDir + "/Config/DefaultUnLuaNovaEditor.ini";
        var configFileReference = new FileReference(configFilePath); 
        var configFile = FileReference.Exists(configFileReference) ? new ConfigFile(configFileReference) : new ConfigFile();
        var config = new ConfigHierarchy(new[] { configFile });
        const string section = "/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings";

        Action<string, string, bool> loadBoolConfig = (key, macro, defaultValue) =>
        {
            bool flag;
            if (!config.GetBool(section, key, out flag))
                flag = defaultValue;
            PublicDefinitions.Add(string.Format("{0}={1}", macro, (flag ? "1" : "0")));
        };
        
        loadBoolConfig("bAutoStartup", "AUTO_UNLUA_STARTUP", true);
        loadBoolConfig("bEnableDebug", "UNLUANOVA_ENABLE_DEBUG", false);
        loadBoolConfig("bEnablePersistentParamBuffer", "ENABLE_PERSISTENT_PARAM_BUFFER", true);
        loadBoolConfig("bEnableTypeChecking", "ENABLE_TYPE_CHECK", true);
        loadBoolConfig("bEnableUnrealInsights", "ENABLE_UNREAL_INSIGHTS", false);
        loadBoolConfig("bEnableCallOverriddenFunction", "ENABLE_CALL_OVERRIDDEN_FUNCTION", true);
        loadBoolConfig("bEnableFText", "UNLUANOVA_ENABLE_FTEXT", false);
        loadBoolConfig("bLuaCompileAsCpp", "LUA_COMPILE_AS_CPP", false);
        loadBoolConfig("bWithUE4Namespace", "WITH_UE4_NAMESPACE", true);
        loadBoolConfig("bLegacyReturnOrder", "UNLUANOVA_LEGACY_RETURN_ORDER", false);
        loadBoolConfig("bLegacyBlueprintPath", "UNLUANOVA_LEGACY_BLUEPRINT_PATH", false);
        loadBoolConfig("bLegacyAllowUTF8WithBOM", "UNLUANOVA_LEGACY_ALLOW_BOM", false);
        loadBoolConfig("bLegacyArgsPassing", "UNLUANOVA_LEGACY_ARGS_PASSING", true);

        // --- Lua backend -------------------------------------------------------------
        // The single place the backend is chosen. Both are submodules:
        //   Lua  -> Source/ThirdParty/Lua   (github.com/lua/lua),          wrapper: LuaLib
        //   Luau -> Source/ThirdParty/Luau  (github.com/luau-lang/luau),   wrapper: LuauLib
        //                                                                    <-- default
        //
        // This is a compile-time choice, not a runtime one: the two backends export the
        // same lua_* symbols and have incompatible bytecode, so they cannot coexist in a
        // single build. Set it here (or override per-project in
        // Config/DefaultUnLuaNovaEditor.ini -> LuaBackend=Lua).
        //
        // Values are matched case-insensitively; anything other than "Lua" selects Luau,
        // which is what LuaBackend.h defaults to when the macro is absent.
        string backend;
        if (!config.GetString(section, "LuaBackend", out backend))
            backend = "Luau";

        if (string.Equals(backend, "Lua", StringComparison.OrdinalIgnoreCase))
        {
            PublicDependencyModuleNames.Add("Lua");
            PublicDefinitions.Add("UNLUANOVA_LUA_BACKEND=0");
        }
        else
        {
            PublicDependencyModuleNames.Add("Luau");
            PublicDefinitions.Add("UNLUANOVA_LUA_BACKEND=1");
        }

        string hotReloadMode;
        if (!config.GetString(section, "HotReloadMode", out hotReloadMode))
            hotReloadMode = "Manual";

        var withHotReload = hotReloadMode != "Never";
        PublicDefinitions.Add("UNLUANOVA_WITH_HOT_RELOAD=" + (withHotReload ? "1" : "0"));
    }
}