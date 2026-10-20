// Official Lua (https://github.com/lua/lua) as a UBT-compiled module.
//
// Named after the backend, not the version: upgrading Lua means moving the submodule
// pointer, not renaming this module. The pinned version lives in .gitmodules / the
// submodule checkout, so it is not repeated here.
//
// This file deliberately lives OUTSIDE the Lua submodule (../Lua): anything written
// inside the submodule belongs to lua/lua's own git repository and would never be
// committed to this project. So this is a wrapper module whose source lives in a sibling
// directory - the same shape as ../LuauLib for the Luau backend.
//
// Upstream ships onelua.c, an aggregation of the whole library, so this compiles it
// through LuaUnity.c: one translation unit, no external build tool, identical behaviour
// on Win64/Mac/Linux/Android/iOS. (The old vendored 5.4.x shelled out to CMake per
// platform instead.)

using UnrealBuildTool;
using System.IO;

public class Lua : ModuleRules
{
    public Lua(ReadOnlyTargetRules Target) : base(Target)
    {
        // Upstream aggregation is already one big file; unity would only add indirection.
        bUseUnity = false;
        PCHUsage = PCHUsageMode.NoPCHs;

        var LuaRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "Lua"));

        // lua.h / lualib.h / lauxlib.h / luaconf.h live flat in the submodule root.
        PublicIncludePaths.Add(LuaRoot);

        // Exclude lua.c + luac.c; see LuaUnity.c.
        PublicDefinitions.Add("MAKE_LIB=1");

        // Third-party C: do not let UE's strict settings fail the build.
        // Off, not Warning, so third-party code never fails the build on diagnostics.
        //
        // The knob moved twice: bool bEnable*Warnings (<=5.4), a flat *WarningLevel property
        // on ModuleRules (5.5), then CppCompileWarningSettings.* (5.6+).
#if UE_5_6_OR_LATER
        CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
#elif UE_5_5_OR_LATER
        UndefinedIdentifierWarningLevel = WarningLevel.Off;
#else
        bEnableUndefinedIdentifierWarnings = false;
#endif

#if UE_5_6_OR_LATER
        CppCompileWarningSettings.NonInlinedGenCppWarningLevel = WarningLevel.Off;
#else
        bEnableNonInlinedGenCppWarnings = false;
#endif

        // BuildSettingsVersion.V7 (UE 5.8) promotes these three to errors. Upstream Lua is
        // not written against them - e.g. C4702 right after lua_error/luaD_throw, which are
        // only "noreturn" in practice, not to the compiler.
#if UE_5_8_OR_LATER
        CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Off;
        CppCompileWarningSettings.ReturnTypeWarningLevel = WarningLevel.Off;
        CppCompileWarningSettings.DanglingWarningLevel = WarningLevel.Off;
#endif

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
#if UE_5_6_OR_LATER
            CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
#else
            bEnableShadowVariableWarnings = false;
#endif
        }

        // Rebuild when the submodule contents change.
        ExternalDependencies.Add(LuaRoot);
    }
}
