// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeArmsPose.h"
#include "Animation/AnimInstanceProxy.h"
#include "TwoBoneIK.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Weapons/CheeseStrikeGrenadeRules.h"

namespace CheeseStrikeArms
{
	float Follow(float Rate, float DeltaSeconds)
	{
		return DeltaSeconds > 0.0f ? 1.0f - FMath::Exp(-Rate * DeltaSeconds) : 0.0f;
	}

	FKnifeAt KnifeGuard()
	{
		FKnifeAt Guard;
		Guard.Blade = FVector(0.85f, -0.2f, 0.48f).GetSafeNormal();
		Guard.Spine = (FVector::UpVector - Guard.Blade * FVector::DotProduct(FVector::UpVector, Guard.Blade)).GetSafeNormal();
		return Guard;
	}

	namespace
	{
		/** One pose of the swipe: when (its phase), where the hand is from guard, and how far round the blade has swept (degrees about the up axis, right of ahead) and tipped up. */
		struct FSwipeKey
		{
			float Phase;
			FVector Hand;
			float Yaw;
			float Pitch;
		};
		// Raised back by the right shoulder, then swept down and across to the left, the edge leading, and back on guard.
		const FSwipeKey SwipeKeys[] = {
			{0.0f, FVector(0.0f, 0.0f, 0.0f), 0.0f, 0.0f},
			{0.2f, FVector(-6.0f, 14.0f, 14.0f), 70.0f, 20.0f},
			{0.38f, FVector(20.0f, -10.0f, 4.0f), 0.0f, 0.0f},
			{0.58f, FVector(12.0f, -34.0f, -12.0f), -75.0f, -15.0f},
			{1.0f, FVector(0.0f, 0.0f, 0.0f), 0.0f, 0.0f},
		};

		/** The blade and its spine swept `Yaw` round and tipped `Pitch` up, the edge leading leftward (the spine to the right of the blade). */
		void Swept(float Yaw, float Pitch, FVector& Blade, FVector& Spine)
		{
			const FQuat Turn = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)) * FQuat(FVector::RightVector, FMath::DegreesToRadians(-Pitch));
			Blade = Turn.RotateVector(FVector::ForwardVector);
			Spine = Turn.RotateVector(FVector::RightVector);
		}
	}

	FKnifeAt KnifeSwipe(float Phase, float Size)
	{
		const float At = FMath::Clamp(Phase, 0.0f, 1.0f);
		int32 Key = 0;
		while (Key < UE_ARRAY_COUNT(SwipeKeys) - 2 && At > SwipeKeys[Key + 1].Phase)
		{
			++Key;
		}
		const FSwipeKey& From = SwipeKeys[Key];
		const FSwipeKey& To = SwipeKeys[Key + 1];
		const float Along = FMath::SmoothStep(0.0f, 1.0f, (At - From.Phase) / FMath::Max(To.Phase - From.Phase, KINDA_SMALL_NUMBER));
		FKnifeAt Out;
		Out.Hand = FMath::Lerp(From.Hand, To.Hand, Along) * Size;
		// Out of guard and back into it the knife turns between the guard's and the swipe's; through the swipe it sweeps.
		const FKnifeAt Guard = KnifeGuard();
		FVector FromBlade, FromSpine, ToBlade, ToSpine;
		Swept(From.Yaw, From.Pitch, FromBlade, FromSpine);
		Swept(To.Yaw, To.Pitch, ToBlade, ToSpine);
		if (Key == 0)
		{
			FromBlade = Guard.Blade;
			FromSpine = Guard.Spine;
		}
		if (Key + 1 == UE_ARRAY_COUNT(SwipeKeys) - 1)
		{
			ToBlade = Guard.Blade;
			ToSpine = Guard.Spine;
		}
		const FQuat FromTurn = FRotationMatrix::MakeFromXZ(FromBlade, FromSpine).ToQuat();
		const FQuat ToTurn = FRotationMatrix::MakeFromXZ(ToBlade, ToSpine).ToQuat();
		const FQuat Turn = FQuat::Slerp(FromTurn, ToTurn, Along);
		Out.Blade = Turn.GetAxisX();
		Out.Spine = Turn.GetAxisZ();
		return Out;
	}

	bool LooksSliding(bool bCrouched, float Speed)
	{
		return bCrouched && Speed > CheeseStrikeMovement::WalkSpeed * CheeseStrikeMovement::CrouchFactor * 1.25f;
	}

	float SlideDepth(float Speed)
	{
		const float Crouched = CheeseStrikeMovement::WalkSpeed * CheeseStrikeMovement::CrouchFactor;
		return FMath::SmoothStep(Crouched * 1.25f, Crouched * 2.0f, Speed);
	}
}

namespace
{
	/** The turn of a model whose own `Along` and `Spine` should lie along `Blade` and `Up`. */
	FQuat ArmsLaid(const FVector& Along, const FVector& Spine, const FVector& Blade, const FVector& Up)
	{
		return FRotationMatrix::MakeFromXZ(Blade, Up).ToQuat() * FRotationMatrix::MakeFromXZ(Along, Spine).ToQuat().Inverse();
	}

	/** Puts an arm (upper, lower, hand) so that the hand is at `Target` turned `Turn`, bending towards `Pole`, stretching a little if it must. */
	void ArmsReach(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex End, const FVector& Target, const FQuat& Turn, const FVector& Pole)
	{
		FTransform UpperNow = Pose.GetComponentSpaceTransform(Upper);
		FTransform LowerNow = Pose.GetComponentSpaceTransform(Lower);
		FTransform EndNow = Pose.GetComponentSpaceTransform(End);
		AnimationCore::SolveTwoBoneIK(UpperNow, LowerNow, EndNow, Pole, Target, true, 0.97, CheeseStrikeArms::MostStretch);
		EndNow.SetRotation(Turn.GetNormalized());
		const FBoneTransform Set[] = {FBoneTransform(Upper, UpperNow), FBoneTransform(Lower, LowerNow), FBoneTransform(End, EndNow)};
		Pose.SafeSetCSBoneTransforms(Set);
	}

	/** Where to bend an arm or leg: beyond its middle joint, away from the line from its root to its end, as the pose coming in has it. */
	FVector ArmsPole(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex End)
	{
		const FVector Root = Pose.GetComponentSpaceTransform(Upper).GetLocation();
		const FVector Joint = Pose.GetComponentSpaceTransform(Lower).GetLocation();
		const FVector Tip = Pose.GetComponentSpaceTransform(End).GetLocation();
		return Joint + (Joint - (Root + Tip) * 0.5).GetSafeNormal() * 50.0f;
	}

	/** Turns a bone about itself, its children with it. */
	void ArmsTurn(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, const FQuat& Turn)
	{
		FTransform Now = Pose.GetComponentSpaceTransform(Bone);
		Now.SetRotation((Turn * Now.GetRotation()).GetNormalized());
		const FBoneTransform Set(Bone, Now);
		Pose.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
	}

	bool ArmsBone(FBoneReference& Reference, const TCHAR* Name, const FBoneContainer& Bones)
	{
		Reference = FBoneReference(FName(Name));
		Reference.Initialize(Bones);
		return Reference.IsValidToEvaluate(Bones);
	}

	/** The pose coming in blended with `Posed` (both in component space) by `Alpha`, bone by bone in local space, into `Pose`. */
	template <typename TPoser>
	void ArmsBlendIn(FCSPose<FCompactPose>& Pose, float Alpha, TPoser&& Posed)
	{
		if (Alpha >= 0.999f)
		{
			Posed(Pose);
			return;
		}
		FCompactPose Before = Pose.GetPose();
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Pose, Before);
		Posed(Pose);
		FCompactPose After = Pose.GetPose();
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Pose, After);
		for (const FCompactPoseBoneIndex Index : After.ForEachBoneIndex())
		{
			FTransform Blended;
			Blended.Blend(Before[Index], After[Index], Alpha);
			After[Index] = Blended;
		}
		Pose.InitPose(MoveTemp(After));
	}
}

// The first-person hold.

void FCheeseStrikeViewHold::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	ComponentPose.Initialize(Context);
	bFollowing = false;
}

void FCheeseStrikeViewHold::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	ComponentPose.CacheBones(Context);
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	bBones = ArmsBone(Head, TEXT("head"), Bones);
	bBones &= ArmsBone(UpperArm, TEXT("upperarm_r"), Bones);
	bBones &= ArmsBone(LowerArm, TEXT("lowerarm_r"), Bones);
	bBones &= ArmsBone(Hand, TEXT("hand_r"), Bones);
	bBones &= ArmsBone(OtherUpperArm, TEXT("upperarm_l"), Bones);
	bBones &= ArmsBone(OtherLowerArm, TEXT("lowerarm_l"), Bones);
	bBones &= ArmsBone(OtherHand, TEXT("hand_l"), Bones);
}

void FCheeseStrikeViewHold::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	ComponentPose.Update(Context);
	DeltaSeconds = Context.GetDeltaTime();
}

void FCheeseStrikeViewHold::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	using namespace CheeseStrikeArms;
	ComponentPose.EvaluateComponentSpace(Output);
	if (!bBones)
	{
		bFollowing = false;
		return;
	}
	FCSPose<FCompactPose>& Pose = Output.Pose;
	const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
	const FCompactPoseBoneIndex HandIndex = Hand.GetCompactPoseIndex(Bones);
	// The eye, where the camera rides the head, and the hand as the animation has it, in view.
	const FVector Eye = Pose.GetComponentSpaceTransform(Head.GetCompactPoseIndex(Bones)).TransformPosition(CameraAtHead);
	const FTransform HandNow = Pose.GetComponentSpaceTransform(HandIndex);
	const FQuat Unview = View.Inverse();
	const FVector AtNow = Unview.RotateVector(HandNow.GetLocation() - Eye);
	const FQuat TurnNow = (Unview * HandNow.GetRotation()).GetNormalized();
	if (!bFollowing)
	{
		SteadyAt = SettledAt = AtNow;
		SteadyTurn = SettledTurn = TurnNow;
		bFollowing = true;
	}
	else
	{
		const float Steady = Follow(SteadyRate, DeltaSeconds);
		const float Settle = Follow(SettleRate, DeltaSeconds);
		SteadyAt = FMath::Lerp(SteadyAt, AtNow, Steady);
		SteadyTurn = FQuat::Slerp(SteadyTurn, TurnNow, Steady).GetNormalized();
		SettledAt = FMath::Lerp(SettledAt, AtNow, Settle);
		SettledTurn = FQuat::Slerp(SettledTurn, TurnNow, Settle).GetNormalized();
	}
	if (Alpha <= 0.001f)
	{
		return;
	}
	// What the animation does over its settled hold: the cycles' bob, steadied, and a montage's movement as it comes.
	const FVector At = FMath::Lerp(SteadyAt, AtNow, Montage);
	const FQuat Turn = FQuat::Slerp(SteadyTurn, TurnNow, Montage).GetNormalized();
	const FVector Bob = (At - SettledAt).GetClampedToMaxSize(MostBob);
	const FQuat Swaying = (Turn * SettledTurn.Inverse()).GetNormalized();
	const FQuat RelTurn = ModelInHand.GetRotation();

	FVector TargetAt;
	FQuat TargetTurn;
	if (bKnife)
	{
		// On guard low on the right, or swiped across the view, the fist round the handle.
		constexpr float ViewSize = 0.8f;
		const FKnifeAt Knife = SwipePhase >= 0.0f ? KnifeSwipe(SwipePhase, ViewSize * SwipeSize) : KnifeGuard();
		TargetAt = KnifeHand + Knife.Hand + Bob;
		const FQuat Laid = ArmsLaid(ModelAlong, FVector(0.0, 0.0, 1.0), Knife.Blade, Knife.Spine);
		TargetTurn = (Swaying * Laid * RelTurn.Inverse()).GetNormalized();
	}
	else if (bHasSight)
	{
		// Down the sights: the model's length along the view, its top up, the eye on its sight line. At the hip, the same
		// moved and turned in view as its hold has it (HipFromAimed), so that every weapon sits alike whatever its shape.
		const FQuat ModelTurn = FRotationMatrix::MakeFromYZ(FVector::ForwardVector, FVector::UpVector).ToQuat();
		const FVector Scale = ModelInHand.GetScale3D();
		const FTransform ModelAimed(ModelTurn, -ModelTurn.RotateVector(Sight * Scale), Scale);
		const FTransform HandAimed = ModelInHand.Inverse() * ModelAimed;
		const FTransform HandHip = HandAimed * HipFromAimed;
		// With the animation's bob and sway, less of them aimed.
		const float Sway = FMath::Lerp(1.0f, AimedSway, Aimed);
		TargetAt = FMath::Lerp(HandHip.GetLocation(), HandAimed.GetLocation(), Aimed) + Bob * Sway;
		TargetTurn = (FQuat::Slerp(FQuat::Identity, Swaying, Sway) * FQuat::Slerp(HandHip.GetRotation(), HandAimed.GetRotation(), Aimed)).GetNormalized();
	}
	else
	{
		// No sights to hold it by: where the animation has it, steadied.
		TargetAt = At;
		TargetTurn = Turn;
	}
	// Carried low at a sprint, canted in a slide, and coming up from below as it is drawn: turned about the hand.
	const float Sprinting = Sprint * (1.0f - Aimed);
	const FQuat Sprinted = FQuat::Slerp(FQuat::Identity, FQuat(SprintTurn), Sprinting);
	const FQuat Slid = FQuat::Slerp(FQuat::Identity, FQuat(SlideTurn), Slide * (1.0f - Aimed));
	const FQuat Drawn = FQuat::Slerp(FQuat::Identity, FQuat(FRotator(-DrawTip, 0.0f, 0.0f)), Lowered);
	TargetTurn = (Drawn * Slid * Sprinted * TargetTurn).GetNormalized();
	TargetAt += SprintShift * Sprinting + SlideShift * Slide * (1.0f - Aimed) - FVector::UpVector * DrawDrop * Lowered;

	// Into the body's space, as much as it is placed.
	const FVector Target = FMath::Lerp(HandNow.GetLocation(), Eye + View.RotateVector(TargetAt), Alpha);
	const FQuat Turned = FQuat::Slerp(HandNow.GetRotation(), View * TargetTurn, Alpha);
	const FCompactPoseBoneIndex UpperIndex = UpperArm.GetCompactPoseIndex(Bones);
	const FCompactPoseBoneIndex LowerIndex = LowerArm.GetCompactPoseIndex(Bones);
	// The other hand, as the animation has it on the weapon (relative to this hand), goes along with it; its grip takes
	// it after, except while a montage (a reload) moves it.
	const FCompactPoseBoneIndex OtherUpperIndex = OtherUpperArm.GetCompactPoseIndex(Bones);
	const FCompactPoseBoneIndex OtherLowerIndex = OtherLowerArm.GetCompactPoseIndex(Bones);
	const FCompactPoseBoneIndex OtherIndex = OtherHand.GetCompactPoseIndex(Bones);
	const FTransform OtherWas = Pose.GetComponentSpaceTransform(OtherIndex).GetRelativeTransform(HandNow) * FTransform(Turned, Target);
	ArmsReach(Pose, UpperIndex, LowerIndex, HandIndex, Target, Turned, ArmsPole(Pose, UpperIndex, LowerIndex, HandIndex));
	if (Montage > 0.001f)
	{
		const FTransform OtherNow = Pose.GetComponentSpaceTransform(OtherIndex);
		const FVector OtherAt = FMath::Lerp(OtherNow.GetLocation(), OtherWas.GetLocation(), Montage);
		const FQuat OtherTurn = FQuat::Slerp(OtherNow.GetRotation(), OtherWas.GetRotation(), Montage);
		ArmsReach(Pose, OtherUpperIndex, OtherLowerIndex, OtherIndex, OtherAt, OtherTurn, ArmsPole(Pose, OtherUpperIndex, OtherLowerIndex, OtherIndex));
	}
}

// The knife.

void FCheeseStrikeKnifeHold::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	ComponentPose.Initialize(Context);
}

void FCheeseStrikeKnifeHold::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	ComponentPose.CacheBones(Context);
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	bBones = ArmsBone(Chest, TEXT("spine_03"), Bones);
	bBones &= ArmsBone(UpperArm, TEXT("upperarm_r"), Bones);
	bBones &= ArmsBone(LowerArm, TEXT("lowerarm_r"), Bones);
	bBones &= ArmsBone(Hand, TEXT("hand_r"), Bones);
	const TCHAR* const IndexNames[] = {TEXT("index_01_r"), TEXT("index_02_r"), TEXT("index_03_r")};
	const TCHAR* const MiddleNames[] = {TEXT("middle_01_r"), TEXT("middle_02_r"), TEXT("middle_03_r")};
	bFingers = true;
	for (int32 Joint = 0; Joint < 3; ++Joint)
	{
		bFingers &= ArmsBone(Index[Joint], IndexNames[Joint], Bones);
		bFingers &= ArmsBone(Middle[Joint], MiddleNames[Joint], Bones);
	}
}

void FCheeseStrikeKnifeHold::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	ComponentPose.Update(Context);
}

void FCheeseStrikeKnifeHold::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	using namespace CheeseStrikeArms;
	ComponentPose.EvaluateComponentSpace(Output);
	if (Alpha <= 0.001f || !bBones)
	{
		return;
	}
	FCSPose<FCompactPose>& Pose = Output.Pose;
	const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
	ArmsBlendIn(Pose, Alpha, [this, &Bones](FCSPose<FCompactPose>& Posed)
	{
		const auto IndexOf = [&Bones](const FBoneReference& Reference) { return Reference.GetCompactPoseIndex(Bones); };
		if (bPlaceHand)
		{
			// The frame the guard and the swipe are in: the way the body faces, tipped a little with the aim.
			const FVector Flat = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
			const FQuat Tip = FQuat(FVector::CrossProduct(FVector::UpVector, Flat).GetSafeNormal(), FMath::DegreesToRadians(-AimPitch * 0.5f));
			const FVector Ahead = Tip.RotateVector(Flat);
			const FVector Up = Tip.RotateVector(FVector::UpVector);
			const FVector Right = FVector::CrossProduct(Up, Ahead);
			const auto InBody = [&](const FVector& V) { return Ahead * V.X + Right * V.Y + Up * V.Z; };
			const FKnifeAt Knife = SwipePhase >= 0.0f ? KnifeSwipe(SwipePhase, SwipeSize) : KnifeGuard();
			// On guard ahead of the chest, low on the right.
			const FVector Guard(26.0f, 16.0f, 2.0f);
			const FVector ChestAt = Posed.GetComponentSpaceTransform(IndexOf(Chest)).GetLocation();
			const FVector Target = ChestAt + InBody(Guard + Knife.Hand);
			const FQuat Laid = ArmsLaid(ModelAlong, ModelSpine, InBody(Knife.Blade), InBody(Knife.Spine));
			const FQuat Turn = Laid * ModelInHand.GetRotation().Inverse();
			const FVector Shoulder = Posed.GetComponentSpaceTransform(IndexOf(UpperArm)).GetLocation();
			// The elbow out to the side and down, behind the hand.
			const FVector Pole = (Shoulder + Target) * 0.5 + Right * 25.0f - Up * 30.0f - Ahead * 5.0f;
			ArmsReach(Posed, IndexOf(UpperArm), IndexOf(LowerArm), IndexOf(Hand), Target, Turn, Pole);
		}
		// The fist closed: the index finger curled as the middle one is.
		if (bFingers)
		{
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				const FCompactPoseBoneIndex MiddleIndex = IndexOf(Middle[Joint]);
				const FCompactPoseBoneIndex IndexIndex = IndexOf(Index[Joint]);
				const FCompactPoseBoneIndex MiddleParent = Bones.GetParentBoneIndex(MiddleIndex);
				const FCompactPoseBoneIndex IndexParent = Bones.GetParentBoneIndex(IndexIndex);
				const FQuat Bend = Posed.GetComponentSpaceTransform(MiddleParent).GetRotation().Inverse() * Posed.GetComponentSpaceTransform(MiddleIndex).GetRotation();
				FTransform Curled = Posed.GetComponentSpaceTransform(IndexIndex);
				Curled.SetRotation((Posed.GetComponentSpaceTransform(IndexParent).GetRotation() * Bend).GetNormalized());
				const FBoneTransform Set(IndexIndex, Curled);
				Posed.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
			}
		}
	});
}

// A grenade in hand.

void FCheeseStrikeGrenadeHold::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	ComponentPose.Initialize(Context);
}

void FCheeseStrikeGrenadeHold::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	ComponentPose.CacheBones(Context);
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	bBones = ArmsBone(Head, TEXT("head"), Bones);
	bBones &= ArmsBone(UpperArm, TEXT("upperarm_r"), Bones);
	bBones &= ArmsBone(LowerArm, TEXT("lowerarm_r"), Bones);
	bBones &= ArmsBone(Hand, TEXT("hand_r"), Bones);
	bBones &= ArmsBone(OtherUpperArm, TEXT("upperarm_l"), Bones);
	bBones &= ArmsBone(OtherLowerArm, TEXT("lowerarm_l"), Bones);
	bBones &= ArmsBone(OtherHand, TEXT("hand_l"), Bones);
	bFingers = ArmsBone(Index, TEXT("index_01_r"), Bones);
	bFingers &= ArmsBone(Middle, TEXT("middle_01_r"), Bones);
	bFingers &= ArmsBone(Pinky, TEXT("pinky_01_r"), Bones);
	const TCHAR* const FingerNames[] = {TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky")};
	bCurl = true;
	for (int32 Finger = 0; Finger < 4; ++Finger)
	{
		for (int32 Joint = 0; Joint < 3; ++Joint)
		{
			bCurl &= ArmsBone(Curl[Finger][Joint], *FString::Printf(TEXT("%s_0%d_r"), FingerNames[Finger], Joint + 1), Bones);
		}
	}
}

void FCheeseStrikeGrenadeHold::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	ComponentPose.Update(Context);
}

void FCheeseStrikeGrenadeHold::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	ComponentPose.EvaluateComponentSpace(Output);
	if (Alpha <= 0.001f || !bBones)
	{
		return;
	}
	ArmsBlendIn(Output.Pose, Alpha, [this](FCSPose<FCompactPose>& Posed) { PoseArms(Posed); });
}

void FCheeseStrikeGrenadeHold::PoseArms(FCSPose<FCompactPose>& Pose) const
{
	const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
	const auto IndexOf = [&Bones](const FBoneReference& Reference) { return Reference.GetCompactPoseIndex(Bones); };
	const auto At = [&Pose, &IndexOf](const FBoneReference& Bone) { return Pose.GetComponentSpaceTransform(IndexOf(Bone)).GetLocation(); };
	const FVector Up = FVector::UpVector;
	const FVector Ahead = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
	if (Ahead.IsNearlyZero())
	{
		return;
	}
	const FVector Right = FVector::CrossProduct(Up, Ahead);
	// The frame of the aim and the eyes: its own player's view, or the way the body faces, tipped by its aim, from its face.
	FQuat Aim;
	FVector Eye;
	const FTransform HeadNow = Pose.GetComponentSpaceTransform(IndexOf(Head));
	if (bInView)
	{
		Aim = View;
		Eye = HeadNow.TransformPosition(CameraAtHead);
	}
	else
	{
		const FQuat Tip(Right, FMath::DegreesToRadians(-FMath::Clamp(AimPitch, -60.0f, 60.0f) * 0.7f));
		Aim = Tip * FRotationMatrix::MakeFromXZ(Ahead, Up).ToQuat();
		Eye = HeadNow.GetLocation() + Ahead * 10.0f + Up * 4.0f;
	}

	// The right hand: where the throw has it, turned as its fingers and palm are wanted (found from its fingers' bones, so
	// whatever way a skeleton's hand bone is turned).
	const CheeseStrikeGrenades::FHandAt Wanted = ThrowPhase >= 0.0f ? CheeseStrikeGrenades::ThrowHand(ThrowPhase) : CheeseStrikeGrenades::HeldHand();
	const FCompactPoseBoneIndex HandIndex = IndexOf(Hand);
	const FTransform HandNow = Pose.GetComponentSpaceTransform(HandIndex);
	FQuat Turn = HandNow.GetRotation();
	if (bFingers)
	{
		const FVector Fingers = (At(Middle) - HandNow.GetLocation()).GetSafeNormal();
		const FVector Palm = FVector::CrossProduct(Fingers, (At(Index) - At(Pinky)).GetSafeNormal()).GetSafeNormal();
		const FQuat Now = FRotationMatrix::MakeFromXZ(Fingers, Palm).ToQuat();
		const FQuat Then = FRotationMatrix::MakeFromXZ(Aim.RotateVector(Wanted.Fingers), Aim.RotateVector(Wanted.Palm)).ToQuat();
		Turn = (Then * Now.Inverse() * Turn).GetNormalized();
	}
	// On the body others see, held nearer the shoulder (ahead of the eyes is for the view, where it must show), until the
	// throw winds back.
	const float NearShoulder = bInView ? 0.0f : (ThrowPhase < 0.0f ? 1.0f : 1.0f - FMath::SmoothStep(0.0f, 0.28f, ThrowPhase));
	const FVector Target = Eye + Aim.RotateVector(Wanted.At + FVector(-22.0f, 4.0f, 0.0f) * NearShoulder);
	const FVector Shoulder = At(UpperArm);
	// The elbow out to the right and down.
	const FVector Pole = (Shoulder + Target) * 0.5 + Aim.RotateVector(FVector(-10.0, 40.0, -30.0));
	ArmsReach(Pose, IndexOf(UpperArm), IndexOf(LowerArm), HandIndex, Target, Turn, Pole);
	// The fingers closed round the grenade in the palm: each joint as the skeleton's own (open) pose has it, then bent toward the palm.
	if (bFingers && bCurl)
	{
		const FVector Fingers = (At(Middle) - At(Hand)).GetSafeNormal();
		const FVector Palm = FVector::CrossProduct(Fingers, (At(Index) - At(Pinky)).GetSafeNormal()).GetSafeNormal();
		const FVector Across = FVector::CrossProduct(Fingers, Palm).GetSafeNormal();
		const float Bends[3] = {35.0f, 45.0f, 30.0f};
		for (int32 Finger = 0; Finger < 4; ++Finger)
		{
			for (int32 Joint = 0; Joint < 3; ++Joint)
			{
				const FCompactPoseBoneIndex Bone = IndexOf(Curl[Finger][Joint]);
				const FTransform Open = Bones.GetRefPoseTransform(Bone) * Pose.GetComponentSpaceTransform(Bones.GetParentBoneIndex(Bone));
				const FBoneTransform Set(Bone, Open);
				Pose.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
				ArmsTurn(Pose, Bone, FQuat(Across, FMath::DegreesToRadians(Bends[Joint])));
			}
		}
	}

	// The left arm let go and lowered by the side, the hand turned on the forearm as the skeleton's own pose has it.
	{
		const FCompactPoseBoneIndex UpperIndex = IndexOf(OtherUpperArm);
		const FCompactPoseBoneIndex LowerIndex = IndexOf(OtherLowerArm);
		const FCompactPoseBoneIndex EndIndex = IndexOf(OtherHand);
		const FVector OtherShoulder = At(OtherUpperArm);
		const float Length = FVector::Dist(OtherShoulder, At(OtherLowerArm)) + FVector::Dist(At(OtherLowerArm), At(OtherHand));
		const FVector Low = OtherShoulder - Up * (Length * 0.9f) + Ahead * 8.0f - Right * 7.0f;
		const FVector OtherPole = (OtherShoulder + Low) * 0.5 - Ahead * 40.0f - Right * 10.0f;
		FTransform UpperNow = Pose.GetComponentSpaceTransform(UpperIndex);
		FTransform LowerNow = Pose.GetComponentSpaceTransform(LowerIndex);
		FTransform EndNow = Pose.GetComponentSpaceTransform(EndIndex);
		AnimationCore::SolveTwoBoneIK(UpperNow, LowerNow, EndNow, OtherPole, Low, false, 1.0, 1.0);
		EndNow.SetRotation((LowerNow.GetRotation() * Bones.GetRefPoseTransform(EndIndex).GetRotation()).GetNormalized());
		const FBoneTransform Set[] = {FBoneTransform(UpperIndex, UpperNow), FBoneTransform(LowerIndex, LowerNow), FBoneTransform(EndIndex, EndNow)};
		Pose.SafeSetCSBoneTransforms(Set);
	}
}

// The slide.

void FCheeseStrikeSlidePose::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	ComponentPose.Initialize(Context);
}

void FCheeseStrikeSlidePose::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	ComponentPose.CacheBones(Context);
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	bBones = ArmsBone(Pelvis, TEXT("pelvis"), Bones);
	bBones &= ArmsBone(Spine[0], TEXT("spine_01"), Bones);
	bBones &= ArmsBone(Spine[1], TEXT("spine_02"), Bones);
	bBones &= ArmsBone(Spine[2], TEXT("spine_03"), Bones);
	bBones &= ArmsBone(ThighL, TEXT("thigh_l"), Bones);
	bBones &= ArmsBone(CalfL, TEXT("calf_l"), Bones);
	bBones &= ArmsBone(FootL, TEXT("foot_l"), Bones);
	bBones &= ArmsBone(ThighR, TEXT("thigh_r"), Bones);
	bBones &= ArmsBone(CalfR, TEXT("calf_r"), Bones);
	bBones &= ArmsBone(FootR, TEXT("foot_r"), Bones);
}

void FCheeseStrikeSlidePose::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	ComponentPose.Update(Context);
}

void FCheeseStrikeSlidePose::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	ComponentPose.EvaluateComponentSpace(Output);
	if (Alpha <= 0.001f || !bBones)
	{
		return;
	}
	ArmsBlendIn(Output.Pose, Alpha, [this](FCSPose<FCompactPose>& Posed) { Slide(Posed); });
}

void FCheeseStrikeSlidePose::Slide(FCSPose<FCompactPose>& Pose) const
{
	const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
	const auto IndexOf = [&Bones](const FBoneReference& Reference) { return Reference.GetCompactPoseIndex(Bones); };
	const auto At = [&Pose, &IndexOf](const FBoneReference& Bone) { return Pose.GetComponentSpaceTransform(IndexOf(Bone)).GetLocation(); };
	const FVector Up = FVector::UpVector;
	const FVector Ahead = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
	if (Ahead.IsNearlyZero())
	{
		return;
	}
	const FVector Right = FVector::CrossProduct(Up, Ahead);
	const float LegFull = FVector::Dist(At(ThighR), At(CalfR)) + FVector::Dist(At(CalfR), At(FootR));
	// The hips dropped near the floor and laid back, a little ahead of the capsule's axis.
	{
		FTransform Hips = Pose.GetComponentSpaceTransform(IndexOf(Pelvis));
		const FQuat Back(Right, FMath::DegreesToRadians(-32.0f));
		Hips.SetRotation((Back * Hips.GetRotation()).GetNormalized());
		Hips.SetLocation(Origin + Ahead * 8.0f + Up * (LegFull * 0.3f));
		const FBoneTransform Set(IndexOf(Pelvis), Hips);
		Pose.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
	}
	// The chest comes up over them, a little behind upright.
	for (int32 Each = 0; Each < 3; ++Each)
	{
		ArmsTurn(Pose, IndexOf(Spine[Each]), FQuat(Right, FMath::DegreesToRadians(Each == 0 ? 6.0f : 7.0f)));
	}
	// The right leg out ahead along the floor, the left folded under it, its knee up.
	{
		const FVector Hip = At(ThighR);
		const FVector Foot = FVector(Hip.X, Hip.Y, 0.0) + Ahead * (LegFull * 0.88f) + Right * 6.0f + Up * 11.0f;
		const FVector Pole = (Hip + Foot) * 0.5 + Up * 40.0f + Right * 10.0f;
		FTransform Thigh = Pose.GetComponentSpaceTransform(IndexOf(ThighR));
		FTransform Calf = Pose.GetComponentSpaceTransform(IndexOf(CalfR));
		FTransform FootNow = Pose.GetComponentSpaceTransform(IndexOf(FootR));
		const FQuat CalfWas = Calf.GetRotation();
		AnimationCore::SolveTwoBoneIK(Thigh, Calf, FootNow, Pole, Foot, false, 1.0, 1.0);
		FootNow.SetRotation((Calf.GetRotation() * CalfWas.Inverse() * FootNow.GetRotation()).GetNormalized());
		const FBoneTransform Set[] = {FBoneTransform(IndexOf(ThighR), Thigh), FBoneTransform(IndexOf(CalfR), Calf), FBoneTransform(IndexOf(FootR), FootNow)};
		Pose.SafeSetCSBoneTransforms(Set);
	}
	{
		const FVector Hip = At(ThighL);
		const FVector Foot = FVector(Hip.X, Hip.Y, 0.0) + Ahead * (LegFull * 0.5f) + Right * 4.0f + Up * 8.0f;
		const FVector Pole = (Hip + Foot) * 0.5 + Up * 35.0f + Ahead * 20.0f - Right * 40.0f;
		FTransform Thigh = Pose.GetComponentSpaceTransform(IndexOf(ThighL));
		FTransform Calf = Pose.GetComponentSpaceTransform(IndexOf(CalfL));
		FTransform FootNow = Pose.GetComponentSpaceTransform(IndexOf(FootL));
		const FQuat CalfWas = Calf.GetRotation();
		AnimationCore::SolveTwoBoneIK(Thigh, Calf, FootNow, Pole, Foot, false, 1.0, 1.0);
		FootNow.SetRotation((Calf.GetRotation() * CalfWas.Inverse() * FootNow.GetRotation()).GetNormalized());
		const FBoneTransform Set[] = {FBoneTransform(IndexOf(ThighL), Thigh), FBoneTransform(IndexOf(CalfL), Calf), FBoneTransform(IndexOf(FootL), FootNow)};
		Pose.SafeSetCSBoneTransforms(Set);
	}
}
