// Luau (https://github.com/luau-lang/luau) compiled by UBT, like any other module
// source.
//
// This mirrors how Unreal integrates self-contained third-party code - see
// Engine/Source/ThirdParty/nanosvg for the same shape: no ModuleType.External, no
// prebuilt archives, no one has to run CMake/make before the editor will build.
// Every platform is compiled by UBT from identical sources.
//
// This file deliberately lives OUTSIDE the Luau submodule (../Luau): anything written
// inside the submodule belongs to luau-lang/luau's own git repository and would never
// be committed to this project.
//
// What actually gets compiled are the thin wrappers under ./Generated - one per Luau
// source, produced by GenerateWrappers.py. They are needed because UBT only compiles
// sources inside the module directory, and because Luau cannot be unity-built (three
// file-scope `static` names collide). See that script for details, and re-run it if
// the Luau submodule changes.

using UnrealBuildTool;
using System.IO;

public class Luau : ModuleRules
{
	public Luau(ReadOnlyTargetRules Target) : base(Target)
	{
#if UE_5_2_OR_LATER
		IWYUSupport = IWYUSupport.None;
#else
		bEnforceIWYU = false;
#endif
		PCHUsage = PCHUsageMode.NoPCHs;

		// Critical: each wrapper must stay its own translation unit. If UBT merges them
		// into a unity blob we are back to the colliding `static` names documented in
		// GenerateWrappers.py.
		bUseUnity = false;

		var LuauRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "Luau"));

		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "VM", "include"));
		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "Compiler", "include"));
		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "Ast", "include"));
		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "Common", "include"));

		// Luau defaults to LUA_USE_LONGJMP=0, i.e. C++ exceptions (class lua_exception).
		// Unreal builds with exceptions disabled, so force setjmp/longjmp instead - that
		// is also what plain Lua does, so pcall/error semantics match.
		PublicDefinitions.Add("LUA_USE_LONGJMP=1");

		// Third-party C++: do not let Unreal's strict settings fail the build.
		bEnableUndefinedIdentifierWarnings = false;
		bEnableNonInlinedGenCppWarnings = false;
		bEnableShadowVariableWarnings = false;

		// Rebuild when the submodule contents change.
		ExternalDependencies.Add(LuauRoot);
	}
}
