// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_ApplyMeshSpaceAdditive.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_LayeredBoneBlend.h"
#include "CheeseStrikeDownedPose.h"
#include "CheeseStrikeExecutionPose.h"
#include "CheeseStrikeGripIK.h"
#include "AnimNodes/AnimNode_MultiWayBlend.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "Weapons/CheeseStrikeShot.h"
#include "CheeseStrikeAnimInstance.generated.h"

class UAnimSequence;
class UBlendSpace;

/**
 * Evaluates a node that is linked elsewhere in the graph again, for a second
 * branch, without updating it a second time (which would play its animations
 * at twice their speed): where the same pose is needed twice.
 */
struct FCheeseStrikeSharedPose : public FAnimNode_Base
{
	FAnimNode_Base* Shared = nullptr;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override
	{
		if (Shared != nullptr)
		{
			Shared->Evaluate_AnyThread(Output);
		}
		else
		{
			Output.ResetToRefPose();
		}
	}
};

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

/** The animations a body is made of for one way of holding a weapon (Lyra's rifle or pistol set), found by path. */
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
	TObjectPtr<UAnimSequence> CrouchWalkForward;
	UPROPERTY()
	TObjectPtr<UAnimSequence> CrouchWalkBackward;
	UPROPERTY()
	TObjectPtr<UAnimSequence> CrouchWalkLeft;
	UPROPERTY()
	TObjectPtr<UAnimSequence> CrouchWalkRight;
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
	/** Bringing the weapon up, a knife's stroke, and the free arm's guard (the knife's); each may be missing. */
	UPROPERTY()
	TObjectPtr<UAnimSequence> Equip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> Stroke;
	UPROPERTY()
	TObjectPtr<UAnimSequence> Guard;
	/** Throwing a grenade over the weapon held (Lyra's toss, the same for every way of holding); may be missing. */
	UPROPERTY()
	TObjectPtr<UAnimSequence> Throw;

	/** Whether every animation it can't do without was found. */
	bool IsComplete() const;
};

/** A body's sets, one for each way of holding a weapon. */
USTRUCT()
struct FCheeseStrikeAnimSets
{
	GENERATED_BODY()

	/** By ECheeseStrikeHold. */
	UPROPERTY()
	FCheeseStrikeAnimSet Holds[NumHolds];

	/** The set for `Hold`, or the rifle's if it hasn't all of one for it. */
	const FCheeseStrikeAnimSet& For(ECheeseStrikeHold Hold) const;
	FCheeseStrikeAnimSet& Of(ECheeseStrikeHold Hold) { return Holds[static_cast<uint8>(Hold)]; }
};

/**
 * The body's animation, composed in code rather than in an animation
 * Blueprint, so that it can be read and changed as text and works for any
 * machine's copy of a character: standing, walking and jogging in four
 * directions, crouched still or walking in four, falling, aimed up and down
 * by the aim offsets (hip, down the sights, crouched), with a slot on top
 * for firing, reloading, drawing and striking, then for the knife the free
 * arm up on guard, and then the left hand held on the weapon
 * (FCheeseStrikeGripIK) except while a montage moves it, and last, for a
 * player down in Execution's last stand, the pose of one lying hurt on
 * their back, propped up, the pistol out (FCheeseStrikeDownedPose), blended
 * in and out. The animations are
 * the set for how the weapon in hand is held (rifle, pistol, shotgun,
 * knife), put on the same nodes when it changes. The numbers it blends by
 * come from the character each frame (UCheeseStrikeAnimInstance), and the
 * blending runs on the animation thread (FCheeseStrikeAnimProxy).
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
	bool bDowned = false;
	/** The way the body faces in component space, where its capsule's axis meets the floor, where it aims down (degrees right of its facing), how much room its legs have, and its scoot (cm/s along its facing, and degrees a second turning round). */
	FVector BodyForward = FVector(0.0, 1.0, 0.0);
	FVector BodyOrigin = FVector::ZeroVector;
	float DownAimYaw = 0.0f;
	float LegRoom = CheeseStrikeDowned::LegLength;
	float ScootSpeed = 0.0f;
	float TurnSpeed = 0.0f;
	bool bFirstPersonBody = false;
	/** The blends under way, eased towards what the character is doing. */
	float CrouchBlend = 0.0f;
	float AirBlend = 0.0f;
	float AimBlend = 0.0f;
	/** How the weapon in hand is held, the set it plays now, and where the left hand reaches past its grip (cm, in the anchor's space). */
	ECheeseStrikeHold Hold = ECheeseStrikeHold::Rifle;
	ECheeseStrikeHold LinkedHold = ECheeseStrikeHold::Rifle;
	FVector LeftHandShift = FVector::ZeroVector;
	float GuardBlend = 0.0f;
	/** Down: how far (0 to 1, at an even pace; the pose takes it smoothed), how much the legs scoot, where they are in their pushes and the breath's phase (radians). */
	float DownLinear = 0.0f;
	float ScootBlend = 0.0f;
	float ScootPhase = 0.0f;
	float BreathPhase = 0.0f;
	/** In an execution: which, whether dealing it, how far in, and (dealing it) where the blow lands and the near shoulder, in component space, and the side. */
	uint8 ExecutionKind = 0;
	bool bExecutor = false;
	float ExecutionTime = 0.0f;
	FVector ExecutionTarget = FVector::ZeroVector;
	FVector ExecutionShoulder = FVector::ZeroVector;
	float ExecutionSide = 1.0f;

protected:
	/** Puts the nodes together, once, the left hand's grip following `GripAnchor`. */
	void Link(FName GripAnchor);
	/** Gives the nodes `Set`'s animations: at first, and when the way the weapon is held changes. */
	void Assign(const FCheeseStrikeAnimSet& Set);
	/** After a change of hold, the left hand's grip is measured afresh once nothing plays in the slot. */
	bool bGripPending = false;

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
	FAnimNode_SequencePlayer_Standalone CrouchWalkForward;
	FAnimNode_SequencePlayer_Standalone CrouchWalkBackward;
	FAnimNode_SequencePlayer_Standalone CrouchWalkLeft;
	FAnimNode_SequencePlayer_Standalone CrouchWalkRight;
	/** The free arm's guard, for the knife. */
	FAnimNode_SequencePlayer_Standalone GuardPose;
	FAnimNode_BlendSpacePlayer_Standalone AimHip;
	FAnimNode_BlendSpacePlayer_Standalone AimAiming;
	FAnimNode_BlendSpacePlayer_Standalone AimCrouch;
	/** The eight moving cycles, weighed by direction and speed, and the four crouched ones, by direction. */
	FAnimNode_MultiWayBlend Cycles;
	FAnimNode_MultiWayBlend CrouchCycles;
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
	/**
	 * Firing and reloading play over the body from the spine up; the legs
	 * keep walking (a reload on the move doesn't slide). The slot's own
	 * source is the same pose again, shared rather than played twice.
	 */
	FCheeseStrikeSharedPose Locomotion;
	FAnimNode_Slot Slot;
	FAnimNode_LayeredBoneBlend UpperBody;
	/** With the knife, the left arm from the shoulder on is the guard's. */
	FAnimNode_LayeredBoneBlend Guarded;
	/**
	 * Then the left hand is held where it grips the rifle, which rides the
	 * weapon_r bone, so that the cycles' and the aim offsets' drift never takes
	 * it off; a montage (the reload) has the hand to itself.
	 */
	FAnimNode_ConvertLocalToComponentSpace ToComponentSpace;
	FCheeseStrikeGripIK LeftHand;
	/** Down in Execution's last stand: on their back, propped on the elbow, the pistol out. */
	FCheeseStrikeDownedPose Downed;
	/** In an execution (CheeseStrikeExecution): the executor's blow, or the one down's last moments. */
	FCheeseStrikeExecutionPose Executing;
	FAnimNode_ConvertComponentToLocalSpace ToLocalSpace;
	bool bLinked = false;
	/** Bones the mesh's reference pose scales, kept at that scale whatever the animations say (Belica's faceAttach). */
	TArray<TPair<FCompactPoseBoneIndex, FVector>> ScaledBones;
};

/**
 * The mannequin's body, animated by Lyra's rifle set (FCheeseStrikeAnimProxy).
 * A subclass knows the sets retargeted to other skeletons, and plays the
 * ones its mesh's skeleton has (UCheeseStrikeHeroAnimInstance); a hold a
 * body has no set for uses its rifle set.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UCheeseStrikeAnimInstance();

	/** The set it plays now, for how the weapon in hand is held; and its body's set for `ForHold` (the rifle's if it has none). */
	const FCheeseStrikeAnimSet& GetSet() const { return GetSet(Hold); }
	const FCheeseStrikeAnimSet& GetSet(ECheeseStrikeHold ForHold) const { return Picked.For(ForHold); }
	/** Every body's sets it knows, a skeleton's each. */
	const TArray<FCheeseStrikeAnimSets>& GetSets() const { return Sets; }
	/** Plays the sets its mesh's skeleton has, of those it knows (the first if none is): as it is initialised. */
	void PickSet();
	/** The bone the rifle rides, which the left hand's grip follows. */
	FName GetGripAnchor() const { return GripAnchor; }
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
	/** Down in Execution's last stand (UCheeseStrikeLastStand), and alive. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Animation")
	bool bDowned = false;
	/** The way the body faces in its mesh's component space (the character's mesh is turned a quarter turn), and where its capsule's axis meets the floor there. */
	FVector BodyForward = FVector(0.0, 1.0, 0.0);
	FVector BodyOrigin = FVector::ZeroVector;
	/** Down: where it aims, in degrees to the right of the way the body faces; the room ahead of its hips for its legs; how fast it scoots along its facing (cm/s) and turns round (degrees a second). */
	float DownAimYaw = 0.0f;
	float LegRoom = CheeseStrikeDowned::LegLength;
	float ScootSpeed = 0.0f;
	float TurnSpeed = 0.0f;
	/** Whether this is the first-person body, its own player's. */
	bool bFirstPersonBody = false;
	/**
	 * In an execution (UCheeseStrikeLastStand::GetExecution): which kind,
	 * whether this body deals it, how far in (seconds), and for the executor
	 * where the blow lands and the near shoulder of the one down (in the
	 * frame the body is animated in, taken as it begins) and the side.
	 */
	uint8 ExecutionKind = 0;
	bool bExecutor = false;
	float ExecutionTime = 0.0f;
	FVector ExecutionTarget = FVector::ZeroVector;
	FVector ExecutionShoulder = FVector::ZeroVector;
	float ExecutionSide = 1.0f;
	/** The server's time the execution whose places were taken began, and those places in the world. */
	float ExecutionTakenFor = -1.0f;
	FVector ExecutionTargetWorld = FVector::ZeroVector;
	FVector ExecutionShoulderWorld = FVector::ZeroVector;
	/** The body's yaw last frame, for its turning. */
	float LastYaw = 0.0f;
	bool bLastYawKnown = false;
	/** How the weapon in hand is held, and where the left hand reaches past its grip on it, in the anchor bone's space (cm). */
	ECheeseStrikeHold Hold = ECheeseStrikeHold::Rifle;
	FVector LeftHandShift = FVector::ZeroVector;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	/**
	 * Finds a set by its Lyra names, `Kind` "Rifle" or "Pistol": the
	 * locomotion in the folder `Locomotion`, the aim offsets in
	 * `AimOffsets`, firing, reloading and drawing in `Actions` (package
	 * paths ending in '/'). Constructors only.
	 */
	static FCheeseStrikeAnimSet FindSet(const TCHAR* Kind, const FString& Locomotion, const FString& AimOffsets, const FString& Actions);
	/**
	 * Finds a hero's sets: its rifle set in `Rifles` (a folder of Lyra's
	 * names, as CheeseStrikeBodies::FBody::Animations), and beside it the
	 * pistol's (Pistol/) and the shotgun's firing and reloading (Shotgun/).
	 */
	static FCheeseStrikeAnimSets FindSets(const FString& Rifles);

	/** The sets it knows, a body's each, and those it plays. */
	UPROPERTY()
	TArray<FCheeseStrikeAnimSets> Sets;
	UPROPERTY()
	FCheeseStrikeAnimSets Picked;
	FName GripAnchor = TEXT("weapon_r");
	FCheeseStrikeAnimProxy Proxy;
};

/**
 * A Paragon hero's body (CheeseStrikeBodies, Content/LICENSES.md): Lyra's
 * rifle, pistol and shotgun animations, retargeted from the mannequin to
 * each hero's skeleton through Lyra's UE4-mannequin IK rig
 * (tools/art/lyra_hero.py); it plays its mesh's. The heroes' weapon_r
 * bones (where they have them) stay where their own guns were, so the
 * weapon and the grip ride the right hand.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeHeroAnimInstance : public UCheeseStrikeAnimInstance
{
	GENERATED_BODY()

public:
	UCheeseStrikeHeroAnimInstance();
};
