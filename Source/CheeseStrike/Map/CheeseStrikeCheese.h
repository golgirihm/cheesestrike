// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeCheese.generated.h"

class UStaticMeshComponent;

/**
 * One block of cheese in a map: cover, and what the mice eat
 * (docs/unreal/design.md, "The mice"): each block has the design's 400 of
 * eating in it, and belongs to a structure of the map (a wall, a rack, a
 * bridge) by name, so that a map's notes can say what eating each one
 * opens up (docs/unreal/maps/). A block is any shape: a brick, a wheel, a
 * slab; its mesh and scale are set where the map is built
 * (tools/art/build_creamery.py), its look is the cheese material.
 *
 * The server eats it (Bite): it is drawn in three stages of being eaten (the
 * material's holes grow, by the mesh's custom primitive data), and when it is
 * gone whatever rested on it falls until something holds it. It replicates,
 * dormant until a bite or a fall wakes it.
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
	/** How far eaten, as it is drawn: 0 whole to 3 nearly gone. */
	int32 GetStage() const { return Stage; }
	bool IsFalling() const { return bFalling; }

	/** Server only: takes `Amount` of eating out of it; at none, it is gone, and what rested on it falls. */
	void Bite(float Amount);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;

protected:
	UFUNCTION()
	void OnRep_Stage();
	void ShowStage();
	/** Server only: starts falling (blocks that rested on one just eaten). */
	void Fall();

	UPROPERTY(VisibleAnywhere, Category="Cheese")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** How much eating is left in it. */
	UPROPERTY(EditAnywhere, Category="Cheese")
	float Health;

	/** The structure of the map it is part of, such as "WheelStack" or "HallSeal". */
	UPROPERTY(EditAnywhere, Category="Cheese")
	FName Structure;

	UPROPERTY(ReplicatedUsing=OnRep_Stage)
	uint8 Stage = 0;
	bool bFalling = false;
	float FallSpeed = 0.0f;
};
