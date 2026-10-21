// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the game in the editor (see PlayTesting.h): the character
// moving and firing as the rules say, and two players killing, coming back
// and ending a match over the network.

#include "Misc/AutomationTest.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Sessions/CheeseStrikeSessionSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
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
	Then([]() { PlayTests::FirstBody = Character(0); });
	// Both have just arrived, and can't be hurt yet.
	Wait(CheeseStrikeRules::SpawnProtection);
	Play(1, TEXT("hold Fire\nwait 1.2\nrelease Fire"));
	WaitUntil(TEXT("the host dying"), 5.0f, []() { const ACheeseStrikeCharacter* Host = Character(0); return Host != nullptr && !Host->IsAlive(); });
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
	WaitUntil(TEXT("the kill reaching the joiner"), 5.0f, []() { const ACheeseStrikeGameState* Match = GameState(1); return Match != nullptr && Match->GetKillFeed().Num() == 1; });
	Then([this]()
	{
		const ACheeseStrikeCharacter* HostAsSeen = CharacterOf(0, 1);
		if (TestNotNull(TEXT("the joiner sees the host"), HostAsSeen))
		{
			TestFalse(TEXT("dead"), HostAsSeen->IsAlive());
		}
		if (ACheeseStrikeCharacter* Joiner = PlayTests::Pawn(1))
		{
			TestTrue(FString::Printf(TEXT("the joiner's hits were confirmed (%d)"), Joiner->GetWeapon()->HitsConfirmed), Joiner->GetWeapon()->HitsConfirmed >= 5);
		}
	});
	WaitUntil(TEXT("the host coming back"), 2.0f + 5.0f, []() { const ACheeseStrikeCharacter* Host = Character(0); return Host != nullptr && Host->IsAlive() && Host != PlayTests::FirstBody.Get(); });
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Host = PlayTests::Pawn(0))
		{
			TestEqual(TEXT("the host is back with full health"), Host->GetHealth(), CheeseStrikeWeapons::MaxHealth);
			TestTrue(TEXT("protected for the moment"), Host->IsProtected());
			TestTrue(TEXT("holding a weapon"), Host->GetWeapon() != nullptr);
			TestTrue(FString::Printf(TEXT("at a player start (%.0f cm from one)"), PlayTests::FromNearestStart(World(0), Host->GetActorLocation())), PlayTests::FromNearestStart(World(0), Host->GetActorLocation()) < 150.0f);
			TestTrue(TEXT("while the match goes on"), !GameState(0)->HasMatchEnded());
		}
	});
	WaitUntil(TEXT("the joiner seeing the host back"), 5.0f, []() { const ACheeseStrikeCharacter* Host = CharacterOf(0, 1); return Host != nullptr && Host->IsAlive(); });
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
	Play(1, TEXT("hold Fire\nwait 1.2\nrelease Fire"));
	WaitUntil(TEXT("the match ending"), 6.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && Match->HasMatchEnded(); });
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
		// The two are on different sides, and the sides are filled out with bots by name, the same on both machines.
		if (const ACheeseStrikeGameState* Match = GameState(0))
		{
			const TArray<CheeseStrikeRules::FRosterEntry> First = Match->TeamColumn(0);
			const TArray<CheeseStrikeRules::FRosterEntry> Second = Match->TeamColumn(1);
			TestEqual(TEXT("the first side is full"), First.Num(), CheeseStrikeRules::TeamSize);
			TestEqual(TEXT("so is the second"), Second.Num(), CheeseStrikeRules::TeamSize);
			TestTrue(TEXT("the host leads the first side"), First.Num() > 0 && !First[0].bBot);
			TestTrue(TEXT("the joiner the second"), Second.Num() > 0 && !Second[0].bBot);
			TestTrue(TEXT("the rest are bots"), First.Num() > 1 && First[1].bBot && Second.Num() > 1 && Second[1].bBot);
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
	Play(1, TEXT("hold Fire\nwait 1.2\nrelease Fire"));
	WaitUntil(TEXT("the match ending"), 6.0f, []() { return GameState(0) != nullptr && GameState(0)->HasMatchEnded(); });
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
