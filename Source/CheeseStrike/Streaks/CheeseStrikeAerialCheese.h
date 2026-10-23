// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeAerialCheese.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * The Unmanned Aerial Cheese, as everyone sees it: a gouda wheel on four
 * rotors, circling high over the map for as long as it shows its side the
 * enemy (CheeseStrikeStreaks::RevealTime), a red light blinking under it;
 * on a map under a roof, under the roof, on the circle that runs into least
 * (CheeseStrikeStreaks::DroneHeight, ChooseDroneWay), smaller if the roof
 * is low.
 * What it shows is the game state's (ACheeseStrikeGameState::IsRevealedTo);
 * this is only the sight of it. Every machine flies it by the server's
 * clock from where and when it set out, so nothing of its flight is sent.
 */
UCLASS(NotPlaceable)
class CHEESESTRIKE_API ACheeseStrikeAerialCheese : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeAerialCheese();

	/** Server only: circles `Centre` at `Radius` from the server's time `From` until `Until`, at `Scale` times its size. */
	void Fly(const FVector& InCentre, float InRadius, float From, float Until, float InScale = 1.0f);
	/** Server only: flies over the map from `From` until `Until`: over the middle of its starts, high above the highest, or under its roof. */
	void FlyOver(float From, float Until);
	/** Where it is at the server's time `Time`, and which way it faces. */
	FVector WhereAt(float Time) const;
	/** Whether it flies under a roof, and how big it is. */
	bool IsUnderRoof() const { return bUnderRoof; }
	float GetScale() const { return Scale; }
	float GetEndAt() const { return EndAt; }
	/** How far its parts reach from its middle, at full size (cm), across and up or down: what it must keep clear of. */
	static constexpr float Reach = 270.0f;
	static constexpr float ReachUp = 90.0f;
	static FVector Extent(float AtScale) { return FVector(Reach, Reach, ReachUp) * AtScale; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	float Now() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Wheel;
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> Rotors;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Beacon;

	UPROPERTY(Replicated)
	FVector_NetQuantize Centre = FVector::ZeroVector;
	UPROPERTY(Replicated)
	float Radius = 2000.0f;
	UPROPERTY(Replicated)
	float StartAt = 0.0f;
	UPROPERTY(Replicated)
	float EndAt = 0.0f;
	UPROPERTY(Replicated)
	float Scale = 1.0f;
	UPROPERTY(Replicated)
	bool bUnderRoof = false;
};
