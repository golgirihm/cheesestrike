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
 * So does carrying a heavy weapon (slower), held by the weapon in hand.
 * A slide begins when the player crouches at a sprint and runs by itself:
 * what it holds (how far into it, from what speed, which way) is saved with
 * each move, so that a replay after a correction comes out the same.
 *
 * The Gouda Overload slows the splatted and hastens its caller: the factors
 * come from times the server replicates on the character (its SplatAt,
 * GooUntil and HasteUntil) and the server's clock, which both sides read
 * alike, so nothing more needs to go with the moves.
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
	/** Whether the character is sprinting now: wants to, on the ground, not crouched, moving, and not held back by the Overload's gouda. */
	bool IsSprinting() const;
	/** The share of its speed the Overload's gouda leaves it (1 for none), and its caller's haste (1 for none). */
	float GooFactor() const;
	float HasteFactor() const;
	/** Whether it may sprint and slide now: not too slowed by the gouda (CheeseStrikeStreaks::MaySprintInGoo). */
	bool MaySprint() const;
	bool IsSliding() const { return SlideElapsed >= 0.0f; }
	/** Seconds into the slide, or -1 for none. */
	float GetSlideElapsed() const { return SlideElapsed; }
	/** The weapon in hand is heavy, slowing its carrier. */
	void SetCarryingHeavy(bool bHeavy) { bCarryingHeavy = bHeavy; }
	bool IsCarryingHeavy() const { return bCarryingHeavy; }

	//~ UCharacterMovementComponent
	virtual float GetMaxSpeed() const override;
	/** Down, the input moves the body only along itself (CheeseStrikeDowned::ScootInput). */
	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual bool CanCrouchInCurrentState() const override;
	/** A bot never steps off an edge where the drop ahead would hurt it (CheeseStrikeFalls::BotMayStepOff); a player may. */
	virtual bool CanWalkOffLedges() const override;
	/** Whether the ground just ahead, going `Going` (flat), is near enough below the feet that stepping off can't hurt. */
	bool IsDropAheadSafe(const FVector& Going) const;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	/** On the server: for a moment after a player's character is held (CheeseStrikeStart::HoldGrace), takes their machine's word for where it is. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	/** Whether its character is alive and held where it stands: the count before the match or its round, or the round or the match over (CheeseStrikeStart::MayAct). */
	bool IsHeldHere() const;
	/** The world time the server saw its character held, or -1 while it isn't. */
	float HeldSince = -1.0f;
	/** Whether the server is taking the player's machine's word for where it is. */
	bool bTakingClientWord = false;
	/** Whether its character is down in a last stand: it scoots, slowly (CheeseStrikeDowned). */
	bool IsDownedHere() const;
	/** Begins a slide from the ground speed the character has. */
	void StartSlide();
	void EndSlide();

	/** What the player holds. */
	bool bWantsToSprint = false;
	bool bCarryingHeavy = false;
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
