// Luau (https://github.com/luau-lang/luau) as an External module.
//
// This file deliberately lives OUTSIDE the Luau submodule (../Luau): anything written inside
// the submodule belongs to luau-lang/luau's own git repository and would never be committed
// to this project. So this is a wrapper module whose source lives in a sibling directory.
//
// Mirrors the existing Lua.Build.cs pattern: CMake builds the static libraries, UBT links them.
//
// Integration notes, verified against the Luau checkout:
//   * Luau defaults to LUA_USE_LONGJMP=0, i.e. C++ exceptions (class lua_exception : std::exception).
//     Unreal builds with exceptions disabled, so we define LUA_USE_LONGJMP=1 to make Luau use
//     setjmp/longjmp instead. That is also what plain Lua does, so pcall/error semantics match.
//   * Only Luau.VM / Luau.Compiler / Luau.Ast / Luau.Common are linked;
//     Analysis, CodeGen, CLI and tests are switched off.
//   * Luau is C++17+; UE 5.8 is on C++20, so no standard override is needed.

using UnrealBuildTool;
using System.IO;

public class Luau : ModuleRules
{
	public Luau(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		var LuauRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "Luau"));

		PublicIncludePaths.Add(Path.Combine(LuauRoot, "VM", "include"));
		PublicIncludePaths.Add(Path.Combine(LuauRoot, "Compiler", "include"));
		PublicIncludePaths.Add(Path.Combine(LuauRoot, "Ast", "include"));
		PublicIncludePaths.Add(Path.Combine(LuauRoot, "Common", "include"));

		// Use setjmp/longjmp instead of C++ exceptions (see header comment).
		PublicDefinitions.Add("LUA_USE_LONGJMP=1");

		// Tell UBT to rebuild when the submodule contents change.
		ExternalDependencies.Add(LuauRoot);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LuauRoot, "build", "Release", "Luau.VM.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LuauRoot, "build", "Release", "Luau.Compiler.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LuauRoot, "build", "Release", "Luau.Ast.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LuauRoot, "build", "Release", "Luau.Common.lib"));
		}
		else
		{
			PublicAdditionalLibraries.Add(Path.Combine(LuauRoot, "build", "libLuau.VM.a"));
			PublicAdditionalLibraries.Add(Path.Combine(LuauRoot, "build", "libLuau.Compiler.a"));
			PublicAdditionalLibraries.Add(Path.Combine(LuauRoot, "build", "libLuau.Ast.a"));
			PublicAdditionalLibraries.Add(Path.Combine(LuauRoot, "build", "libLuau.Common.a"));
		}
	}
}
