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

using System;
using System.IO;
#if UE_5_0_OR_LATER
using EpicGames.Core;
#else
using Tools.DotNETCommon;
#endif
using UnrealBuildTool;

public class UnLuaNovaTestSuite : ModuleRules
{
	public UnLuaNovaTestSuite(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[]
			{
			}
		);

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
				"Slate"
			}
		);

		// The backend has to be listed here too, not just by UnLuaNova: the specs and
		// tests instantiate UnLuaNova's header-only templates (FLuaFunction::Call,
		// TExportedFunction<>::Invoke, TSmartPtrConstructor<>::Register, ...), so the
		// lua_* references end up in this module's objects. UEBuildModule.GetDependencies()
		// is not recursive, so dependencies never reach a consumer's link line - hence
		// UnrealEditor-Luau.lib is on UnLuaNova's link line but not on ours, and every
		// lua_* here was an LNK2019.
		//
		// Read the same key UnLuaNova does so the choice can never disagree with it; naming
		// the other backend would put both on the include path and trip LuaBackend.h's
		// check for two backend modules in one build.
		var configFile = new FileReference(Path.Combine(Target.ProjectFile.Directory.FullName, "Config", "DefaultUnLuaNovaEditor.ini"));
		var config = new ConfigHierarchy(new[] { FileReference.Exists(configFile) ? new ConfigFile(configFile) : new ConfigFile() });
		const string section = "/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings";

		string backend;
		if (!config.GetString(section, "LuaBackend", out backend))
			backend = "Luau";

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"UnLuaNova",
				"UMG",
				string.Equals(backend, "Lua", StringComparison.OrdinalIgnoreCase) ? "Lua" : "Luau"
			}
		);

		if (Target.bBuildEditor)
		{
			// UnrealEd: FKismetEditorUtilities / UEditorLoadingAndSavingUtils, used by the
			// asset repair test (UnLuaNova.Editor.RepairTestAssets) to compile and resave
			// the test content. AssetRegistry is named explicitly rather than inherited
			// from UnrealEd because that test calls IAssetRegistry directly.
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("AssetRegistry");
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

		PrecompileForTargets = PrecompileTargetsType.Any;
	}
}