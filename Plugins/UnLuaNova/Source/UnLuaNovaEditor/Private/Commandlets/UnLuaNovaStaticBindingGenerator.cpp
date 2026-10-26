#include "Commandlets/UnLuaNovaStaticBindingGenerator.h"

#include "Binding.h"
#include "StaticBinding/StaticBindingTypes.h"
#include "StaticBinding/StaticBindingRegistry.h"
#include "StaticBinding/StaticBindingRegistrar.h"
#include "StaticBinding/StaticBindingThunk.h"

#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "Misc/App.h"
#include "Misc/Crc.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/PlatformFilemanager.h"
#include "Interfaces/IPluginManager.h"

namespace
{
	const TCHAR* PropKindName(EStaticPropKind Kind)
	{
		switch (Kind)
		{
		case EStaticPropKind::Bool:					return TEXT("Bool");
		case EStaticPropKind::Int8:					return TEXT("Int8");
		case EStaticPropKind::Int16:				return TEXT("Int16");
		case EStaticPropKind::Int:					return TEXT("Int");
		case EStaticPropKind::Int64:				return TEXT("Int64");
		case EStaticPropKind::Byte:					return TEXT("Byte");
		case EStaticPropKind::Float:				return TEXT("Float");
		case EStaticPropKind::Double:				return TEXT("Double");
		case EStaticPropKind::Name:					return TEXT("Name");
		case EStaticPropKind::String:				return TEXT("String");
		case EStaticPropKind::Text:					return TEXT("Text");
		case EStaticPropKind::Object:				return TEXT("Object");
		case EStaticPropKind::Class:				return TEXT("Class");
		case EStaticPropKind::SoftObject:			return TEXT("SoftObject");
		case EStaticPropKind::LazyObject:			return TEXT("LazyObject");
		case EStaticPropKind::WeakObject:			return TEXT("WeakObject");
		case EStaticPropKind::Interface:			return TEXT("Interface");
		case EStaticPropKind::Struct:				return TEXT("Struct");
		case EStaticPropKind::Enum:					return TEXT("Enum");
		case EStaticPropKind::Array:				return TEXT("Array");
		case EStaticPropKind::Set:					return TEXT("Set");
		case EStaticPropKind::Map:					return TEXT("Map");
		case EStaticPropKind::Delegate:				return TEXT("Delegate");
		case EStaticPropKind::MulticastDelegate:	return TEXT("MulticastDelegate");
		case EStaticPropKind::FieldPath:			return TEXT("FieldPath");
		default:									return TEXT("Unknown");
		}
	}

	/**
	 * Resolve a type by either its C++ name or its UObject name.
	 *
	 * The two disagree: FVector's UScriptStruct is called "Vector", and the export
	 * registry keys types by the C++ name the script sees. Try both, because the same
	 * generator has to accept names coming from reflection (UObject names) and names
	 * coming from the export registry (C++ names).
	 */
	UStruct* FindType(const FString& Name)
	{
		if (UClass* Class = FindFirstObject<UClass>(*Name))					return Class;
		if (UScriptStruct* Struct = FindFirstObject<UScriptStruct>(*Name))	return Struct;

		if (Name.Len() > 1 && (Name[0] == TEXT('F') || Name[0] == TEXT('U') || Name[0] == TEXT('A') || Name[0] == TEXT('S')))
		{
			const FString Stripped = Name.Mid(1);
			if (UClass* Class = FindFirstObject<UClass>(*Stripped))					return Class;
			if (UScriptStruct* Struct = FindFirstObject<UScriptStruct>(*Stripped))	return Struct;
		}

		if (UClass* Class = LoadObject<UClass>(nullptr, *Name))				return Class;
		if (UScriptStruct* Struct = LoadObject<UScriptStruct>(nullptr, *Name)) return Struct;
		return nullptr;
	}

	FString SanitizeIdentifier(const FString& Name)
	{
		FString Result;
		for (const TCHAR Ch : Name)
			Result += FChar::IsAlnum(Ch) ? Ch : TCHAR('_');
		return Result;
	}

	/** A C++ array needs at least one element; the counts are tracked separately. */
	void AppendArray(FString& Out, const FString& ElementType, const FString& Name,
		const TArray<FString>& Lines, const TCHAR* DummyElement)
	{
		// Only the count is const. The tables themselves are writable on purpose:
		// FStaticBindingRegistry::Resolve() fills in the runtime-only fields (FProperty*,
		// UFunction*, offsets, flags) *in place*, at the first lookup. Emitting them as
		// const puts them in a read-only section and the first resolve is an access
		// violation - see Resolve() in StaticBindingRegistry.cpp.
		Out += FString::Printf(TEXT("static const int32 Num%s_%s = %d;\n"), *ElementType, *Name, Lines.Num());
		Out += FString::Printf(TEXT("static %s %s_%s[] =\n{\n"), *ElementType, *ElementType, *Name);
		if (Lines.Num() > 0)
			Out += FString::Join(Lines, TEXT("\n")) + TEXT("\n");
		else
			Out += FString::Printf(TEXT("    %s\n"), DummyElement);
		Out += TEXT("};\n\n");
	}

	// Names the engine generates for its own bookkeeping. Never worth binding.
	bool IsInternalName(const FString& Name)
	{
		return Name.StartsWith(TEXT("SKEL_"))
			|| Name.StartsWith(TEXT("REINST_"))
			|| Name.StartsWith(TEXT("TRASHCLASS_"))
			|| Name.StartsWith(TEXT("HOTRELOADED_"))
			|| Name.StartsWith(TEXT("PLACEHOLDER-CLASS"));
	}

	bool IsExcludedByFlags(const UStruct* Struct)
	{
		const UClass* AsClass = Cast<UClass>(Struct);
		if (!AsClass)
			return false;

		return AsClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_Transient);
	}

	// An opt-in anywhere on the super chain pulls the type in. Marking a single base class
	// therefore covers its entire hierarchy, which is what keeps this usable on a project
	// with thousands of types. It also acts as the escape hatch: a type dropped by the
	// filters above can be recovered by tagging it (or any ancestor) explicitly.
	bool HasOptedInAncestor(const UStruct* Struct)
	{
		for (const UStruct* Current = Struct; Current; Current = Current->GetInheritanceSuper())
		{
			if (Current->HasMetaData(TEXT("UnLuaNovaExport")))
				return true;
		}
		return false;
	}

	/**
	 * Decides whether a UFUNCTION gets a generated marshalling thunk.
	 *
	 * A thunk is what makes a call as cheap as slua's CppBinding: the parameter kinds are
	 * template arguments, so the call site compiles to a handful of stores instead of a
	 * walk over FPropertyDesc. Being wrong about eligibility is worse than being
	 * conservative - everything rejected here still gets the pre-resolved descriptor and
	 * runs through FFunctionDesc::CallUEFast - so the list below is deliberately narrow.
	 *
	 * Returns the parameter kinds (in declaration order) when eligible.
	 */
	bool CollectThunkKinds(UFunction* Function, int32& OutReturnKindIndex, TArray<FString>& OutParamKinds)
	{
		OutParamKinds.Reset();

		// Static functions take no `self`; the thunk reads slot 1 as the receiver.
		if (Function->HasAnyFunctionFlags(FUNC_Static))
			return false;

		// Net needs GetFunctionCallspace/CallRemoteFunction routing, latent needs the
		// coroutine, interface needs a per-object FindFunctionByName, variadic has no
		// fixed signature. None of them can be resolved at generation time.
		if (Function->HasAnyFunctionFlags(FUNC_Net))
			return false;

		// 'UInterface' itself carries CLASS_Interface and no callable functions, so the
		// name test is enough - and it avoids pulling UObject/Interface.h in here.
		if (const UClass* Owner = Function->GetOwnerClass())
		{
			if (Owner->HasAnyClassFlags(CLASS_Interface) && Owner->GetName() != TEXT("Interface"))
				return false;
		}

		// Keep the frame inside the thunk's stack buffer, and keep the number of template
		// instantiations bounded: one instantiation per distinct signature, not per class.
		if (Function->ParmsSize <= 0 || Function->ParmsSize > 256)
			return false;

		static const FName NAME_LatentInfo = TEXT("LatentInfo");

		int32 ReturnKindIndex = -1;

		for (TFieldIterator<FProperty> It(Function); It; ++It)
		{
			const FProperty* Param = *It;
			if (!Param->HasAnyPropertyFlags(CPF_Parm))
				continue;

			const EStaticPropKind Kind = FStaticBindingRegistry::GetPropertyKind(Param);

			if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				if (ReturnKindIndex != -1)
					return false;
				ReturnKindIndex = (int32)Kind;
				continue;
			}

			// A non-const out/reference parameter has to be copied back into Lua after the
			// call, which the thunk does not model.
			if (Param->HasAnyPropertyFlags(CPF_OutParm))
				return false;

			if (Param->GetFName() == NAME_LatentInfo)
				return false;

			// A C++ default argument arrives as CPP_Default_<name> metadata on the
			// UFunction, and the reflective path fills such a slot from the collected
			// default when a script omits it. A thunk has no access to that - it would
			// leave the slot zeroed - so a function with any default argument stays on the
			// reflective path rather than quietly changing what an omitted argument means.
			if (Function->HasMetaData(*FString::Printf(TEXT("CPP_Default_%s"), *Param->GetName())))
				return false;

			if (!UnLuaNova::IsThunkableParamKind(Kind))
				return false;

			// Fully qualified: these strings become template *arguments* of
			// StaticFuncThunk<EStaticPropKind Ret, EStaticPropKind... Args>, so a bare
			// "Int" is an undeclared identifier at the call site - exactly as the return
			// kind is qualified at the end of this function.
			OutParamKinds.Add(FString::Printf(TEXT("EStaticPropKind::%s"), PropKindName(Kind)));
		}

		if (OutParamKinds.Num() > 8)
			return false;

		if (ReturnKindIndex != -1 && !UnLuaNova::IsThunkableParamKind((EStaticPropKind)ReturnKindIndex))
			return false;

		OutReturnKindIndex = ReturnKindIndex;
		return true;
	}

	const TCHAR* FingerprintMarker = TEXT("fingerprint: ");
}

namespace UnLuaNova
{
	FString GetTypePackageName(const UStruct* Struct)
	{
		const UObject* Outer = Struct ? Struct->GetOutermost() : nullptr;
		return Outer ? Outer->GetName() : FString();
	}

	TArray<FString> CollectExportedReflectedTypeNames()
	{
		// IMPLEMENT_EXPORTED_CLASS registers through a static instance, so by the time
		// either caller runs (commandlet Main, or the editor's post-engine-init hook) the
		// registry is populated.
		const TMap<FString, IExportedClass*> Exported = UnLuaNova::GetExportedReflectedClasses();

		TArray<FString> Names;
		Names.Reserve(Exported.Num());
		for (const TPair<FString, IExportedClass*>& Pair : Exported)
		{
			if (Pair.Value)
				Names.Add(Pair.Key);
		}
		Names.Sort();
		return Names;
	}

	TArray<FString> CollectStaticBindingTypeNames(const FStaticBindingGenerateOptions& Options)
	{
		if (Options.Selection == EStaticBindingSelection::ExportedReflected)
			return CollectExportedReflectedTypeNames();

		TArray<FString> TypeNames;

		// Opt-out, not opt-in. A large project declares thousands of types; making every
		// one of them carry an export tag is exactly the friction this is meant to remove.
		//
		//   default       -> exported
		//   minus         -> engine bookkeeping names, deprecated / transient types
		//   minus         -> meta=(UnLuaNovaNoExport)      <-- the only thing a project writes
		//   minus         -> not in StaticBindingModules    <-- when that list is non-empty
		//   plus (escape) -> meta=(UnLuaNovaExport) on the type or any ancestor; wins over
		//                    every filter above
		auto Consider = [&TypeNames, &Options](const UStruct* Struct)
		{
			if (!Struct)
				return;

			if (HasOptedInAncestor(Struct))
			{
				TypeNames.Add(Struct->GetName());
				return;
			}

			if (IsInternalName(Struct->GetName()))
				return;

			if (IsExcludedByFlags(Struct))
				return;

			if (Struct->HasMetaData(TEXT("UnLuaNovaNoExport")))
				return;

			if (Options.Selection == EStaticBindingSelection::Modules
				&& !Options.Modules.Contains(GetTypePackageName(Struct)))
				return;

			TypeNames.Add(Struct->GetName());
		};

		for (TObjectIterator<UClass> It; It; ++It)
			Consider(*It);
		for (TObjectIterator<UScriptStruct> It; It; ++It)
			Consider(*It);

		return TypeNames;
	}

	FString ComputeStaticBindingFingerprint(const FStaticBindingGenerateOptions& Options)
	{
		TArray<FString> Names = CollectStaticBindingTypeNames(Options);
		Names.Sort();

		uint32 Crc = 0;
		for (const FString& Name : Names)
			Crc = FCrc::StrCrc32(*Name, Crc);

		// Generator fixes must invalidate existing output even when the type set is unchanged.
		return FString::Printf(TEXT("v2-%d-%08X"), Names.Num(), Crc);
	}

	FString ReadStaticBindingFingerprint(const FString& File)
	{
		// Only the header is needed, and the generated file runs to megabytes on a real
		// project - read the first block rather than loading the whole thing every start.
		constexpr int32 HeadSize = 4096;

		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*File));
		if (!Reader)
			return FString();

		TArray<ANSICHAR> Buffer;
		Buffer.SetNumUninitialized(HeadSize + 1);
		// FArchive::Serialize returns void - how much was read comes from the archive's
		// own size, clamped to the head (a file shorter than the head is fine).
		const int64 Read = FMath::Max<int64>(0, FMath::Min<int64>(Reader->TotalSize(), HeadSize));
		Reader->Serialize(Buffer.GetData(), Read);
		Reader->Close();

		if (Read <= 0)
			return FString();

		Buffer[FMath::Min<int64>(Read, HeadSize)] = 0;
		FString Head(UTF8_TO_TCHAR(Buffer.GetData()));

		const int32 Index = Head.Find(FingerprintMarker);
		if (Index == INDEX_NONE)
			return FString();

		FString Tail = Head.Mid(Index + FCString::Strlen(FingerprintMarker));
		const int32 End = Tail.Find(TEXT("\n"));
		return End == INDEX_NONE ? Tail : Tail.Left(End);
	}

	FString GetDefaultStaticBindingOutputDir()
	{
		// The output belongs to UnLuaNova itself: <UnLuaNova>/Source/UnLuaNova/Generated.
		//
		// It used to be <Project>/Source/<ProjectName>/Generated, which assumes the project
		// name is also the main module directory. That is false often enough to be a trap:
		// the Lyra host is named LyraStarterGame but its main module lives in Source/LyraGame,
		// so the file landed in a directory with no .Build.cs and UBT never compiled it -
		// a binding table that is not compiled buys nothing at all.
		//
		// The plugin's own module directory always exists and always depends on UnLuaNova,
		// so whatever lands there is compiled for sure, and the host source tree stays clean.
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnLuaNova")))
		{
			return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source"), TEXT("UnLuaNova"), TEXT("Generated"));
		}

		// Unreachable in practice (the generator ships inside that very plugin), but a silent
		// fallback to a directory UBT ignores is exactly the bug being fixed here, so say so.
		const FString Fallback = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), FApp::GetProjectName(), TEXT("Generated"));
		UE_LOG(LogTemp, Warning,
			TEXT("[StaticBindingGen] UnLuaNova plugin not found; falling back to %s, which is not a module directory - set StaticBindingOutputDir explicitly."),
			*Fallback);
		return Fallback;
	}

	FStaticBindingGenerateResult GenerateStaticBindings(const FStaticBindingGenerateOptions& Options)
	{
		FStaticBindingGenerateResult Result;

		const TArray<FString> TypeNames = CollectStaticBindingTypeNames(Options);
		if (TypeNames.Num() == 0)
		{
			Result.Message = TEXT("every type was filtered out; nothing to generate.");
			UE_LOG(LogTemp, Warning, TEXT("[StaticBindingGen] %s"), *Result.Message);
			return Result;
		}

		Result.Fingerprint = ComputeStaticBindingFingerprint(Options);

		UE_LOG(LogTemp, Display, TEXT("[StaticBindingGen] selected %d types."), TypeNames.Num());

		FString Out;

		Out += TEXT("// Generated by UnLuaNovaStaticBindingGenerator. Do not edit.\n");
		Out += TEXT("// Regenerate after changing UPROPERTY / UFUNCTION on the listed types.\n");
		Out += FString::Printf(TEXT("// %s%s\n\n"), FingerprintMarker, *Result.Fingerprint);
		Out += TEXT("#include \"CoreMinimal.h\"\n");
		Out += TEXT("#include \"StaticBinding/StaticBindingTypes.h\"\n");
		Out += TEXT("#include \"StaticBinding/StaticBindingRegistrar.h\"\n");
		Out += TEXT("#include \"StaticBinding/StaticBindingThunk.h\"\n\n");

		int32 GeneratedTypes = 0;

		for (const FString& TypeName : TypeNames)
		{
			UStruct* Struct = FindType(TypeName);
			if (!Struct)
			{
				UE_LOG(LogTemp, Warning, TEXT("[StaticBindingGen] '%s' not found, skipped."), *TypeName);
				continue;
			}

			const FString Id = SanitizeIdentifier(TypeName);

			// ---- properties -------------------------------------------------
			TArray<FString> PropLines;
			TSet<FName> SeenProps;
			for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FProperty* Prop = *It;
				if (SeenProps.Contains(Prop->GetFName()))
					continue;
				SeenProps.Add(Prop->GetFName());

				PropLines.Add(FString::Printf(
					TEXT("    { TEXT(\"%s\"), 0, EStaticPropKind::%s, nullptr },"),
					*Prop->GetName(), PropKindName(FStaticBindingRegistry::GetPropertyKind(Prop))));
			}

			AppendArray(Out, TEXT("FStaticPropBinding"), Id, PropLines,
				TEXT("{ nullptr, 0, EStaticPropKind::Unknown, nullptr }"));

			// ---- functions --------------------------------------------------
			const UClass* AsClass = Cast<UClass>(Struct);
			TArray<FString> FuncLines;
			TSet<FName> SeenFuncs;

			if (AsClass)
			{
				for (TFieldIterator<UFunction> It(AsClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
				{
					UFunction* Function = *It;
					if (SeenFuncs.Contains(Function->GetFName()))
						continue;
					SeenFuncs.Add(Function->GetFName());

					const FString FuncId = Id + TEXT("_") + SanitizeIdentifier(Function->GetName());

					TArray<FString> ParamLines;
					for (TFieldIterator<FProperty> PIt(Function); PIt; ++PIt)
					{
						const FProperty* Param = *PIt;
						if (!Param->HasAnyPropertyFlags(CPF_Parm) || Param->HasAnyPropertyFlags(CPF_ReturnParm))
							continue;

						ParamLines.Add(FString::Printf(
							TEXT("    { TEXT(\"%s\"), 0, EStaticPropKind::%s, nullptr },"),
							*Param->GetName(), PropKindName(FStaticBindingRegistry::GetPropertyKind(Param))));
					}

					AppendArray(Out, TEXT("FStaticParamBinding"), FuncId, ParamLines,
						TEXT("{ nullptr, 0, EStaticPropKind::Unknown, nullptr }"));

					int32 ReturnKindIndex = -1;
					TArray<FString> ThunkParamKinds;
					const bool bThunkable = CollectThunkKinds(Function, ReturnKindIndex, ThunkParamKinds);

					FString ReturnKindExpr = TEXT("EStaticPropKind::Unknown");
					FString ThunkExpr = TEXT("nullptr");

					if (bThunkable)
					{
						if (ReturnKindIndex != -1)
							ReturnKindExpr = FString::Printf(TEXT("EStaticPropKind::%s"), PropKindName((EStaticPropKind)ReturnKindIndex));

						TArray<FString> TemplateArgs;
						TemplateArgs.Add(ReturnKindExpr);
						TemplateArgs.Append(ThunkParamKinds);

						ThunkExpr = FString::Printf(TEXT("&UnLuaNova::StaticFuncThunk<%s>"), *FString::Join(TemplateArgs, TEXT(", ")));
						++Result.NumThunks;
					}

					++Result.NumFuncs;

					FuncLines.Add(FString::Printf(
						TEXT("    { TEXT(\"%s\"), 0, MAX_uint16, NumFStaticParamBinding_%s, FStaticFuncFlags(), FStaticParamBinding_%s, nullptr, %s, nullptr, %s },"),
						*Function->GetName(), *FuncId, *FuncId, *ReturnKindExpr, *ThunkExpr));
				}
			}

			AppendArray(Out, TEXT("FStaticFuncBinding"), Id, FuncLines,
				TEXT("{ nullptr, 0, MAX_uint16, 0, FStaticFuncFlags(), nullptr, nullptr }"));

			Out += FString::Printf(
				// Not const either: Resolve() sets bResolved on it, same reason as the
				// arrays above.
				TEXT("static FStaticTypeBinding TypeBinding_%s =\n{\n"
					 "    TEXT(\"%s\"),\n"
					 "    FStaticPropBinding_%s, NumFStaticPropBinding_%s,\n"
					 "    FStaticFuncBinding_%s, NumFStaticFuncBinding_%s,\n"
					 "    nullptr,\n"
					 "    false\n"
					 "};\n\n"
					 "UNLUANOVA_REGISTER_STATIC_BINDING(%s);\n\n"),
				*Id, *TypeName, *Id, *Id, *Id, *Id, *Id);

			++GeneratedTypes;
			UE_LOG(LogTemp, Display, TEXT("[StaticBindingGen] %s ok."), *TypeName);
		}

		Result.NumTypes = GeneratedTypes;

		if (!Options.bWriteFile)
			return Result;

		const FString OutputDir = Options.OutputDir.IsEmpty() ? GetDefaultStaticBindingOutputDir() : Options.OutputDir;
		const FString OutputFile = FPaths::Combine(OutputDir, TEXT("UnLuaNovaStaticBinding_Generated.cpp"));

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*OutputDir);

		if (!FFileHelper::SaveStringToFile(Out, *OutputFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Result.Message = FString::Printf(TEXT("failed to write %s"), *OutputFile);
			UE_LOG(LogTemp, Error, TEXT("[StaticBindingGen] %s"), *Result.Message);
			Result.NumTypes = 0;
			return Result;
		}

		Result.OutputFile = OutputFile;

		UE_LOG(LogTemp, Display, TEXT("[StaticBindingGen] %d type(s) -> %s"), GeneratedTypes, *OutputFile);

		if (Result.NumFuncs > 0)
		{
			UE_LOG(LogTemp, Display, TEXT("[StaticBindingGen] thunk coverage: %d/%d functions (%.1f%%)"),
				Result.NumThunks, Result.NumFuncs, 100.f * (float)Result.NumThunks / (float)Result.NumFuncs);
		}

		return Result;
	}
}
