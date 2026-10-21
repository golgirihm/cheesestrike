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
 * enemy (CheeseStrikeStreaks::RevealTime), a red light blinking under it.
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

	/** Server only: circles `Centre` at `Radius` from the server's time `From` until `Until`. */
	void Fly(const FVector& InCentre, float InRadius, float From, float Until);
	/** Where it is at the server's time `Time`, and which way it faces. */
	FVector WhereAt(float Time) const;

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
};
