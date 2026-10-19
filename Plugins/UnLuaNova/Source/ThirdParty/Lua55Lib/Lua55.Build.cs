// Lua 5.5.1 (https://github.com/lua/lua) as a UBT-compiled module.
//
// This file deliberately lives OUTSIDE the Lua55 submodule (../Lua55): anything written
// inside the submodule belongs to lua/lua's own git repository and would never be
// committed to this project. So this is a wrapper module whose source lives in a sibling
// directory - the same shape as ../LuauLib for the Luau backend.
//
// Unlike ../Lua (the retired vendored 5.4.x, which shelled out to CMake per platform),
// this module compiles upstream's onelua.c directly through Lua55Unity.c. One translation
// unit, no external build tool, identical behaviour on Win64/Mac/Linux/Android/iOS.

using UnrealBuildTool;
using System.IO;

public class Lua55 : ModuleRules
{
    public Lua55(ReadOnlyTargetRules Target) : base(Target)
    {
        // Upstream aggregation is already one big file; unity would only add indirection.
        bUseUnity = false;
        PCHUsage = PCHUsageMode.NoPCHs;

        var Lua55Root = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "Lua55"));

        // lua.h / lualib.h / lauxlib.h / luaconf.h live flat in the submodule root.
        PublicIncludePaths.Add(Lua55Root);

        // Exclude lua.c + luac.c; see Lua55Unity.c.
        PublicDefinitions.Add("MAKE_LIB=1");

        // Third-party C: do not let UE's strict settings fail the build.
        bEnableUndefinedIdentifierWarnings = false;
        bEnableNonInlinedGenCppWarnings = false;
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            bEnableShadowVariableWarnings = false;
        }

        // Rebuild when the submodule contents change.
        ExternalDependencies.Add(Lua55Root);
    }
}
