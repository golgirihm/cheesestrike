// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeCheese.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UPrimitiveComponent;

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
 * material's holes grow, by the mesh's custom primitive data). When it is
 * gone, or falls, what rested on it falls too unless what else is under it
 * still holds it (CheeseStrikeMice::IsHeld: a lintel over a door stands on
 * the wall either side until one side goes), down until something holds it.
 * It replicates, dormant until a bite or a fall wakes it.
 *
 * A map marks the cheese the mice must leave (its walkable way through, what
 * its starts stand on) as not edible: the mice never go for it, and the maps
 * give it a rind of red wax so that players can tell
 * (docs/unreal/maps/principles.md).
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
	/** Whether the mice may eat it: false for the cheese a map keeps (its way through, its starts). */
	bool IsEdible() const { return bEdible; }
	/** How many blocks of cheese in `World` the mice may eat (what the number of mice goes by). */
	static int32 CountEdible(const UWorld* World);
	/** The health of all the cheese in `World` the mice may eat (what their pace goes by: CheeseStrikeMice::Pace). */
	static float EdibleHealth(const UWorld* World);
	/** Whether what is under it now holds it up (CheeseStrikeMice::IsHeld); `Why`, if given, gets what it stands on. */
	bool IsHeld(FString* Why = nullptr) const;
	/** The blocks in `World` that nothing under them holds up, and that aren't falling (the console's CheeseStrikeUnheld lists them). */
	static TArray<ACheeseStrikeCheese*> Unheld(const UWorld* World);

	/**
	 * The block's box: its collision, where it is now, not what is drawn. A rounded block's mesh stands 2 cm proud of
	 * its collision on every face (tools/art/block_mesh.py), and its bounds are the mesh's, so that the renderer culls
	 * it by what it draws; what rests on what, the mice's holds and what hangs near what are measured by this box.
	 */
	FBox GetBlockBox() const;
	/** `Component`'s box as the cheese measures it: a block's GetBlockBox, anything else's bounds. */
	static FBox BoxOf(const UPrimitiveComponent* Component);
	/** The block's box (GetBlockBox), whatever is asked: a block has its mesh and nothing else. */
	virtual FBox GetComponentsBoundingBox(bool bNonColliding = false, bool bIncludeFromChildActors = false) const override;

	/** Server only: takes `Amount` of eating out of it; at none, it is gone, and what rested on it falls. */
	void Bite(float Amount);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;

protected:
	UFUNCTION()
	void OnRep_Stage();
	void ShowStage();
	/** Server only: starts falling, and what rested on it falls too unless something else holds it. */
	void Fall();
	/** Server only: falls if what is still under it doesn't hold it (CheeseStrikeMice::IsHeld). */
	void FallUnlessHeld();
	/** The blocks resting on this one: their bottoms at its top, over it. */
	TArray<ACheeseStrikeCheese*> RestingOnIt() const;
	/** Whether something holds it up here: the level, or cheese that is neither gone nor falling. */
	static bool Holds(const UPrimitiveComponent* Component);

	UPROPERTY(VisibleAnywhere, Category="Cheese")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** How much eating is left in it. */
	UPROPERTY(EditAnywhere, Category="Cheese")
	float Health;

	/** The structure of the map it is part of, such as "WheelStack" or "HallSeal". */
	UPROPERTY(EditAnywhere, Category="Cheese")
	FName Structure;

	/** Whether the mice may eat it; a map clears it for its walkable way through and under its starts. */
	UPROPERTY(EditAnywhere, Category="Cheese")
	bool bEdible = true;

	UPROPERTY(ReplicatedUsing=OnRep_Stage)
	uint8 Stage = 0;
	bool bFalling = false;
	float FallSpeed = 0.0f;

	/** The collision's box in the mesh's own space, and the mesh it was read from (GetBlockBox). */
	mutable FBox LocalBlockBox = FBox(ForceInit);
	mutable TWeakObjectPtr<const UStaticMesh> LocalBlockBoxOf;
};
