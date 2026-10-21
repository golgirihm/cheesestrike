// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMovementComponent.h"
#include "CheeseStrikeMovement.h"
#include "GameFramework/Character.h"

UCheeseStrikeMovementComponent::UCheeseStrikeMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Qualified throughout: the engine's component has members by some of these names.
	MaxWalkSpeed = CheeseStrikeMovement::WalkSpeed;
	MaxWalkSpeedCrouched = CheeseStrikeMovement::WalkSpeed * CheeseStrikeMovement::CrouchFactor;
	MaxAcceleration = CheeseStrikeMovement::Acceleration;
	JumpZVelocity = CheeseStrikeMovement::JumpSpeed;
	GravityScale = 1.0f;
	// In the air the player steers at AirSteer, pushing a way or letting go.
	AirControl = CheeseStrikeMovement::AirControl();
	AirControlBoostMultiplier = 1.0f;
	BrakingDecelerationFalling = CheeseStrikeMovement::AirSteer;
	MaxStepHeight = CheeseStrikeMovement::StepHeight;
	SetCrouchedHalfHeight(CheeseStrikeMovement::CrouchedHalfHeight);
	NavAgentProps.bCanCrouch = true;
	bCanWalkOffLedgesWhenCrouching = true;
}

void UCheeseStrikeMovementComponent::SetWantsToSprint(bool bWants)
{
	bWantsToSprint = bWants;
}

bool UCheeseStrikeMovementComponent::IsSprinting() const
{
	return bWantsToSprint && IsMovingOnGround() && !IsCrouching() && !IsSliding() && !Velocity.IsNearlyZero(1.0f);
}

float UCheeseStrikeMovementComponent::GetMaxSpeed() const
{
	if (IsSliding())
	{
		return CheeseStrikeMovement::SlideSpeed(SlideFrom, SlideElapsed);
	}
	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
	case MOVE_Falling:
		return CheeseStrikeMovement::WithWeapon(CheeseStrikeMovement::Speed(bWantsToSprint, false, IsCrouching()), bCarryingHeavy, bLunging);
	default:
		return Super::GetMaxSpeed();
	}
}

float UCheeseStrikeMovementComponent::GetMaxAcceleration() const
{
	// No steering in a slide; a lunge gets there at once.
	if (IsSliding())
	{
		return 0.0f;
	}
	return bLunging ? CheeseStrikeMovement::LungeAcceleration : Super::GetMaxAcceleration();
}

float UCheeseStrikeMovementComponent::GetMaxBrakingDeceleration() const
{
	// The slide sets its own speed: nothing else slows it.
	return IsSliding() ? 0.0f : Super::GetMaxBrakingDeceleration();
}

bool UCheeseStrikeMovementComponent::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState();
}

void UCheeseStrikeMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// The slide's own clock, before the engine acts on crouching.
	if (IsSliding())
	{
		SlideElapsed += DeltaSeconds;
		const bool bOver = SlideElapsed >= CheeseStrikeMovement::SlideTime || !bWantsToCrouch || !IsMovingOnGround();
		if (bOver)
		{
			EndSlide();
		}
		else
		{
			// Keeps going the way it began, at the slide's speed, whatever the ground did.
			const float Speed = CheeseStrikeMovement::SlideSpeed(SlideFrom, SlideElapsed);
			Velocity = SlideDirection * Speed + FVector(0, 0, Velocity.Z);
		}
	}
	else
	{
		SinceSlide += DeltaSeconds;
		const float GroundSpeed = Velocity.Size2D();
		if (bWantsToCrouch && bWantsToSprint && IsMovingOnGround() && !IsCrouching()
			&& SinceSlide >= CheeseStrikeMovement::SlideRest && CheeseStrikeMovement::CanSlide(GroundSpeed))
		{
			StartSlide();
		}
	}
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UCheeseStrikeMovementComponent::StartSlide()
{
	SlideElapsed = 0.0f;
	SlideFrom = Velocity.Size2D();
	SlideDirection = FVector(Velocity.X, Velocity.Y, 0.0f).GetSafeNormal();
	GroundFrictionBefore = GroundFriction;
	GroundFriction = 0.0f;
	Velocity = SlideDirection * CheeseStrikeMovement::SlideSpeed(SlideFrom, 0.0f) + FVector(0, 0, Velocity.Z);
}

void UCheeseStrikeMovementComponent::EndSlide()
{
	SlideElapsed = -1.0f;
	SinceSlide = 0.0f;
	GroundFriction = GroundFrictionBefore;
}

void UCheeseStrikeMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bCarryingHeavy = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bLunging = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
}

FNetworkPredictionData_Client* UCheeseStrikeMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UCheeseStrikeMovementComponent* MutableThis = const_cast<UCheeseStrikeMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_CheeseStrike(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.0f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.0f;
	}
	return ClientPredictionData;
}

// The saved move.

void FSavedMove_CheeseStrike::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
	bSavedCarryingHeavy = false;
	bSavedLunging = false;
	SavedSlideElapsed = -1.0f;
	SavedSlideFrom = 0.0f;
	SavedSlideDirection = FVector::ForwardVector;
	SavedSinceSlide = 1000.0f;
}

uint8 FSavedMove_CheeseStrike::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Flags |= FLAG_Custom_0;
	}
	if (bSavedCarryingHeavy)
	{
		Flags |= FLAG_Custom_1;
	}
	if (bSavedLunging)
	{
		Flags |= FLAG_Custom_2;
	}
	return Flags;
}

bool FSavedMove_CheeseStrike::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_CheeseStrike* Other = static_cast<const FSavedMove_CheeseStrike*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedCarryingHeavy != Other->bSavedCarryingHeavy || bSavedLunging != Other->bSavedLunging || SavedSlideElapsed >= 0.0f || Other->SavedSlideElapsed >= 0.0f)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_CheeseStrike::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const UCheeseStrikeMovementComponent* Movement = Cast<UCheeseStrikeMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Movement->bWantsToSprint;
		bSavedCarryingHeavy = Movement->bCarryingHeavy;
		bSavedLunging = Movement->bLunging;
		SavedSlideElapsed = Movement->SlideElapsed;
		SavedSlideFrom = Movement->SlideFrom;
		SavedSlideDirection = Movement->SlideDirection;
		SavedSinceSlide = Movement->SinceSlide;
	}
}

void FSavedMove_CheeseStrike::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UCheeseStrikeMovementComponent* Movement = Cast<UCheeseStrikeMovementComponent>(C->GetCharacterMovement()))
	{
		Movement->bWantsToSprint = bSavedWantsToSprint;
		Movement->bCarryingHeavy = bSavedCarryingHeavy;
		Movement->bLunging = bSavedLunging;
		Movement->SlideElapsed = SavedSlideElapsed;
		Movement->SlideFrom = SavedSlideFrom;
		Movement->SlideDirection = SavedSlideDirection;
		Movement->SinceSlide = SavedSinceSlide;
	}
}

FNetworkPredictionData_Client_CheeseStrike::FNetworkPredictionData_Client_CheeseStrike(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_CheeseStrike::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_CheeseStrike());
}
