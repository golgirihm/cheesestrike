// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the game in the editor (see PlayTesting.h): the character
// moving and firing as the rules say, and two players killing, coming back
// and ending a match over the network.

#include "Misc/AutomationTest.h"
#include "AnimationRuntime.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Materials/MaterialInterface.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Sessions/CheeseStrikeSessionSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"
#include "Weapons/CheeseStrikeShot.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace PlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** Open ground on the shooter level: a long clear run from here, this way. */
	const FVector OpenGround(1150.0f, -1450.0f, 100.0f);
	constexpr float OpenGroundYaw = 117.0f;
	/** Two spots facing each other across 4 m of clear ground. */
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	constexpr float HostYaw = -90.0f;
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);
	constexpr float JoinerYaw = 90.0f;

	/**
	 * How far a body's face has moved on its head from where its pose puts it: of Belica's face
	 * (faceAttach) and jaw (C_jaw), the most either is turned (degrees) from its place against what it
	 * hangs from, as the reference pose has it. -1 if the body has no such bones.
	 */
	float FaceOutOfPlace(const ACheeseStrikeCharacter* Character)
	{
		const USkeletalMeshComponent* Mesh = Character != nullptr ? Character->GetMesh() : nullptr;
		const USkeletalMesh* Model = Mesh != nullptr ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (Model == nullptr)
		{
			return -1.0f;
		}
		const FReferenceSkeleton& Skeleton = Model->GetRefSkeleton();
		float Most = -1.0f;
		for (const TCHAR* Name : {TEXT("faceAttach"), TEXT("C_jaw")})
		{
			const int32 Bone = Skeleton.FindBoneIndex(Name);
			if (Bone == INDEX_NONE)
			{
				return -1.0f;
			}
			const FName Parent = Skeleton.GetBoneName(Skeleton.GetParentIndex(Bone));
			const FQuat Relative = Mesh->GetBoneQuaternion(Parent, EBoneSpaces::ComponentSpace).Inverse() * Mesh->GetBoneQuaternion(Name, EBoneSpaces::ComponentSpace);
			const FQuat Off = Skeleton.GetRefBonePose()[Bone].GetRotation().Inverse() * Relative;
			Most = FMath::Max(Most, FMath::RadiansToDegrees(Off.AngularDistance(FQuat::Identity)));
		}
		return Most;
	}

	/**
	 * How far a fallen body's joints are past what its ragdoll allows (degrees): of every joint, the most its
	 * twist about the bone, its bend or its lean to the side is beyond its limit, and the joint in `Worst`. 0 if
	 * all are within.
	 */
	float JointsPastLimits(const ACheeseStrikeCharacter* Character, FString& Worst)
	{
		const USkeletalMeshComponent* Mesh = Character != nullptr ? Character->GetMesh() : nullptr;
		const UPhysicsAsset* Ragdoll = Mesh != nullptr ? Mesh->GetPhysicsAsset() : nullptr;
		float Most = 0.0f;
		if (Ragdoll == nullptr)
		{
			Worst = TEXT("no ragdoll");
			return 180.0f;
		}
		for (const UPhysicsConstraintTemplate* Template : Ragdoll->ConstraintSetup)
		{
			const FConstraintInstance& Joint = Template->DefaultInstance;
			const FConstraintProfileProperties& Profile = Joint.ProfileInstance;
			FTransform Child = Mesh->GetBoneTransform(Mesh->GetBoneIndex(Joint.ConstraintBone1));
			FTransform Parent = Mesh->GetBoneTransform(Mesh->GetBoneIndex(Joint.ConstraintBone2));
			Child.RemoveScaling();
			Parent.RemoveScaling();
			// The child's frame as the parent's sees it: twist about X, swing about Y (Swing2) and Z (Swing1).
			const FQuat Relative = (Joint.GetRefFrame(EConstraintFrame::Frame2) * Parent).GetRotation().Inverse() * (Joint.GetRefFrame(EConstraintFrame::Frame1) * Child).GetRotation();
			FQuat Swing;
			FQuat Twist;
			Relative.ToSwingTwist(FVector::ForwardVector, Swing, Twist);
			const float Twisted = FMath::Abs(FMath::RadiansToDegrees(Twist.GetTwistAngle(FVector::ForwardVector)));
			FVector Axis;
			float Swung;
			Swing.ToAxisAndAngle(Axis, Swung);
			Swung = FMath::RadiansToDegrees(Swung);
			// The engine limits the swing's two parts each to its own limit: the bend, about Y, to Swing2, and the lean
			// to the side, about Z, to Swing1.
			const float Bent = Swung * Axis.Y;
			const float Leant = Swung * Axis.Z;
			const float Past = FMath::Max3(Twisted - Profile.TwistLimit.TwistLimitDegrees, FMath::Abs(Bent) - Profile.ConeLimit.Swing2LimitDegrees, FMath::Abs(Leant) - Profile.ConeLimit.Swing1LimitDegrees);
			if (Past > Most)
			{
				Most = Past;
				Worst = FString::Printf(TEXT("%s, twisted %.0f of %.0f, bent %.0f of %.0f and leant %.0f of %.0f"), *Joint.ConstraintBone1.ToString(),
					Twisted, Profile.TwistLimit.TwistLimitDegrees, Bent, Profile.ConeLimit.Swing2LimitDegrees, Leant, Profile.ConeLimit.Swing1LimitDegrees);
			}
		}
		return Most;
	}

	/** How far a body's chest (spine_03) is twisted round against its hips (pelvis), about the spine, from how they stand in its reference pose (degrees); -1 if it has no such bones. */
	float SpineTwist(const ACheeseStrikeCharacter* Character)
	{
		const USkeletalMeshComponent* Mesh = Character != nullptr ? Character->GetMesh() : nullptr;
		const USkeletalMesh* Model = Mesh != nullptr ? Mesh->GetSkeletalMeshAsset() : nullptr;
		const int32 Hips = Model != nullptr ? Model->GetRefSkeleton().FindBoneIndex(TEXT("pelvis")) : INDEX_NONE;
		const int32 Chest = Model != nullptr ? Model->GetRefSkeleton().FindBoneIndex(TEXT("spine_03")) : INDEX_NONE;
		if (Hips == INDEX_NONE || Chest == INDEX_NONE)
		{
			return -1.0f;
		}
		const FTransform HipsAsMade = FAnimationRuntime::GetComponentSpaceTransformRefPose(Model->GetRefSkeleton(), Hips);
		const FTransform ChestAsMade = FAnimationRuntime::GetComponentSpaceTransformRefPose(Model->GetRefSkeleton(), Chest);
		const FQuat Now = Mesh->GetBoneQuaternion(TEXT("pelvis"), EBoneSpaces::ComponentSpace).Inverse() * Mesh->GetBoneQuaternion(TEXT("spine_03"), EBoneSpaces::ComponentSpace);
		const FQuat AsMade = HipsAsMade.GetRotation().Inverse() * ChestAsMade.GetRotation();
		// The turn from as made to now, in the hips' space, about the line from the hips to the chest.
		const FVector Spine = HipsAsMade.InverseTransformVectorNoScale(ChestAsMade.GetLocation() - HipsAsMade.GetLocation()).GetSafeNormal();
		return FMath::Abs(FMath::RadiansToDegrees((Now * AsMade.Inverse()).GetTwistAngle(Spine)));
	}

	/** Whether a fallen body has come to rest: none of its bodies moving faster than 5 cm/s. */
	bool AtRest(const ACheeseStrikeCharacter* Character)
	{
		const USkeletalMeshComponent* Mesh = Character != nullptr ? Character->GetMesh() : nullptr;
		if (Mesh == nullptr || Mesh->Bodies.IsEmpty())
		{
			return false;
		}
		for (const FBodyInstance* Body : Mesh->Bodies)
		{
			if (Body != nullptr && Body->GetUnrealWorldVelocity().Size() > 5.0f)
			{
				return false;
			}
		}
		return true;
	}

	bool Near(float Value, float Wanted, float Within)
	{
		return FMath::Abs(Value - Wanted) <= Within;
	}

	/** The character player `Player` has, or an error on the test. */
	ACheeseStrikeCharacter* Pawn(int32 Player)
	{
		ACheeseStrikeCharacter* Character = PlayTesting::Character(Player);
		if (Character == nullptr)
		{
			PlayTesting::Test()->AddError(FString::Printf(TEXT("player %d has no character"), Player));
		}
		return Character;
	}

	/** How far `At` is from the nearest player start in `World`. */
	float FromNearestStart(UWorld* World, const FVector& At)
	{
		float Nearest = TNumericLimits<float>::Max();
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist2D(It->GetActorLocation(), At)));
		}
		return Nearest;
	}

	/** Remembered between steps. */
	TWeakObjectPtr<ACheeseStrikeCharacter> FirstBody;
	/** Whether the host, back after being killed, was protected when first seen back; unset until then. */
	TOptional<bool> bBackProtected;

	/** Notes how the host first comes back, which may be while the test waits for something else: protection lasts only a few seconds. */
	void WatchTheHostComeBack()
	{
		const ACheeseStrikeCharacter* Host = PlayTesting::Character(0);
		if (!bBackProtected.IsSet() && Host != nullptr && Host->IsAlive() && Host != FirstBody.Get())
		{
			bBackProtected = Host->IsProtected();
		}
	}
	/** `Of`'s fallen body as world `Seen` has it (not their next character, who may be back already), or null. */
	const ACheeseStrikeCharacter* FallenBodyOf(int32 Of, int32 Seen)
	{
		const APlayerController* Own = PlayTesting::Controller(Of);
		UWorld* Viewer = PlayTesting::World(Seen);
		if (Own == nullptr || Own->PlayerState == nullptr || Viewer == nullptr)
		{
			return nullptr;
		}
		for (TActorIterator<ACheeseStrikeCharacter> It(Viewer); It; ++It)
		{
			if (!It->IsAlive() && It->GetPlayerState() != nullptr && It->GetPlayerState()->GetPlayerId() == Own->PlayerState->GetPlayerId())
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** The first body as the other machine sees it. */
	TWeakObjectPtr<const ACheeseStrikeCharacter> FirstBodySeen;
	int32 ShotsBefore = 0;
	float HeadAbove = 0.0f;
	/** Where the left hand was on the rifle down the sights, to check it stays there. */
	FVector LeftHandAimed = FVector::ZeroVector;

	/** How high `Character`'s head bone is above where it stands. */
	float HeadHeight(const ACheeseStrikeCharacter* Character)
	{
		return Character->GetMesh()->GetSocketLocation(FName(TEXT("head"))).Z - Character->GetMesh()->GetComponentLocation().Z;
	}

	/** The body's animation of `Character`'s mesh seen by everyone. */
	const UCheeseStrikeAnimInstance* BodyAnimation(const ACheeseStrikeCharacter* Character)
	{
		return Cast<UCheeseStrikeAnimInstance>(Character->GetMesh()->GetAnimInstance());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItWalksSprintsAndSlides, "CheeseStrike.Play.WalksSprintsAndSlides", PlayTests::Flags)

bool FItWalksSprintsAndSlides::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace CheeseStrikeMovement;
	StartPlaying({1});
	PutAt(0, PlayTests::OpenGround, PlayTests::OpenGroundYaw);
	Play(0, TEXT("hold Move 0 1\nwait 0.8\nstate"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			TestTrue(FString::Printf(TEXT("walks at walking speed (%.0f)"), Character->GetVelocity().Size2D()), PlayTests::Near(Character->GetVelocity().Size2D(), WalkSpeed, WalkSpeed * 0.1f));
			TestFalse(TEXT("standing"), Character->bIsCrouched);
			PlayTests::HeadAbove = PlayTests::HeadHeight(Character);
			if (const UCheeseStrikeAnimInstance* Animation = PlayTests::BodyAnimation(Character))
			{
				TestTrue(FString::Printf(TEXT("the body knows its speed (%.0f)"), Animation->Speed), PlayTests::Near(Animation->Speed, WalkSpeed, WalkSpeed * 0.15f));
				TestTrue(FString::Printf(TEXT("and that it goes forward (%.0f)"), Animation->Direction), FMath::Abs(Animation->Direction) < 10.0f);
			}
		}
	});
	// From the start again, so that the run that follows has the ground it needs.
	PutAt(0, PlayTests::OpenGround, PlayTests::OpenGroundYaw);
	Play(0, TEXT("hold Sprint\nwait 0.8\nstate"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			TestTrue(FString::Printf(TEXT("sprints at sprinting speed (%.0f)"), Character->GetVelocity().Size2D()), PlayTests::Near(Character->GetVelocity().Size2D(), WalkSpeed * SprintFactor, WalkSpeed * 0.1f));
			TestTrue(TEXT("and knows it"), Character->GetCheeseStrikeMovement()->IsSprinting());
		}
	});
	Play(0, TEXT("hold Crouch\nwait 0.25\nstate"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			TestTrue(TEXT("crouching from a sprint slides"), Character->GetCheeseStrikeMovement()->IsSliding());
			TestTrue(FString::Printf(TEXT("faster than a walk (%.0f)"), Character->GetVelocity().Size2D()), Character->GetVelocity().Size2D() > WalkSpeed);
		}
	});
	Play(0, TEXT("wait 1.5\nstate"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			TestFalse(TEXT("the slide is over"), Character->GetCheeseStrikeMovement()->IsSliding());
			TestTrue(TEXT("leaving it crouched"), Character->bIsCrouched);
			TestTrue(FString::Printf(TEXT("at a crouched walk (%.0f)"), Character->GetVelocity().Size2D()), PlayTests::Near(Character->GetVelocity().Size2D(), WalkSpeed * CrouchFactor, WalkSpeed * CrouchFactor * 0.15f));
			const float EyesAboveFloor = Character->GetEyeOffset() + Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			TestTrue(FString::Printf(TEXT("with its eyes lowered (%.0f above the floor)"), EyesAboveFloor), EyesAboveFloor < EyeHeight + CapsuleHalfHeight - 30.0f);
			TestTrue(FString::Printf(TEXT("and its head down with them (%.0f, from %.0f)"), PlayTests::HeadHeight(Character), PlayTests::HeadAbove), PlayTests::HeadHeight(Character) < PlayTests::HeadAbove - 20.0f);
			const UCheeseStrikeAnimInstance* Animation = PlayTests::BodyAnimation(Character);
			TestTrue(TEXT("the body knows it is crouched"), Animation != nullptr && Animation->bCrouched);
		}
	});
	Play(0, TEXT("release Crouch\nrelease Sprint\nrelease Move\nwait 0.8\nstate"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			TestTrue(FString::Printf(TEXT("let go, it stops (%.0f)"), Character->GetVelocity().Size2D()), Character->GetVelocity().Size2D() < 30.0f);
			TestFalse(TEXT("and stands"), Character->bIsCrouched);
		}
	});
	Play(0, TEXT("tap Jump\nwait 0.2\nstate"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			TestTrue(TEXT("jumping leaves the ground"), Character->GetCheeseStrikeMovement()->IsFalling());
			TestTrue(FString::Printf(TEXT("upwards (%.0f)"), Character->GetVelocity().Z), Character->GetVelocity().Z > 0.0f);
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItFiresAimsAndReloads, "CheeseStrike.Play.FiresAimsAndReloads", PlayTests::Flags)

bool FItFiresAimsAndReloads::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	StartPlaying({1});
	PutAt(0, PlayTests::JoinerSpot, PlayTests::JoinerYaw);
	Play(0, TEXT("hold Fire\nwait 0.6\nrelease Fire\nwait 0.2"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
			PlayTests::ShotsBefore = Weapon->ShotsFired;
			TestTrue(FString::Printf(TEXT("holding the trigger fires a burst (%d)"), Weapon->ShotsFired), Weapon->ShotsFired >= 4 && Weapon->ShotsFired <= 6);
			TestEqual(TEXT("each from the magazine"), Weapon->GetMagazine(), Weapon->GetStats().MagazineSize - Weapon->ShotsFired);
			TestEqual(TEXT("none from the reserve"), Weapon->GetReserve(), Weapon->GetStats().Reserve);
		}
	});
	Then([this]()
	{
		if (const ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			// The player looks out from just under the character's own eyes, with its head not drawn for them.
			USkeletalMeshComponent* Arms = Character->GetFirstPersonMesh();
			FName Left;
			FName Right;
			TestTrue(TEXT("the body has eyes"), ACheeseStrikeCharacter::FindEyes(Arms->GetSkeletalMeshAsset(), Left, Right));
			const FVector Eyes = (Arms->GetBoneLocation(Left) + Arms->GetBoneLocation(Right)) * 0.5;
			const float Off = FVector::Dist(Character->GetFirstPersonCameraComponent()->GetComponentLocation(), Eyes);
			TestTrue(FString::Printf(TEXT("the camera is just under the eyes (%.1f cm off)"), Off), Off > 4.0f && Off < 12.0f);
			TestTrue(TEXT("below them"), Character->GetFirstPersonCameraComponent()->GetComponentLocation().Z < Eyes.Z);
			const int32 Head = Arms->GetMaterialIndex(TEXT("M03_head"));
			TestTrue(TEXT("the first-person body has no head"), Head != INDEX_NONE && !Arms->IsMaterialSectionShown(Head, 0));
			TestTrue(TEXT("while the body others see has one"), Character->GetMesh()->IsMaterialSectionShown(Head, 0));
		}
	});
	Play(0, TEXT("hold Aim\nwait 0.6"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			const float Wanted = 90.0f * Character->GetWeapon()->GetStats().AimZoom;
			TestTrue(TEXT("aiming"), Character->GetWeapon()->IsAimed());
			TestTrue(FString::Printf(TEXT("narrows the view (%.0f)"), Character->GetFirstPersonCameraComponent()->FieldOfView), PlayTests::Near(Character->GetFirstPersonCameraComponent()->FieldOfView, Wanted, 3.0f));
		}
	});
	Then([]()
	{
		if (const ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			PlayTests::HeadAbove = PlayTests::HeadHeight(Character);
			PlayTests::LeftHandAimed = Character->LeftHandOnRifle();
		}
	});
	Play(0, TEXT("release Aim\nlook 0 -50\nwait 0.5"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			// The left hand keeps its grip on the rifle whatever the aim: where it was down the sights, it is looking down from the hip.
			const FVector LeftHand = Character->LeftHandOnRifle();
			TestTrue(FString::Printf(TEXT("the left hand stays on the rifle (%.1f, %.1f, %.1f against %.1f, %.1f, %.1f)"), LeftHand.X, LeftHand.Y, LeftHand.Z, PlayTests::LeftHandAimed.X, PlayTests::LeftHandAimed.Y, PlayTests::LeftHandAimed.Z), FVector::Dist(LeftHand, PlayTests::LeftHandAimed) < 3.0f);
			// Looking down bends the body down with the aim offset: both bodies know the pitch, and both rifles point down with it.
			for (const USkeletalMeshComponent* Body : {Character->GetMesh(), Character->GetFirstPersonMesh()})
			{
				const UCheeseStrikeAnimInstance* Animation = Cast<UCheeseStrikeAnimInstance>(Body->GetAnimInstance());
				if (TestNotNull(TEXT("the body is animated in code"), Animation))
				{
					TestTrue(FString::Printf(TEXT("and knows the aim pitch (%.0f)"), Animation->AimPitch), PlayTests::Near(Animation->AimPitch, -50.0f, 3.0f));
				}
			}
			for (const USkeletalMeshComponent* Rifle : {Character->GetWeapon()->GetThirdPersonMesh(), Character->GetWeapon()->GetFirstPersonMesh()})
			{
				// The rifle is modelled along its Y axis.
				const float BarrelPitch = FMath::RadiansToDegrees(FMath::Asin(Rifle->GetRightVector().Z));
				TestTrue(FString::Printf(TEXT("the rifle points down with the view (%.0f)"), BarrelPitch), BarrelPitch < -25.0f && BarrelPitch > -75.0f);
			}
		}
	});
	Play(0, TEXT("look 0 50\ntap Reload\nwait 0.3"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			TestTrue(TEXT("R reloads"), Character->GetWeapon()->IsReloading());
		}
	});
	Play(0, TEXT("wait 1.8"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = PlayTests::Pawn(0))
		{
			const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
			TestFalse(TEXT("and is done in the reload time"), Weapon->IsReloading());
			TestEqual(TEXT("with the magazine full"), Weapon->GetMagazine(), Weapon->GetStats().MagazineSize);
			TestEqual(TEXT("from the reserve"), Weapon->GetReserve(), Weapon->GetStats().Reserve - PlayTests::ShotsBefore);
			const FVector LeftHand = Character->LeftHandOnRifle();
			TestTrue(FString::Printf(TEXT("and the left hand back on the rifle, looking up (%.1f, %.1f, %.1f)"), LeftHand.X, LeftHand.Y, LeftHand.Z), FVector::Dist(LeftHand, PlayTests::LeftHandAimed) < 3.0f);
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAKillIsScoredAndTheKilledComeBack, "CheeseStrike.Play.AKillIsScoredAndTheKilledComeBack", PlayTests::Flags)

bool FAKillIsScoredAndTheKilledComeBack::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	StartPlaying({2, TEXT("?RespawnDelay=2")});
	PutAt(0, PlayTests::HostSpot, PlayTests::HostYaw);
	PutAt(1, PlayTests::JoinerSpot, PlayTests::JoinerYaw);
	Then([]() { PlayTests::FirstBody = Character(0); PlayTests::bBackProtected.Reset(); });
	// Both have just arrived, and can't be hurt yet.
	Wait(CheeseStrikeRules::SpawnProtection);
	Then([this]()
	{
		// Everyone for themself: on both machines, the two are different heroes, each in its own colours.
		for (int32 Machine = 0; Machine < 2; ++Machine)
		{
			const ACheeseStrikeCharacter* Host = CharacterOf(0, Machine);
			const ACheeseStrikeCharacter* Joiner = CharacterOf(1, Machine);
			if (TestNotNull(TEXT("both players are there"), Host) && TestNotNull(TEXT("both"), Joiner))
			{
				TestTrue(FString::Printf(TEXT("machine %d shows two heroes"), Machine), CheeseStrikeBodies::All().IsValidIndex(Host->GetBody()) && CheeseStrikeBodies::All().IsValidIndex(Joiner->GetBody()));
				TestNotEqual(TEXT("different ones"), Host->GetBody(), Joiner->GetBody());
				TestEqual(TEXT("in the host's own colours"), Host->GetBodyTeam(), static_cast<int32>(INDEX_NONE));
				TestEqual(TEXT("and the joiner's"), Joiner->GetBodyTeam(), static_cast<int32>(INDEX_NONE));
			}
		}
	});
	FireUntil(1, TEXT("the host dying"), 5.0f, []() { const ACheeseStrikeCharacter* Host = Character(0); return Host != nullptr && !Host->IsAlive(); });
	Then([this]()
	{
		ACheeseStrikeCharacter* Host = PlayTests::Pawn(0);
		ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		const ACheeseStrikeGameState* Match = GameState(0);
		if (Host == nullptr || !TestNotNull(TEXT("the server has the joiner"), Joiner) || !TestNotNull(TEXT("and the match"), Match))
		{
			return;
		}
		TestEqual(TEXT("the host was killed by the joiner"), Host->GetKilledBy(), Joiner->DisplayName());
		TestTrue(TEXT("and knows when"), Host->GetDiedAt() > 0.0f);
		TestEqual(TEXT("the kill is in the feed"), Match->GetKillFeed().Num(), 1);
		if (Match->GetKillFeed().Num() == 1)
		{
			TestEqual(TEXT("as the joiner's"), Match->GetKillFeed()[0].Killer, Joiner->DisplayName());
			TestEqual(TEXT("of the host"), Match->GetKillFeed()[0].Victim, Host->DisplayName());
		}
		TestEqual(TEXT("the joiner has a kill"), Joiner->GetPlayerState<ACheeseStrikePlayerState>()->GetKills(), 1);
		TestTrue(FString::Printf(TEXT("and a magazine's worth more in reserve (%d)"), Joiner->GetWeapon()->GetReserve()), Joiner->GetWeapon()->GetReserve() > Joiner->GetWeapon()->GetStats().Reserve);
		TestEqual(TEXT("and no deaths"), Joiner->GetPlayerState<ACheeseStrikePlayerState>()->GetDeaths(), 0);
		TestEqual(TEXT("the host a death"), Host->GetPlayerState<ACheeseStrikePlayerState>()->GetDeaths(), 1);
		TestEqual(TEXT("the joiner leads"), Match->Standings()[0].Name, Joiner->DisplayName());
		TestFalse(TEXT("and the match goes on"), Match->HasMatchEnded());
	});
	WaitUntil(TEXT("the kill reaching the joiner"), 5.0f, []() { PlayTests::WatchTheHostComeBack(); const ACheeseStrikeGameState* Match = GameState(1); return Match != nullptr && Match->GetKillFeed().Num() == 1; });
	// The host comes back two seconds after dying, which on a busy PC may be before the kill reaches the joiner: look at the body.
	WaitUntil(TEXT("the joiner seeing the host dead"), 5.0f, []() { PlayTests::WatchTheHostComeBack(); return PlayTests::FallenBodyOf(0, 1) != nullptr; });
	Then([this]()
	{
		const ACheeseStrikeCharacter* HostAsSeen = PlayTests::FallenBodyOf(0, 1);
		TestNotNull(TEXT("the joiner sees the host's body"), HostAsSeen);
		// The fallen body's face stays on its head, on both machines: the ragdoll moves the head, and the face and jaw follow it as posed.
		for (const ACheeseStrikeCharacter* Body : {static_cast<const ACheeseStrikeCharacter*>(PlayTests::FirstBody.Get()), HostAsSeen})
		{
			const float Off = PlayTests::FaceOutOfPlace(Body);
			TestTrue(FString::Printf(TEXT("the fallen body's face and jaw are in place (%.1f degrees out)"), Off), Off >= 0.0f && Off < 10.0f);
		}
		PlayTests::FirstBodySeen = HostAsSeen;
	});
	WaitUntil(TEXT("the fallen body coming to rest on both machines"), 12.0f, []() { PlayTests::WatchTheHostComeBack(); return PlayTests::AtRest(PlayTests::FirstBody.Get()) && PlayTests::AtRest(PlayTests::FirstBodySeen.Get()); });
	Then([this]()
	{
		// It lies as a body does, on both machines: no joint bent or twisted further than its ragdoll lets a person's
		// go, and the chest not twisted round on the hips.
		for (const ACheeseStrikeCharacter* Body : {static_cast<const ACheeseStrikeCharacter*>(PlayTests::FirstBody.Get()), PlayTests::FirstBodySeen.Get()})
		{
			FString Worst = TEXT("none");
			const float Past = PlayTests::JointsPastLimits(Body, Worst);
			TestTrue(FString::Printf(TEXT("the fallen body's joints are within their limits (%.1f degrees past, the most at %s)"), Past, *Worst), Past < 10.0f);
			const float Twist = PlayTests::SpineTwist(Body);
			AddInfo(FString::Printf(TEXT("A fallen body: its joints %.1f degrees past their limits at most (%s), its chest twisted %.0f degrees on its hips"), Past, *Worst, Twist));
			TestTrue(FString::Printf(TEXT("and its chest is not twisted round on its hips (%.0f degrees)"), Twist), Twist >= 0.0f && Twist < 35.0f);
		}
	});
	WaitUntil(TEXT("the host coming back"), 2.0f + 5.0f, []() { PlayTests::WatchTheHostComeBack(); return PlayTests::bBackProtected.IsSet(); });
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Host = PlayTests::Pawn(0))
		{
			TestEqual(TEXT("the host is back with full health"), Host->GetHealth(), CheeseStrikeWeapons::MaxHealth);
			TestTrue(TEXT("protected for the moment"), PlayTests::bBackProtected.Get(false));
			TestTrue(TEXT("holding a weapon"), Host->GetWeapon() != nullptr);
			TestTrue(FString::Printf(TEXT("at a player start (%.0f cm from one)"), PlayTests::FromNearestStart(World(0), Host->GetActorLocation())), PlayTests::FromNearestStart(World(0), Host->GetActorLocation()) < 150.0f);
			TestTrue(TEXT("while the match goes on"), !GameState(0)->HasMatchEnded());
		}
	});
	WaitUntil(TEXT("the joiner seeing the host back"), 5.0f, []() { const ACheeseStrikeCharacter* Host = CharacterOf(0, 1); return Host != nullptr && Host->IsAlive(); });
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheSidesLookDifferent, "CheeseStrike.Play.TheSidesLookDifferent", PlayTests::Flags)

bool FTheSidesLookDifferent::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	StartPlaying({2, TEXT("?Mode=Deathmatch")});
	PutAt(0, PlayTests::HostSpot, PlayTests::HostYaw);
	PutAt(1, PlayTests::JoinerSpot, PlayTests::JoinerYaw);
	Wait(1.0f);
	Then([this]()
	{
		// The two are on different sides, and look it on both machines: each a hero of their side's, in its colours.
		for (int32 Machine = 0; Machine < 2; ++Machine)
		{
			const ACheeseStrikeCharacter* Host = CharacterOf(0, Machine);
			const ACheeseStrikeCharacter* Joiner = CharacterOf(1, Machine);
			const ACheeseStrikeGameState* Match = GameState(Machine);
			if (!TestNotNull(TEXT("both players are there"), Host) || !TestNotNull(TEXT("both"), Joiner) || !TestNotNull(TEXT("in a match"), Match))
			{
				continue;
			}
			TestTrue(TEXT("with sides"), Match->HasTeams());
			const int32 HostTeam = Match->TeamOf(Host->GetPlayerState());
			const int32 JoinerTeam = Match->TeamOf(Joiner->GetPlayerState());
			TestNotEqual(TEXT("the host and the joiner are on different sides"), HostTeam, JoinerTeam);
			TestEqual(FString::Printf(TEXT("machine %d shows the host as one of their side's heroes"), Machine), CheeseStrikeBodies::TeamOf(Host->GetBody()), HostTeam);
			TestEqual(TEXT("and the joiner as one of theirs"), CheeseStrikeBodies::TeamOf(Joiner->GetBody()), JoinerTeam);
			TestEqual(TEXT("in their side's colours"), Host->GetBodyTeam(), HostTeam);
			TestEqual(TEXT("both of them"), Joiner->GetBodyTeam(), JoinerTeam);
			TestTrue(TEXT("so they look different"), Host->GetMesh()->GetSkeletalMeshAsset() != Joiner->GetMesh()->GetSkeletalMeshAsset());
			for (const ACheeseStrikeCharacter* Player : {Host, Joiner})
			{
				const UMaterialInterface* Dress = Player->GetMesh()->GetMaterial(0);
				const int32 Side = Player == Host ? HostTeam : JoinerTeam;
				TestTrue(FString::Printf(TEXT("each wears their side's colours (%s)"), Dress != nullptr ? *Dress->GetName() : TEXT("none")), Dress != nullptr && Side >= 0 && Dress->GetName().Contains(CheeseStrikeBodies::TeamWord(Side)));
			}
			// The first-person arms are the same body, holding the rifle as that body does.
			if (Machine == 0)
			{
				TestTrue(TEXT("the host's arms are their body's"), Host->GetFirstPersonMesh()->GetSkeletalMeshAsset() == Host->GetMesh()->GetSkeletalMeshAsset());
				const ACheeseStrikeWeapon* Weapon = Host->GetWeapon();
				if (TestNotNull(TEXT("holding a weapon"), Weapon))
				{
					const FTransform InHand = Weapon->GetThirdPersonMesh()->GetComponentTransform().GetRelativeTransform(Host->GetMesh()->GetSocketTransform(TEXT("hand_r")));
					const FTransform Wanted = CheeseStrikeBodies::All()[Host->GetBody()].HandGrip;
					TestTrue(TEXT("in the right hand, at their body's grip"), InHand.GetRotation().AngularDistance(Wanted.GetRotation()) < 0.05f);
				}
			}
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMatchEndsAtTheKillLimit, "CheeseStrike.Play.TheMatchEndsAtTheKillLimit", PlayTests::Flags)

bool FTheMatchEndsAtTheKillLimit::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	StartPlaying({2, TEXT("?KillLimit=1?RespawnDelay=1")});
	Then([this]()
	{
		if (const ACheeseStrikeGameState* Match = GameState(0))
		{
			TestEqual(TEXT("the host's kill limit is on"), Match->GetKillLimit(), 1);
			TestEqual(TEXT("and respawn delay"), Match->GetRespawnDelay(), 1.0f);
		}
	});
	PutAt(0, PlayTests::HostSpot, PlayTests::HostYaw);
	PutAt(1, PlayTests::JoinerSpot, PlayTests::JoinerYaw);
	Wait(CheeseStrikeRules::SpawnProtection);
	FireUntil(1, TEXT("the match ending"), 6.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && Match->HasMatchEnded(); });
	Then([this]()
	{
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		if (TestNotNull(TEXT("the server has the joiner"), Joiner))
		{
			TestEqual(TEXT("the joiner wins"), GameState(0)->GetWinnerName(), Joiner->DisplayName());
		}
	});
	WaitUntil(TEXT("the joiner hearing the match is over"), 5.0f, []() { const ACheeseStrikeGameState* Match = GameState(1); return Match != nullptr && Match->HasMatchEnded() && !Match->GetWinnerName().IsEmpty(); });
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Joiner = PlayTests::Pawn(1))
		{
			TestEqual(TEXT("and who won"), GameState(1)->GetWinnerName(), Joiner->DisplayName());
			PlayTests::ShotsBefore = Joiner->GetWeapon()->ShotsFired;
		}
	});
	// Past the respawn delay, nobody comes back; and nobody can fire.
	Wait(2.5f);
	Play(1, TEXT("hold Fire\nwait 0.5\nrelease Fire"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		TestTrue(TEXT("the host stays dead"), Host == nullptr || !Host->IsAlive());
		if (ACheeseStrikeCharacter* Joiner = PlayTests::Pawn(1))
		{
			TestEqual(TEXT("and the joiner can't fire"), Joiner->GetWeapon()->ShotsFired, PlayTests::ShotsBefore);
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAJoinerFindsAndJoinsTheHostsMatch, "CheeseStrike.Play.AJoinerFindsAndJoinsTheHostsMatch", PlayTests::Flags)

bool FAJoinerFindsAndJoinsTheHostsMatch::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	// Two players apart, each alone in the level; in the editor the online service is the Null one, so over the local network.
	StartPlaying({2, FString(), true});
	Then([this]()
	{
		UCheeseStrikeSessionSubsystem* Host = World(0)->GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>();
		if (TestNotNull(TEXT("the host has the session subsystem"), Host))
		{
			TestFalse(TEXT("and no match yet"), Host->IsInAMatch());
			Host->Host(TEXT("?KillLimit=3?StartNow"), true);
		}
	});
	WaitUntil(TEXT("the host listening on the level"), 30.0f, []()
	{
		UWorld* Hosts = World(0);
		const ACheeseStrikeCharacter* Pawn = Character(0);
		return Hosts != nullptr && Hosts->GetNetMode() == NM_ListenServer && Hosts->GetNetDriver() != nullptr && Pawn != nullptr && Pawn->IsAlive() && GameState(0) != nullptr && GameState(0)->HasMatchStarted();
	});
	// The host's answer to a search names the port it listens on, once it does.
	Wait(1.0f);
	Then([this]()
	{
		const UCheeseStrikeSessionSubsystem* Host = World(0)->GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>();
		TestTrue(TEXT("the host is in its match"), Host->IsInAMatch());
		TestEqual(TEXT("with the kill limit it asked for"), GameState(0)->GetKillLimit(), 3);
		if (UCheeseStrikeSessionSubsystem* Joiner = World(1)->GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
		{
			Joiner->Find(true);
		}
	});
	WaitUntil(TEXT("the joiner finding the match"), 30.0f, []()
	{
		UWorld* Theirs = World(1);
		const UCheeseStrikeSessionSubsystem* Joiner = Theirs != nullptr ? Theirs->GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
		return Joiner != nullptr && !Joiner->IsBusy() && Joiner->GetFound().Num() >= 1;
	});
	Then([this]()
	{
		UCheeseStrikeSessionSubsystem* Joiner = World(1)->GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>();
		TestEqual(TEXT("one match found"), Joiner->GetFound().Num(), 1);
		if (Joiner->GetFound().Num() == 1)
		{
			TestTrue(FString::Printf(TEXT("described with its rules (%s)"), *Joiner->GetFound()[0]), Joiner->GetFound()[0].Contains(TEXT("first to 3 kills")));
		}
		Joiner->Join(0);
	});
	WaitUntil(TEXT("the joiner being in the host's match"), 30.0f, []()
	{
		UWorld* Theirs = World(1);
		const ACheeseStrikeCharacter* Pawn = Character(1);
		return Theirs != nullptr && Theirs->GetNetMode() == NM_Client && Pawn != nullptr && Pawn->IsAlive() && Pawn->GetWeapon() != nullptr
			&& GameState(1) != nullptr && GameState(1)->PlayerArray.Num() == 2 && GameState(0) != nullptr && GameState(0)->PlayerArray.Num() == 2;
	});
	Then([this]()
	{
		TestEqual(TEXT("the joiner plays by the host's rules"), GameState(1)->GetKillLimit(), 3);
		TestTrue(TEXT("and is in the match"), World(1)->GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>()->IsInAMatch());
		TestNotNull(TEXT("the host sees the joiner"), CharacterOf(1, 0));
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAMatchWaitsInTheLobbyAndComesBackToIt, "CheeseStrike.Play.AMatchWaitsInTheLobbyAndComesBackToIt", PlayTests::Flags)

bool FAMatchWaitsInTheLobbyAndComesBackToIt::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	// A host and a joiner in the lobby of a match to five kills, which the host brings down to one from the lobby.
	StartPlaying({2, TEXT("?KillLimit=5?RespawnDelay=1"), false, true});
	Then([this]()
	{
		for (int32 Player = 0; Player < 2; ++Player)
		{
			TestNull(TEXT("nobody has a character in the lobby"), Character(Player));
			TestTrue(TEXT("and the menu shows the lobby"), Controller(Player) != nullptr && Controller(Player)->GetMenuPage() == EMenuPage::Lobby);
		}
		// The two are on different sides, and the sides have the bots a hosted match starts with, by name, the same on both machines.
		if (const ACheeseStrikeGameState* Match = GameState(0))
		{
			const TArray<CheeseStrikeRules::FRosterEntry> First = Match->TeamColumn(0);
			const TArray<CheeseStrikeRules::FRosterEntry> Second = Match->TeamColumn(1);
			TestEqual(TEXT("the sides have the players and seven bots"), First.Num() + Second.Num(), 2 + CheeseStrikeRules::DefaultBots);
			TestTrue(TEXT("dealt evenly"), FMath::Abs(First.Num() - Second.Num()) <= 1);
			TestTrue(TEXT("the host leads the first side"), First.Num() > 0 && !First[0].bBot);
			TestTrue(TEXT("the joiner the second"), Second.Num() > 0 && !Second[0].bBot);
			// No bots by default, so there are none to check: with some, the rest of each side are bots.
			TestTrue(TEXT("the rest are bots"), CheeseStrikeRules::DefaultBots == 0 || (First.Num() > 1 && First[1].bBot && Second.Num() > 1 && Second[1].bBot));
			TestEqual(TEXT("with names the match drew"), Match->GetBotNames().Num(), CheeseStrikeRules::TeamSize * CheeseStrikeRules::Teams);
			if (const ACheeseStrikeGameState* Theirs = GameState(1))
			{
				TestTrue(TEXT("which the joiner has too"), Theirs->GetBotNames() == Match->GetBotNames());
			}
		}
		if (ACheeseStrikeGameMode* Mode = World(0)->GetAuthGameMode<ACheeseStrikeGameMode>())
		{
			TestTrue(TEXT("the match waits for the host"), Mode->IsWaitingToStart());
			TestTrue(TEXT("who is named as the host"), GameState(0)->GetHostName() == GameState(0)->PlayerArray[0]->GetPlayerName());
			Mode->AskToStart(false);
			TestTrue(TEXT("who starts the countdown"), GameState(0)->IsCountingDown());
			TestTrue(FString::Printf(TEXT("of the design's length (%.1f)"), GameState(0)->SecondsToStart()), GameState(0)->SecondsToStart() > CheeseStrikeRules::MatchCountdown - 1.0f);
			Mode->CancelStart();
			TestFalse(TEXT("and may call it off"), GameState(0)->IsCountingDown());
			TestTrue(TEXT("back to waiting"), Mode->IsWaitingToStart());
			// The rules are the host's to change from the lobby.
			Controller(0)->SetRulesFromMenu(1, 1.0f);
			TestEqual(TEXT("the host sets the kill limit"), GameState(0)->GetKillLimit(), 1);
			TestEqual(TEXT("and the game mode has it"), Mode->GetKillLimit(), 1);
		}
	});
	WaitUntil(TEXT("the joiner seeing the new rules"), 3.0f, []() { return GameState(1) != nullptr && GameState(1)->GetKillLimit() == 1 && GameState(1)->GetHostName() == GameState(0)->GetHostName() && !GameState(1)->IsCountingDown(); });
	Then([this]()
	{
		if (ACheeseStrikeGameMode* Mode = World(0)->GetAuthGameMode<ACheeseStrikeGameMode>())
		{
			Mode->AskToStart(false);
			TestTrue(TEXT("the host starts the countdown again"), GameState(0)->IsCountingDown());
		}
	});
	WaitUntil(TEXT("the joiner seeing the countdown"), 3.0f, []() { return GameState(1) != nullptr && GameState(1)->IsCountingDown(); });
	WaitUntil(TEXT("the match starting with everyone in it"), CheeseStrikeRules::MatchCountdown + 10.0f, []()
	{
		return GameState(0) != nullptr && GameState(0)->HasMatchStarted() && Character(0) != nullptr && Character(1) != nullptr && Character(1)->GetWeapon() != nullptr;
	});
	Then([this]()
	{
		TestFalse(TEXT("the countdown is over"), GameState(0)->IsCountingDown());
		// The bots that filled in stand still from here: what follows is the players' pausing and shooting, not a fight.
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			It->StopMovement();
			It->SetActorTickEnabled(false);
		}
	});
	WaitUntil(TEXT("the menus going"), 3.0f, []() { return Controller(0) != nullptr && Controller(1) != nullptr && Controller(0)->GetMenuPage() == EMenuPage::None && Controller(1)->GetMenuPage() == EMenuPage::None; });
	PutAt(0, PlayTests::HostSpot, PlayTests::HostYaw);
	PutAt(1, PlayTests::JoinerSpot, PlayTests::JoinerYaw);
	Wait(CheeseStrikeRules::SpawnProtection);
	// A paused player's input goes nowhere: the host pauses, and firing does nothing.
	Then([this]() { Controller(0)->TogglePause(); });
	WaitUntil(TEXT("the host's pause menu"), 2.0f, []() { return Controller(0) != nullptr && Controller(0)->GetMenuPage() == EMenuPage::Pause; });
	Play(0, TEXT("hold Fire\nwait 0.5\nrelease Fire"));
	Wait(1.0f);
	Then([this]()
	{
		TestTrue(TEXT("paused, the host's firing does nothing"), Character(0) != nullptr && Character(0)->GetWeapon() != nullptr && Character(0)->GetWeapon()->ShotsFired == 0);
		TestFalse(TEXT("nor takes input"), Character(0) != nullptr && Character(0)->TakesInput());
		Controller(0)->TogglePause();
	});
	WaitUntil(TEXT("the host's pause menu going"), 2.0f, []() { return Controller(0) != nullptr && Controller(0)->GetMenuPage() == EMenuPage::None; });
	// The joiner wins, and after the results everyone is back in the lobby of the same match.
	FireUntil(1, TEXT("the match ending"), 6.0f, []() { return GameState(0) != nullptr && GameState(0)->HasMatchEnded(); });
	WaitUntil(TEXT("everyone being back in the lobby"), CheeseStrikeRules::ResultsTime + 15.0f, []()
	{
		const ACheeseStrikeGameState* Hosts = GameState(0);
		const ACheeseStrikeGameState* Joiners = GameState(1);
		return Hosts != nullptr && !Hosts->HasMatchStarted() && Hosts->PlayerArray.Num() == 2 && Character(0) == nullptr
			&& Joiners != nullptr && !Joiners->HasMatchStarted() && Joiners->PlayerArray.Num() == 2 && Character(1) == nullptr;
	});
	Then([this]()
	{
		TestEqual(TEXT("with the rules as the host set them, not as the match was hosted"), GameState(0)->GetKillLimit(), 1);
		TestTrue(TEXT("and the lobby up again"), Controller(1)->GetMenuPage() == EMenuPage::Lobby);
	});
	StopPlaying();
	return true;
}

#endif
