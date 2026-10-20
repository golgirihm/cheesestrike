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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCharacterIsBelicaWithTheMannequinsArms, "CheeseStrike.Animation.TheCharacterIsBelicaWithTheMannequinsArms", AnimationTests::Flags)

bool FTheCharacterIsBelicaWithTheMannequinsArms::RunTest(const FString& Parameters)
{
	const ACheeseStrikeCharacter* Character = GetDefault<ACheeseStrikeCharacter>();
	struct FExpected
	{
		const USkeletalMeshComponent* Body;
		const TCHAR* Mesh;
		UClass* Animation;
		const TCHAR* RifleBone;
	};
	// Others see Lt. Belica; the player sees the mannequin's arms.
	const FExpected Bodies[] = {
		{Character->GetMesh(), TEXT("Belica"), UCheeseStrikeBelicaAnimInstance::StaticClass(), TEXT("hand_r")},
		{Character->GetFirstPersonMesh(), TEXT("SKM_Manny"), UCheeseStrikeAnimInstance::StaticClass(), TEXT("weapon_r")},
	};
	for (const FExpected& Expected : Bodies)
	{
		if (!TestNotNull(TEXT("a body"), Expected.Body))
		{
			continue;
		}
		TestNotNull(TEXT("has a mesh"), Expected.Body->GetSkeletalMeshAsset());
		if (Expected.Body->GetSkeletalMeshAsset() != nullptr)
		{
			TestEqual(TEXT("which is the one wanted"), Expected.Body->GetSkeletalMeshAsset()->GetName(), FString(Expected.Mesh));
			TestTrue(FString::Printf(TEXT("with a bone for the rifle (%s)"), Expected.RifleBone), Expected.Body->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(FName(Expected.RifleBone)) != INDEX_NONE);
			TestTrue(TEXT("and a physics asset to fall with"), Expected.Body->GetSkeletalMeshAsset()->GetPhysicsAsset() != nullptr);
		}
		TestTrue(TEXT("animated in code, by its own set"), Expected.Body->AnimClass == Expected.Animation);
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
