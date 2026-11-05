// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the arms in the editor (see PlayTesting.h): every hero's
// left hand on every weapon, in both bodies, before and after the knife; the
// Curd Pistol in the right hand alone; two guns either side of the
// crosshair, neither hand over it;
// the weapon in its own player's view at the hip and down the sights, held
// steadier than the animations hold it, and following a look without a jump
// at any frame rate; nothing of the body before the eyes at a sprint; a
// slide's sprawl and lean; the knife's swipe reaching someone off to the
// side, without a lunge; a body down lying on the floor, as every hero, on
// flat ground and against a wall, up and down a slope and by a step; and the
// view of a player down kept out of the floor.

#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Animation/CheeseStrikeArmsPose.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Animation/CheeseStrikeGripIK.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerController.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "PlayTesting.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rules/CheeseStrikeDowned.h"
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
	FVector AtWall = FVector::ZeroVector;

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
		// A handle held in the fist (the Cheese Stick): the fist's middle, round the handle; else the middle of the closed fingers.
		CheeseStrikeArms::FFist Fist;
		if (Weapon->IsSledge() && Body->GetSkeletalMeshAsset() != nullptr && CheeseStrikeArms::FistOfSkeleton(Body->GetSkeletalMeshAsset()->GetRefSkeleton(), false, Fist))
		{
			return FVector::Dist(Body->GetSocketTransform(TEXT("hand_l")).TransformPosition(Fist.Centre), Wanted);
		}
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
		// The pistol, held in the right hand alone, the left lowered by the side, out of view; the knife in one, the other
		// up on guard.
		Play(0, TEXT("tap Sidearm\nwait 0.9"));
		Then([this, Who]()
		{
			if (const ACheeseStrikeCharacter* Character = Pawn(0))
			{
				TestEqual(Who + TEXT(": the pistol in hand"), Character->GetWeapon()->GetWeaponId(), CheeseStrikeWeapons::Pistol().Id);
				FVector Hold;
				TestFalse(Who + TEXT(": the pistol is held in the right hand alone"), Character->GetWeapon()->LeftHandHoldFor(Character->GetMesh(), Hold));
				for (const USkeletalMeshComponent* Body : {Character->GetFirstPersonMesh(), Character->GetMesh()})
				{
					const float Below = Body->GetBoneLocation(TEXT("hand_r")).Z - Body->GetBoneLocation(TEXT("hand_l")).Z;
					TestTrue(FString::Printf(TEXT("%s with the pistol: the left hand let go, low by the side in %s (%.0f cm under the right)"), *Who, Body == Character->GetMesh() ? TEXT("the body others see") : TEXT("its own view"), Below), Below > 30.0f);
				}
				const FVector Left = ViewOf(Character).InverseTransformPosition(Character->GetFirstPersonMesh()->GetBoneLocation(TEXT("hand_l")));
				TestTrue(FString::Printf(TEXT("%s with the pistol: the left hand out of view (%.0f, %.0f, %.0f)"), *Who, Left.X, Left.Y, Left.Z), Left.X < 5.0f || Left.Z < -Left.X * 0.6f);
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
	// Two guns aimed, as every hero: the pair either side of the crosshair, evenly, and neither hand nor its fingers over
	// the middle of the view.
	for (int32 Hero = 0; Hero < CheeseStrikeBodies::All().Num(); ++Hero)
	{
		for (const TCHAR* Two : {TEXT("burratas"), TEXT("munster")})
		{
			Play(0, FString::Printf(TEXT("cmd CheeseStrikeBody %d %d\ncmd CheeseStrikeArm %s\nwait 0.6\nhold Aim\nwait 0.9"), Hero, Hero % 2, Two));
			Then([this, Hero, Two]()
			{
				const ACheeseStrikeCharacter* Character = Pawn(0);
				if (Character == nullptr)
				{
					return;
				}
				const FString What = FString::Printf(TEXT("%s with the %s aimed"), CheeseStrikeBodies::All()[Hero].Name, Two);
				const FTransform View = ViewOf(Character);
				const USkeletalMeshComponent* Body = Character->GetFirstPersonMesh();
				const FVector Right = View.InverseTransformPosition(Body->GetBoneLocation(TEXT("hand_r")));
				const FVector Left = View.InverseTransformPosition(Body->GetBoneLocation(TEXT("hand_l")));
				TestTrue(FString::Printf(TEXT("%s: the hands either side of the crosshair, evenly (%.1f and %.1f cm)"), *What, Right.Y, Left.Y), Right.Y > 3.0f && Left.Y < -3.0f && FMath::Abs(Right.Y + Left.Y) < 2.5f);
				float Nearest = 180.0f;
				FString NearestBone;
				for (const TCHAR* Side : {TEXT("_r"), TEXT("_l")})
				{
					for (const TCHAR* Bone : {TEXT("hand"), TEXT("thumb_02"), TEXT("thumb_03"), TEXT("index_02"), TEXT("index_03"), TEXT("middle_03")})
					{
						const FVector At = View.InverseTransformPosition(Body->GetBoneLocation(FName(FString(Bone) + Side)));
						const float Off = At.X > 0.0f ? FMath::RadiansToDegrees(FMath::Atan2(FVector2D(At.Y, At.Z).Size(), At.X)) : 180.0f;
						if (Off < Nearest)
						{
							Nearest = Off;
							NearestBone = FString(Bone) + Side;
						}
					}
				}
				TestTrue(FString::Printf(TEXT("%s: nothing of the hands near the crosshair (%s %.1f degrees off it)"), *What, *NearestBone, Nearest), Nearest > 10.0f);
			});
			Play(0, TEXT("release Aim\nwait 0.3"));
		}
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheWeaponFollowsTheLookWithoutReversing, "CheeseStrike.Play.TheWeaponFollowsTheLookWithoutReversing", ArmsPlayTests::Flags)

bool FTheWeaponFollowsTheLookWithoutReversing::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ArmsPlayTests;
	StartPlaying({1});
	PutAt(0, Spot, SpotYaw);
	// At 60 and at 144 frames a second (a fixed step, as tools/play.ps1 -FixedFps plays), with the pistol and then the
	// Blaster: a look up for a second at 0.3 degrees a frame, then the view held half a second; then the same down. The
	// weapon never jumps in view from one frame to the next (a dip as a look up begins and a pop as it ends, which arms
	// animated from the frame before the camera's gave: 0.18 cm at this look; the hold's own movement is under 0.04), and
	// through a look and after it its height in view goes one way (a smooth lag) but for the idle's slow breathing, which
	// the hold keeps (CheeseStrikeArms::MostBob) and which wanders up to half a centimetre back over a second's look.
	constexpr float MostJump = 0.08f;
	constexpr float MostBack = 1.0f;
	for (const int32 Fps : {60, 144})
	{
		Then([Fps]()
		{
			FApp::SetUseFixedTimeStep(true);
			FApp::SetFixedDeltaTime(1.0 / Fps);
		});
		for (const TCHAR* Draw : {TEXT("Sidearm"), TEXT("Primary")})
		{
			Play(0, FString::Printf(TEXT("tap %s\nwait 1"), Draw));
			for (const float Way : {1.0f, -1.0f})
			{
				// 0.12 of a mouse's count a frame is 0.3 degrees (CheeseStrikeInput::MouseDegreesPerCount), at any rate.
				const float Counts = 0.12f * Way;
				Then([]() { if (ACheeseStrikeCharacter* Character = Pawn(0)) { Character->TakeViewTrack(); } });
				Play(0, FString::Printf(TEXT("hold MouseLook 0 %.4f\nwait 1\nrelease MouseLook\nwait 0.5"), Counts));
				Then([this, Fps, Draw, Way]()
				{
					ACheeseStrikeCharacter* Character = Pawn(0);
					if (Character == nullptr)
					{
						return;
					}
					const TArray<ACheeseStrikeCharacter::FViewFrame> Track = Character->TakeViewTrack();
					const FString What = FString::Printf(TEXT("at %d fps, the %s, looking %s"), Fps, Draw, Way > 0.0f ? TEXT("up") : TEXT("down"));
					// The frames the view turned through, and those after it stopped.
					int32 First = INDEX_NONE;
					int32 Last = INDEX_NONE;
					for (int32 Frame = 1; Frame < Track.Num(); ++Frame)
					{
						if (FMath::Abs(Track[Frame].Pitch - Track[Frame - 1].Pitch) > 0.05f)
						{
							First = First == INDEX_NONE ? Frame - 1 : First;
							Last = Frame;
						}
					}
					if (!TestTrue(FString::Printf(TEXT("%s: the view turned for a second (%d frames of %d)"), *What, Last - First, Track.Num()), First != INDEX_NONE && Last - First > Fps * 0.8f && Track.Num() - Last > Fps * 0.3f))
					{
						return;
					}
					// The furthest the weapon went back against the way it was going, over each stretch.
					const auto FurthestBack = [&Track](int32 From, int32 To, float& OutMoved)
					{
						OutMoved = Track[To].WeaponAt.Z - Track[From].WeaponAt.Z;
						const float Direction = OutMoved >= 0.0f ? 1.0f : -1.0f;
						float Back = 0.0f;
						float Furthest = Track[From].WeaponAt.Z * Direction;
						for (int32 Frame = From; Frame <= To; ++Frame)
						{
							const float Along = Track[Frame].WeaponAt.Z * Direction;
							Furthest = FMath::Max(Furthest, Along);
							Back = FMath::Max(Back, Furthest - Along);
						}
						return Back;
					};
					float Jump = 0.0f;
					for (int32 Frame = First + 1; Frame < Track.Num(); ++Frame)
					{
						Jump = FMath::Max(Jump, FMath::Abs(Track[Frame].WeaponAt.Z - Track[Frame - 1].WeaponAt.Z));
					}
					TestTrue(FString::Printf(TEXT("%s: the weapon never jumps in view from one frame to the next (at most %.3f cm)"), *What, Jump), Jump < MostJump);
					float Moved = 0.0f;
					const float Through = FurthestBack(First, Last, Moved);
					TestTrue(FString::Printf(TEXT("%s: through the look the weapon's height in view goes one way but for its breathing (%.2f cm, at most %.3f back)"), *What, Moved, Through), Through < MostBack);
					const float After = FurthestBack(Last, Track.Num() - 1, Moved);
					TestTrue(FString::Printf(TEXT("%s: and settles one way after it (%.2f cm, at most %.3f back)"), *What, Moved, After), After < MostBack);
				});
			}
		}
	}
	Then([]() { FApp::SetUseFixedTimeStep(false); });
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

namespace ArmsPlayTests
{
	/** How far under the floor a bone of a body lying on it may be (cm). */
	constexpr float FloorSlack = 2.0f;
	/** Whether `Bone` of `Body` is one of the heroes' own guns' bones (hidden with their sections, or skinned into a body section: Murdock's sniper can), or hangs from one. */
	bool IsOwnGunBone(const USkeletalMeshComponent* Body, FName Bone)
	{
		for (FName Each = Bone; Each != NAME_None; Each = Body->GetParentBone(Each))
		{
			for (const TCHAR* Gun : CheeseStrikeDowned::OwnGunBones)
			{
				if (Each == FName(Gun))
				{
					return true;
				}
			}
		}
		return false;
	}

	/**
	 * Checks every bone of `Body` that its drawn parts are skinned to (not the hero's own guns, hidden with their sections
	 * and parked far from the body by Paragon's animators, nor a bone hidden for the view) lies on or over the floor under
	 * it, and inside no wall; `What` says whose body, where.
	 */
	void CheckOnTheFloor(const ACheeseStrikeCharacter* Character, USkeletalMeshComponent* Body, const FString& What)
	{
		FAutomationTestBase* Test = PlayTesting::Test();
		const FSkeletalMeshRenderData* Render = Body != nullptr ? Body->GetSkeletalMeshRenderData() : nullptr;
		if (Body == nullptr || Body->GetSkeletalMeshAsset() == nullptr || Render == nullptr || Render->LODRenderData.IsEmpty())
		{
			Test->AddError(What + TEXT(": no body"));
			return;
		}
		TSet<int32> Skinned;
		for (const FSkelMeshRenderSection& Section : Render->LODRenderData[0].RenderSections)
		{
			if (Body->IsMaterialSectionShown(Section.MaterialIndex, 0))
			{
				for (const FBoneIndexType Bone : Section.BoneMap)
				{
					Skinned.Add(Bone);
				}
			}
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeTest), false);
		for (TActorIterator<ACheeseStrikeCharacter> It(Character->GetWorld()); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		float Deepest = 0.0f;
		FString DeepestBone;
		TArray<FString> Under;
		for (int32 Bone = 0; Bone < Body->GetNumBones(); ++Bone)
		{
			const FName Name = Body->GetBoneName(Bone);
			if (!Skinned.Contains(Bone) || Body->IsBoneHiddenByName(Name) || IsOwnGunBone(Body, Name))
			{
				continue;
			}
			const FVector At = Body->GetBoneLocation(Name);
			FHitResult Floor;
			if (!Character->GetWorld()->LineTraceSingleByChannel(Floor, At + FVector(0.0f, 0.0f, 30.0f), At - FVector(0.0f, 0.0f, 200.0f), ECC_Visibility, Params))
			{
				continue;
			}
			const float Depth = Floor.bStartPenetrating ? 30.0f : Floor.ImpactPoint.Z - At.Z;
			if (Depth > FloorSlack)
			{
				Under.Add(FString::Printf(TEXT("%s %.1f%s"), *Name.ToString(), Depth, Floor.bStartPenetrating ? TEXT(" (inside something)") : TEXT("")));
			}
			if (Depth > Deepest)
			{
				Deepest = Depth;
				DeepestBone = Name.ToString();
				for (FName Parent = Body->GetParentBone(Name); Parent != NAME_None; Parent = Body->GetParentBone(Parent))
				{
					DeepestBone += TEXT(" < ") + Parent.ToString();
				}
			}
		}
		Test->TestTrue(FString::Printf(TEXT("%s: every bone lies on or over the floor (%s is %.1f cm under it%s%s)"), *What, *DeepestBone, Deepest, Under.Num() > 1 ? TEXT("; under: ") : TEXT(""), Under.Num() > 1 ? *FString::Join(Under, TEXT(", ")) : TEXT("")), Deepest <= FloorSlack);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FADownedBodyLiesOnTheFloor, "CheeseStrike.Play.ADownedBodyLiesOnTheFloor", ArmsPlayTests::Flags)

bool FADownedBodyLiesOnTheFloor::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ArmsPlayTests;
	StartPlaying({1, TEXT("?Mode=Execution")});
	// Down on flat ground, then against a wall (walked into until stopped by it, as downed_wall.txt does; going down backs
	// the body off it as far as the legs are short of room), as every hero: nothing of the body others see, nor of its own
	// player's, goes under the floor.
	for (const bool bWall : {false, true})
	{
		PutAt(0, Spot, bWall ? 0.0f : SpotYaw);
		if (bWall)
		{
			Play(0, TEXT("hold Move 0 1\nwait 1.5"));
			WaitUntil(TEXT("the host walking into a wall"), 20.0f, []()
			{
				const ACheeseStrikeCharacter* Host = Character(0);
				return Host != nullptr && FVector::Dist2D(Host->GetActorLocation(), Spot) > 200.0f && Host->GetVelocity().Size2D() < 5.0f;
			});
			Play(0, TEXT("release Move\nwait 0.3"));
			Then([this]()
			{
				if (const ACheeseStrikeCharacter* Host = Pawn(0))
				{
					AtWall = Host->GetActorLocation();
					UE_LOG(LogCheeseStrike, Display, TEXT("ADownedBodyLiesOnTheFloor: at the wall at (%.0f, %.0f, %.0f) facing %.0f, %.0f cm of room for the legs"), AtWall.X, AtWall.Y, AtWall.Z, Host->GetActorRotation().Yaw, Host->MeasureLegRoom());
					TestTrue(FString::Printf(TEXT("standing at the wall, the legs would be short of room (%.0f cm)"), Host->MeasureLegRoom()), Host->MeasureLegRoom() < CheeseStrikeDowned::LegLength);
				}
			});
		}
		Play(0, TEXT("cmd CheeseStrikeDie\nwait 1.6"));
		for (int32 Hero = 0; Hero < CheeseStrikeBodies::All().Num(); ++Hero)
		{
			Play(0, FString::Printf(TEXT("cmd CheeseStrikeBody %d %d\nwait 1.2"), Hero, Hero % 2));
			Then([this, Hero, bWall]()
			{
				const ACheeseStrikeCharacter* Host = Pawn(0);
				if (Host == nullptr || !TestTrue(TEXT("down"), Host->IsDowned()))
				{
					return;
				}
				const FString Where = bWall ? TEXT("against a wall") : TEXT("on flat ground");
				const FString Who = CheeseStrikeBodies::All()[Hero].Name;
				if (bWall)
				{
					// Going down backed the body off the wall as far as the legs were short of room (MakeRoomToLieDown), or
					// as far as there was room behind, and the legs lie out as far as there is room.
					const float BackedOff = FVector::DotProduct(AtWall - Host->GetActorLocation(), FVector::ForwardVector);
					TestTrue(FString::Printf(TEXT("%s %s: the body backed off the wall (%.0f cm) and the legs lie out as far as there is room (%.0f cm)"), *Who, *Where, BackedOff, Host->GetDownLegRoom()), BackedOff > 30.0f && Host->GetDownLegRoom() < CheeseStrikeDowned::LegLength + CheeseStrikeDowned::ToeGap + 15.0f);
				}
				CheckOnTheFloor(Host, Host->GetMesh(), FString::Printf(TEXT("%s %s, the body others see"), *Who, *Where));
				CheckOnTheFloor(Host, Host->GetFirstPersonMesh(), FString::Printf(TEXT("%s %s, in its own view"), *Who, *Where));
			});
		}
		Play(0, TEXT("cmd CheeseStrikeBody\ncmd CheeseStrikeRevive\nwait 1.5"));
	}
	StopPlaying();
	return true;
}

namespace ArmsPlayTests
{
	/** The floor under the host standing at Spot, and the blocks put down for a test, gone after it. */
	float SpotFloor = 0.0f;
	TArray<TWeakObjectPtr<AActor>> Blocks;

	/** A block of `Size` (cm) with its middle at `At`, turned `Turn`, that stops players as the floor does. */
	void PutBlock(const FVector& At, const FRotator& Turn, const FVector& Size)
	{
		const ACheeseStrikeCharacter* Host = PlayTesting::Character(0);
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (Host == nullptr || Cube == nullptr)
		{
			PlayTesting::Test()->AddError(TEXT("no world or no cube for a block"));
			return;
		}
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Block = Host->GetWorld()->SpawnActor<AStaticMeshActor>(At, Turn, Parameters);
		Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Block->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Block->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Block->SetActorScale3D(Size / 100.0f);
		Blocks.Add(Block);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FADownedBodyLiesOnASlopeOrAStep, "CheeseStrike.Play.ADownedBodyLiesOnASlopeOrAStep", ArmsPlayTests::Flags)

bool FADownedBodyLiesOnASlopeOrAStep::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ArmsPlayTests;
	StartPlaying({1, TEXT("?Mode=Execution")});
	PutAt(0, Spot, SpotYaw);
	Wait(0.5f);
	Then([]()
	{
		if (const ACheeseStrikeCharacter* Host = Pawn(0))
		{
			SpotFloor = Host->GetActorLocation().Z - Host->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
	});
	// Down on a slope of 20 degrees, the legs up it and down it; with a step of 30 cm just behind; and on a step's edge, the
	// legs out over the drop; as every hero: nothing of the body others see, nor of its own player's, under the floor.
	struct FCase
	{
		const TCHAR* Where;
		float Yaw;
		/** The block: its middle from Spot on the floor (cm: along the facing, up), its pitch, and its size along the facing, across and up. */
		FVector Middle;
		float Pitch;
		FVector Size;
		/** Where the host stands, from Spot: up from the floor. */
		float StandUp;
	};
	const FCase Cases[] = {
		{TEXT("with the legs up a slope"), SpotYaw, FVector(0.0f, 0.0f, 60.0f), 20.0f, FVector(500.0f, 300.0f, 20.0f), 70.0f},
		{TEXT("with the legs down a slope"), SpotYaw + 180.0f, FVector(0.0f, 0.0f, 60.0f), 20.0f, FVector(500.0f, 300.0f, 20.0f), 70.0f},
		{TEXT("with a step just behind"), SpotYaw, FVector(-187.0f, 0.0f, 15.0f), 0.0f, FVector(300.0f, 300.0f, 30.0f), 2.0f},
		{TEXT("on a step's edge, the legs over the drop"), SpotYaw, FVector(-140.0f, 0.0f, 15.0f), 0.0f, FVector(300.0f, 300.0f, 30.0f), 32.0f},
	};
	for (const FCase& Case : Cases)
	{
		Then([Case]()
		{
			// The slope rises along the way the spot faces (+Y), whichever way the host lies on it.
			const FVector Along = FRotator(0.0f, SpotYaw, 0.0f).Vector();
			const FVector Floor(Spot.X, Spot.Y, SpotFloor);
			const float Offset = Case.Yaw == SpotYaw ? 1.0f : -1.0f;
			PutBlock(Floor + Along * Case.Middle.X * Offset + FVector(0.0f, 0.0f, Case.Middle.Z) - FVector(0.0f, 0.0f, Case.Size.Z * 0.5f * FMath::Cos(FMath::DegreesToRadians(Case.Pitch))),
				FRotator(Case.Pitch, SpotYaw, 0.0f), Case.Size);
		});
		Then([Case]()
		{
			if (ACheeseStrikeCharacter* Host = Pawn(0))
			{
				Host->TeleportTo(FVector(Spot.X, Spot.Y, SpotFloor + Case.StandUp + Host->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.0f), FRotator(0.0f, Case.Yaw, 0.0f));
				if (AController* Controller = Host->GetController())
				{
					Controller->SetControlRotation(FRotator(0.0f, Case.Yaw, 0.0f));
				}
			}
		});
		Play(0, TEXT("wait 0.8\ncmd CheeseStrikeDie\nwait 1.6"));
		for (int32 Hero = 0; Hero < CheeseStrikeBodies::All().Num(); ++Hero)
		{
			Play(0, FString::Printf(TEXT("cmd CheeseStrikeBody %d %d\nwait 1.2"), Hero, Hero % 2));
			Then([this, Hero, Case]()
			{
				const ACheeseStrikeCharacter* Host = Pawn(0);
				if (Host == nullptr || !TestTrue(TEXT("down"), Host->IsDowned()))
				{
					return;
				}
				const FString Who = CheeseStrikeBodies::All()[Hero].Name;
				UE_LOG(LogCheeseStrike, Display, TEXT("ADownedBodyLiesOnASlopeOrAStep: %s %s at (%.0f, %.0f, %.0f) facing %.0f"), *Who, Case.Where, Host->GetActorLocation().X, Host->GetActorLocation().Y, Host->GetActorLocation().Z, Host->GetActorRotation().Yaw);
				TestFalse(FString::Printf(TEXT("%s %s: the floor round the body measured not flat"), *Who, Case.Where), Host->GetDownFloor().IsFlat());
				CheckOnTheFloor(Host, Host->GetMesh(), FString::Printf(TEXT("%s %s, the body others see"), *Who, Case.Where));
				CheckOnTheFloor(Host, Host->GetFirstPersonMesh(), FString::Printf(TEXT("%s %s, in its own view"), *Who, Case.Where));
			});
		}
		Play(0, TEXT("cmd CheeseStrikeBody\ncmd CheeseStrikeRevive\nwait 1.5"));
		Then([]()
		{
			for (const TWeakObjectPtr<AActor>& Block : Blocks)
			{
				if (Block.IsValid())
				{
					Block->Destroy();
				}
			}
			Blocks.Reset();
		});
	}
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
