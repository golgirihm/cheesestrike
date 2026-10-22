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
		&& CrouchIdle && CrouchWalkForward && CrouchWalkBackward && CrouchWalkLeft && CrouchWalkRight && FallLoop && AimHip && AimAiming && AimCrouch && Fire && Reload;
}

namespace
{
	template <typename T>
	TObjectPtr<T> Find(const FString& Folder, const FString& Name)
	{
		ConstructorHelpers::FObjectFinder<T> Finder(*(Folder + Name + TEXT(".") + Name));
		return Finder.Object;
	}
}

UCheeseStrikeAnimInstance::UCheeseStrikeAnimInstance()
{
	// Lyra's rifle set for the mannequin (Content/LICENSES.md).
	const FString Mannequin = TEXT("/Game/Characters/Heroes/Mannequin/Animations/");
	FCheeseStrikeAnimSets Rifle;
	Rifle.Of(ECheeseStrikeHold::Rifle) = FindSet(TEXT("Rifle"), Mannequin + TEXT("Locomotion/Rifle/"), Mannequin + TEXT("AimOffsets/"), Mannequin + TEXT("Actions/"));
	Sets.Add(Rifle);
	Picked = Sets[0];
}

UCheeseStrikeHeroAnimInstance::UCheeseStrikeHeroAnimInstance()
{
	Sets.Reset();
	for (const CheeseStrikeBodies::FBody& Body : CheeseStrikeBodies::All())
	{
		Sets.Add(FindSets(Body.Animations));
	}
	Picked = Sets.IsEmpty() ? FCheeseStrikeAnimSets() : Sets[0];
	GripAnchor = TEXT("hand_r");
}

void UCheeseStrikeAnimInstance::PickSet()
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const USkeleton* Skeleton = Mesh != nullptr && Mesh->GetSkeletalMeshAsset() != nullptr ? Mesh->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
	for (const FCheeseStrikeAnimSets& Each : Sets)
	{
		const FCheeseStrikeAnimSet& Rifle = Each.For(ECheeseStrikeHold::Rifle);
		if (Rifle.Idle != nullptr && Rifle.Idle->GetSkeleton() == Skeleton)
		{
			Picked = Each;
			return;
		}
	}
}

const FCheeseStrikeAnimSet& FCheeseStrikeAnimSets::For(ECheeseStrikeHold Hold) const
{
	const FCheeseStrikeAnimSet& Wanted = Holds[static_cast<uint8>(Hold)];
	return Wanted.IsComplete() ? Wanted : Holds[static_cast<uint8>(ECheeseStrikeHold::Rifle)];
}

FCheeseStrikeAnimSets UCheeseStrikeAnimInstance::FindSets(const FString& Rifles)
{
	// <hero>/Animations/Rifle/, and beside it Pistol/ and Shotgun/.
	const FString Animations = Rifles.EndsWith(TEXT("Rifle/")) ? Rifles.LeftChop(6) : Rifles;
	const FString Pistols = Animations + TEXT("Pistol/");
	const FString Shotguns = Animations + TEXT("Shotgun/");
	FCheeseStrikeAnimSets Found;
	FCheeseStrikeAnimSet& Rifle = Found.Of(ECheeseStrikeHold::Rifle);
	Rifle = FindSet(TEXT("Rifle"), Rifles, Rifles, Rifles);
	Rifle.Equip = Find<UAnimSequence>(Rifles, TEXT("MM_Rifle_Equip"));
	FCheeseStrikeAnimSet& Pistol = Found.Of(ECheeseStrikeHold::Pistol);
	Pistol = FindSet(TEXT("Pistol"), Pistols, Pistols, Pistols);
	Pistol.Equip = Find<UAnimSequence>(Pistols, TEXT("MM_Pistol_Equip"));
	// The shotgun is held as the rifle is, and fired and reloaded its own way.
	FCheeseStrikeAnimSet& Shotgun = Found.Of(ECheeseStrikeHold::Shotgun);
	Shotgun = Rifle;
	Shotgun.Fire = Find<UAnimSequence>(Shotguns, TEXT("MM_Shotgun_Fire"));
	Shotgun.Reload = Find<UAnimSequence>(Shotguns, TEXT("MM_Shotgun_Reload"));
	// The knife in the pistol's stance, struck with the pistol's blow, the free arm up on guard as the unarmed stand ready.
	FCheeseStrikeAnimSet& Knife = Found.Of(ECheeseStrikeHold::Knife);
	Knife = Pistol;
	Knife.Stroke = Find<UAnimSequence>(Pistols, TEXT("MM_Pistol_Melee"));
	Knife.Guard = Find<UAnimSequence>(Pistols, TEXT("MM_Unarmed_Idle_Ready"));
	return Found;
}

FCheeseStrikeAnimSet UCheeseStrikeAnimInstance::FindSet(const TCHAR* Kind, const FString& Locomotion, const FString& AimOffsets, const FString& Actions)
{
	const FString Prefix = FString(TEXT("MM_")) + Kind + TEXT("_");
	FCheeseStrikeAnimSet Set;
	Set.Idle = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Idle_Hipfire"));
	Set.IdleAiming = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Idle_ADS"));
	Set.WalkForward = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Walk_Fwd"));
	Set.WalkBackward = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Walk_Bwd"));
	Set.WalkLeft = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Walk_Left"));
	Set.WalkRight = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Walk_Right"));
	Set.JogForward = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Jog_Fwd"));
	Set.JogBackward = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Jog_Bwd"));
	Set.JogLeft = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Jog_Left"));
	Set.JogRight = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Jog_Right"));
	Set.CrouchIdle = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Crouch_Idle"));
	Set.CrouchWalkForward = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Crouch_Walk_Fwd"));
	Set.CrouchWalkBackward = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Crouch_Walk_Bwd"));
	Set.CrouchWalkLeft = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Crouch_Walk_Left"));
	Set.CrouchWalkRight = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Crouch_Walk_Right"));
	Set.FallLoop = Find<UAnimSequence>(Locomotion, Prefix + TEXT("Jump_Fall_Loop"));
	if (FCString::Strcmp(Kind, TEXT("Pistol")) == 0)
	{
		// Lyra's pistol has one aim offset, down the sights, for every stance.
		Set.AimHip = Find<UBlendSpace>(AimOffsets, TEXT("AO_MM_Pistol_Idle_ADS"));
		Set.AimAiming = Set.AimHip;
		Set.AimCrouch = Set.AimHip;
	}
	else
	{
		Set.AimHip = Find<UBlendSpace>(AimOffsets, TEXT("AO_") + Prefix + TEXT("Idle_Hipfire"));
		Set.AimAiming = Find<UBlendSpace>(AimOffsets, TEXT("AO_") + Prefix + TEXT("Idle_ADS"));
		Set.AimCrouch = Find<UBlendSpace>(AimOffsets, TEXT("AO_") + Prefix + TEXT("Crouch_Idle"));
	}
	Set.Fire = Find<UAnimSequence>(Actions, Prefix + TEXT("Fire"));
	Set.Reload = Find<UAnimSequence>(Actions, Prefix + TEXT("Reload"));
	return Set;
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
		const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
		bAiming = Weapon != nullptr && Weapon->IsAimed();
		bDowned = Character->IsAlive() && Character->IsDowned();
		// Both bodies, the one others see and its player's own, are animated in the frame of the first, turned to face the capsule's front.
		if (const USkeletalMeshComponent* Main = Character->GetMesh())
		{
			BodyForward = Main->GetRelativeRotation().UnrotateVector(FVector::ForwardVector);
		}
		Hold = Weapon != nullptr ? Weapon->GetHold() : ECheeseStrikeHold::Rifle;
		LeftHandShift = Weapon != nullptr ? Weapon->LeftHandShiftFor(GetSkelMeshComponent()) : FVector::ZeroVector;
	}
}

void FCheeseStrikeAnimProxy::Assign(const FCheeseStrikeAnimSet& Set)
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
	Loop(CrouchWalkForward, Set.CrouchWalkForward);
	Loop(CrouchWalkBackward, Set.CrouchWalkBackward);
	Loop(CrouchWalkLeft, Set.CrouchWalkLeft);
	Loop(CrouchWalkRight, Set.CrouchWalkRight);
	Loop(FallLoop, Set.FallLoop);
	Loop(GuardPose, Set.Guard != nullptr ? Set.Guard.Get() : Set.Idle.Get());
	AimHip.SetBlendSpace(Set.AimHip);
	AimAiming.SetBlendSpace(Set.AimAiming);
	AimCrouch.SetBlendSpace(Set.AimCrouch);
}

void FCheeseStrikeAnimProxy::Link(FName GripAnchor)
{
	// The moving cycles: walking and jogging, each in four directions; and crouched, in four.
	FAnimNode_SequencePlayer_Standalone* Moving[] = {&WalkForward, &WalkBackward, &WalkLeft, &WalkRight, &JogForward, &JogBackward, &JogLeft, &JogRight};
	for (FAnimNode_SequencePlayer_Standalone* Cycle : Moving)
	{
		// AddPose gives the count, not the index.
		const int32 Index = Cycles.AddPose() - 1;
		Cycles.Poses[Index].SetLinkNode(Cycle);
	}
	FAnimNode_SequencePlayer_Standalone* Creeping[] = {&CrouchWalkForward, &CrouchWalkBackward, &CrouchWalkLeft, &CrouchWalkRight};
	for (FAnimNode_SequencePlayer_Standalone* Cycle : Creeping)
	{
		const int32 Index = CrouchCycles.AddPose() - 1;
		CrouchCycles.Poses[Index].SetLinkNode(Cycle);
	}
	StandingStill.A.SetLinkNode(&Idle);
	StandingStill.B.SetLinkNode(&IdleAiming);
	Standing.A.SetLinkNode(&StandingStill);
	Standing.B.SetLinkNode(&Cycles);
	Crouched.A.SetLinkNode(&CrouchIdle);
	Crouched.B.SetLinkNode(&CrouchCycles);
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
	// The knife's guard: the left arm, from the collarbone on, from the unarmed ready stance.
	Guarded.BasePose.SetLinkNode(&UpperBody);
	Guarded.AddPose();
	Guarded.BlendPoses[0].SetLinkNode(&GuardPose);
	FBranchFilter LeftArm;
	LeftArm.BoneName = FName(TEXT("clavicle_l"));
	LeftArm.BlendDepth = 0;
	Guarded.LayerSetup[0].BranchFilters.Add(LeftArm);
	Guarded.bMeshSpaceRotationBlend = true;
	Guarded.BlendWeights[0] = 0.0f;

	// The left hand keeps its grip on the rifle, which rides the weapon_r bone in the right hand.
	LeftHand.IKBone = FBoneReference(FName(TEXT("hand_l")));
	LeftHand.AnchorBone = FBoneReference(GripAnchor);
	LeftHand.bAllowStretching = false;
	LeftHand.Alpha = 1.0f;
	ToComponentSpace.LocalPose.SetLinkNode(&Guarded);
	LeftHand.ComponentPose.SetLinkNode(&ToComponentSpace);
	// Last, down: kneeling, hunched over, the left hand off the grip and on the belly.
	Downed.ComponentPose.SetLinkNode(&LeftHand);
	ToLocalSpace.ComponentPose.SetLinkNode(&Downed);
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
			Link(Instance->GetGripAnchor());
			Hold = Instance->Hold;
			LinkedHold = Hold;
			Assign(Instance->GetSet(Hold));
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
		bDowned = Instance->bDowned;
		BodyForward = Instance->BodyForward;
		Hold = Instance->Hold;
		LeftHandShift = Instance->LeftHandShift;
		// A change of weapon to one held another way: its animations on the same nodes, and the left hand's grip on it
		// measured again once nothing else moves it.
		if (bLinked && Hold != LinkedHold)
		{
			Assign(Instance->GetSet(Hold));
			LinkedHold = Hold;
			LeftHand.bGripMeasured = false;
			bGripPending = true;
		}
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
	for (int32 Index = 0; Index < 4; ++Index)
	{
		CrouchCycles.DesiredAlphas[Index] = Ways[Index];
	}
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
	// The knife's free arm goes up on guard and lets go of the grip.
	GuardBlend = FMath::FInterpTo(GuardBlend, Hold == ECheeseStrikeHold::Knife ? 1.0f : 0.0f, DeltaSeconds, BlendRate);
	Guarded.BlendWeights[0] = GuardBlend;
	if (bGripPending && SourceWeight > 0.999f)
	{
		bGripPending = false;
	}
	LeftHand.Alpha = bGripPending ? 0.0f : SourceWeight * (1.0f - GuardBlend);
	LeftHand.Shift = LeftHandShift;

	// Going down and getting up take a moment each, eased at both ends; the legs creep with the crouched walk while
	// moving and kneel while still; the breath comes hard, all the while (so that it doesn't start from nothing).
	DownLinear = FMath::FInterpConstantTo(DownLinear, bDowned ? 1.0f : 0.0f, DeltaSeconds, 1.0f / CheeseStrikeDowned::Seconds);
	KneelBlend = FMath::FInterpTo(KneelBlend, 1.0f - MovingWeight(Speed), DeltaSeconds, CheeseStrikeDowned::KneelRate);
	BreathPhase = FMath::Fmod(BreathPhase + DeltaSeconds * UE_TWO_PI * CheeseStrikeDowned::BreathsPerMinute / 60.0f, UE_TWO_PI * 100.0f);
	Downed.Alpha = FMath::SmoothStep(0.0f, 1.0f, DownLinear);
	Downed.Kneel = KneelBlend;
	Downed.Breath = CheeseStrikeDowned::Breath(BreathPhase);
	// A montage (the reload) takes the left hand off the belly, as it does off the grip.
	Downed.HandOnWound = SourceWeight;
	Downed.Forward = BodyForward;
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
