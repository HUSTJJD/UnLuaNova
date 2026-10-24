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

        // No Private/ folder to add: every header of this module lives in Public/, so a
        // consumer (UnLuaNovaEditor, UnLuaNovaTestSuite) includes it the ordinary way
        // instead of reaching for a private path it has no business knowing about.
        PrivateIncludePaths.AddRange(
            new string[]
            {
            }
        );

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "InputCore",
                // IPluginManager：按插件实例解析脚本根目录与 ini，别再拼"Plugins/UnLuaNova/..."
                "Projects"
            }
        );

        if (Target.bBuildEditor)
        {
            OptimizeCode = CodeOptimization.Never;
            PrivateDependencyModuleNames.Add("UnrealEd");
        }

        var projectDir = Target.ProjectFile.Directory;

        // 全插件只有一个配置文件：Config/DefaultUnLuaNova.ini，两个 section 同处其中。
        // 没有旧文件名的兼容分支 —— 配置入口只有一个，读的地方也就只该有一个。
        var configFilePath = projectDir + "/Config/DefaultUnLuaNova.ini";
        var configFileReference = new FileReference(configFilePath);
        var configFile = FileReference.Exists(configFileReference) ? new ConfigFile(configFileReference) : new ConfigFile();
        var config = new ConfigHierarchy(new[] { configFile });

        const string section = "/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings";
        const string settingsSection = "/Script/UnLuaNova.UnLuaNovaSettings";

        bool TryGetBool(string sect, string key, out bool value)
        {
            return config.GetBool(sect, key, out value);
        }

        string GetString(string sect, string key, string defaultValue)
        {
            string value;
            return config.GetString(sect, key, out value) ? value : defaultValue;
        }

        // Everything below is read out of this ini, and UBT has no reason to watch it: the
        // makefile it caches holds the rules' output, and without this the only file it checks
        // is the .Build.cs itself. Editing LuaBackend then changed nothing - the build happily
        // handed back the previous backend's objects, and Tools/RunTests.ps1 had to delete
        // Plugins/UnLuaNova/Intermediate after every switch to force the issue.
        //
        // Listed here, a modified ini invalidates the makefile: the rules are evaluated again,
        // the definitions change, and whatever includes them is rebuilt. That is still a
        // recompile - a different backend is a different build - but it is UBT's own decision
        // about what is stale, instead of "throw the whole directory away".
        ExternalDependencies.Add(configFilePath);

        Action<string, string, bool> loadBoolConfig = (key, macro, defaultValue) =>
        {
            bool flag;
            if (!TryGetBool(section, key, out flag))
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
        // Config/DefaultUnLuaNova.ini -> LuaBackend=Lua).
        //
        // Values are matched case-insensitively; anything other than "Lua" selects Luau,
        // which is what LuaBackend.h defaults to when the macro is absent.
        var backend = GetString(section, "LuaBackend", "Luau");

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

        var hotReloadMode = GetString(section, "HotReloadMode", "Manual");
        var withHotReload = hotReloadMode != "Never";
        PublicDefinitions.Add("UNLUANOVA_WITH_HOT_RELOAD=" + (withHotReload ? "1" : "0"));

        // --- 特性开关（编译期那部分） -----------------------------------------
        // 内置库是编译期开关：关掉的代码不进二进制，require 直接失败。
        // 键就在**同一个文件**的运行时 section 里（bTimer / bEvent / bOOP），
        // 和运行时设置完全同名同处 —— 不存在"第二处开关"。
        // 其余特性（数据驱动绑定、DAP…）是运行时的，由 UUnLuaNovaSettings 直接读，不经过这里。

        Action<string, string, bool> loadFeature = (key, macro, defaultValue) =>
        {
            bool flag;
            if (!TryGetBool(settingsSection, key, out flag))
                flag = defaultValue;
            PublicDefinitions.Add(string.Format("{0}={1}", macro, (flag ? "1" : "0")));
        };

        loadFeature("bTimer", "UNLUANOVA_HAS_STDLIB_TIMER", true);
        loadFeature("bEvent", "UNLUANOVA_HAS_STDLIB_EVENT", true);
        loadFeature("bOOP", "UNLUANOVA_HAS_STDLIB_OOP", true);
    }
}