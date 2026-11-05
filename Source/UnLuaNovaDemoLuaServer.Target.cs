// Backend-isolated Dedicated Server target. The name is a contract read by every
// Build.cs ([UAK BACKEND LAYOUT]): "UnLuaNovaDemo" + <Backend> + "Server".
// A Server target uses Target.Name as its AppName, so this target produces
// UnLuaNovaDemoLuaServer-Lua.dll, UnLuaNovaDemoLuaServer.exe and its own
// UnLuaNovaDemoLuaServer.modules manifest under Binaries/Win64/Backends/Lua.
// Tools/SwitchBackend.ps1 activates one backend's manifests at a time. Do not add
// GlobalDefinitions here: it is [RequiresUniqueBuildEnvironment].

using UnrealBuildTool;

public class UnLuaNovaDemoLuaServerTarget : TargetRules
{
    public UnLuaNovaDemoLuaServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UnLuaNovaDemo");
    }
}
