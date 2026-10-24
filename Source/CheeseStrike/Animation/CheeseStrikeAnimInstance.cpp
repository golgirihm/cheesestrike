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
#include "Modes/CheeseStrikeLastStand.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "Weapons/CheeseStrikeWeapon.h"
#include "Weapons/CheeseStrikeWeaponLooks.h"

static TAutoConsoleVariable<int32> CVarHipHold(TEXT("CheeseStrike.HipHold"), 1, TEXT("Place the weapon hand in its player's view where its hold has it (0: where the animation puts it)."));

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
	// The knife in the pistol's stance (its hand placed and swiped in code: FCheeseStrikeKnifeHold), the free arm up on
	// guard as the unarmed stand ready.
	FCheeseStrikeAnimSet& Knife = Found.Of(ECheeseStrikeHold::Knife);
	Knife = Pistol;
	Knife.Guard = Find<UAnimSequence>(Pistols, TEXT("MM_Unarmed_Idle_Ready"));
	// A grenade is tossed the one way, whatever is in the other hand.
	UAnimSequence* Toss = Find<UAnimSequence>(Animations + TEXT("Grenade/"), TEXT("MM_Rifle_GrenadeToss"));
	for (FCheeseStrikeAnimSet& Set : Found.Holds)
	{
		Set.Throw = Toss;
	}
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
		// Both bodies, the one others see and its player's own, are animated in the frame of the first, turned to face the
		// capsule's front, its root on the capsule's floor.
		if (const USkeletalMeshComponent* Main = Character->GetMesh())
		{
			const FTransform Placed = Main->GetRelativeTransform();
			BodyForward = Placed.GetRotation().UnrotateVector(FVector::ForwardVector);
			BodyOrigin = Placed.InverseTransformPosition(FVector(0.0, 0.0, Placed.GetLocation().Z));
		}
		// Down: where it aims within its cone, the room for its legs, and how it scoots and turns round.
		DownAimYaw = Character->GetDownAimYaw();
		bFirstPersonBody = Character->GetFirstPersonMesh() == GetSkelMeshComponent();
		LegRoom = Character->GetDownLegRoom();
		ScootSpeed = FVector::DotProduct(Velocity, Pawn->GetActorForwardVector());
		const float Yaw = Pawn->GetActorRotation().Yaw;
		TurnSpeed = bLastYawKnown && DeltaSeconds > 0.0f ? FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / DeltaSeconds : 0.0f;
		LastYaw = Yaw;
		bLastYawKnown = true;
		Hold = Weapon != nullptr ? Weapon->GetHold() : ECheeseStrikeHold::Rifle;
		bLeftHandHolds = Weapon != nullptr && Weapon->LeftHandHoldFor(GetSkelMeshComponent(), LeftHandHold);
		bDual = Weapon != nullptr && Weapon->GetStats().bDual && !bDowned;
		// The weapon's model as this body holds it, and the knife's swipe.
		const UPrimitiveComponent* Model = Weapon != nullptr ? Weapon->GetModel(bFirstPersonBody) : nullptr;
		ModelInHand = Model != nullptr ? Model->GetRelativeTransform() : FTransform::Identity;
		ModelAlong = Weapon != nullptr ? Weapon->GetModelAlong() : FVector(0.0, 1.0, 0.0);
		SwipePhase = Weapon != nullptr ? Weapon->SwipePhase() : -1.0f;
		// Its own player's body: the arms placed in view.
		View.bPlace = bFirstPersonBody && Character->IsLocallyControlled() && Character->IsAlive() && Weapon != nullptr && CVarHipHold.GetValueOnGameThread() != 0;
		if (View.bPlace)
		{
			View.Turn = GetSkelMeshComponent()->GetComponentQuat().Inverse() * Pawn->GetControlRotation().Quaternion();
			View.CameraAtHead = Character->GetCameraAtHead();
			View.bHasSight = Character->WeaponSight(View.Sight);
			View.Aimed = Character->GetAimFraction();
			View.Lowered = FMath::Square(1.0f - Weapon->DrawnFraction());
			const UCheeseStrikeMovementComponent* Movement = Character->GetCheeseStrikeMovement();
			View.bSprinting = Movement != nullptr && Movement->IsSprinting();
		}
		// An execution: where the blow lands on the one down, taken as it begins, in the frame the body is animated in.
		const UCheeseStrikeLastStand* Stand = Character->GetLastStand();
		const FCheeseStrikeExecutionState* Execution = Stand != nullptr ? &Stand->GetExecution() : nullptr;
		ExecutionKind = Execution != nullptr && Character->IsAlive() ? Execution->Kind : 0;
		if (ExecutionKind != 0)
		{
			bExecutor = Execution->bExecutor;
			ExecutionTime = Stand->ExecutionElapsed();
			ExecutionSide = Execution->Side >= 0 ? 1.0f : -1.0f;
			const ACheeseStrikeCharacter* Down = Execution->Other;
			const USkeletalMeshComponent* Main = Character->GetMesh();
			if (bExecutor && Down != nullptr && Main != nullptr && ExecutionTakenFor != Execution->StartedAt)
			{
				ExecutionTakenFor = Execution->StartedAt;
				const bool bStomp = Execution->GetKind() == CheeseStrikeExecution::EKind::Stomp;
				const FVector Hit = Down->GetMesh()->GetBoneLocation(bStomp ? TEXT("head") : TEXT("spine_03"));
				const FVector Hips = Down->GetMesh()->GetBoneLocation(TEXT("pelvis"));
				const FVector Shoulder = Down->GetMesh()->GetBoneLocation(ExecutionSide > 0.0f ? TEXT("upperarm_r") : TEXT("upperarm_l"));
				// The knife goes in a little below the top of the chest.
				ExecutionTargetWorld = bStomp ? Hit : FMath::Lerp(Hit, Hips, 0.2f);
				ExecutionShoulderWorld = Shoulder;
			}
			if (Main != nullptr)
			{
				// Kept where they were in the world as the executor is put in its place.
				ExecutionTarget = Main->GetComponentTransform().InverseTransformPosition(ExecutionTargetWorld);
				ExecutionShoulder = Main->GetComponentTransform().InverseTransformPosition(ExecutionShoulderWorld);
			}
		}
		else
		{
			bExecutor = false;
			ExecutionTime = 0.0f;
		}
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
	// The left hand's grip on the weapon, measured afresh from this set's idle.
	LeftHand.GripPose = Set.Idle;
	LeftHand.bGripMeasured = false;
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

	// Posed in code: a slide's sprawl, the knife's fist, guard and swipe, and the weapon hand in its own player's view.
	ToComponentSpace.LocalPose.SetLinkNode(&Guarded);
	SlidePose.ComponentPose.SetLinkNode(&ToComponentSpace);
	KnifeHold.ComponentPose.SetLinkNode(&SlidePose);
	ViewHold.ComponentPose.SetLinkNode(&KnifeHold);
	// The left hand keeps its grip on the weapon, which rides the anchor bone in the right hand.
	LeftHand.IKBone = FBoneReference(FName(TEXT("hand_l")));
	LeftHand.AnchorBone = FBoneReference(GripAnchor);
	LeftHand.Alpha = 1.0f;
	// Two guns, one in each hand: the hands apart, the left the mirror of the right.
	DualHold.ComponentPose.SetLinkNode(&ViewHold);
	LeftHand.ComponentPose.SetLinkNode(&DualHold);
	// Last, down: on the back, propped on the left elbow, the pistol out.
	Downed.ComponentPose.SetLinkNode(&LeftHand);
	Executing.ComponentPose.SetLinkNode(&Downed);
	ToLocalSpace.ComponentPose.SetLinkNode(&Executing);
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
		BodyOrigin = Instance->BodyOrigin;
		DownAimYaw = Instance->DownAimYaw;
		LegRoom = Instance->LegRoom;
		ScootSpeed = Instance->ScootSpeed;
		TurnSpeed = Instance->TurnSpeed;
		bFirstPersonBody = Instance->bFirstPersonBody;
		Hold = Instance->Hold;
		ExecutionKind = Instance->ExecutionKind;
		bExecutor = Instance->bExecutor;
		ExecutionTime = Instance->ExecutionTime;
		ExecutionTarget = Instance->ExecutionTarget;
		ExecutionShoulder = Instance->ExecutionShoulder;
		ExecutionSide = Instance->ExecutionSide;
		bLeftHandHolds = Instance->bLeftHandHolds;
		bDual = Instance->bDual;
		LeftHandHold = Instance->LeftHandHold;
		ModelInHand = Instance->ModelInHand;
		ModelAlong = Instance->ModelAlong;
		SwipePhase = Instance->SwipePhase;
		View = Instance->View;
		// A change of weapon to one held another way: its animations on the same nodes (and the left hand's grip on it,
		// measured from the new set's idle).
		if (bLinked && Hold != LinkedHold)
		{
			Assign(Instance->GetSet(Hold));
			LinkedHold = Hold;
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

	// Going down and getting up take a moment each, eased at both ends.
	DownLinear = FMath::FInterpConstantTo(DownLinear, bDowned ? 1.0f : 0.0f, DeltaSeconds, 1.0f / (bDowned ? CheeseStrikeDowned::Seconds : CheeseStrikeDowned::RiseSeconds));
	Downed.Alpha = FMath::SmoothStep(0.0f, 1.0f, DownLinear);

	// The aim offsets: yaw is the body's own turning, so only the pitch applies; down, the pose aims the pistol itself.
	const FVector Aim(0.0, AimPitch * (1.0f - Downed.Alpha), 0.0);
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
	// The knife's free arm goes up on guard and lets go of the grip, as does a hand a weapon held in one hand leaves free.
	const bool bKnife = Hold == ECheeseStrikeHold::Knife;
	GuardBlend = FMath::FInterpTo(GuardBlend, bKnife ? 1.0f : 0.0f, DeltaSeconds, BlendRate);
	Guarded.BlendWeights[0] = GuardBlend;
	TwoHandBlend = FMath::FInterpTo(TwoHandBlend, bLeftHandHolds ? 1.0f : 0.0f, DeltaSeconds, BlendRate);
	LeftHand.Alpha = SourceWeight * (1.0f - GuardBlend) * TwoHandBlend;
	LeftHand.bHasHold = bLeftHandHolds;
	LeftHand.Hold = LeftHandHold;
	// Two guns: held apart and mirrored, except while a montage (a reload) has the hands.
	DualBlend = FMath::FInterpTo(DualBlend, bDual ? 1.0f : 0.0f, DeltaSeconds, BlendRate);
	DualHold.Alpha = DualBlend * SourceWeight;
	DualHold.Spread = bFirstPersonBody ? CheeseStrikeDual::ViewSpread : CheeseStrikeDual::Spread;
	DualHold.Forward = BodyForward;
	DualHold.Origin = BodyOrigin;

	// Down, the legs push in turn as the body scoots along or round (a push a stride, the phase running on as it goes
	// toward the head, back as it goes toward the feet); the breath comes hard, all the while (so that it doesn't start
	// from nothing).
	const float TurnScoot = FMath::Abs(FMath::DegreesToRadians(TurnSpeed)) * 40.0f;
	const float Pushing = FMath::Abs(ScootSpeed) > 8.0f ? 1.0f : (TurnScoot > 8.0f ? CheeseStrikeDowned::TurnPush : 0.0f);
	ScootBlend = FMath::FInterpTo(ScootBlend, Pushing, DeltaSeconds, CheeseStrikeDowned::ScootBlendRate);
	ScootPhase = FMath::Fmod(ScootPhase + DeltaSeconds * UE_PI * (TurnScoot - ScootSpeed) / CheeseStrikeDowned::ScootStride, UE_TWO_PI * 100.0f);
	BreathPhase = FMath::Fmod(BreathPhase + DeltaSeconds * UE_TWO_PI * CheeseStrikeDowned::BreathsPerMinute / 60.0f, UE_TWO_PI * 100.0f);
	Downed.Breath = CheeseStrikeDowned::Breath(BreathPhase);
	// A montage (the reload) takes the arms, as it takes the left hand off the grip.
	Downed.ArmsOwn = SourceWeight;
	Downed.Forward = BodyForward;
	Downed.Origin = BodyOrigin;
	Downed.AimYaw = DownAimYaw;
	Downed.AimPitch = FMath::Clamp(AimPitch, CheeseStrikeDowned::LowestPitch, CheeseStrikeDowned::HighestPitch);
	Downed.LegReach = CheeseStrikeDowned::LegReach(LegRoom);
	Downed.Scoot = ScootBlend;
	Downed.ScootPhase = ScootPhase;
	Downed.bFirstPerson = bFirstPersonBody;
	Executing.Kind = CheeseStrikeExecution::KindAt(ExecutionKind);
	Executing.bExecutor = bExecutor;
	Executing.Time = ExecutionTime;
	Executing.Forward = BodyForward;
	Executing.Origin = BodyOrigin;
	Executing.Target = ExecutionTarget;
	Executing.TargetShoulder = ExecutionShoulder;
	Executing.Side = ExecutionSide;

	// Sliding (crouched, faster than a crouched walk: any machine can tell), the body laid back with its legs out. None of
	// what follows while down, nor in an execution, which poses the body itself.
	const float Up = (1.0f - Downed.Alpha) * (ExecutionKind != 0 ? 0.0f : 1.0f);
	const float SlideWanted = CheeseStrikeArms::LooksSliding(bCrouched, Speed) && !bFalling ? CheeseStrikeArms::SlideDepth(Speed) : 0.0f;
	SlideBlend = FMath::FInterpTo(SlideBlend, SlideWanted, DeltaSeconds, CheeseStrikeArms::HoldBlendRate);
	SlidePose.Alpha = SlideBlend * Up;
	SlidePose.Forward = BodyForward;
	SlidePose.Origin = BodyOrigin;
	// The knife: the fist closed, and on the body others see, the hand on guard and swiping (a montage, the draw, has it first).
	KnifeBlend = FMath::FInterpTo(KnifeBlend, bKnife ? 1.0f : 0.0f, DeltaSeconds, BlendRate * 2.0f);
	KnifeHold.Alpha = (bKnife ? KnifeBlend : 0.0f) * Up * SourceWeight;
	KnifeHold.bPlaceHand = !bFirstPersonBody;
	KnifeHold.Forward = BodyForward;
	KnifeHold.AimPitch = AimPitch;
	KnifeHold.ModelInHand = ModelInHand;
	KnifeHold.ModelAlong = ModelAlong;
	KnifeHold.SwipePhase = SwipePhase;
	// Its own player's arms, placed in view.
	SprintBlend = FMath::FInterpTo(SprintBlend, View.bSprinting ? 1.0f : 0.0f, DeltaSeconds, CheeseStrikeArms::HoldBlendRate);
	ViewHold.Alpha = View.bPlace ? Up : 0.0f;
	ViewHold.View = View.Turn;
	ViewHold.CameraAtHead = View.CameraAtHead;
	ViewHold.ModelInHand = ModelInHand;
	ViewHold.ModelAlong = ModelAlong;
	ViewHold.Sight = View.Sight;
	ViewHold.bHasSight = View.bHasSight;
	ViewHold.HipFromAimed = CheeseStrikeWeaponLooks::HipFromAimed(Hold);
	ViewHold.KnifeHand = CheeseStrikeWeaponLooks::KnifeHand();
	ViewHold.Aimed = View.Aimed;
	ViewHold.Lowered = View.Lowered;
	ViewHold.Sprint = SprintBlend;
	ViewHold.Slide = SlideBlend;
	ViewHold.bKnife = bKnife;
	ViewHold.SwipePhase = SwipePhase;
	ViewHold.Montage = 1.0f - SourceWeight;
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
