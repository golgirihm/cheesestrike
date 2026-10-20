// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrikeCharacter.h"
#include "Weapons/CheeseStrikeWeapon.h"

namespace CheeseStrikeAnimation
{
	void DirectionWeights(float Direction, float& Forward, float& Backward, float& Left, float& Right)
	{
		// Each cycle weighs by how much of the way it is: forward at 0, right at 90, and so on, fading over a quarter turn.
		const float Angle = FRotator::NormalizeAxis(Direction);
		const auto Weigh = [Angle](float Way) { return FMath::Max(0.0f, 1.0f - FMath::Abs(FRotator::NormalizeAxis(Angle - Way)) / 90.0f); };
		Forward = Weigh(0.0f);
		Right = Weigh(90.0f);
		Backward = Weigh(180.0f);
		Left = Weigh(-90.0f);
		const float Sum = Forward + Backward + Left + Right;
		if (Sum > KINDA_SMALL_NUMBER)
		{
			Forward /= Sum;
			Backward /= Sum;
			Left /= Sum;
			Right /= Sum;
		}
		else
		{
			Forward = 1.0f;
		}
	}

	float MovingWeight(float Speed)
	{
		return FMath::Clamp((Speed - MovingFrom) / (WalkFull - MovingFrom), 0.0f, 1.0f);
	}

	float JogWeight(float Speed)
	{
		return FMath::Clamp((Speed - WalkFull) / (JogFull - WalkFull), 0.0f, 1.0f);
	}

	float DirectionOf(const FVector& Velocity, float FacingYaw)
	{
		if (Velocity.SizeSquared2D() < 1.0f)
		{
			return 0.0f;
		}
		return FRotator::NormalizeAxis(Velocity.GetSafeNormal2D().Rotation().Yaw - FacingYaw);
	}
}

bool FCheeseStrikeAnimSet::IsComplete() const
{
	return Idle && IdleAiming && WalkForward && WalkBackward && WalkLeft && WalkRight && JogForward && JogBackward && JogLeft && JogRight
		&& CrouchIdle && CrouchWalk && FallLoop && AimHip && AimAiming && AimCrouch && Fire && Reload;
}

namespace
{
	template <typename T>
	TObjectPtr<T> Find(const TCHAR* Path)
	{
		ConstructorHelpers::FObjectFinder<T> Finder(Path);
		return Finder.Object;
	}
}

UCheeseStrikeAnimInstance::UCheeseStrikeAnimInstance()
{
	// Lyra's rifle set for the mannequin (Content/LICENSES.md).
	Set.Idle = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_Hipfire.MM_Rifle_Idle_Hipfire"));
	Set.IdleAiming = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_ADS.MM_Rifle_Idle_ADS"));
	Set.WalkForward = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Walk_Fwd.MM_Rifle_Walk_Fwd"));
	Set.WalkBackward = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Walk_Bwd.MM_Rifle_Walk_Bwd"));
	Set.WalkLeft = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Walk_Left.MM_Rifle_Walk_Left"));
	Set.WalkRight = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Walk_Right.MM_Rifle_Walk_Right"));
	Set.JogForward = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Jog_Fwd.MM_Rifle_Jog_Fwd"));
	Set.JogBackward = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Jog_Bwd.MM_Rifle_Jog_Bwd"));
	Set.JogLeft = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Jog_Left.MM_Rifle_Jog_Left"));
	Set.JogRight = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Jog_Right.MM_Rifle_Jog_Right"));
	Set.CrouchIdle = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Crouch_Idle.MM_Rifle_Crouch_Idle"));
	Set.CrouchWalk = Find<UBlendSpace>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/BS_MM_Rifle_Crouch_Walk.BS_MM_Rifle_Crouch_Walk"));
	Set.FallLoop = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Jump_Fall_Loop.MM_Rifle_Jump_Fall_Loop"));
	Set.AimHip = Find<UBlendSpace>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/AimOffsets/AO_MM_Rifle_Idle_Hipfire.AO_MM_Rifle_Idle_Hipfire"));
	Set.AimAiming = Find<UBlendSpace>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/AimOffsets/AO_MM_Rifle_Idle_ADS.AO_MM_Rifle_Idle_ADS"));
	Set.AimCrouch = Find<UBlendSpace>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/AimOffsets/AO_MM_Rifle_Crouch_Idle.AO_MM_Rifle_Crouch_Idle"));
	Set.Fire = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Actions/MM_Rifle_Fire.MM_Rifle_Fire"));
	Set.Reload = Find<UAnimSequence>(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Actions/MM_Rifle_Reload.MM_Rifle_Reload"));
}

FAnimInstanceProxy* UCheeseStrikeAnimInstance::CreateAnimInstanceProxy()
{
	return &Proxy;
}

void UCheeseStrikeAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	// Ours lives and dies with the instance.
}

void UCheeseStrikeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const APawn* Pawn = TryGetPawnOwner();
	if (Pawn == nullptr)
	{
		return;
	}
	const FVector Velocity = Pawn->GetVelocity();
	Speed = Velocity.Size2D();
	Direction = CheeseStrikeAnimation::DirectionOf(Velocity, Pawn->GetActorRotation().Yaw);
	// Where the player looks, as every machine has it: the controller's for its own, the replicated view pitch for others'.
	AimPitch = FRotator::NormalizeAxis(Pawn->GetBaseAimRotation().Pitch);
	if (const ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(Pawn))
	{
		bCrouched = Character->bIsCrouched;
		bFalling = Character->GetCharacterMovement() != nullptr && Character->GetCharacterMovement()->IsFalling();
		bAiming = Character->GetWeapon() != nullptr && Character->GetWeapon()->IsAimed();
	}
}

void FCheeseStrikeAnimProxy::Link(const FCheeseStrikeAnimSet& Set)
{
	const auto Loop = [](FAnimNode_SequencePlayer_Standalone& Player, UAnimSequence* Sequence)
	{
		Player.SetSequence(Sequence);
		Player.SetLoopAnimation(true);
	};
	Loop(Idle, Set.Idle);
	Loop(IdleAiming, Set.IdleAiming);
	Loop(WalkForward, Set.WalkForward);
	Loop(WalkBackward, Set.WalkBackward);
	Loop(WalkLeft, Set.WalkLeft);
	Loop(WalkRight, Set.WalkRight);
	Loop(JogForward, Set.JogForward);
	Loop(JogBackward, Set.JogBackward);
	Loop(JogLeft, Set.JogLeft);
	Loop(JogRight, Set.JogRight);
	Loop(CrouchIdle, Set.CrouchIdle);
	Loop(FallLoop, Set.FallLoop);
	CrouchWalk.SetBlendSpace(Set.CrouchWalk);
	CrouchWalk.SetLoop(true);
	AimHip.SetBlendSpace(Set.AimHip);
	AimAiming.SetBlendSpace(Set.AimAiming);
	AimCrouch.SetBlendSpace(Set.AimCrouch);

	// The moving cycles: walking and jogging, each in four directions.
	FAnimNode_SequencePlayer_Standalone* Moving[] = {&WalkForward, &WalkBackward, &WalkLeft, &WalkRight, &JogForward, &JogBackward, &JogLeft, &JogRight};
	for (FAnimNode_SequencePlayer_Standalone* Cycle : Moving)
	{
		// AddPose gives the count, not the index.
		const int32 Index = Cycles.AddPose() - 1;
		Cycles.Poses[Index].SetLinkNode(Cycle);
	}
	StandingStill.A.SetLinkNode(&Idle);
	StandingStill.B.SetLinkNode(&IdleAiming);
	Standing.A.SetLinkNode(&StandingStill);
	Standing.B.SetLinkNode(&Cycles);
	Crouched.A.SetLinkNode(&CrouchIdle);
	Crouched.B.SetLinkNode(&CrouchWalk);
	OnGround.A.SetLinkNode(&Standing);
	OnGround.B.SetLinkNode(&Crouched);
	Body.A.SetLinkNode(&OnGround);
	Body.B.SetLinkNode(&FallLoop);
	AimedFromHip.Base.SetLinkNode(&Body);
	AimedFromHip.Additive.SetLinkNode(&AimHip);
	AimedDownSights.Base.SetLinkNode(&AimedFromHip);
	AimedDownSights.Additive.SetLinkNode(&AimAiming);
	AimedCrouched.Base.SetLinkNode(&AimedDownSights);
	AimedCrouched.Additive.SetLinkNode(&AimCrouch);
	Slot.SlotName = UCheeseStrikeAnimInstance::SlotName();
	Slot.Source.SetLinkNode(&AimedCrouched);

	// The left hand keeps its grip on the rifle, which rides the weapon_r bone in the right hand.
	LeftHand.IKBone = FBoneReference(FName(TEXT("hand_l")));
	LeftHand.AnchorBone = FBoneReference(FName(TEXT("weapon_r")));
	LeftHand.bAllowStretching = false;
	LeftHand.Alpha = 1.0f;
	ToComponentSpace.LocalPose.SetLinkNode(&Slot);
	LeftHand.ComponentPose.SetLinkNode(&ToComponentSpace);
	ToLocalSpace.ComponentPose.SetLinkNode(&LeftHand);
	bLinked = true;
}

void FCheeseStrikeAnimProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	if (const UCheeseStrikeAnimInstance* Instance = Cast<UCheeseStrikeAnimInstance>(InAnimInstance))
	{
		if (!bLinked)
		{
			Link(Instance->GetSet());
		}
		ToLocalSpace.Initialize_AnyThread(FAnimationInitializeContext(this));
	}
}

void FCheeseStrikeAnimProxy::CacheBones()
{
	FAnimInstanceProxy::CacheBones();
	if (bLinked)
	{
		ToLocalSpace.CacheBones_AnyThread(FAnimationCacheBonesContext(this));
	}
}

void FCheeseStrikeAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UCheeseStrikeAnimInstance* Instance = Cast<UCheeseStrikeAnimInstance>(InAnimInstance))
	{
		Speed = Instance->Speed;
		Direction = Instance->Direction;
		AimPitch = Instance->AimPitch;
		bCrouched = Instance->bCrouched;
		bFalling = Instance->bFalling;
		bAiming = Instance->bAiming;
	}
}

void FCheeseStrikeAnimProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);
	if (!bLinked)
	{
		return;
	}
	using namespace CheeseStrikeAnimation;
	CrouchBlend = FMath::FInterpTo(CrouchBlend, bCrouched ? 1.0f : 0.0f, DeltaSeconds, BlendRate);
	AirBlend = FMath::FInterpTo(AirBlend, bFalling ? 1.0f : 0.0f, DeltaSeconds, BlendRate);
	AimBlend = FMath::FInterpTo(AimBlend, bAiming ? 1.0f : 0.0f, DeltaSeconds, BlendRate);

	float Forward, Backward, Left, Right;
	DirectionWeights(Direction, Forward, Backward, Left, Right);
	const float Jog = JogWeight(Speed);
	const float Ways[] = {Forward, Backward, Left, Right};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Cycles.DesiredAlphas[Index] = Ways[Index] * (1.0f - Jog);
		Cycles.DesiredAlphas[Index + 4] = Ways[Index] * Jog;
	}
	StandingStill.Alpha = AimBlend;
	Standing.Alpha = MovingWeight(Speed);
	Crouched.Alpha = MovingWeight(Speed);
	CrouchWalk.SetPosition(FVector(Direction, 0.0, 0.0));
	OnGround.Alpha = CrouchBlend;
	Body.Alpha = AirBlend;

	// The aim offsets: yaw is the body's own turning, so only the pitch applies.
	const FVector Aim(0.0, AimPitch, 0.0);
	AimHip.SetPosition(Aim);
	AimAiming.SetPosition(Aim);
	AimCrouch.SetPosition(Aim);
	AimedCrouched.Alpha = CrouchBlend;
	AimedDownSights.Alpha = (1.0f - CrouchBlend) * AimBlend;
	AimedFromHip.Alpha = (1.0f - CrouchBlend) * (1.0f - AimBlend);
	// A montage in the slot (the reload) moves the left hand itself; the grip takes over as it fades.
	float SlotWeight = 0.0f;
	float SourceWeight = 1.0f;
	float TotalWeight = 0.0f;
	GetSlotWeight(UCheeseStrikeAnimInstance::SlotName(), SlotWeight, SourceWeight, TotalWeight);
	LeftHand.Alpha = SourceWeight;
}

void FCheeseStrikeAnimProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	if (bLinked)
	{
		ToLocalSpace.Update_AnyThread(InContext);
	}
}

bool FCheeseStrikeAnimProxy::Evaluate(FPoseContext& Output)
{
	if (!bLinked)
	{
		return false;
	}
	ToLocalSpace.Evaluate_AnyThread(Output);
	return true;
}
