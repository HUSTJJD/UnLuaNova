// The commandlet is no longer the only way in - it is the manual one.
//
// Generation is a library call now (Commandlets/UnLuaNovaStaticBindingGenerator.h), so
// the editor can run it on startup behind an ini switch. This entry point stays for the
// two cases that have no editor startup to hang off: a build machine, which needs the
// exit code, and a project that wants generation pinned to an explicit step in CI.
//
//   UnrealEditor-Cmd.exe <Project> -run=UnLuaNovaStaticBindingGenerator
//       [-OutputDir=<Project>/Source/<Module>/Generated]

#include "Commandlets/UnLuaNovaStaticBindingGeneratorCommandlet.h"
#include "Commandlets/UnLuaNovaStaticBindingGenerator.h"

#include "Misc/Parse.h"

UUnLuaNovaStaticBindingGeneratorCommandlet::UUnLuaNovaStaticBindingGeneratorCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UUnLuaNovaStaticBindingGeneratorCommandlet::Main(const FString& Params)
{
	FStaticBindingGenerateOptions Options;

	FParse::Value(*Params, TEXT("-OutputDir="), Options.OutputDir);

	// -Selection=ExportedReflected|Modules|All
	// ExportedReflected is the default and matches the ini: only the types UnLuaNova
	// already exports by hand (UObject, UClass, UWorld, the math structs). /Script/Engine
	// on its own is thousands of types, so generating it is opt-in, not the default.
	FString Selection;
	if (FParse::Value(*Params, TEXT("-Selection="), Selection))
	{
		if (Selection.Equals(TEXT("Modules")))				Options.Selection = EStaticBindingSelection::Modules;
		else if (Selection.Equals(TEXT("All")))				Options.Selection = EStaticBindingSelection::All;
		else if (Selection.Equals(TEXT("ExportedReflected")))	Options.Selection = EStaticBindingSelection::ExportedReflected;
		else UE_LOG(LogTemp, Warning, TEXT("[StaticBindingGen] unknown -Selection=%s, using ExportedReflected."), *Selection);
	}

	// -Modules=/Script/Engine,/Script/UMG narrows generation to those packages. Comma
	// separated rather than repeated flags because FParse::Value has no way to resume a
	// scan, so a repeated key would keep returning the first one.
	// Only consulted when -Selection=Modules.
	FString ModuleList;
	if (FParse::Value(*Params, TEXT("-Modules="), ModuleList, false))
	{
		ModuleList.ParseIntoArray(Options.Modules, TEXT(","), true);
	}

	const FStaticBindingGenerateResult Result = UnLuaNova::GenerateStaticBindings(Options);
	return Result.NumTypes > 0 ? 0 : 1;
}
