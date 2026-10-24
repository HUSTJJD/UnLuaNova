// Build-time generation of static binding tables, separated from the commandlet.
//
// The commandlet was the only way in, which made "generate" a thing a person had to
// remember to do: forget it after adding a UPROPERTY and the project silently runs the
// reflective path forever, with nothing in the log saying so. Generation is a normal
// library call now, so the editor can do it on startup behind an ini switch - the same
// arrangement IntelliSense already uses.
//
// The commandlet stays as the manual / CI entry point (a build machine has no editor
// startup to hang this off, and needs the exit code).

#pragma once

#include "CoreMinimal.h"

class UStruct;

/** What one generation run produced. */
struct FStaticBindingGenerateResult
{
	/** 0 when nothing was written - see Message for why. */
	int32 NumTypes = 0;
	int32 NumFuncs = 0;

	/** Of NumFuncs, how many got a compiled thunk. This is the number that matters: a
	 *  thunk is what removes the reflective call, a descriptor alone only trims it. */
	int32 NumThunks = 0;

	/** Empty when bWriteFile was false or nothing was generated. */
	FString OutputFile;

	/** Fingerprint written into the file, so the next startup can tell it is current. */
	FString Fingerprint;

	/** Set when NumTypes is 0. */
	FString Message;
};

/**
 * Which types a generation run covers.
 *
 * ExportedReflected is the default on purpose: /Script/Engine alone is several thousand
 * types, and generating all of them trades a large win on a handful of hot types for a
 * translation unit nobody wants to compile. The types UnLuaNova already exports by hand
 * (UObject, UClass, UWorld, the math structs) are exactly the ones scripts touch in
 * inner loops, so that is where a binding table pays for itself.
 */
enum class EStaticBindingSelection : uint8
{
	/** Types registered with BEGIN_EXPORT_REFLECTED_CLASS, read from the registry at
	 *  runtime so the list cannot drift from the code that declares it. */
	ExportedReflected,

	/** Every type in StaticBindingModules. */
	Modules,

	/** Every type, opt-out only (meta=(UnLuaNovaNoExport)). Large. */
	All,
};

struct FStaticBindingGenerateOptions
{
	/** Empty -> <Project>/Source/<ProjectModule>/Generated, which is where UBT picks it up. */
	FString OutputDir;

	EStaticBindingSelection Selection = EStaticBindingSelection::ExportedReflected;

	/** Used when Selection is Modules, e.g. /Script/Engine, /Script/UMG. */
	TArray<FString> Modules;

	/** false -> compute everything, touch no file. Used to check whether a regeneration
	 *  would change anything without paying for the write. */
	bool bWriteFile = true;
};

namespace UnLuaNova
{
	/** Packages the given type belongs to, e.g. "/Script/Engine". */
	UNLUANOVAEDITOR_API FString GetTypePackageName(const UStruct* Struct);

	/** The types generation would cover, for the given selection. */
	UNLUANOVAEDITOR_API TArray<FString> CollectStaticBindingTypeNames(const FStaticBindingGenerateOptions& Options);

	/** Types registered with BEGIN_EXPORT_REFLECTED_CLASS. */
	UNLUANOVAEDITOR_API TArray<FString> CollectExportedReflectedTypeNames();

	/**
	 * A cheap identity for the type set: count plus a CRC over the sorted names.
	 *
	 * Deliberately not a hash of every signature - that would mean walking every
	 * UPROPERTY and UFUNCTION of every type on every editor start, which is exactly the
	 * cost this feature exists to avoid. Signature changes are caught downstream instead:
	 * FStaticBindingRegistry::Resolve() re-derives each kind at registration and drops the
	 * thunk when it no longer matches, so an out-of-date file degrades rather than
	 * misbehaves. This only has to notice types appearing or disappearing.
	 */
	UNLUANOVAEDITOR_API FString ComputeStaticBindingFingerprint(const FStaticBindingGenerateOptions& Options);

	/** Reads the fingerprint out of a generated file. Empty when absent or unreadable. */
	UNLUANOVAEDITOR_API FString ReadStaticBindingFingerprint(const FString& File);

	/** Default output directory when none is given. */
	UNLUANOVAEDITOR_API FString GetDefaultStaticBindingOutputDir();

	UNLUANOVAEDITOR_API FStaticBindingGenerateResult GenerateStaticBindings(const FStaticBindingGenerateOptions& Options);
}
