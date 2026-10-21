// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikePlace.generated.h"

class UBoxComponent;

/**
 * A named place in a map, such as "The Squeeze" or "Gouda Dock": a box
 * over the ground it names (docs/unreal/design.md, "Arenas": places are
 * shown on the HUD and in the kill feed, as "Nice shot · The Fountain").
 * A map script lays them out (tools/art/map_kit.py, place); it blocks and
 * draws nothing.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikePlace : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikePlace();

	const FString& GetPlaceName() const { return PlaceName; }
	/** The world box it covers. */
	FBox GetArea() const;

	/** The name of the place `Where` is in (the smallest that holds it, where places overlap), or empty if none does. */
	static FString NameAt(const UWorld* World, const FVector& Where);

protected:
	UPROPERTY(VisibleAnywhere, Category="Place")
	TObjectPtr<UBoxComponent> Area;

	/** What players call it. */
	UPROPERTY(EditAnywhere, Category="Place")
	FString PlaceName;
};
