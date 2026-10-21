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
        // Removed: ENABLE_UNREAL_INSIGHTS was a compile-time flag for the UE-side trace
        // scopes in FunctionDesc.cpp. Those are now gated at runtime on the profiler
        // toggle ('lua.profiler'), so a rebuild is no longer needed to turn them on.
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
        //   Lua  -> Source/ThirdParty/LuaLib/Lua    (github.com/lua/lua)
        //   Luau -> Source/ThirdParty/LuauLib/Luau  (github.com/luau-lang/luau)
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

        // The selected backend's headers must be on disk before anything is compiled.
        // They are submodules, and an uninitialised one used to fail in the worst way
        // possible: UBT notes "Luau has been deleted since makefile was built", drops it
        // from the build, and the Luau call sites then quietly compile against whatever
        // other backend is lying around. Refuse to generate a build at all instead.
        Action<string, string> requireBackendHeaders = (name, header) =>
        {
            if (!File.Exists(header))
            {
                throw new BuildException(
                    string.Format("The {0} backend is selected but its sources are missing: {1} does not exist. "
                        + "The Lua backends are git submodules - run 'git submodule update --init --recursive' "
                        + "and rebuild.", name, header));
            }
        };

        // Only "Lua" selects the plain-Lua backend; anything else selects Luau, which is
        // the default.
        //
        // The name is the backend, never its version: upgrading Lua is moving the submodule
        // pointer, so a version baked into this string - or into the module name - goes
        // stale the moment the submodule moves. Versioned aliases were accepted here once
        // and have been removed on purpose; do not re-add them.
        if (string.Equals(backend, "Lua", StringComparison.OrdinalIgnoreCase))
        {
            requireBackendHeaders("Lua", Path.Combine(ModuleDirectory, "..", "ThirdParty", "LuaLib", "Lua", "lua.h"));
            PublicDependencyModuleNames.Add("Lua");
            PublicDefinitions.Add("UNLUANOVA_LUA_BACKEND=0");
            // See the note below the if/else: switching backends leaves the other one's DLL
            // behind in Binaries/, so state which one this build is actually using.
            Console.WriteLine("[UnLuaNova] Lua backend: Lua (official Lua) -> {0}-Lua.dll", Target.Name);
        }
        else
        {
            requireBackendHeaders("Luau", Path.Combine(ModuleDirectory, "..", "ThirdParty", "LuauLib", "Luau", "VM", "include", "lua.h"));
            PublicDependencyModuleNames.Add("Luau");
            PublicDefinitions.Add("UNLUANOVA_LUA_BACKEND=1");
            Console.WriteLine("[UnLuaNova] Lua backend: Luau -> {0}-Luau.dll", Target.Name);
        }

        // Two backend DLLs with near-identical names can sit side by side in
        // Binaries/<Platform>/: nothing deletes the one that is no longer selected, so
        // "UnrealEditor-Luau.dll exists" does not mean Luau is what this build uses.
        // The name can never disagree with the contents - the module name decides which
        // source tree UBT compiles - but which of the two is current cannot be read off
        // the directory, hence the line above.

        string hotReloadMode;
        if (!config.GetString(section, "HotReloadMode", out hotReloadMode))
            hotReloadMode = "Manual";

        var withHotReload = hotReloadMode != "Never";
        PublicDefinitions.Add("UNLUANOVA_WITH_HOT_RELOAD=" + (withHotReload ? "1" : "0"));
    }
}