// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CheeseStrikeMovementComponent.generated.h"

/**
 * The character's movement: the engine's walking, falling, jumping and
 * crouching, tuned to CheeseStrikeMovement's numbers, with sprinting and
 * sliding added.
 *
 * Sprinting is held, like crouching, and goes to the server in each move's
 * flags, so that the server simulates the same speed the client predicted.
 * So do carrying a heavy weapon (slower) and a knife's lunge (a burst
 * forward), held by the weapon in hand.
 * A slide begins when the player crouches at a sprint and runs by itself:
 * what it holds (how far into it, from what speed, which way) is saved with
 * each move, so that a replay after a correction comes out the same.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UCheeseStrikeMovementComponent(const FObjectInitializer& ObjectInitializer);

	/** The player holds, or has let go of, the sprint key. */
	void SetWantsToSprint(bool bWants);
	bool WantsToSprint() const { return bWantsToSprint; }
	/** Whether the character is sprinting now: wants to, on the ground, not crouched, and moving. */
	bool IsSprinting() const;
	bool IsSliding() const { return SlideElapsed >= 0.0f; }
	/** Seconds into the slide, or -1 for none. */
	float GetSlideElapsed() const { return SlideElapsed; }
	/** The weapon in hand is heavy (slowing its carrier), or is lunging them forward. */
	void SetCarryingHeavy(bool bHeavy) { bCarryingHeavy = bHeavy; }
	void SetLunging(bool bInLunge) { bLunging = bInLunge; }
	bool IsCarryingHeavy() const { return bCarryingHeavy; }
	bool IsLunging() const { return bLunging; }

	//~ UCharacterMovementComponent
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	/** Begins a slide from the ground speed the character has. */
	void StartSlide();
	void EndSlide();

	/** What the player holds. */
	bool bWantsToSprint = false;
	bool bCarryingHeavy = false;
	bool bLunging = false;
	/** Seconds into the slide, or -1 for none; the speed it began at; and which way it goes, flat. */
	float SlideElapsed = -1.0f;
	float SlideFrom = 0.0f;
	FVector SlideDirection = FVector::ForwardVector;
	/** Seconds since the last slide ended. */
	float SinceSlide = 1000.0f;
	/** The ground friction the slide takes away, to put back after. */
	float GroundFrictionBefore = 8.0f;

	friend class FSavedMove_CheeseStrike;
};

/** A move with the sprint key's state in its flags and the slide's state saved alongside. */
class FSavedMove_CheeseStrike : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;

	bool bSavedWantsToSprint = false;
	bool bSavedCarryingHeavy = false;
	bool bSavedLunging = false;
	float SavedSlideElapsed = -1.0f;
	float SavedSlideFrom = 0.0f;
	FVector SavedSlideDirection = FVector::ForwardVector;
	float SavedSinceSlide = 1000.0f;
};

class FNetworkPredictionData_Client_CheeseStrike : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	FNetworkPredictionData_Client_CheeseStrike(const UCharacterMovementComponent& ClientMovement);
	virtual FSavedMovePtr AllocateNewMove() override;
};
