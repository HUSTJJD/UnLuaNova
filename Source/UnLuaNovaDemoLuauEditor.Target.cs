// Backend-isolated Editor target for the Luau VM. See UnLuaNovaDemoLuaEditor.Target.cs
// for the full rationale; the two names differ only in the backend segment, which is
// what the [UAK BACKEND LAYOUT] block in every Build.cs reads.

using UnrealBuildTool;

public class UnLuaNovaDemoLuauEditorTarget : TargetRules
{
    public UnLuaNovaDemoLuauEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UnLuaNovaDemo");
    }
}
