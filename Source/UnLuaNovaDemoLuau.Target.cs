// Backend-isolated Game target. The name is a contract read by every Build.cs
// ([UAK BACKEND LAYOUT]): "UnLuaNovaDemo" + <Backend> + "" for the Game carrier.
// A Game target uses Target.Name as its AppName, so this target also produces
// UnLuaNovaDemoLuau-Luau.dll, UnLuaNovaDemoLuau.exe and its own UnLuaNovaDemoLuau.modules
// manifest under Binaries/Win64/Backends/Luau. Tools/SwitchBackend.ps1 activates one
// backend's manifests at a time. Do not add GlobalDefinitions here: it is
// [RequiresUniqueBuildEnvironment] and invalidates the shared build environment.

using UnrealBuildTool;

public class UnLuaNovaDemoLuauTarget : TargetRules
{
    public UnLuaNovaDemoLuauTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UnLuaNovaDemo");
    }
}
