// Gives the showcase levels something to render.
//
//   UnrealEditor-Cmd.exe <Project>.uproject -run=UnLuaNovaShowcaseDressing
//       [-Map=/Game/Tutorials/01_HelloWorld/HelloWorld] [-Undo]
//
// Why this exists: the four levels the showcase tests photograph are empty worlds - no light,
// no geometry, no PlayerStart - and the demos in them print to the screen and the log. A photo
// of the scene was therefore a photo of nothing: every pixel measured Y=16, and the docs
// gallery was four identical black rectangles. Turning the capture flags back on does not fix
// that on its own - the level itself has to render as something.
//
// What gets added per level: a movable sun, a movable sky light, a floor to catch the light,
// three cubes so the shot reads as a 3D space, and a PlayerStart so PIE has a camera.
// Everything is Movable on purpose: static lights need a lighting build, and a commandlet
// cannot run one, so static lights would still come out black.
//
// Idempotent: a level carrying the dressing is left alone. -Undo removes it again, which is
// how this change gets reverted without hand-editing binary content.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "UnLuaNovaShowcaseDressingCommandlet.generated.h"

UCLASS()
class UNLUANOVAEDITOR_API UUnLuaNovaShowcaseDressingCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UUnLuaNovaShowcaseDressingCommandlet();

	virtual int32 Main(const FString& Params) override;
};
