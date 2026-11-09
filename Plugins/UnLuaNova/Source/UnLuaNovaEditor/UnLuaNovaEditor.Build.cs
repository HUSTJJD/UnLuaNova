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

using UnrealBuildTool;
using System.IO;

public class UnLuaNovaEditor : ModuleRules
{
    public UnLuaNovaEditor(ReadOnlyTargetRules Target) : base(Target)
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

        string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
        PrivateIncludePaths.AddRange(
            new[]
            {
                // The two engine folders below are the only Private/ paths here on purpose:
                // they hold engine headers this module needs. This plugin's own headers are
                // all in Public/ now, so neither module has to be reached into.
                Path.Combine(EngineDir, "Source/Editor/AnimationBlueprintEditor/Private"),
                Path.Combine(EngineDir, "Source/Runtime/Slate/Private"),
            }
        );

        PrivateIncludePathModuleNames.AddRange(
            new[]
            {
                "Kismet",
                "MainFrame",
                "AnimationBlueprintEditor",
                "Slate",
                "SlateCore",
                "Persona",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
#if UE_5_0_OR_LATER
                "DeveloperToolSettings",
#endif
                "EditorStyle",
                "ApplicationCore",
                "Projects",
                "InputCore",
                "UMG",
                "UMGEditor",
                "BlueprintGraph",
                "Slate",
                "SlateCore",
                "DirectoryWatcher",
                "Networking",
                "Sockets",
                "UnLuaNova",
                "ToolMenus",
                // ISettingsModule.h / ISettingsSection.h live in the Settings module and
                // LevelEditor.h in LevelEditor; UE 6.0 no longer exposes either header
                // transitively through UnrealEd's public includes.
                "Settings",
                "LevelEditor",
                // DAP 适配器的 JSON 编解码（LuaDebugAdapter）
                "Json",
                "JsonUtilities",
                // The IntelliSense generator and its commandlet read blueprint assets out of
                // the registry (and wait for its scan) instead of relying on the cache alone.
                "AssetRegistry"
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new[]
            {
                "Kismet",
                "MainFrame",
                "AnimationBlueprintEditor",
            }
        );
    }
}