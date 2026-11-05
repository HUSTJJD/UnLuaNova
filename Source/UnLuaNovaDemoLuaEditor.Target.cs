// Backend-isolated Editor target for the plain Lua VM.
//
// Why a separate target instead of flipping Config/DefaultUnLuaNova.ini:
// every Editor target shares UBT's "UnrealEditor" AppName, so a second build with a
// different backend used to overwrite UnrealEditor-UnLuaNova.dll and the plugin's
// object files. Every module's Build.cs derives the backend from this target's name and
// sets ModuleRules.BinariesSubFolder, which redirects both the DLL and the intermediate
// directory into Binaries/<Platform>/Backends/Lua and Intermediate/.../Backends/Lua.
//
// The name is the contract: UnLuaNovaDemo + <Backend> + Editor. Do not rename it
// without updating the [UAK BACKEND LAYOUT] block in every Build.cs and
// Tools/SwitchBackend.ps1.
//
// Kept identical to UnLuaNovaDemoEditor.Target.cs otherwise: a project Editor target
// with a shared build environment must not change DefaultBuildSettings or
// GlobalDefinitions (ValidateSharedEnvironment rejects both).

using UnrealBuildTool;

public class UnLuaNovaDemoLuaEditorTarget : TargetRules
{
    public UnLuaNovaDemoLuaEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UnLuaNovaDemo");
    }
}
