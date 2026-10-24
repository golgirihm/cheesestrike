// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeExecutionPose.h"
#include "Animation/AnimInstanceProxy.h"
#include "TwoBoneIK.h"

using CheeseStrikeExecution::EKind;
using CheeseStrikeExecution::Ease;
using CheeseStrikeExecution::Envelope;

namespace
{
	/** A keyframe: a place (or amount) at a time, reached from the one before eased at both ends, or (`bStrike`) speeding up into it, as a blow does. */
	struct FExecKey
	{
		float At;
		FVector Value;
		bool bStrike = false;
	};

	FVector ExecKeyed(float Time, std::initializer_list<FExecKey> Keys)
	{
		const FExecKey* Before = nullptr;
		for (const FExecKey& Key : Keys)
		{
			if (Time < Key.At)
			{
				if (Before == nullptr)
				{
					return Key.Value;
				}
				const float Span = FMath::Max(Key.At - Before->At, 0.001f);
				const float Linear = FMath::Clamp((Time - Before->At) / Span, 0.0f, 1.0f);
				const float Shaped = Key.bStrike ? Linear * Linear : FMath::SmoothStep(0.0f, 1.0f, Linear);
				return FMath::Lerp(Before->Value, Key.Value, Shaped);
			}
			Before = &Key;
		}
		return Before != nullptr ? Before->Value : FVector::ZeroVector;
	}

	float ExecAmount(float Time, std::initializer_list<FExecKey> Keys)
	{
		return static_cast<float>(ExecKeyed(Time, Keys).X);
	}

	FQuat ExecToward(const FVector& From, const FVector& To, float Degrees)
	{
		const FVector Axis = FVector::CrossProduct(From, To).GetSafeNormal();
		return Axis.IsNearlyZero() ? FQuat::Identity : FQuat(Axis, FMath::DegreesToRadians(Degrees));
	}

	void ExecTurn(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, const FQuat& Turn)
	{
		FTransform Now = Pose.GetComponentSpaceTransform(Bone);
		Now.SetRotation((Turn * Now.GetRotation()).GetNormalized());
		const FBoneTransform Set(Bone, Now);
		Pose.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
	}

	void ExecMove(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, const FVector& To)
	{
		FTransform Now = Pose.GetComponentSpaceTransform(Bone);
		Now.SetLocation(To);
		const FBoneTransform Set(Bone, Now);
		Pose.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
	}

	/** A limb (upper, lower, end) reaching `Effector`, bending toward `JointTarget`; the end keeps its turn unless `EndTurn` is given. */
	void ExecReach(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex End, const FVector& Effector, const FVector& JointTarget, const FQuat* EndTurn = nullptr)
	{
		FTransform UpperNow = Pose.GetComponentSpaceTransform(Upper);
		FTransform LowerNow = Pose.GetComponentSpaceTransform(Lower);
		FTransform EndNow = Pose.GetComponentSpaceTransform(End);
		const FQuat EndWas = EndNow.GetRotation();
		AnimationCore::SolveTwoBoneIK(UpperNow, LowerNow, EndNow, JointTarget, Effector, false, 1.0, 1.0);
		EndNow.SetRotation(EndTurn != nullptr ? EndTurn->GetNormalized() : EndWas);
		const FBoneTransform Set[] = {FBoneTransform(Upper, UpperNow), FBoneTransform(Lower, LowerNow), FBoneTransform(End, EndNow)};
		Pose.SafeSetCSBoneTransforms(Set);
	}

	/** How far the one stabbed is pushed back from propped on the elbow toward lying flat, pinned by the shoulder (degrees). */
	constexpr float PinnedBack = 24.0f;

	/** The turn that lays a model's own `Along` and `Spine` along `Blade` and `Up` (as FCheeseStrikeKnifeHold lays the knife). */
	FQuat ExecLaid(const FVector& Along, const FVector& Spine, const FVector& Blade, const FVector& Up)
	{
		return FRotationMatrix::MakeFromXZ(Blade, Up).ToQuat() * FRotationMatrix::MakeFromXZ(Along, Spine).ToQuat().Inverse();
	}

	/** The executor's frame: ahead (toward the blow), right and up, and the floor under the capsule. */
	struct FExecFrame
	{
		FVector F;
		FVector R;
		FVector U = FVector::UpVector;
		FVector O;
		FVector At(float Ahead, float Right, float Up) const { return O + F * Ahead + R * Right + U * Up; }
	};
}

float FCheeseStrikeExecutionPose::Weight(EKind Kind, bool bExecutor, float Time)
{
	const float Seconds = CheeseStrikeExecution::Seconds(Kind);
	if (Kind == EKind::None || Time < 0.0f)
	{
		return 0.0f;
	}
	if (!bExecutor)
	{
		return Ease(Time, 0.0f, 0.25f);
	}
	return Envelope(Time, 0.0f, 0.3f, Seconds - 0.35f, Seconds);
}

void FCheeseStrikeExecutionPose::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	ComponentPose.Initialize(Context);
}

void FCheeseStrikeExecutionPose::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	ComponentPose.CacheBones(Context);
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	const auto Bone = [&Bones](FBoneReference& Reference, const TCHAR* Name)
	{
		Reference = FBoneReference(FName(Name));
		Reference.Initialize(Bones);
		return Reference.IsValidToEvaluate(Bones);
	};
	bool bAll = Bone(Pelvis, TEXT("pelvis"));
	bAll &= Bone(Spine[0], TEXT("spine_01"));
	bAll &= Bone(Spine[1], TEXT("spine_02"));
	bAll &= Bone(Spine[2], TEXT("spine_03"));
	bAll &= Bone(Neck, TEXT("neck_01"));
	bAll &= Bone(Head, TEXT("head"));
	bAll &= Bone(ThighL, TEXT("thigh_l"));
	bAll &= Bone(CalfL, TEXT("calf_l"));
	bAll &= Bone(FootL, TEXT("foot_l"));
	bAll &= Bone(ThighR, TEXT("thigh_r"));
	bAll &= Bone(CalfR, TEXT("calf_r"));
	bAll &= Bone(FootR, TEXT("foot_r"));
	bAll &= Bone(UpperArmL, TEXT("upperarm_l"));
	bAll &= Bone(LowerArmL, TEXT("lowerarm_l"));
	bAll &= Bone(HandL, TEXT("hand_l"));
	bAll &= Bone(UpperArmR, TEXT("upperarm_r"));
	bAll &= Bone(LowerArmR, TEXT("lowerarm_r"));
	bAll &= Bone(HandR, TEXT("hand_r"));
	bBones = bAll;
}

void FCheeseStrikeExecutionPose::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	ComponentPose.Update(Context);
}

void FCheeseStrikeExecutionPose::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	ComponentPose.EvaluateComponentSpace(Output);
	const float Alpha = Weight(Kind, bExecutor, Time);
	if (Alpha <= 0.001f || !bBones)
	{
		return;
	}
	FCompactPose Before = Output.Pose.GetPose();
	if (Alpha < 0.999f)
	{
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Output.Pose, Before);
	}
	if (!bExecutor)
	{
		Struck(Output.Pose);
	}
	else if (Kind == EKind::Stab)
	{
		Stab(Output.Pose);
	}
	else
	{
		Stomp(Output.Pose);
	}
	if (Alpha >= 0.999f)
	{
		return;
	}
	// Part of the way: what came in and the execution's, blended bone by bone in local space.
	FCompactPose After = Output.Pose.GetPose();
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Output.Pose, After);
	for (const FCompactPoseBoneIndex Index : After.ForEachBoneIndex())
	{
		FTransform Blended;
		Blended.Blend(Before[Index], After[Index], Alpha);
		After[Index] = Blended;
	}
	Output.Pose.InitPose(MoveTemp(After));
}

void FCheeseStrikeExecutionPose::Stab(FCSPose<FCompactPose>& Pose) const
{
	using namespace CheeseStrikeExecution;
	const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
	const auto Index = [&Bones](const FBoneReference& Reference) { return Reference.GetCompactPoseIndex(Bones); };
	const auto At = [&Pose](FCompactPoseBoneIndex Bone) { return Pose.GetComponentSpaceTransform(Bone).GetLocation(); };
	FExecFrame Frame;
	Frame.F = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
	Frame.R = FVector::CrossProduct(Frame.U, Frame.F).GetSafeNormal();
	// The right side from the hips, whichever way the axes turn.
	if (FVector::DotProduct(At(Index(ThighR)) - At(Index(ThighL)), Frame.R) < 0.0f)
	{
		Frame.R = -Frame.R;
	}
	Frame.O = Origin;
	const float T = Time;
	const FVector FootLWas = At(Index(FootL));
	const FVector FootRWas = At(Index(FootR));
	const FVector HandRWas = At(Index(HandR));
	const FVector HandLWas = At(Index(HandL));
	const FQuat HandRTurnWas = Pose.GetComponentSpaceTransform(Index(HandR)).GetRotation();
	const float PelvisUp = static_cast<float>(FVector::DotProduct(At(Index(Pelvis)) - Frame.O, Frame.U));

	// Down on one knee and back up: the hips low and a little ahead, over the knee.
	const float Kneel = Envelope(T, 0.05f, 0.6f, StabSeconds - 0.65f, StabSeconds - 0.1f);
	ExecMove(Pose, Index(Pelvis), At(Index(Pelvis)) + Frame.F * (6.0f * Kneel) - Frame.U * ((PelvisUp - 41.0f) * Kneel));
	// Leaning over the one down: back a little as the knife is drawn, into the thrust, and onto it while it is in.
	const float Lean = ExecAmount(T, {{0.05f, FVector(0)}, {StabCocked, FVector(26)}, {StabThrust, FVector(19)}, {StabLands, FVector(50), true}, {1.45f, FVector(54)},
		{StabOut, FVector(52)}, {StabFree, FVector(34)}, {2.45f, FVector(14)}, {StabSeconds - 0.1f, FVector(0)}});
	ExecTurn(Pose, Index(Spine[0]), ExecToward(Frame.U, Frame.F, Lean * 0.45f));
	ExecTurn(Pose, Index(Spine[1]), ExecToward(Frame.U, Frame.F, Lean * 0.35f));
	ExecTurn(Pose, Index(Spine[2]), ExecToward(Frame.U, Frame.F, Lean * 0.2f));
	// The right shoulder brought round into the thrust, and back.
	const float Twist = ExecAmount(T, {{0.3f, FVector(0)}, {StabThrust, FVector(-8)}, {StabLands, FVector(9), true}, {StabOut, FVector(7)}, {StabFree, FVector(-3)}, {2.6f, FVector(0)}});
	ExecTurn(Pose, Index(Spine[1]), FQuat(Frame.U, FMath::DegreesToRadians(Twist * 0.6f)));
	ExecTurn(Pose, Index(Spine[2]), FQuat(Frame.U, FMath::DegreesToRadians(Twist * 0.4f)));
	// The head looks down at the chest the knife goes into.
	ExecTurn(Pose, Index(Head), ExecToward(Frame.F, -Frame.U, 24.0f * Kneel));

	// The right knee on the floor, its foot back on its toes; the left foot planted ahead, its knee up.
	{
		const FVector Hip = At(Index(ThighR));
		const FVector Foot = FMath::Lerp(FootRWas, Frame.At(-34.0f, 14.0f, 14.0f), Kneel);
		ExecReach(Pose, Index(ThighR), Index(CalfR), Index(FootR), Foot, Hip + Frame.F * 70.0f - Frame.U * 30.0f);
		ExecTurn(Pose, Index(FootR), ExecToward(Frame.F, -Frame.U, 45.0f * Kneel));
	}
	{
		const FVector Hip = At(Index(ThighL));
		const FVector Foot = FMath::Lerp(FootLWas, Frame.At(30.0f, -16.0f, 9.0f), Kneel);
		ExecReach(Pose, Index(ThighL), Index(CalfL), Index(FootL), Foot, Hip + Frame.F * 80.0f + Frame.U * 20.0f);
	}

	// The knife. Its point is placed, and the hand put where the knife it holds has its point there, the blade along the
	// thrust: from above and from the executor's side, down into the chest.
	const FVector Thrust = (-Frame.U * 0.85f + Frame.F * 0.5f).GetSafeNormal();
	const FVector KnifeSpine = FVector::VectorPlaneProject(Frame.F, Thrust).GetSafeNormal();
	const FVector Along = ModelAlong.GetSafeNormal();
	const FVector PointInHand = bKnifeInHand ? ModelInHand.TransformPosition(Along * KnifePoint) : FVector::ZeroVector;
	const FQuat Laid = bKnifeInHand ? ExecLaid(Along, FVector(0.0, 0.0, 1.0), Thrust, KnifeSpine) * ModelInHand.GetRotation().Inverse() : HandRTurnWas;
	// How much the knife is the execution's rather than the hold's: all of it from when it is drawn back until it is put away.
	const float Own = Envelope(T, 0.1f, StabCocked - 0.05f, 2.35f, StabSeconds - 0.1f);
	// A slight working of the blade while it is in.
	const float Work = 4.0f * FMath::Sin((T - StabLands) * 14.0f) * Envelope(T, StabLands + 0.1f, StabLands + 0.25f, StabOut - 0.15f, StabOut);
	const FQuat HandTurn = FQuat::Slerp(HandRTurnWas, FQuat(Thrust, FMath::DegreesToRadians(Work)) * Laid, Own).GetNormalized();
	const FVector PointWas = HandRWas + HandRTurnWas.RotateVector(PointInHand);
	// In the chest: the point a hand's breadth into it, pushed a little further; drawn back along the thrust to strike from.
	const FVector In = Target + Frame.U * 6.0f - Frame.F * 5.0f;
	// Drawn back high and out to the right, where it shows, before it comes down.
	const FVector Cocked = In - Thrust * 46.0f + Frame.U * 14.0f + Frame.R * 12.0f;
	const FVector Point = ExecKeyed(T, {{0.1f, PointWas}, {StabCocked, Cocked}, {StabThrust, Cocked - Thrust * 5.0f}, {StabLands, In, true}, {1.45f, In + Thrust * 3.0f},
		{StabOut, In + Thrust * 2.5f}, {StabFree, In - Thrust * 30.0f, true}, {2.45f, In - Thrust * 34.0f - Frame.U * 12.0f}, {StabSeconds - 0.1f, PointWas}});
	const FVector HandR2 = Point - HandTurn.RotateVector(PointInHand);
	{
		const FVector Shoulder = At(Index(UpperArmR));
		// The elbow out to the right and back, up as the knife is drawn back.
		const FVector Pole = Shoulder + Frame.R * 40.0f - Frame.F * 15.0f + Frame.U * (10.0f + 15.0f * Envelope(T, 0.3f, StabCocked, StabLands, 1.5f));
		ExecReach(Pose, Index(UpperArmR), Index(LowerArmR), Index(HandR), HandR2, Pole, &HandTurn);
	}

	// The left hand pins the near shoulder, comes onto the right fist to push the knife home, braces on the chest as the
	// knife is pulled out, and lets go.
	{
		const FVector Shoulder = At(Index(UpperArmL));
		const FVector Fist = At(Index(HandR));
		const FVector Pin = TargetShoulder + Frame.U * 9.0f - Frame.F * 4.0f;
		const FVector OnFist = Fist - Thrust * 8.0f - Frame.R * 3.0f;
		const FVector Brace = Target + Frame.U * 14.0f - Frame.R * 16.0f - Frame.F * 2.0f;
		const FVector Hand = ExecKeyed(T, {{0.15f, HandLWas}, {0.55f, Pin}, {StabLands + 0.05f, Pin}, {StabLands + 0.2f, OnFist}, {StabOut - 0.05f, OnFist}, {StabOut + 0.1f, Brace},
			{StabFree + 0.05f, Brace}, {2.5f, HandLWas}});
		ExecReach(Pose, Index(UpperArmL), Index(LowerArmL), Index(HandL), Hand, Shoulder - Frame.R * 35.0f - Frame.U * 10.0f);
	}
}

void FCheeseStrikeExecutionPose::Stomp(FCSPose<FCompactPose>& Pose) const
{
	using namespace CheeseStrikeExecution;
	const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
	const auto Index = [&Bones](const FBoneReference& Reference) { return Reference.GetCompactPoseIndex(Bones); };
	const auto At = [&Pose](FCompactPoseBoneIndex Bone) { return Pose.GetComponentSpaceTransform(Bone).GetLocation(); };
	FExecFrame Frame;
	Frame.F = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
	Frame.R = FVector::CrossProduct(Frame.U, Frame.F).GetSafeNormal();
	if (FVector::DotProduct(At(Index(ThighR)) - At(Index(ThighL)), Frame.R) < 0.0f)
	{
		Frame.R = -Frame.R;
	}
	Frame.O = Origin;
	const float T = Time;
	const FVector FootLWas = At(Index(FootL));
	const FVector FootRWas = At(Index(FootR));
	const FQuat FootRTurnWas = Pose.GetComponentSpaceTransform(Index(FootR)).GetRotation();
	// The head, on the floor in front: where the boot comes down.
	const FVector Face = FVector(Target.X, Target.Y, Origin.Z);
	const float HeadUp = static_cast<float>(Target.Z - Origin.Z);
	const float Over = StompLands - 0.15f;

	// The weight onto the left leg as the right comes up, back as it raises the boot, then down and forward into the stamp.
	const FVector Shift = ExecKeyed(T, {{0.0f, FVector(0, 0, 0)}, {0.35f, FVector(-2, -7, 0)}, {Over, FVector(-6, -8, 3)}, {StompLands, FVector(8, -2, -12), true},
		{1.5f, FVector(5, -2, -8)}, {1.95f, FVector(0, -3, 0)}, {StompSeconds - 0.1f, FVector(0, 0, 0)}});
	ExecMove(Pose, Index(Pelvis), At(Index(Pelvis)) + Frame.F * Shift.X + Frame.R * Shift.Y + Frame.U * Shift.Z);
	const float Lean = ExecAmount(T, {{0.0f, FVector(0)}, {0.35f, FVector(-4)}, {Over, FVector(-12)}, {StompLands, FVector(18), true}, {1.5f, FVector(14)}, {2.05f, FVector(0)}});
	ExecTurn(Pose, Index(Spine[0]), ExecToward(Frame.U, Frame.F, Lean * 0.45f));
	ExecTurn(Pose, Index(Spine[1]), ExecToward(Frame.U, Frame.F, Lean * 0.35f));
	ExecTurn(Pose, Index(Spine[2]), ExecToward(Frame.U, Frame.F, Lean * 0.2f));
	// Looking down at the face under the boot.
	ExecTurn(Pose, Index(Head), ExecToward(Frame.F, -Frame.U, 30.0f * Envelope(T, 0.1f, 0.5f, 1.7f, 2.2f)));

	// The right boot: the knee up, the boot high over the face, a moment's hang, down on it hard, a grind, and back.
	{
		const FVector Hip = At(Index(ThighR));
		const FVector Raised = Frame.At(18.0f, 10.0f, 44.0f);
		const FVector OverFace = Face + Frame.U * (HeadUp + 38.0f) - Frame.F * 6.0f;
		const FVector Landed = Face + Frame.U * 14.0f;
		const FVector Foot = ExecKeyed(T, {{0.15f, FootRWas}, {0.6f, Raised}, {0.88f, OverFace}, {Over, OverFace + Frame.U * 3.0f}, {StompLands, Landed, true},
			{1.5f, Landed - Frame.U * 2.0f}, {1.8f, Frame.At(16.0f, 12.0f, 24.0f)}, {2.15f, FootRWas}});
		const float Grind = 10.0f * FMath::Sin((T - StompLands) * 18.0f) * Envelope(T, StompLands, StompLands + 0.05f, 1.4f, 1.5f);
		const float Toes = ExecAmount(T, {{0.15f, FVector(0)}, {0.6f, FVector(25)}, {Over, FVector(10)}, {StompLands, FVector(0)}, {2.15f, FVector(0)}});
		const FQuat Turned = FQuat(Frame.U, FMath::DegreesToRadians(Grind)) * ExecToward(Frame.F, -Frame.U, Toes) * FootRTurnWas;
		ExecReach(Pose, Index(ThighR), Index(CalfR), Index(FootR), Foot, Hip + Frame.F * 90.0f + Frame.U * 25.0f + Frame.R * 8.0f, &Turned);
	}
	// The left stays planted where it stood.
	{
		const FVector Hip = At(Index(ThighL));
		ExecReach(Pose, Index(ThighL), Index(CalfL), Index(FootL), FootLWas, Hip + Frame.F * 80.0f + Frame.U * 10.0f);
	}
}

void FCheeseStrikeExecutionPose::Struck(FCSPose<FCompactPose>& Pose) const
{
	using namespace CheeseStrikeExecution;
	const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
	const auto Index = [&Bones](const FBoneReference& Reference) { return Reference.GetCompactPoseIndex(Bones); };
	const auto At = [&Pose](FCompactPoseBoneIndex Bone) { return Pose.GetComponentSpaceTransform(Bone).GetLocation(); };
	const FVector Up = FVector::UpVector;
	const FVector Ahead = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
	FVector Right = FVector::CrossProduct(Up, Ahead).GetSafeNormal();
	if (FVector::DotProduct(At(Index(ThighR)) - At(Index(ThighL)), Right) < 0.0f)
	{
		Right = -Right;
	}
	const float T = Time;
	if (Kind == EKind::Stab)
	{
		// The thrust jerks the chest and throws the head back; the pull jolts it again, and the body goes limp.
		const float Jerk = ExecAmount(T, {{StabLands - 0.03f, FVector(0)}, {StabLands + 0.06f, FVector(1)}, {StabLands + 0.3f, FVector(0.55f)},
			{StabOut, FVector(0.5f)}, {StabOut + 0.08f, FVector(0.9f)}, {StabFree + 0.2f, FVector(0.2f)}});
		const float Limp = Ease(T, StabOut + 0.1f, StabFree + 0.4f);
		// Pinned by the shoulder: pushed back from the elbow to lie nearly flat, the propping arm going out flat on the floor.
		const float Pinned = Ease(T, 0.3f, 0.75f);
		{
			const FVector ElbowWas = At(Index(LowerArmL));
			ExecTurn(Pose, Index(Spine[0]), ExecToward(Up, -Ahead, PinnedBack * Pinned));
			const FVector Shoulder = At(Index(UpperArmL));
			const FVector Flat = FVector(Origin.X, Origin.Y, Origin.Z) + Ahead * 2.0f - Right * 34.0f + Up * 4.0f;
			const FVector Hand = FMath::Lerp(At(Index(HandL)), Flat, Pinned);
			ExecReach(Pose, Index(UpperArmL), Index(LowerArmL), Index(HandL), Hand, FMath::Lerp(ElbowWas, Shoulder - Right * 30.0f - Ahead * 10.0f + Up * 6.0f, Pinned));
		}
		ExecTurn(Pose, Index(Spine[1]), ExecToward(Up, -Ahead, 9.0f * Jerk));
		ExecTurn(Pose, Index(Spine[2]), ExecToward(Up, -Ahead, 7.0f * Jerk));
		ExecTurn(Pose, Index(Neck), ExecToward(Ahead, Up, 22.0f * Jerk));
		// Limp, the head rolls away from the executor and drops.
		ExecTurn(Pose, Index(Neck), ExecToward(Up, -Right * Side, 25.0f * Limp));
		ExecTurn(Pose, Index(Neck), ExecToward(Up, Ahead, 15.0f * Limp));
		// The pistol hand grabs at the executor's wrist over the chest as the knife goes in, and falls to the floor beside the
		// hips as it comes out.
		const FVector Shoulder = At(Index(UpperArmR));
		const FVector HandWas = At(Index(HandR));
		const FVector Chest = At(Index(Spine[2]));
		const FVector Grab = Chest + Up * 18.0f + Ahead * 8.0f + Right * (Side * 10.0f);
		const FVector Floor = FVector(Origin.X, Origin.Y, Origin.Z) + Ahead * 30.0f + Right * 32.0f + Up * 5.0f;
		const float Grabbing = Envelope(T, StabLands + 0.02f, StabLands + 0.25f, StabOut + 0.05f, StabOut + 0.3f);
		const float Drop = Ease(T, StabOut + 0.05f, StabOut + 0.45f);
		const FVector Hand = FMath::Lerp(FMath::Lerp(HandWas, Grab, Grabbing), Floor, Drop);
		ExecReach(Pose, Index(UpperArmR), Index(LowerArmR), Index(HandR), Hand, Shoulder + Right * 30.0f - Up * 10.0f);
		return;
	}
	// The stomp: the pistol arm up over the face against the boot, the head turned up to it; at the blow, the head and
	// chest driven down toward the floor.
	const float Shield = Envelope(T, 0.45f, 0.85f, StompLands, StompLands + 0.2f);
	const FVector Shoulder = At(Index(UpperArmR));
	const FVector HeadNow = At(Index(Head));
	const FVector Guard = HeadNow + Up * 22.0f + Ahead * 10.0f + Right * (Side * 8.0f);
	ExecReach(Pose, Index(UpperArmR), Index(LowerArmR), Index(HandR), FMath::Lerp(At(Index(HandR)), Guard, Shield), Shoulder + Right * 25.0f + Up * 10.0f);
	const float Look = Envelope(T, 0.4f, 0.8f, StompLands, StompLands + 0.05f);
	ExecTurn(Pose, Index(Neck), ExecToward(Ahead, Right * Side + Up, 25.0f * Look));
	const float Crushed = Ease(T, StompLands - 0.05f, StompLands + 0.04f);
	ExecTurn(Pose, Index(Spine[0]), ExecToward(Up, -Ahead, 14.0f * Crushed));
	ExecTurn(Pose, Index(Spine[2]), ExecToward(Up, -Ahead, 10.0f * Crushed));
	ExecTurn(Pose, Index(Neck), ExecToward(Up, -Ahead, 30.0f * Crushed));
}
