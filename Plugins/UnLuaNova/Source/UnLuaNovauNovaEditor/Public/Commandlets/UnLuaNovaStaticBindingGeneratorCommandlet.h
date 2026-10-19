// Generates compile-time static binding tables for declared UE types.
//
//   UnrealEditor-Cmd.exe <Project> -run=UnLuaNovaStaticBindingGenerator
//       -Types=Actor,Pawn,Character
//       [-OutputDir=<Project>/Source/<Module>/Generated]
//
// Without -Types, every class/struct/enum carrying the `UnLuaNovaExport` metadata tag is used.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "UnLuaNovaStaticBindingGeneratorCommandlet.generated.h"

UCLASS()
class UUnLuaNovaStaticBindingGeneratorCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUnLuaNovaStaticBindingGeneratorCommandlet();

	virtual int32 Main(const FString& Params) override;
};
