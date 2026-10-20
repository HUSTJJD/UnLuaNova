// Commandlet front-end for static binding generation.
//
//   UnrealEditor-Cmd.exe <Project> -run=UnLuaNovaStaticBindingGenerator
//       [-OutputDir=<Project>/Source/<Module>/Generated]
//       [-Modules=/Script/Engine,/Script/UMG]
//
// This is the manual / CI entry point, not the normal one: a project turns on
// bStaticBinding in Config/DefaultUnLuaNova.ini and the editor generates on startup
// whenever the fingerprint shows the type set moved. See
// Commandlets/UnLuaNovaStaticBindingGenerator.h for the call both paths share.
//
// Type selection is opt-out: every class and struct is exported unless it is filtered out.
// There is no -Types switch - the only thing a project writes is
// meta=(UnLuaNovaNoExport) (and meta=(UnLuaNovaExport) to pull a hierarchy back in),
// plus StaticBindingModules when it wants to narrow by package.

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
