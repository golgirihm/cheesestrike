// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeCheese.generated.h"

class UStaticMeshComponent;

/**
 * One block of cheese in a map: cover now, and what the mice will eat
 * (docs/unreal/design.md, "The mice"): each block has the design's 400 of
 * eating in it, and belongs to a structure of the map (a wall, a rack, a
 * bridge) by name, so that a map's notes can say what eating each one
 * opens up (docs/unreal/maps/). A block is any shape: a brick, a wheel, a
 * slab; its mesh and scale are set where the map is built
 * (tools/art/build_creamery.py), its look is the cheese material.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeCheese : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeCheese();

	UStaticMeshComponent* GetMesh() const { return Mesh; }
	float GetHealth() const { return Health; }
	FName GetStructure() const { return Structure; }

protected:
	UPROPERTY(VisibleAnywhere, Category="Cheese")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** How much eating is left in it. */
	UPROPERTY(EditAnywhere, Category="Cheese")
	float Health;

	/** The structure of the map it is part of, such as "WheelStack" or "HallSeal". */
	UPROPERTY(EditAnywhere, Category="Cheese")
	FName Structure;
};
