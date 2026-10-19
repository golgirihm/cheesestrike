// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The body's animation: the numbers it blends by, the animations it is made
// of, and that the character and the weapon are made of Lyra's mannequin
// and rifle.

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
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
	const UCheeseStrikeAnimInstance* Animation = GetDefault<UCheeseStrikeAnimInstance>();
	const FCheeseStrikeAnimSet& Set = Animation->GetSet();
	TestTrue(TEXT("every animation was found"), Set.IsComplete());
	TestNotNull(TEXT("standing"), Set.Idle.Get());
	TestNotNull(TEXT("crouched"), Set.CrouchIdle.Get());
	TestNotNull(TEXT("crouch walking"), Set.CrouchWalk.Get());
	TestNotNull(TEXT("aiming from the hip"), Set.AimHip.Get());
	TestNotNull(TEXT("firing"), Set.Fire.Get());
	TestNotNull(TEXT("reloading"), Set.Reload.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCharacterIsLyrasMannequin, "CheeseStrike.Animation.TheCharacterIsLyrasMannequin", AnimationTests::Flags)

bool FTheCharacterIsLyrasMannequin::RunTest(const FString& Parameters)
{
	const ACheeseStrikeCharacter* Character = GetDefault<ACheeseStrikeCharacter>();
	for (const USkeletalMeshComponent* Body : {Character->GetMesh(), Character->GetFirstPersonMesh()})
	{
		if (!TestNotNull(TEXT("a body"), Body))
		{
			continue;
		}
		TestNotNull(TEXT("has a mesh"), Body->GetSkeletalMeshAsset());
		if (Body->GetSkeletalMeshAsset() != nullptr)
		{
			TestEqual(TEXT("which is Lyra's Manny"), Body->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SKM_Manny")));
			TestTrue(TEXT("with a bone for the rifle"), Body->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(FName(TEXT("weapon_r"))) != INDEX_NONE);
			TestTrue(TEXT("and a physics asset to fall with"), Body->GetSkeletalMeshAsset()->GetPhysicsAsset() != nullptr);
		}
		TestTrue(TEXT("animated in code"), Body->AnimClass == UCheeseStrikeAnimInstance::StaticClass());
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

#endif
