// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the arms in the editor (see PlayTesting.h): every hero's
// left hand on every weapon, in both bodies, before and after the knife;
// the weapon in its own player's view at the hip and down the sights, held
// steadier than the animations hold it; nothing of the body before the eyes
// at a sprint; a slide's sprawl and lean; the knife's swipe reaching someone
// off to the side, without a lunge; and the view of a player down kept out
// of the floor.

#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Animation/CheeseStrikeArmsPose.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Animation/CheeseStrikeGripIK.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerController.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "PlayTesting.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace ArmsPlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** Open ground on the shooter level, facing along it (as PlayTests' own spots), and a long run of it. */
	const FVector Spot(-231.0f, -700.0f, 100.0f);
	constexpr float SpotYaw = 90.0f;
	const FVector OpenGround(1150.0f, -1450.0f, 100.0f);
	constexpr float OpenGroundYaw = 117.0f;
	/** How far the middle of the left hand's closed fingers may be from where the weapon has it held (cm). */
	constexpr float HoldSlack = 3.5f;

	/** Remembered between steps. */
	FVector2D Tremble = FVector2D::ZeroVector;
	float StandingEye = 0.0f;
	float HealthBefore = 0.0f;

	ACheeseStrikeCharacter* Pawn(int32 Player)
	{
		ACheeseStrikeCharacter* Character = PlayTesting::Character(Player);
		if (Character == nullptr)
		{
			PlayTesting::Test()->AddError(FString::Printf(TEXT("player %d has no character"), Player));
		}
		return Character;
	}

	/** Its own player's view: from the camera, along the controls. */
	FTransform ViewOf(const ACheeseStrikeCharacter* Character)
	{
		return FTransform(Character->GetControlRotation(), Character->GetFirstPersonCameraComponent()->GetComponentLocation());
	}

	/** How far `Body`'s left hand closes from where the weapon in hand has it held (cm); 0 for a weapon held in one hand. */
	float LeftHandOff(const ACheeseStrikeCharacter* Character, const USkeletalMeshComponent* Body)
	{
		FVector Hold;
		const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
		if (Weapon == nullptr || !Weapon->LeftHandHoldFor(Body, Hold))
		{
			return 0.0f;
		}
		const FVector Wanted = Body->GetSocketTransform(TEXT("hand_r")).TransformPosition(Hold);
		FVector Palm = FVector::ZeroVector;
		for (const TCHAR* Bone : CheeseStrikeGrip::PalmBones)
		{
			Palm += Body->GetBoneLocation(FName(FString(Bone) + TEXT("_l")));
		}
		Palm /= UE_ARRAY_COUNT(CheeseStrikeGrip::PalmBones);
		return FVector::Dist(Palm, Wanted);
	}

	/** Checks both of `Character`'s bodies hold the weapon in hand with both hands, if it is held so; `What` says what is being held how. */
	void CheckLeftHand(const ACheeseStrikeCharacter* Character, const FString& What)
	{
		for (const USkeletalMeshComponent* Body : {Character->GetFirstPersonMesh(), Character->GetMesh()})
		{
			const float Off = LeftHandOff(Character, Body);
			PlayTesting::Test()->TestTrue(FString::Printf(TEXT("%s: the left hand closes on its hold in %s (%.1f cm off)"), *What, Body == Character->GetMesh() ? TEXT("the body others see") : TEXT("its own view"), Off), Off < HoldSlack);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheArmsHoldEveryWeapon, "CheeseStrike.Play.TheArmsHoldEveryWeapon", ArmsPlayTests::Flags)

bool FTheArmsHoldEveryWeapon::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ArmsPlayTests;
	StartPlaying({1});
	PutAt(0, Spot, SpotYaw);
	for (int32 Hero = 0; Hero < CheeseStrikeBodies::All().Num(); ++Hero)
	{
		const FString Who = CheeseStrikeBodies::All()[Hero].Name;
		for (const FWeaponStats* Primary : CheeseStrikeArsenal::All())
		{
			if (Primary->Slot != EWeaponSlot::Primary)
			{
				continue;
			}
			const FName Id = Primary->Id;
			Play(0, FString::Printf(TEXT("cmd CheeseStrikeBody %d %d\ncmd CheeseStrikeArm %s\nwait 0.4"), Hero, Hero % 2, *Id.ToString()));
			Then([this, Who, Id]()
			{
				const ACheeseStrikeCharacter* Character = Pawn(0);
				if (Character == nullptr || Character->GetWeapon() == nullptr)
				{
					return;
				}
				const FString What = FString::Printf(TEXT("%s with the %s"), *Who, *Id.ToString());
				TestEqual(What + TEXT(": in hand"), Character->GetWeapon()->GetWeaponId(), Id);
				CheckLeftHand(Character, What);
				// At the hip the hand on its grip is low on the right of the view, ahead of the eye.
				const FVector At = ViewOf(Character).InverseTransformPosition(Character->GetFirstPersonMesh()->GetBoneLocation(TEXT("hand_r")));
				TestTrue(FString::Printf(TEXT("%s: held low on the right at the hip (%.0f, %.0f, %.0f)"), *What, At.X, At.Y, At.Z), At.X > 0.0f && At.X < 60.0f && At.Y > 2.0f && At.Y < 35.0f && At.Z < -2.0f && At.Z > -45.0f);
			});
		}
		// The pistol, held in both hands; the knife in one, the other up on guard.
		Play(0, TEXT("tap Sidearm\nwait 0.7"));
		Then([this, Who]()
		{
			if (const ACheeseStrikeCharacter* Character = Pawn(0))
			{
				TestEqual(Who + TEXT(": the pistol in hand"), Character->GetWeapon()->GetWeaponId(), CheeseStrikeWeapons::Pistol().Id);
				CheckLeftHand(Character, Who + TEXT(" with the pistol"));
			}
		});
		Play(0, TEXT("tap Knife\nwait 0.7"));
		Then([this, Who]()
		{
			if (const ACheeseStrikeCharacter* Character = Pawn(0))
			{
				FVector Hold;
				TestFalse(Who + TEXT(": the knife is held in one hand"), Character->GetWeapon()->LeftHandHoldFor(Character->GetMesh(), Hold));
				const FVector At = ViewOf(Character).InverseTransformPosition(Character->GetFirstPersonMesh()->GetBoneLocation(TEXT("hand_r")));
				TestTrue(FString::Printf(TEXT("%s: the knife hand on guard low on the right of the view (%.0f, %.0f, %.0f)"), *Who, At.X, At.Y, At.Z), At.X > 10.0f && At.Y > 0.0f && At.Z < 0.0f);
			}
		});
		// Back from the knife, the left hand takes hold again (it was left on guard, out to the side).
		Play(0, TEXT("tap Primary\nwait 0.8"));
		Then([this, Who]()
		{
			if (const ACheeseStrikeCharacter* Character = Pawn(0))
			{
				CheckLeftHand(Character, Who + TEXT(" back from the knife"));
			}
		});
	}
	// Down the sights the weapon's sight line runs through the eye.
	Play(0, TEXT("cmd CheeseStrikeBody 0 1\ncmd CheeseStrikeArm carbine\nwait 0.4\nhold Aim\nwait 0.8"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			FVector Sight;
			if (TestTrue(TEXT("the carbine has a sight"), Character->WeaponSight(Sight)))
			{
				const FVector Eye = ViewOf(Character).InverseTransformPosition(Character->GetWeapon()->GetModel(true)->GetComponentTransform().TransformPosition(Sight));
				TestTrue(FString::Printf(TEXT("aimed, the eye is on the sight line (%.1f, %.1f, %.1f)"), Eye.X, Eye.Y, Eye.Z), FMath::Abs(Eye.Y) < 1.0f && FMath::Abs(Eye.Z) < 1.0f && FMath::Abs(Eye.X) < 2.0f);
			}
			CheckLeftHand(Character, TEXT("the carbine aimed"));
		}
	});
	Play(0, TEXT("release Aim\nwait 0.3"));
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheWeaponIsSteadyInView, "CheeseStrike.Play.TheWeaponIsSteadyInView", ArmsPlayTests::Flags)

bool FTheWeaponIsSteadyInView::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ArmsPlayTests;
	StartPlaying({1});
	PutAt(0, Spot, SpotYaw);
	// Standing still, as the animations alone hold it (their compression shakes each joint a little, which a gun's
	// length shows), and as the arms hold it in view.
	Play(0, TEXT("tap Sidearm\nwait 0.8\ncmd CheeseStrike.HipHold 0\nwait 0.3"));
	Then([]() { if (ACheeseStrikeCharacter* Character = Pawn(0)) { Character->TakeViewTremble(); } });
	Wait(1.5f);
	Then([]() { if (ACheeseStrikeCharacter* Character = Pawn(0)) { Tremble = Character->TakeViewTremble(); } });
	Play(0, TEXT("cmd CheeseStrike.HipHold 1\nwait 0.5"));
	Then([]() { if (ACheeseStrikeCharacter* Character = Pawn(0)) { Character->TakeViewTremble(); } });
	Wait(1.5f);
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			const FVector2D Held = Character->TakeViewTremble();
			TestTrue(FString::Printf(TEXT("held in view, the pistol trembles less than the animation alone holds it (%.4f against %.4f degrees a frame)"), Held.Y, Tremble.Y), Held.Y < Tremble.Y * 0.6f);
			TestTrue(FString::Printf(TEXT("and shifts less (%.4f against %.4f cm a frame)"), Held.X, Tremble.X), Held.X < Tremble.X * 0.8f + 0.001f);
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSprintingAndSlidingShowRight, "CheeseStrike.Play.SprintingAndSlidingShowRight", ArmsPlayTests::Flags)

bool FSprintingAndSlidingShowRight::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ArmsPlayTests;
	StartPlaying({1});
	PutAt(0, OpenGround, OpenGroundYaw);
	Play(0, TEXT("cmd CheeseStrikeBody 1 0\nwait 0.5"));
	Then([]()
	{
		if (const ACheeseStrikeCharacter* Character = Pawn(0))
		{
			StandingEye = Character->GetFirstPersonCameraComponent()->GetComponentLocation().Z - Character->GetActorLocation().Z;
		}
	});
	// At a sprint, nothing of the body comes before the eyes (Murdock's armour is the biggest): only the arms and the weapon.
	Play(0, TEXT("hold Sprint\nhold Move 0 1\nwait 1"));
	Then([this]()
	{
		if (const ACheeseStrikeCharacter* Character = Pawn(0))
		{
			TestTrue(TEXT("sprinting"), Character->GetCheeseStrikeMovement()->IsSprinting());
			const FTransform View = ViewOf(Character);
			for (const TCHAR* Bone : {TEXT("spine_02"), TEXT("spine_03"), TEXT("neck_01"), TEXT("clavicle_l"), TEXT("clavicle_r")})
			{
				const FVector At = View.InverseTransformPosition(Character->GetFirstPersonMesh()->GetBoneLocation(Bone));
				TestTrue(FString::Printf(TEXT("sprinting, the body's %s stays behind the eyes (%.0f cm ahead)"), Bone, At.X), At.X < 2.0f);
			}
			const FVector Weapon = View.InverseTransformPosition(Character->GetWeapon()->GetModel(true)->GetComponentLocation());
			TestTrue(FString::Printf(TEXT("and the weapon is carried low in front (%.0f, %.0f, %.0f)"), Weapon.X, Weapon.Y, Weapon.Z), Weapon.X > 0.0f && Weapon.Z < 0.0f && Weapon.Z > -45.0f);
		}
	});
	// Crouching at a sprint slides: low, leaning into it, the legs out ahead.
	Play(0, TEXT("hold Crouch\nwait 0.3"));
	Then([this]()
	{
		if (const ACheeseStrikeCharacter* Character = Pawn(0))
		{
			TestTrue(TEXT("sliding"), Character->GetCheeseStrikeMovement()->IsSliding());
			const float Eye = Character->GetFirstPersonCameraComponent()->GetComponentLocation().Z - (Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const float Standing = StandingEye + CheeseStrikeMovement::CapsuleHalfHeight;
			TestTrue(FString::Printf(TEXT("the view sits low (%.0f cm up, against %.0f standing)"), Eye, Standing), Eye < Standing - 50.0f);
			TestTrue(FString::Printf(TEXT("leaning into the slide (%.1f degrees)"), Character->GetViewRoll()), Character->GetViewRoll() < -2.0f);
			// The body others see: its right foot well out ahead of its hips.
			const USkeletalMeshComponent* Body = Character->GetMesh();
			const FVector Forward = Character->GetActorForwardVector();
			const float Out = FVector::DotProduct(Body->GetBoneLocation(TEXT("foot_r")) - Body->GetBoneLocation(TEXT("pelvis")), Forward);
			TestTrue(FString::Printf(TEXT("the body's leg out ahead of it (%.0f cm)"), Out), Out > 45.0f);
		}
	});
	Play(0, TEXT("release Crouch\nrelease Move\nrelease Sprint\nwait 0.8"));
	Then([this]()
	{
		if (const ACheeseStrikeCharacter* Character = Pawn(0))
		{
			TestTrue(FString::Printf(TEXT("the view rights itself after (%.1f degrees)"), Character->GetViewRoll()), FMath::Abs(Character->GetViewRoll()) < 0.5f);
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheKnifeSwipesAcross, "CheeseStrike.Play.TheKnifeSwipesAcross", ArmsPlayTests::Flags)

bool FTheKnifeSwipesAcross::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ArmsPlayTests;
	StartPlaying({2});
	// The host stands a step and a half ahead of the joiner, who looks 35 degrees off to the side of them.
	const FVector Victim(-231.0f, -555.0f, 100.0f);
	PutAt(0, Victim, -90.0f);
	PutAt(1, Spot, SpotYaw + 35.0f);
	Play(1, TEXT("tap Knife\nwait 0.6"));
	Then([]()
	{
		if (ACheeseStrikeCharacter* Host = Pawn(0))
		{
			Host->EndProtection();
			HealthBefore = Host->GetHealth();
		}
	});
	Play(1, TEXT("tap Fire\nwait 0.5"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Pawn(0);
		const ACheeseStrikeCharacter* Striker = CharacterOf(1, 0);
		if (Host != nullptr && Striker != nullptr)
		{
			TestTrue(FString::Printf(TEXT("a swipe reaches someone off to the side (%.0f taken)"), HealthBefore - Host->GetHealth()), FMath::IsNearlyEqual(HealthBefore - Host->GetHealth(), CheeseStrikeWeapons::Knife().Damage, 0.5f));
			TestTrue(FString::Printf(TEXT("without the striker lunging (%.0f cm)"), FVector::Dist2D(Striker->GetActorLocation(), Spot)), FVector::Dist2D(Striker->GetActorLocation(), Spot) < 10.0f);
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheDownViewKeepsOffTheFloor, "CheeseStrike.Play.TheDownViewKeepsOffTheFloor", ArmsPlayTests::Flags)

bool FTheDownViewKeepsOffTheFloor::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ArmsPlayTests;
	StartPlaying({1, TEXT("?Mode=Execution")});
	PutAt(0, Spot, SpotYaw);
	Play(0, TEXT("cmd CheeseStrikeDie\nwait 1.5"));
	// Down on the back, looking as far down, up and round as the cone lets, as each body: the view stays over the floor.
	for (const TCHAR* Look : {TEXT("look 0 -80"), TEXT("look 0 120"), TEXT("look 70 -60"), TEXT("look -140 0")})
	{
		Play(0, FString(Look) + TEXT("\nwait 0.5"));
		Then([this, Look]()
		{
			const ACheeseStrikeCharacter* Character = Pawn(0);
			if (Character == nullptr || !TestTrue(TEXT("down"), Character->IsDowned()))
			{
				return;
			}
			const FVector Eye = Character->GetFirstPersonCameraComponent()->GetComponentLocation();
			FHitResult Floor;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeTest), false, Character);
			const bool bFloor = Character->GetWorld()->LineTraceSingleByChannel(Floor, Eye + FVector(0.0f, 0.0f, 50.0f), Eye - FVector(0.0f, 0.0f, 200.0f), ECC_Camera, Params);
			const float Above = bFloor ? Eye.Z - Floor.ImpactPoint.Z : 200.0f;
			TestTrue(FString::Printf(TEXT("after %s, the view is over the floor (%.0f cm)"), Look, Above), Above >= ACheeseStrikeCharacter::DownViewHeight - 2.0f);
		});
	}
	StopPlaying();
	return true;
}

#endif
