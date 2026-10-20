#include "Commandlets/UnLuaNovaShowcaseDressingCommandlet.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Level.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"

namespace
{
	// Everything this commandlet creates carries this tag. It is what makes a second run a
	// no-op, what -Undo looks for, and the only thing telling our actors apart from anything a
	// human put in the level - so nothing else is ever touched or removed.
	const TCHAR* GDressingTag = TEXT("UnLuaNovaShowcaseDressing");

	const TCHAR* GPlaneAsset = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* GCubeAsset = TEXT("/Engine/BasicShapes/Cube.Cube");

	// The four levels the showcase tests photograph (Private/Tests/ShowcaseTest.cpp). A map
	// given with -Map is dressed instead of these.
	const TCHAR* GShowcaseMaps[] = {
		TEXT("/Game/Tutorials/01_HelloWorld/HelloWorld"),
		TEXT("/Game/Tutorials/02_OverrideBlueprintEvents/OverrideBlueprintEvents"),
		TEXT("/Game/Tutorials/06_NativeContainers/NativeContainers"),
		TEXT("/Game/Tutorials/11_ReleaseUMG/ReleaseUMG"),
	};

	bool IsDressed(ULevel* Level)
	{
		for (AActor* Actor : Level->Actors)
		{
			if (Actor && Actor->ActorHasTag(GDressingTag))
				return true;
		}
		return false;
	}

	template <class T>
	T* SpawnDressing(ULevel* Level, const FVector& Location, const FRotator& Rotation, const FString& Label)
	{
		FActorSpawnParameters Params;
		Params.OverrideLevel = Level;

		T* Actor = Level->GetWorld()->SpawnActor<T>(T::StaticClass(), Location, Rotation, Params);
		if (Actor)
		{
			Actor->Tags.Add(GDressingTag);
			Actor->SetActorLabel(Label);
		}
		return Actor;
	}

	// Find-or-create one piece of the dressing, keyed by its label. This is what makes the run
	// safe to re-run after the set of pieces grows: existing levels only gain what is missing,
	// instead of being skipped forever or accumulating duplicate suns.
	template <class T>
	T* EnsureDressing(ULevel* Level, const FVector& Location, const FRotator& Rotation, const FString& Label)
	{
		for (AActor* Actor : Level->Actors)
		{
			if (Actor && Actor->ActorHasTag(GDressingTag) && Actor->GetActorLabel() == Label)
				return Cast<T>(Actor);
		}
		return SpawnDressing<T>(Level, Location, Rotation, Label);
	}

	bool DressLevel(ULevel* Level)
	{
		// A sky. Without it the world above the horizon is a black void, and a real-time-capture
		// sky light has nothing to capture: the engine then paints a red warning across every
		// screenshot. SkyAtmosphere is the engine's procedural sky, so the skylight gets a source
		// and the upper half of every shot stops being pure black.
		EnsureDressing<ASkyAtmosphere>(Level, FVector::ZeroVector, FRotator::ZeroRotator,
			TEXT("UnLuaNovaShowcase_Atmosphere"));

		// A floor to catch the light. The basic plane is 1m across, so 40x is a 40m room.
		if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, GPlaneAsset))
		{
			if (AStaticMeshActor* Floor = EnsureDressing<AStaticMeshActor>(
					Level, FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator, TEXT("UnLuaNovaShowcase_Floor")))
			{
				Floor->GetStaticMeshComponent()->SetStaticMesh(Plane);
				Floor->SetActorScale3D(FVector(40.f, 40.f, 1.f));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ShowcaseDressing] %s is missing - no floor was added."), GPlaneAsset);
		}

		// Three cubes in front of the camera: a lit floor on its own reads as a flat gradient,
		// and the point of the shot is that there is a world in the level.
		if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, GCubeAsset))
		{
			const FVector Spots[3] = {
				FVector(300.f, -180.f, 60.f), FVector(420.f, 40.f, 60.f), FVector(320.f, 220.f, 60.f) };

			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Spots); ++Index)
			{
				if (AStaticMeshActor* Box = EnsureDressing<AStaticMeshActor>(Level, Spots[Index],
						FRotator(0.f, Index * 25.f, 0.f), FString::Printf(TEXT("UnLuaNovaShowcase_Cube_%d"), Index + 1)))
				{
					Box->GetStaticMeshComponent()->SetStaticMesh(Cube);
					Box->SetActorScale3D(FVector(1.2f));
				}
			}
		}

		if (ADirectionalLight* Sun = EnsureDressing<ADirectionalLight>(Level, FVector(0.f, 0.f, 400.f),
				FRotator(-55.f, 30.f, 0.f), TEXT("UnLuaNovaShowcase_Sun")))
		{
			if (UDirectionalLightComponent* Component = Sun->GetComponent())
			{
				// Movable: a static light renders black until a lighting build runs, and this
				// commandlet cannot run one.
				Component->SetMobility(EComponentMobility::Movable);
				Component->Intensity = 10.f;
				Component->LightColor = FColor(255, 244, 232);
			}
		}

		if (ASkyLight* Sky = EnsureDressing<ASkyLight>(Level, FVector(0.f, 0.f, 400.f),
				FRotator::ZeroRotator, TEXT("UnLuaNovaShowcase_Sky")))
		{
			if (USkyLightComponent* Component = Sky->GetLightComponent())
			{
				Component->SetMobility(EComponentMobility::Movable);
				Component->Intensity = 1.f;
				// Deliberately NOT SetRealTimeCapture(true): that re-captures and re-uploads the
				// sky every rendered frame, and the first run with it enabled crashed the render
				// thread in RHIReadSurfaceData while the clip burst was reading back one Slate
				// screenshot per frame - a per-frame GPU readback storm. A movable sky light
				// captures once on registration, which is all a static scene needs.
				Component->RecaptureSky();
			}
		}

		// Without a PlayerStart PIE spawns at the origin looking nowhere in particular, which
		// makes the framing of every shot accidental.
		EnsureDressing<APlayerStart>(Level, FVector(-300.f, 0.f, 100.f), FRotator::ZeroRotator,
			TEXT("UnLuaNovaShowcase_PlayerStart"));

		return IsDressed(Level);
	}

	bool UndressLevel(ULevel* Level)
	{
		TArray<AActor*> Tagged;
		for (AActor* Actor : Level->Actors)
		{
			if (Actor && Actor->ActorHasTag(GDressingTag))
				Tagged.Add(Actor);
		}

		UWorld* World = Level->GetWorld();
		int32 Removed = 0;
		for (AActor* Actor : Tagged)
		{
			if (World->DestroyActor(Actor))
				++Removed;
		}

		UE_LOG(LogTemp, Display, TEXT("[ShowcaseDressing] removed %d actor(s)."), Removed);
		return Removed > 0;
	}
}

UUnLuaNovaShowcaseDressingCommandlet::UUnLuaNovaShowcaseDressingCommandlet()
{
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UUnLuaNovaShowcaseDressingCommandlet::Main(const FString& Params)
{
	const bool bUndo = FParse::Param(*Params, TEXT("Undo"));

	TArray<FString> Maps;
	FString SingleMap;
	if (FParse::Value(*Params, TEXT("-Map="), SingleMap) && !SingleMap.IsEmpty())
	{
		Maps.Add(SingleMap);
	}
	else
	{
		for (const TCHAR* Map : GShowcaseMaps)
			Maps.Add(Map);
	}

	int32 Changed = 0;
	int32 Failed = 0;

	for (const FString& Map : Maps)
	{
		// The level has to be open in the editor world, not merely loaded: spawning a light
		// registers it with the world's scene, and a package-only load has no scene to register
		// with. Loading it properly is also what makes the save at the end a normal map save.
		UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(Map);
		ULevel* Level = World ? World->PersistentLevel : nullptr;
		if (!Level)
		{
			UE_LOG(LogTemp, Error, TEXT("[ShowcaseDressing] %s: could not open the level."), *Map);
			++Failed;
			continue;
		}

		// DressLevel is find-or-create per piece, so re-running after the set of pieces grows
		// only adds what is missing - no duplicate suns, no reason to skip a dressed level.
		if (!(bUndo ? UndressLevel(Level) : DressLevel(Level)))
		{
			UE_LOG(LogTemp, Error, TEXT("[ShowcaseDressing] %s: nothing was %s."), *Map,
				bUndo ? TEXT("removed") : TEXT("added"));
			++Failed;
			continue;
		}

		const FString Filename = FPackageName::LongPackageNameToFilename(Map, FPackageName::GetMapPackageExtension());
		if (!FEditorFileUtils::SaveMap(World, Filename))
		{
			UE_LOG(LogTemp, Error, TEXT("[ShowcaseDressing] %s: could not save to %s."), *Map, *Filename);
			++Failed;
			continue;
		}

		++Changed;
		UE_LOG(LogTemp, Display, TEXT("[ShowcaseDressing] %s: %s -> %s"), *Map,
			bUndo ? TEXT("dressing removed") : TEXT("dressed"), *Filename);
	}

	UE_LOG(LogTemp, Display, TEXT("[ShowcaseDressing] %d level(s) changed, %d failed."), Changed, Failed);
	return Failed > 0 ? 1 : 0;
}
