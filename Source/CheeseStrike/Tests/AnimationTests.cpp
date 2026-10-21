// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The body's animation: the numbers it blends by, the animations it is made
// of, and that the character and the weapon are made of Lyra's mannequin
// and rifle.

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "CheeseStrikeCharacter.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnimationTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	bool Near(float Value, float Wanted)
	{
		return FMath::Abs(Value - Wanted) < 0.001f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCyclesWeighByDirection, "CheeseStrike.Animation.TheCyclesWeighByDirection", AnimationTests::Flags)

bool FTheCyclesWeighByDirection::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeAnimation;
	float Forward, Backward, Left, Right;
	DirectionWeights(0.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("straight ahead is all forward"), AnimationTests::Near(Forward, 1.0f) && AnimationTests::Near(Backward + Left + Right, 0.0f));
	DirectionWeights(90.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("to the right is all right"), AnimationTests::Near(Right, 1.0f));
	DirectionWeights(-90.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("to the left is all left"), AnimationTests::Near(Left, 1.0f));
	DirectionWeights(180.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("backwards is all backward"), AnimationTests::Near(Backward, 1.0f));
	DirectionWeights(-180.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("either way round"), AnimationTests::Near(Backward, 1.0f));
	DirectionWeights(45.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("between, half and half"), AnimationTests::Near(Forward, 0.5f) && AnimationTests::Near(Right, 0.5f));
	TestTrue(TEXT("summing to one"), AnimationTests::Near(Forward + Backward + Left + Right, 1.0f));

	TestTrue(TEXT("standing still weighs nothing"), AnimationTests::Near(MovingWeight(0.0f), 0.0f));
	TestTrue(TEXT("a walk weighs the cycles fully"), AnimationTests::Near(MovingWeight(WalkFull), 1.0f) && AnimationTests::Near(JogWeight(WalkFull), 0.0f));
	TestTrue(TEXT("and a jog the jog"), AnimationTests::Near(JogWeight(JogFull), 1.0f) && AnimationTests::Near(JogWeight(JogFull + 500.0f), 1.0f));
	TestTrue(TEXT("halfway between, half"), AnimationTests::Near(JogWeight((WalkFull + JogFull) / 2.0f), 0.5f));

	TestTrue(TEXT("moving the way one faces is forward"), AnimationTests::Near(DirectionOf(FVector(100.0, 0.0, 0.0), 0.0f), 0.0f));
	TestTrue(TEXT("to one's right is right"), AnimationTests::Near(DirectionOf(FVector(0.0, 100.0, 0.0), 0.0f), 90.0f));
	TestTrue(TEXT("whichever way one faces"), AnimationTests::Near(DirectionOf(FVector(0.0, 100.0, 0.0), 90.0f), 0.0f));
	TestTrue(TEXT("and standing still is forward"), AnimationTests::Near(DirectionOf(FVector::ZeroVector, 45.0f), 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheBodyHasEveryAnimation, "CheeseStrike.Animation.TheBodyHasEveryAnimation", AnimationTests::Flags)

bool FTheBodyHasEveryAnimation::RunTest(const FString& Parameters)
{
	// The mannequin's set, for the first-person arms, and Belica's, retargeted to her skeleton, for the body.
	const UCheeseStrikeAnimInstance* Mannequin = GetDefault<UCheeseStrikeAnimInstance>();
	const UCheeseStrikeAnimInstance* Belica = GetDefault<UCheeseStrikeBelicaAnimInstance>();
	for (const UCheeseStrikeAnimInstance* Animation : {Mannequin, Belica})
	{
		const FCheeseStrikeAnimSet& Set = Animation->GetSet();
		TestTrue(FString::Printf(TEXT("%s: every animation was found"), *Animation->GetClass()->GetName()), Set.IsComplete());
		TestNotNull(TEXT("standing"), Set.Idle.Get());
		TestNotNull(TEXT("crouched"), Set.CrouchIdle.Get());
		TestNotNull(TEXT("crouch walking"), Set.CrouchWalk.Get());
		TestNotNull(TEXT("aiming from the hip"), Set.AimHip.Get());
		TestNotNull(TEXT("firing"), Set.Fire.Get());
		TestNotNull(TEXT("reloading"), Set.Reload.Get());
	}
	if (Belica->GetSet().Idle != nullptr && Mannequin->GetSet().Idle != nullptr)
	{
		TestTrue(TEXT("Belica's are her skeleton's, not the mannequin's"), Belica->GetSet().Idle->GetSkeleton() != Mannequin->GetSet().Idle->GetSkeleton());
	}
	TestEqual(TEXT("the mannequin's grip follows weapon_r"), Mannequin->GetGripAnchor(), FName(TEXT("weapon_r")));
	TestEqual(TEXT("Belica's, her right hand"), Belica->GetGripAnchor(), FName(TEXT("hand_r")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCharacterIsBelicaInBothViews, "CheeseStrike.Animation.TheCharacterIsBelicaInBothViews", AnimationTests::Flags)

bool FTheCharacterIsBelicaInBothViews::RunTest(const FString& Parameters)
{
	const ACheeseStrikeCharacter* Character = GetDefault<ACheeseStrikeCharacter>();
	// The body others see and the one its own player sees are the same: the arms in view are the character's own.
	for (const USkeletalMeshComponent* Body : {Character->GetMesh(), Character->GetFirstPersonMesh()})
	{
		if (!TestNotNull(TEXT("a body"), Body))
		{
			continue;
		}
		TestNotNull(TEXT("has a mesh"), Body->GetSkeletalMeshAsset());
		if (Body->GetSkeletalMeshAsset() != nullptr)
		{
			TestEqual(TEXT("which is Belica"), Body->GetSkeletalMeshAsset()->GetName(), FString(TEXT("Belica")));
			TestTrue(TEXT("with a hand for the rifle"), Body->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(FName(TEXT("hand_r"))) != INDEX_NONE);
			FName Left;
			FName Right;
			TestTrue(TEXT("and eyes for the camera"), ACheeseStrikeCharacter::FindEyes(Body->GetSkeletalMeshAsset(), Left, Right));
			TestTrue(TEXT("and a physics asset to fall with"), Body->GetSkeletalMeshAsset()->GetPhysicsAsset() != nullptr);
		}
		TestTrue(TEXT("animated in code, by her set"), Body->AnimClass == UCheeseStrikeBelicaAnimInstance::StaticClass());
	}
	const ACheeseStrikeWeapon* Weapon = GetDefault<ACheeseStrikeWeapon>();
	for (const USkeletalMeshComponent* Rifle : {Weapon->GetFirstPersonMesh(), Weapon->GetThirdPersonMesh()})
	{
		if (TestNotNull(TEXT("a rifle"), Rifle) && TestNotNull(TEXT("with a mesh"), Rifle->GetSkeletalMeshAsset()))
		{
			TestEqual(TEXT("which is Lyra's"), Rifle->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Rifle")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheRagdollBendsAsABodyDoes, "CheeseStrike.Animation.TheRagdollBendsAsABodyDoes", AnimationTests::Flags)

bool FTheRagdollBendsAsABodyDoes::RunTest(const FString& Parameters)
{
	const USkeletalMesh* Body = GetDefault<ACheeseStrikeCharacter>()->GetMesh()->GetSkeletalMeshAsset();
	const UPhysicsAsset* Ragdoll = Body != nullptr ? Body->GetPhysicsAsset() : nullptr;
	if (!TestNotNull(TEXT("the body has a ragdoll"), Ragdoll))
	{
		return false;
	}
	// A body for each part of her that moves on its own; none under the head (the face and jaw would slide on the
	// skull) or for her own guns, which are hidden.
	for (const TCHAR* Bone : {TEXT("pelvis"), TEXT("spine_03"), TEXT("head"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("upperarm_r"), TEXT("lowerarm_r"),
		TEXT("hand_r"), TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")})
	{
		TestTrue(FString::Printf(TEXT("a body for %s"), Bone), Ragdoll->FindBodyIndex(Bone) != INDEX_NONE);
	}
	const FReferenceSkeleton& Skeleton = Body->GetRefSkeleton();
	const int32 Head = Skeleton.FindBoneIndex(TEXT("head"));
	for (const USkeletalBodySetup* Part : Ragdoll->SkeletalBodySetups)
	{
		const int32 Bone = Skeleton.FindBoneIndex(Part->BoneName);
		TestFalse(FString::Printf(TEXT("no body under the head (%s)"), *Part->BoneName.ToString()), Bone != Head && Skeleton.BoneIsChildOf(Bone, Head));
		TestFalse(FString::Printf(TEXT("no body for a gun (%s)"), *Part->BoneName.ToString()), Part->BoneName == TEXT("pistol") || Part->BoneName == TEXT("weapon"));
	}
	// Every joint limited every way, on hard limits, twisting no further than a person's; the knees and elbows
	// hinges; and the spine twisting little at each joint.
	float SpineTwist = 0.0f;
	for (const UPhysicsConstraintTemplate* Template : Ragdoll->ConstraintSetup)
	{
		const FConstraintProfileProperties& Joint = Template->DefaultInstance.ProfileInstance;
		const FString Bone = Template->DefaultInstance.ConstraintBone1.ToString();
		TestTrue(FString::Printf(TEXT("%s is limited every way"), *Bone), Joint.ConeLimit.Swing1Motion != ACM_Free && Joint.ConeLimit.Swing2Motion != ACM_Free && Joint.TwistLimit.TwistMotion != ACM_Free);
		TestFalse(FString::Printf(TEXT("%s on hard limits"), *Bone), Joint.ConeLimit.bSoftConstraint || Joint.TwistLimit.bSoftConstraint);
		TestTrue(FString::Printf(TEXT("%s twists no more than 40 degrees (%.0f)"), *Bone, Joint.TwistLimit.TwistLimitDegrees), Joint.TwistLimit.TwistLimitDegrees <= 40.0f);
		if (Bone.StartsWith(TEXT("lowerarm")) || Bone.StartsWith(TEXT("calf")))
		{
			TestTrue(FString::Printf(TEXT("%s is a hinge (%.0f to the side)"), *Bone, Joint.ConeLimit.Swing1LimitDegrees), Joint.ConeLimit.Swing1LimitDegrees <= 5.0f);
		}
		if (Bone.StartsWith(TEXT("spine")))
		{
			SpineTwist += Joint.TwistLimit.TwistLimitDegrees;
		}
	}
	TestTrue(FString::Printf(TEXT("the spine twists no more than 30 degrees in all (%.0f)"), SpineTwist), SpineTwist > 0.0f && SpineTwist <= 30.0f);
	return true;
}

#endif
