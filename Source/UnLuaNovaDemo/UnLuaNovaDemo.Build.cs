// Tencent is pleased to support the open source community by making UnLua available.
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

public class UnLuaNovaDemo : ModuleRules
{
    public UnLuaNovaDemo(ReadOnlyTargetRules Target) : base(Target)
    {
        // [UAK BACKEND LAYOUT - keep this block identical in every Build.cs]
        // Isolated targets are named UnLuaNovaDemo<Backend>{Editor|Server|''}: one name per
        // carrier, <Backend> being Lua or Luau. Editor targets share AppName "UnrealEditor";
        // Game/Server targets use their own name as AppName. Either way a second target alone
        // would overwrite the same DLLs and object files, so ModuleRules.BinariesSubFolder
        // redirects both (GetModuleIntermediateDirectory / CreateDynamicLibraryForModule).
        // Target.Name is the only per-target value a ModuleRules constructor can read without
        // changing the shared build environment (GlobalDefinitions is
        // [RequiresUniqueBuildEnvironment]).
        const string backendTargetPrefix = "UnLuaNovaDemo";
        string backendVariant = "";
        if (Target.Name.StartsWith(backendTargetPrefix))
        {
            string backendRest = Target.Name.Substring(backendTargetPrefix.Length);
            string[] backendSuffixes = { "Editor", "Server", "" };
            foreach (string backendSuffix in backendSuffixes)
            {
                if (backendRest.EndsWith(backendSuffix))
                {
                    backendVariant = backendRest.Substring(0, backendRest.Length - backendSuffix.Length);
                    break;
                }
            }
        }
        if (backendVariant.Length > 0)
        {
            if (backendVariant != "Lua" && backendVariant != "Luau")
                throw new BuildException("Unknown backend target '" + Target.Name
                    + "': expected UnLuaNovaDemo{,Lua,Luau}{,Editor,Server}.");
            BinariesSubFolder = "Backends/" + backendVariant;
        }
        // [/UAK BACKEND LAYOUT]
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
            });

        // The backend has to be listed here, even though UnLuaNova already depends on it.
        //
        // UnLuaNova's public headers call lua_* directly - LuaBackend.h and the templates
        // built on it (FLuaFunction::Call, FLuaTable::Call, TConstructor<>::Register, ...)
        // are all instantiated in *this* module, so the references to lua_gettop & friends
        // live in UnLuaNovaDemo's own objects. UEBuildModule.GetDependencies() is not
        // recursive: a module's dependencies never reach its consumers' link lines, only
        // its include paths. That is why UnrealEditor-Luau.lib is on UnLuaNova's link line
        // but not on this one, and why every lua_* here was an LNK2019.
        //
        // Which backend is a compile-time choice owned by UnLuaNova - read the same key so
        // the two can never disagree. Naming the other backend here would put both on the
        // include path (both ship lua.h / lualib.h under those names), and would trip
        // LuaBackend.h's check for two backend modules in one build.
        // 全插件只有一个配置文件：Config/DefaultUnLuaNova.ini
        var configFile = new FileReference(Path.Combine(Target.ProjectFile.Directory.FullName, "Config", "DefaultUnLuaNova.ini"));
        var config = new ConfigHierarchy(new[] { FileReference.Exists(configFile) ? new ConfigFile(configFile) : new ConfigFile() });
        const string section = "/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings";

        string backend;
        if (!config.GetString(section, "LuaBackend", out backend))
            backend = "Luau";
        // Isolated target wins over the ini; Tools/SwitchBackend.ps1 keeps the ini as the
        // runtime label for whichever backend is currently activated.
        if (backendVariant.Length > 0) backend = backendVariant;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnLuaNova",
            string.Equals(backend, "Lua", StringComparison.OrdinalIgnoreCase) ? "Lua" : "Luau",
        });
    }
}
