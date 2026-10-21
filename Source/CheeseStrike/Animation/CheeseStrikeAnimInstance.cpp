// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrikeBodies.h"
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
	const FString Mannequin = TEXT("/Game/Characters/Heroes/Mannequin/Animations/");
	Sets.Add(FindSet(Mannequin + TEXT("Locomotion/Rifle/"), Mannequin + TEXT("AimOffsets/"), Mannequin + TEXT("Actions/")));
	Set = Sets[0];
}

UCheeseStrikeHeroAnimInstance::UCheeseStrikeHeroAnimInstance()
{
	Sets.Reset();
	for (const CheeseStrikeBodies::FBody& Body : CheeseStrikeBodies::All())
	{
		Sets.Add(FindSet(Body.Animations, Body.Animations, Body.Animations));
	}
	Set = Sets.IsEmpty() ? FCheeseStrikeAnimSet() : Sets[0];
	GripAnchor = TEXT("hand_r");
}

void UCheeseStrikeAnimInstance::PickSet()
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const USkeleton* Skeleton = Mesh != nullptr && Mesh->GetSkeletalMeshAsset() != nullptr ? Mesh->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
	for (const FCheeseStrikeAnimSet& Each : Sets)
	{
		if (Each.Idle != nullptr && Each.Idle->GetSkeleton() == Skeleton)
		{
			Set = Each;
			return;
		}
	}
}

FCheeseStrikeAnimSet UCheeseStrikeAnimInstance::FindSet(const FString& Locomotion, const FString& AimOffsets, const FString& Actions)
{
	FCheeseStrikeAnimSet Found;
	const auto Path = [](const FString& Folder, const TCHAR* Name) { return Folder + Name + TEXT(".") + Name; };
	Found.Idle = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Idle_Hipfire")));
	Found.IdleAiming = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Idle_ADS")));
	Found.WalkForward = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Walk_Fwd")));
	Found.WalkBackward = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Walk_Bwd")));
	Found.WalkLeft = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Walk_Left")));
	Found.WalkRight = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Walk_Right")));
	Found.JogForward = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Jog_Fwd")));
	Found.JogBackward = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Jog_Bwd")));
	Found.JogLeft = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Jog_Left")));
	Found.JogRight = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Jog_Right")));
	Found.CrouchIdle = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Crouch_Idle")));
	Found.CrouchWalk = Find<UBlendSpace>(*Path(Locomotion, TEXT("BS_MM_Rifle_Crouch_Walk")));
	Found.FallLoop = Find<UAnimSequence>(*Path(Locomotion, TEXT("MM_Rifle_Jump_Fall_Loop")));
	Found.AimHip = Find<UBlendSpace>(*Path(AimOffsets, TEXT("AO_MM_Rifle_Idle_Hipfire")));
	Found.AimAiming = Find<UBlendSpace>(*Path(AimOffsets, TEXT("AO_MM_Rifle_Idle_ADS")));
	Found.AimCrouch = Find<UBlendSpace>(*Path(AimOffsets, TEXT("AO_MM_Rifle_Crouch_Idle")));
	Found.Fire = Find<UAnimSequence>(*Path(Actions, TEXT("MM_Rifle_Fire")));
	Found.Reload = Find<UAnimSequence>(*Path(Actions, TEXT("MM_Rifle_Reload")));
	return Found;
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
	// Where the player looks, as every machine has it: the controls' for its own (which the view follows in play, but not
	// when it is moved outside the body to look at it), the replicated view pitch for others'.
	AimPitch = FRotator::NormalizeAxis(Pawn->IsLocallyControlled() ? Pawn->GetControlRotation().Pitch : Pawn->GetBaseAimRotation().Pitch);
	if (const ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(Pawn))
	{
		bCrouched = Character->bIsCrouched;
		bFalling = Character->GetCharacterMovement() != nullptr && Character->GetCharacterMovement()->IsFalling();
		bAiming = Character->GetWeapon() != nullptr && Character->GetWeapon()->IsAimed();
	}
}

void FCheeseStrikeAnimProxy::Link(const FCheeseStrikeAnimSet& Set, FName GripAnchor)
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
	Locomotion.Shared = &AimedCrouched;
	Slot.Source.SetLinkNode(&Locomotion);
	UpperBody.BasePose.SetLinkNode(&AimedCrouched);
	UpperBody.AddPose();
	UpperBody.BlendPoses[0].SetLinkNode(&Slot);
	FBranchFilter Spine;
	Spine.BoneName = FName(TEXT("spine_01"));
	Spine.BlendDepth = 0;
	UpperBody.LayerSetup[0].BranchFilters.Add(Spine);
	UpperBody.bMeshSpaceRotationBlend = true;

	// The left hand keeps its grip on the rifle, which rides the weapon_r bone in the right hand.
	LeftHand.IKBone = FBoneReference(FName(TEXT("hand_l")));
	LeftHand.AnchorBone = FBoneReference(GripAnchor);
	LeftHand.bAllowStretching = false;
	LeftHand.Alpha = 1.0f;
	ToComponentSpace.LocalPose.SetLinkNode(&UpperBody);
	LeftHand.ComponentPose.SetLinkNode(&ToComponentSpace);
	ToLocalSpace.ComponentPose.SetLinkNode(&LeftHand);
	bLinked = true;
}

void FCheeseStrikeAnimProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	if (UCheeseStrikeAnimInstance* Instance = Cast<UCheeseStrikeAnimInstance>(InAnimInstance))
	{
		if (!bLinked)
		{
			Instance->PickSet();
			Link(Instance->GetSet(), Instance->GetGripAnchor());
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
	// The bones the mesh's reference pose scales (Belica's face hangs from faceAttach at 0.79), which the animations,
	// retargeted ones especially, would put back to 1.
	ScaledBones.Reset();
	const FBoneContainer& Bones = GetRequiredBones();
	if (Bones.IsValid())
	{
		for (int32 Each = 0; Each < Bones.GetCompactPoseNumBones(); ++Each)
		{
			const FCompactPoseBoneIndex Index(Each);
			const FVector Scale = Bones.GetRefPoseTransform(Index).GetScale3D();
			if (!Scale.Equals(FVector::OneVector, 1.0e-3))
			{
				ScaledBones.Add(TPair<FCompactPoseBoneIndex, FVector>(Index, Scale));
			}
		}
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
	for (const TPair<FCompactPoseBoneIndex, FVector>& Bone : ScaledBones)
	{
		Output.Pose[Bone.Key].SetScale3D(Bone.Value);
	}
	return true;
}
