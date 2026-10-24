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
				"Slate",
				// Json: UnLuaNovaTestCommon.h carries DescribeRpcs(), which reads the peers'
				// JSON reports, so every translation unit that includes it needs the DOM.
				"Json"
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
		// 全插件只有一个配置文件：Config/DefaultUnLuaNova.ini
		var configFile = new FileReference(Path.Combine(Target.ProjectFile.Directory.FullName, "Config", "DefaultUnLuaNova.ini"));
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
				// Showcase 截图的黑图检测要解码 PNG（TestCommands.cpp 的 DescribeUniformScreenshot）
				"ImageWrapper",
				// Sockets: DebugAdapter.spec.cpp probes the DAP port with ISocketSubsystem
				// instead of guessing a free one. Not a public dependency - no header here
				// exposes a socket type to a consumer.
				"Sockets",
				string.Equals(backend, "Lua", StringComparison.OrdinalIgnoreCase) ? "Lua" : "Luau"
			}
		);

		if (Target.bBuildEditor)
		{
			// UnrealEd: the tests open levels and start PIE through the editor
			// (FEditorFileUtils::LoadMap, GEditor->RequestPlaySession), and the showcase
			// tests need the level viewport to take their screenshots from.
			// (It used to also pull AssetRegistry + BlueprintGraph for the one-off asset
			// repair test; that test is gone - the assets are fixed and committed.)
			PrivateDependencyModuleNames.Add("UnrealEd");

			// UnLuaNovaEditor: drives UnLuaNovaStaticBindingGenerator end to end
			// (Private/Specs/StaticBindingGenerator.spec.cpp). Editor-only, like the module.
			PrivateDependencyModuleNames.Add("UnLuaNovaEditor");

			// Nothing else to add: LuaDebugAdapter.h lives in UnLuaNovaEditor's Public/
			// folder, so the dependency above already puts it on the include path. It used
			// to be a private header the test reached for by path, which is not a thing a
			// consumer may do to another module - the header moved instead.
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

		PrecompileForTargets = PrecompileTargetsType.Any;
	}
}