// Backend-isolated Game target. The name is a contract read by every Build.cs
// ([UAK BACKEND LAYOUT]): "UnLuaNovaDemo" + <Backend> + "" for the Game carrier.
// A Game target uses Target.Name as its AppName, so this target also produces
// UnLuaNovaDemoLua-Lua.dll, UnLuaNovaDemoLua.exe and its own UnLuaNovaDemoLua.modules
// manifest under Binaries/Win64/Backends/Lua. Tools/SwitchBackend.ps1 activates one
// backend's manifests at a time. Do not add GlobalDefinitions here: it is
// [RequiresUniqueBuildEnvironment] and invalidates the shared build environment.

using UnrealBuildTool;

public class UnLuaNovaDemoLuaTarget : TargetRules
{
    public UnLuaNovaDemoLuaTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UnLuaNovaDemo");
    }
}
