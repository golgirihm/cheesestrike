// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeDoor.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

/**
 * A door in a map (docs/unreal/maps/principles.md, "Doors"): it opens as
 * anyone comes up to it (a player, a bot or a mouse) and shuts once nobody
 * is near; no key and no button. Its leaves slide aside, two parting in the
 * middle or one to the side, into the wall it stands in. Shut, it is cover
 * that hides who is behind it and stops shots; open, it is a gap.
 *
 * The navigation mesh runs through it as though it were open, so bots and
 * mice find their way through it and it opens before they reach it. The
 * server decides whether it is open, which replicates; every machine slides
 * the leaves itself. A map script places it in a doorway
 * (tools/art/map_kit.py, door): its actor is at the middle of the doorway's
 * bottom, its X axis through the doorway.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeDoor : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeDoor();

	bool IsOpen() const { return bOpen; }
	/** How far open the leaves are, as this machine shows them: 0 shut, 1 open. */
	float GetOpenness() const { return Openness; }

	/** Whether someone at `Distance` (cm, flat, from the doorway's middle) and `Height` (above or below its bottom) opens it. */
	static bool Opens(float Distance, float Height, float Range);

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	/** Server only: opens or shuts it by who is near. */
	void Watch();
	UFUNCTION()
	void OnRep_Open();
	/** Sizes the leaves to the doorway and puts them where Openness has them. */
	void PlaceLeaves();

	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<UStaticMeshComponent> LeafA;
	UPROPERTY(VisibleAnywhere, Category="Door")
	TObjectPtr<UStaticMeshComponent> LeafB;

	/** The doorway: its width and height, and the leaves' thickness (cm). */
	UPROPERTY(EditAnywhere, Category="Door")
	float Width = 200.0f;
	UPROPERTY(EditAnywhere, Category="Door")
	float Height = 250.0f;
	UPROPERTY(EditAnywhere, Category="Door")
	float Thickness = 8.0f;
	/** Two leaves parting in the middle, or one sliding to the side. */
	UPROPERTY(EditAnywhere, Category="Door")
	bool bDouble = true;
	/** How near (cm, from the doorway's middle) someone comes for it to open; a mouse's middle is further from its nose. */
	UPROPERTY(EditAnywhere, Category="Door")
	float Range = 350.0f;
	/** The leaves' look. */
	UPROPERTY(EditAnywhere, Category="Door")
	TObjectPtr<UMaterialInterface> LeafMaterial;

	UPROPERTY(ReplicatedUsing=OnRep_Open)
	bool bOpen = false;
	float Openness = 0.0f;
	FTimerHandle WatchTimer;
};
