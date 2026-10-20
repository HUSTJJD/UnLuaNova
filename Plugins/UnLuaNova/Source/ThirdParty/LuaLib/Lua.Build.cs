// Official Lua (https://github.com/lua/lua) as a UBT-compiled module.
//
// Named after the backend, not the version: upgrading Lua means moving the submodule
// pointer, not renaming this module. The pinned version lives in .gitmodules / the
// submodule checkout, so it is not repeated here.
//
// The submodule is checked out at ./Lua, inside this wrapper. This file deliberately
// lives OUTSIDE it: anything written
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

        var LuaRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "Lua"));

        // Keep UBT from compiling the submodule's own sources. It now sits inside this
        // module's directory, and UBT collects .c/.cpp recursively from there - so lua.c
        // and luac.c (each with a main()) and onelua.c (already included by LuaUnity.c)
        // would all be compiled on their own. A .ubtignore makes UBT skip a directory
        // tree entirely.
        //
        // It has to live inside the checkout, which is the upstream repo's and not ours to
        // commit to, so the wrapper writes it when it is missing.
        string UbtIgnoreFile = Path.Combine(LuaRoot, ".ubtignore");
        if (!System.IO.File.Exists(UbtIgnoreFile))
        {
            System.IO.File.WriteAllText(UbtIgnoreFile, "Compiled through ../LuaUnity.c, not directly.\n");
        }

        // lua.h / lualib.h / lauxlib.h / luaconf.h live flat in the submodule root.
        //
        // System, not a plain include path, for the same reason as Luau.Build.cs: these
        // four names are shared with Luau, and a plain /I outranks Luau's /external:I
        // directories - so if both backend modules ever end up in one build, this
        // directory would silently answer every #include "lua.h". As /external:I it is
        // searched last instead, and nothing in this project asks for lua.h by name
        // anyway (see ./Public/UnLuaNovaBackend_Lua.h).
        PublicSystemIncludePaths.Add(LuaRoot);

        // Entry point for this backend. These headers live outside the submodule, so
        // they are ours and carry unique names - see ./Public/UnLuaNovaBackend_Lua.h.
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

        // Tells LuaBackend.h that this backend module is in the build, so it can fail
        // loudly if the other one is too (or if neither is).
        PublicDefinitions.Add("UNLUANOVA_BACKEND_MODULE_LUA=1");

        // Exclude lua.c + luac.c; see LuaUnity.c.
        PublicDefinitions.Add("MAKE_LIB=1");

        // Same export problem as Luau.Build.cs: editor builds are modular, so this module
        // links as its own DLL and a bare `extern` LUA_API would leave it exporting
        // nothing, failing every lua_* call in UnLuaNova with LNK2019.
        //
        // Unlike Luau, upstream Lua has this built in: with LUA_BUILD_AS_DLL, luaconf.h
        // makes LUA_API dllexport for TUs defining LUA_CORE/LUA_LIB (the library itself)
        // and dllimport for everyone else. That is why this one is Public - consumers get
        // the dllimport half. The macro is ignored off Windows.
        PublicDefinitions.Add("LUA_BUILD_AS_DLL=1");

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
