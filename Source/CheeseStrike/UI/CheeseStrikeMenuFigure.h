// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeMenuFigure.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * The figure on the front end: the player's character standing with the
 * rifle on a low round pedestal in the middle of the view, turning slowly.
 * Made on each machine for its own menu, never replicated.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeMenuFigure : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeMenuFigure();

	/** Degrees a second it turns. */
	static constexpr float TurnRate = 18.0f;
	/** The pedestal's width and height, in centimetres. */
	static constexpr float PedestalWidth = 130.0f;
	static constexpr float PedestalHeight = 8.0f;

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Pedestal;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Rifle;
};
