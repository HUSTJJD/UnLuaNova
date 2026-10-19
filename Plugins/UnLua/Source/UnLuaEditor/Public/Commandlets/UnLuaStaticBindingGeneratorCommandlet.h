// Generates compile-time static binding tables for declared UE types.
//
//   UnrealEditor-Cmd.exe <Project> -run=UnLuaStaticBindingGenerator
//       -Types=Actor,Pawn,Character
//       [-OutputDir=<Project>/Source/<Module>/Generated]
//
// Without -Types, every class/struct/enum carrying the `UnLuaExport` metadata tag is used.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "UnLuaStaticBindingGeneratorCommandlet.generated.h"

UCLASS()
class UUnLuaStaticBindingGeneratorCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUnLuaStaticBindingGeneratorCommandlet();

	virtual int32 Main(const FString& Params) override;
};
