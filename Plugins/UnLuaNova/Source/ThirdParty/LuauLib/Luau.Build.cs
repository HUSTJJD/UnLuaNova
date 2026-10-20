// Luau (https://github.com/luau-lang/luau) compiled by UBT, like any other module
// source.
//
// This mirrors how Unreal integrates self-contained third-party code - see
// Engine/Source/ThirdParty/nanosvg for the same shape: no ModuleType.External, no
// prebuilt archives, no one has to run CMake/make before the editor will build.
// Every platform is compiled by UBT from identical sources.
//
// The submodule is checked out at ./Luau, inside this wrapper. This file deliberately
// lives OUTSIDE it: anything written
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

		var LuauRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "Luau"));

		// Keep UBT from compiling the submodule's own sources. It now sits inside this
		// module's directory, and UBT collects .cpp recursively from there - so Analysis/
		// (the type checker), CLI/, tests/, fuzz/ and bench/ would be dragged in too, none
		// of which are part of the VM/Ast/Compiler/Bytecode/CodeGen subset we build. A
		// .ubtignore makes UBT skip a directory tree entirely.
		//
		// It has to live inside the checkout, which is luau-lang's repo and not ours to
		// commit to, so the wrapper writes it when it is missing.
		string UbtIgnoreFile = Path.Combine(LuauRoot, ".ubtignore");
		if (!System.IO.File.Exists(UbtIgnoreFile))
		{
			System.IO.File.WriteAllText(UbtIgnoreFile, "Compiled through the wrappers in ../Generated, not directly.\n");
		}

		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "VM", "include"));
		// CodeGen is built against VM internals - CodeGenX64.cpp includes lstate.h and
		// company - which upstream gets from Luau.VM.Internals. Without this those
		// translation units fail on C1083. It is harmless for the VM's own sources: their
		// quoted includes already resolve relative to the file doing the including.
		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "VM", "src"));
		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "Compiler", "include"));
		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "Ast", "include"));
		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "Common", "include"));
		// BytecodeBuilder.h/BytecodeUtils.h live here since upstream release/743; they used
		// to sit under Compiler/include/Luau. Without this, Luau/Compiler/src/Compiler.cpp
		// fails on #include "Luau/BytecodeBuilder.h".
		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "Bytecode", "include"));
		// luacodegen.h, for the optional native compiler.
		PublicSystemIncludePaths.Add(Path.Combine(LuauRoot, "CodeGen", "include"));

		// Entry point for this backend. These headers live outside the submodule, so
		// they are ours and carry unique names - see ./Public/UnLuaNovaBackend_Luau.h.
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

		// Tells LuaBackend.h that this backend module is in the build, so it can fail
		// loudly if the other one is too (or if neither is).
		PublicDefinitions.Add("UNLUANOVA_BACKEND_MODULE_LUAU=1");

		// Luau still uses strcpy/strcat/strncpy; MSVC flags each of them (C4996).
		// Private, so only the Luau sources see it - dependents keep UE's default.
		PrivateDefinitions.Add("_CRT_SECURE_NO_WARNINGS=1");

		// Luau defaults to LUA_USE_LONGJMP=0, i.e. C++ exceptions (class lua_exception).
		// Unreal builds with exceptions disabled, so force setjmp/longjmp instead - that
		// is also what plain Lua does, so pcall/error semantics match.
		PublicDefinitions.Add("LUA_USE_LONGJMP=1");

		// Export the Luau API out of this module's DLL.
		//
		// Editor builds are modular: every module links as its own DLL, and on Windows a
		// symbol only leaves a DLL if it was declared __declspec(dllexport). Luau declares
		// its whole public API with `LUA_API` / `LUACODE_API`, which luaconf.h / luacode.h
		// define as bare `extern` - so UnrealEditor-Luau.dll used to export *nothing*, and
		// every lua_*/luaL_*/luau_* call in UnLuaNova failed with LNK2019 at link time.
		// The module itself compiled and linked cleanly, which hides the real cause.
		//
		// Private, not Public, on purpose: dependents must keep seeing the header default
		// (`extern`), otherwise every module that includes these headers would re-export
		// the entire Luau API. A plain `extern` reference resolves against the import
		// library, since MSVC emits both the decorated name and a thunk per export.
		//
		// Both macros sit behind #ifndef upstream, so the /D wins instead of colliding.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDefinitions.Add("LUA_API=__declspec(dllexport)");
			PrivateDefinitions.Add("LUACODE_API=__declspec(dllexport)");
			// Same for the native compiler's three entry points, or luau_codegen_* in
			// UnLuaNova fails with LNK2019 exactly like the lua_* ones did.
			PrivateDefinitions.Add("LUACODEGEN_API=__declspec(dllexport)");
		}

		// Force-include our own warning header into every Luau translation unit.
		//
		// UBT has no switch for the two diagnostics upstream Luau emits (C4457
		// "declaration hides function parameter" and C4389 "signed/unsigned
		// mismatch"): ShadowVariableWarningLevel only maps to /wd4456,4458,4459 on
		// MSVC, and SignCompareWarningLevel maps to clang -Wsign-compare. Patching
		// the sources is not an option either - they live in the submodule.
		//
		// The header sits next to this file, i.e. outside Luau/, so it is ours.
		ForceIncludeFiles.Add(Path.Combine(ModuleDirectory, "LuauWarnings.h"));

		// Third-party C++: do not let Unreal's strict settings fail the build.
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

#if UE_5_6_OR_LATER
		CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
#else
		bEnableShadowVariableWarnings = false;
#endif

		// See Lua.Build.cs: these become errors at BuildSettingsVersion.V7 (UE 5.8), and
		// upstream Luau does not compile clean under them (C4702 in ltable.cpp, etc).
#if UE_5_8_OR_LATER
		CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Off;
		CppCompileWarningSettings.ReturnTypeWarningLevel = WarningLevel.Off;
		CppCompileWarningSettings.DanglingWarningLevel = WarningLevel.Off;
#endif

		// Rebuild when the submodule contents change.
		ExternalDependencies.Add(LuauRoot);
	}
}
