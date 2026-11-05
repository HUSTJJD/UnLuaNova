// Backend-isolated Dedicated Server target. The name is a contract read by every
// Build.cs ([UAK BACKEND LAYOUT]): "UnLuaNovaDemo" + <Backend> + "Server".
// A Server target uses Target.Name as its AppName, so this target produces
// UnLuaNovaDemoLuauServer-Luau.dll, UnLuaNovaDemoLuauServer.exe and its own
// UnLuaNovaDemoLuauServer.modules manifest under Binaries/Win64/Backends/Luau.
// Tools/SwitchBackend.ps1 activates one backend's manifests at a time. Do not add
// GlobalDefinitions here: it is [RequiresUniqueBuildEnvironment].

using UnrealBuildTool;

public class UnLuaNovaDemoLuauServerTarget : TargetRules
{
    public UnLuaNovaDemoLuauServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UnLuaNovaDemo");
    }
}
