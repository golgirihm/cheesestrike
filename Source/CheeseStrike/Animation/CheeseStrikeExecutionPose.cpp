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
	const float Kneel = Envelope(T, 0.0f, 0.5f, StabSeconds - 0.55f, StabSeconds - 0.1f);
	ExecMove(Pose, Index(Pelvis), At(Index(Pelvis)) + Frame.F * (6.0f * Kneel) - Frame.U * ((PelvisUp - 52.0f) * Kneel));
	// Leaning over the one down, further into each blow.
	const float Lean = 28.0f * Kneel + ExecAmount(T, {{0.6f, FVector(0)}, {0.85f, FVector(-6)}, {StabLands[0], FVector(10), true}, {1.25f, FVector(4)},
		{StabLands[1] - 0.15f, FVector(-2)}, {StabLands[1], FVector(12), true}, {2.0f, FVector(6)}, {2.3f, FVector(0)}}) * Kneel;
	ExecTurn(Pose, Index(Spine[0]), ExecToward(Frame.U, Frame.F, Lean * 0.45f));
	ExecTurn(Pose, Index(Spine[1]), ExecToward(Frame.U, Frame.F, Lean * 0.35f));
	ExecTurn(Pose, Index(Spine[2]), ExecToward(Frame.U, Frame.F, Lean * 0.2f));
	// The head looks down at the blow.
	ExecTurn(Pose, Index(Head), ExecToward(Frame.F, -Frame.U, 18.0f * Kneel));

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

	// The left hand pins the near shoulder.
	{
		const float Pin = Envelope(T, 0.2f, 0.6f, 2.05f, 2.4f);
		const FVector Shoulder = At(Index(UpperArmL));
		const FVector Hand = FMath::Lerp(HandLWas, TargetShoulder + Frame.U * 9.0f - Frame.F * 4.0f, Pin);
		ExecReach(Pose, Index(UpperArmL), Index(LowerArmL), Index(HandL), Hand, Shoulder - Frame.R * 35.0f - Frame.U * 10.0f);
	}

	// The knife: up overhead, down into the chest, twice, and out.
	{
		const FVector Shoulder = At(Index(UpperArmR));
		const FVector Raised = Shoulder + Frame.U * 40.0f + Frame.F * 6.0f + Frame.R * 6.0f;
		const FVector Raised2 = Shoulder + Frame.U * 30.0f + Frame.F * 10.0f + Frame.R * 6.0f;
		// The hand stops a blade's length over the chest, the blade in it.
		const FVector Blow = Target + Frame.U * 20.0f - Frame.F * 4.0f;
		const FVector Out = Shoulder + Frame.F * 25.0f - Frame.U * 12.0f + Frame.R * 10.0f;
		const FVector Hand = ExecKeyed(T, {{0.1f, HandRWas}, {0.5f, Raised}, {0.85f, Raised + Frame.U * 4.0f}, {StabLands[0], Blow, true},
			{1.25f, Blow - Frame.U * 3.0f}, {StabLands[1] - 0.15f, Raised2}, {StabLands[1], Blow - Frame.U * 2.0f, true}, {2.0f, Blow - Frame.U * 5.0f},
			{2.3f, Out}, {StabSeconds - 0.1f, HandRWas}});
		// The blade down: the hand turned from the hold's (the knife along the way it faces) to point it at the floor.
		const float Down = ExecAmount(T, {{0.1f, FVector(0)}, {0.5f, FVector(70)}, {0.85f, FVector(75)}, {StabLands[0], FVector(95)}, {2.0f, FVector(95)}, {2.3f, FVector(40)}, {StabSeconds - 0.1f, FVector(0)}});
		const FQuat Turned = ExecToward(Frame.F, -Frame.U, Down) * HandRTurnWas;
		ExecReach(Pose, Index(UpperArmR), Index(LowerArmR), Index(HandR), Hand, Shoulder + Frame.R * 45.0f + Frame.U * 5.0f - Frame.F * 10.0f, &Turned);
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

	// The weight onto the left leg as the right comes up, back as it raises the boot, then down and forward into the stamp.
	const FVector Shift = ExecKeyed(T, {{0.0f, FVector(0, 0, 0)}, {0.35f, FVector(-2, -6, 0)}, {0.85f, FVector(-6, -7, 2)}, {StompLands, FVector(8, -2, -12), true},
		{1.45f, FVector(5, -2, -8)}, {1.9f, FVector(0, 0, 0)}});
	ExecMove(Pose, Index(Pelvis), At(Index(Pelvis)) + Frame.F * Shift.X + Frame.R * Shift.Y + Frame.U * Shift.Z);
	const float Lean = ExecAmount(T, {{0.0f, FVector(0)}, {0.35f, FVector(-4)}, {0.85f, FVector(-12)}, {StompLands, FVector(18), true}, {1.45f, FVector(14)}, {1.95f, FVector(0)}});
	ExecTurn(Pose, Index(Spine[0]), ExecToward(Frame.U, Frame.F, Lean * 0.45f));
	ExecTurn(Pose, Index(Spine[1]), ExecToward(Frame.U, Frame.F, Lean * 0.35f));
	ExecTurn(Pose, Index(Spine[2]), ExecToward(Frame.U, Frame.F, Lean * 0.2f));
	// Looking down at the face under the boot.
	ExecTurn(Pose, Index(Head), ExecToward(Frame.F, -Frame.U, 30.0f * Envelope(T, 0.1f, 0.5f, 1.6f, 2.1f)));

	// The right boot: the knee up, the boot high over the face, down on it hard, a grind, and back.
	{
		const FVector Hip = At(Index(ThighR));
		const FVector Raised = Frame.At(18.0f, 10.0f, 42.0f);
		const FVector Over = Face + Frame.U * (HeadUp + 36.0f) - Frame.F * 6.0f;
		const FVector Landed = Face + Frame.U * 14.0f;
		const FVector Foot = ExecKeyed(T, {{0.15f, FootRWas}, {0.55f, Raised}, {0.88f, Over}, {StompLands, Landed, true}, {1.45f, Landed - Frame.U * 2.0f},
			{1.75f, Frame.At(16.0f, 12.0f, 24.0f)}, {2.05f, FootRWas}});
		const float Grind = 10.0f * FMath::Sin((T - StompLands) * 18.0f) * Envelope(T, StompLands, StompLands + 0.05f, 1.35f, 1.45f);
		const float Toes = ExecAmount(T, {{0.15f, FVector(0)}, {0.55f, FVector(25)}, {0.88f, FVector(10)}, {StompLands, FVector(0)}, {2.05f, FVector(0)}});
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
		// Each blow jerks the chest down and throws the head back; after the second, limp.
		const float Jerk = ExecAmount(T, {{StabLands[0] - 0.02f, FVector(0)}, {StabLands[0] + 0.06f, FVector(1)}, {1.35f, FVector(0.3f)},
			{StabLands[1] - 0.02f, FVector(0.3f)}, {StabLands[1] + 0.06f, FVector(1.2f)}, {2.0f, FVector(0.8f)}});
		const float Limp = Ease(T, StabLands[1], 2.0f);
		ExecTurn(Pose, Index(Spine[1]), ExecToward(Up, -Ahead, 10.0f * Jerk));
		ExecTurn(Pose, Index(Spine[2]), ExecToward(Up, -Ahead, 8.0f * Jerk));
		ExecTurn(Pose, Index(Neck), ExecToward(Ahead, Up, 20.0f * Jerk + 15.0f * Limp));
		// The pistol arm drops to the floor beside the hips at the first blow.
		const float Drop = Ease(T, StabLands[0], StabLands[0] + 0.35f);
		const FVector Shoulder = At(Index(UpperArmR));
		const FVector Floor = FVector(Origin.X, Origin.Y, Origin.Z) + Ahead * 30.0f + Right * 32.0f + Up * 5.0f;
		ExecReach(Pose, Index(UpperArmR), Index(LowerArmR), Index(HandR), FMath::Lerp(At(Index(HandR)), Floor, Drop), Shoulder + Right * 30.0f - Up * 10.0f);
		return;
	}
	// The stomp: the pistol arm up over the face against the boot, the head turned up to it; at the blow, the head driven down.
	const float Shield = Envelope(T, 0.45f, 0.85f, StompLands, StompLands + 0.2f);
	const FVector Shoulder = At(Index(UpperArmR));
	const FVector HeadNow = At(Index(Head));
	const FVector Guard = HeadNow + Up * 22.0f + Ahead * 10.0f + Right * (Side * 8.0f);
	ExecReach(Pose, Index(UpperArmR), Index(LowerArmR), Index(HandR), FMath::Lerp(At(Index(HandR)), Guard, Shield), Shoulder + Right * 25.0f + Up * 10.0f);
	const float Look = Envelope(T, 0.4f, 0.8f, StompLands, StompLands + 0.05f);
	ExecTurn(Pose, Index(Neck), ExecToward(Ahead, Right * Side + Up, 25.0f * Look));
	const float Crushed = Ease(T, StompLands - 0.05f, StompLands);
	ExecTurn(Pose, Index(Neck), ExecToward(Up, -Ahead, 35.0f * Crushed));
	ExecTurn(Pose, Index(Spine[2]), ExecToward(Up, -Ahead, 10.0f * Crushed));
}
