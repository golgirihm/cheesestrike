// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_ApplyMeshSpaceAdditive.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_MultiWayBlend.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "CheeseStrikeAnimInstance.generated.h"

class UAnimSequence;
class UBlendSpace;

/** The numbers the body's animation is blended by. */
namespace CheeseStrikeAnimation
{
	/** Speed (cm/s) from which the character is moving rather than standing, and at which the walk and jog cycles are at full weight. */
	constexpr float MovingFrom = 30.0f;
	constexpr float WalkFull = 250.0f;
	constexpr float JogFull = 450.0f;
	/** How fast the crouch, air and aim blends go, per second. */
	constexpr float BlendRate = 10.0f;

	/** Weights of the forward, backward, left and right cycles for movement `Direction` degrees from facing (0 forward, 90 right); they sum to 1. */
	void DirectionWeights(float Direction, float& Forward, float& Backward, float& Left, float& Right);
	/** How much the moving cycles weigh against standing at `Speed`: 0 standing, 1 from WalkFull. */
	float MovingWeight(float Speed);
	/** How much the jog cycle weighs against the walk at `Speed`: 0 up to WalkFull, 1 from JogFull. */
	float JogWeight(float Speed);
	/** The way the character moves, in degrees from the way it faces: 0 forward, 90 right, -90 left, 180 backward. */
	float DirectionOf(const FVector& Velocity, float FacingYaw);
}

/** The animations a body is made of: Lyra's rifle set for the mannequin, found by path. */
USTRUCT()
struct FCheeseStrikeAnimSet
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAnimSequence> Idle;
	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleAiming;
	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkForward;
	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkBackward;
	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkLeft;
	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkRight;
	UPROPERTY()
	TObjectPtr<UAnimSequence> JogForward;
	UPROPERTY()
	TObjectPtr<UAnimSequence> JogBackward;
	UPROPERTY()
	TObjectPtr<UAnimSequence> JogLeft;
	UPROPERTY()
	TObjectPtr<UAnimSequence> JogRight;
	UPROPERTY()
	TObjectPtr<UAnimSequence> CrouchIdle;
	UPROPERTY()
	TObjectPtr<UBlendSpace> CrouchWalk;
	UPROPERTY()
	TObjectPtr<UAnimSequence> FallLoop;
	UPROPERTY()
	TObjectPtr<UBlendSpace> AimHip;
	UPROPERTY()
	TObjectPtr<UBlendSpace> AimAiming;
	UPROPERTY()
	TObjectPtr<UBlendSpace> AimCrouch;
	UPROPERTY()
	TObjectPtr<UAnimSequence> Fire;
	UPROPERTY()
	TObjectPtr<UAnimSequence> Reload;

	/** Whether every animation was found. */
	bool IsComplete() const;
};

/**
 * The body's animation, composed in code rather than in an animation
 * Blueprint, so that it can be read and changed as text and works for any
 * machine's copy of a character: standing, walking and jogging in four
 * directions, crouched still or walking, falling, aimed up and down by the
 * rifle aim offsets (hip, down the sights, crouched), with a slot on top
 * for firing and reloading. The numbers it blends by come from the
 * character each frame (UCheeseStrikeAnimInstance), and the blending runs
 * on the animation thread (FCheeseStrikeAnimProxy).
 */
struct FCheeseStrikeAnimProxy : public FAnimInstanceProxy
{
	FCheeseStrikeAnimProxy() = default;
	explicit FCheeseStrikeAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void CacheBones() override;

	/** What the character is doing, copied in on the game thread. */
	float Speed = 0.0f;
	float Direction = 0.0f;
	float AimPitch = 0.0f;
	bool bCrouched = false;
	bool bFalling = false;
	bool bAiming = false;
	/** The blends under way, eased towards what the character is doing. */
	float CrouchBlend = 0.0f;
	float AirBlend = 0.0f;
	float AimBlend = 0.0f;

protected:
	/** Puts the nodes together, once. */
	void Link(const FCheeseStrikeAnimSet& Set);

	FAnimNode_SequencePlayer_Standalone Idle;
	FAnimNode_SequencePlayer_Standalone IdleAiming;
	FAnimNode_SequencePlayer_Standalone WalkForward;
	FAnimNode_SequencePlayer_Standalone WalkBackward;
	FAnimNode_SequencePlayer_Standalone WalkLeft;
	FAnimNode_SequencePlayer_Standalone WalkRight;
	FAnimNode_SequencePlayer_Standalone JogForward;
	FAnimNode_SequencePlayer_Standalone JogBackward;
	FAnimNode_SequencePlayer_Standalone JogLeft;
	FAnimNode_SequencePlayer_Standalone JogRight;
	FAnimNode_SequencePlayer_Standalone CrouchIdle;
	FAnimNode_SequencePlayer_Standalone FallLoop;
	FAnimNode_BlendSpacePlayer_Standalone CrouchWalk;
	FAnimNode_BlendSpacePlayer_Standalone AimHip;
	FAnimNode_BlendSpacePlayer_Standalone AimAiming;
	FAnimNode_BlendSpacePlayer_Standalone AimCrouch;
	/** The eight moving cycles, weighed by direction and speed. */
	FAnimNode_MultiWayBlend Cycles;
	/** Standing still: from the hip, or down the sights. */
	FAnimNode_TwoWayBlend StandingStill;
	/** Standing: still, or moving. */
	FAnimNode_TwoWayBlend Standing;
	/** Crouched: still, or walking. */
	FAnimNode_TwoWayBlend Crouched;
	/** On the ground: standing, or crouched. */
	FAnimNode_TwoWayBlend OnGround;
	/** On the ground, or in the air. */
	FAnimNode_TwoWayBlend Body;
	/** The aim offsets, each weighed by how much it applies. */
	FAnimNode_ApplyMeshSpaceAdditive AimedFromHip;
	FAnimNode_ApplyMeshSpaceAdditive AimedDownSights;
	FAnimNode_ApplyMeshSpaceAdditive AimedCrouched;
	/** Firing and reloading play over everything. */
	FAnimNode_Slot Slot;
	bool bLinked = false;
};

UCLASS()
class CHEESESTRIKE_API UCheeseStrikeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UCheeseStrikeAnimInstance();

	const FCheeseStrikeAnimSet& GetSet() const { return Set; }
	/** The slot firing and reloading play in. */
	static FName SlotName() { return FName(TEXT("DefaultSlot")); }

	/** What the body is doing, as read from its character each frame. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Animation")
	float Speed = 0.0f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Animation")
	float Direction = 0.0f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Animation")
	float AimPitch = 0.0f;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Animation")
	bool bCrouched = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Animation")
	bool bFalling = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Animation")
	bool bAiming = false;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	UPROPERTY()
	FCheeseStrikeAnimSet Set;
	FCheeseStrikeAnimProxy Proxy;
};
