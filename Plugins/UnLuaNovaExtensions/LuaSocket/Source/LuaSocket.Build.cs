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

public class LuaSocket : ModuleRules
{
    public LuaSocket(ReadOnlyTargetRules Target) : base(Target)
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
        bUseUnity = false;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
#if UE_5_6_OR_LATER
        CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
#elif UE_5_5_OR_LATER
		UndefinedIdentifierWarningLevel = WarningLevel.Off;
#else
        bEnableUndefinedIdentifierWarnings = false;
#endif

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
            });

        // The backend module has to be listed here, even though UnLuaNova already
        // depends on it - same reason as UnLuaNovaDemo.Build.cs: this module calls
        // lua_* directly (it *is* a Lua C module), so those references live in
        // LuaSocket's own objects, and UEBuildModule.GetDependencies() is not
        // recursive. Without it every lua_* here is an LNK2019.
        //
        // Which backend is a compile-time choice owned by UnLuaNova, so read the same
        // key instead of naming one here.
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

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "UnLuaNova",
            string.Equals(backend, "Lua", StringComparison.OrdinalIgnoreCase) ? "Lua" : "Luau",
        });

        // WinSock. wsocket.cpp calls WSAStartup/socket/select directly and upstream
        // expects the caller to link ws2_32 (there is no #pragma comment(lib) in it).
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicSystemLibraries.Add("ws2_32.lib");
        }

        // Same story for the deprecations: _CRT_SECURE_NO_WARNINGS above cannot reach
        // <stdio.h>, because the shared PCH has already included it, so the deprecated
        // declarations are already baked in by the time any of our macros is seen.
        CppCompileWarningSettings.DeprecationWarningLevel = WarningLevel.Off;

        // Third-party C: do not let UE's strict settings fail the build.
        // BuildSettingsVersion.V7 (UE 5.8) promotes these three to errors, and
        // upstream LuaSocket - like the backends themselves - is not written against
        // them (C4702 right after lua_error, which is only "noreturn" in practice).
#if UE_5_8_OR_LATER
        CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Off;
        CppCompileWarningSettings.ReturnTypeWarningLevel = WarningLevel.Off;
        CppCompileWarningSettings.DanglingWarningLevel = WarningLevel.Off;
#endif

        PrivateDefinitions.AddRange(
            new[]
            {
                "EXTENSION_NAMESPACE_BEGIN=namespace UnLuaNovaExtensions { namespace LuaSocket {",
                "EXTENSION_NAMESPACE_END=}}",
                "LUA_LIB",
                // LuaSocket 3.0-rc1 predates the Winsock/MSVCRT replacements (inet_ntoa,
                // gethostbyname, sscanf, ...) and every call site is C4996. It is vendored
                // code we are not going to rewrite, so opt out instead of logging a page of
                // deprecations on every build.
                "_WINSOCK_DEPRECATED_NO_WARNINGS=1",
                "_CRT_SECURE_NO_WARNINGS=1"
            }
        );
        
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "src"));
    }
}