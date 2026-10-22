// Generates compile-time static binding tables for declared UE types.
//
//   UnrealEditor-Cmd.exe <Project> -run=UnLuaNovaStaticBindingGenerator
//       [-OutputDir=<Project>/Source/<Module>/Generated]
//
// Type selection is opt-out: every class and struct is exported unless it is filtered out.
// There is no -Types switch - the only thing a project writes is
// meta=(UnLuaNovaNoExport) (and meta=(UnLuaNovaExport) to pull a hierarchy back in).

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "UnLuaNovaStaticBindingGeneratorCommandlet.generated.h"

UCLASS()
class UNLUANOVAEDITOR_API UUnLuaNovaStaticBindingGeneratorCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUnLuaNovaStaticBindingGeneratorCommandlet();

	virtual int32 Main(const FString& Params) override;
};
