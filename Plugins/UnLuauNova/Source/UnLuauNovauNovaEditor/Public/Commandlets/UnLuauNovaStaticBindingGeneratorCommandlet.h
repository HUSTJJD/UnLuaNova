// Generates compile-time static binding tables for declared UE types.
//
//   UnrealEditor-Cmd.exe <Project> -run=UnLuauNovaStaticBindingGenerator
//       -Types=Actor,Pawn,Character
//       [-OutputDir=<Project>/Source/<Module>/Generated]
//
// Without -Types, every class/struct/enum carrying the `UnLuauNovaExport` metadata tag is used.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "UnLuauNovaStaticBindingGeneratorCommandlet.generated.h"

UCLASS()
class UUnLuauNovaStaticBindingGeneratorCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUnLuauNovaStaticBindingGeneratorCommandlet();

	virtual int32 Main(const FString& Params) override;
};
