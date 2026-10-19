#include "Commandlets/UnLuaNovaStaticBindingGeneratorCommandlet.h"

#include "StaticBinding/StaticBindingTypes.h"
#include "StaticBinding/StaticBindingRegistry.h"
#include "StaticBinding/StaticBindingRegistrar.h"

#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/PlatformFilemanager.h"

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

	UStruct* FindType(const FString& Name)
	{
		if (UClass* Class = FindFirstObject<UClass>(*Name))					return Class;
		if (UScriptStruct* Struct = FindFirstObject<UScriptStruct>(*Name))	return Struct;
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
		Out += FString::Printf(TEXT("static const int32 Num%s_%s = %d;\n"), *ElementType, *Name, Lines.Num());
		Out += FString::Printf(TEXT("static const %s %s_%s[] =\n{\n"), *ElementType, *ElementType, *Name);
		if (Lines.Num() > 0)
			Out += FString::Join(Lines, TEXT("\n")) + TEXT("\n");
		else
			Out += FString::Printf(TEXT("    %s\n"), DummyElement);
		Out += TEXT("};\n\n");
	}
}

UUnLuaNovaStaticBindingGeneratorCommandlet::UUnLuaNovaStaticBindingGeneratorCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

namespace
{
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
}

int32 UUnLuaNovaStaticBindingGeneratorCommandlet::Main(const FString& Params)
{
	FString OutputDir;
	if (!FParse::Value(*Params, TEXT("-OutputDir="), OutputDir) || OutputDir.IsEmpty())
	{
		// Land inside the project's main module so the file is actually compiled.
		OutputDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), FApp::GetProjectName(), TEXT("Generated"));
	}

	// Opt-out, not opt-in. A large project declares thousands of types; making every one
	// of them carry an export tag is exactly the friction this is meant to remove.
	//
	//   default       -> exported
	//   minus         -> engine bookkeeping names, deprecated / transient types
	//   minus         -> meta=(UnLuaNovaNoExport)      <-- the only thing a project writes
	//   plus (escape) -> meta=(UnLuaNovaExport) on the type or any ancestor; wins over
	//                    every filter above
	TArray<FString> TypeNames;

	auto Consider = [&TypeNames](const UStruct* Struct)
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

		TypeNames.Add(Struct->GetName());
	};

	for (TObjectIterator<UClass> It; It; ++It)
		Consider(*It);
	for (TObjectIterator<UScriptStruct> It; It; ++It)
		Consider(*It);

	if (TypeNames.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[StaticBindingGen] every type was filtered out; nothing to generate."));
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[StaticBindingGen] selected %d types."), TypeNames.Num());

	FString Out;

	Out += TEXT("// Generated by UnLuaNovaStaticBindingGenerator. Do not edit.\n");
	Out += TEXT("// Regenerate after changing UPROPERTY / UFUNCTION on the listed types.\n\n");
	Out += TEXT("#include \"CoreMinimal.h\"\n");
	Out += TEXT("#include \"StaticBinding/StaticBindingTypes.h\"\n");
	Out += TEXT("#include \"StaticBinding/StaticBindingRegistrar.h\"\n\n");

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

				FuncLines.Add(FString::Printf(
					TEXT("    { TEXT(\"%s\"), 0, MAX_uint16, NumFStaticParamBinding_%s, FStaticFuncFlags(), FStaticParamBinding_%s, nullptr },"),
					*Function->GetName(), *FuncId, *FuncId));
			}
		}

		AppendArray(Out, TEXT("FStaticFuncBinding"), Id, FuncLines,
			TEXT("{ nullptr, 0, MAX_uint16, 0, FStaticFuncFlags(), nullptr, nullptr }"));

		Out += FString::Printf(
			TEXT("static const FStaticTypeBinding TypeBinding_%s =\n{\n"
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

	const FString OutFile = FPaths::Combine(OutputDir, TEXT("UnLuaNovaStaticBinding_Generated.cpp"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*OutputDir);

	if (!FFileHelper::SaveStringToFile(Out, *OutFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Error, TEXT("[StaticBindingGen] failed to write %s"), *OutFile);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[StaticBindingGen] %d type(s) -> %s"), GeneratedTypes, *OutFile);
	return 0;
}
