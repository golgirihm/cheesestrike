// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the arsenal in the editor (see PlayTesting.h): carrying a
// primary, the sidearm and the knife, changing between them with their
// keys, each with its own rounds and its draw time; choosing a primary for
// the next life; and over the network, the server seeing the change, the
// sidearm's damage, the knife's stroke and its stroke in the back, and the
// modes' calls (one slot only, one weapon only).

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "CheeseStrikeGameState.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "PlayTesting.h"
#include "Engine/GameInstance.h"
#include "Records/CheeseStrikeRecords.h"
#include "Rules/CheeseStrikeRules.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace WeaponPlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** Two spots facing each other across 4 m of clear ground on the shooter level (as PlayTests' own). */
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	constexpr float HostYaw = -90.0f;
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);
	constexpr float JoinerYaw = 90.0f;
	/** A step behind or before the host: in the knife's reach. */
	const FVector BesideHost(-231.0f, -410.0f, 100.0f);

	/** Remembered between steps. */
	FVector Before = FVector::ZeroVector;
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

	/** The id of the weapon in `Character`'s hand, or none. */
	FName HeldId(const ACheeseStrikeCharacter* Character)
	{
		return Character != nullptr && Character->GetWeapon() != nullptr ? Character->GetWeapon()->GetWeaponId() : NAME_None;
	}

	/** Whether only the weapon in hand is drawn, of every one `Character` carries, on both bodies. */
	bool OnlyHeldShows(const ACheeseStrikeCharacter* Character)
	{
		bool bRight = true;
		for (const EWeaponSlot Slot : {EWeaponSlot::Primary, EWeaponSlot::Sidearm, EWeaponSlot::Knife})
		{
			const ACheeseStrikeWeapon* Weapon = Character->GetInventory()->GetInSlot(Slot);
			if (Weapon == nullptr)
			{
				continue;
			}
			const bool bHeld = Weapon == Character->GetWeapon();
			const bool bShown = Weapon->GetStats().IsMelee() ? !Weapon->FindComponentByClass<UStaticMeshComponent>()->bHiddenInGame : !Weapon->GetThirdPersonMesh()->bHiddenInGame;
			bRight &= bHeld == bShown && Weapon->IsInHand() == bHeld;
		}
		return bRight;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItChangesWeapons, "CheeseStrike.Play.ChangesWeapons", WeaponPlayTests::Flags)

bool FItChangesWeapons::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace WeaponPlayTests;
	StartPlaying({1, TEXT("?RespawnDelay=1")});
	PutAt(0, JoinerSpot, JoinerYaw);
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			const UCheeseStrikeInventory* Inventory = Character->GetInventory();
			TestEqual(TEXT("it comes in with the Cheddar Blaster in hand"), HeldId(Character), CheeseStrikeWeapons::Blaster().Id);
			TestTrue(TEXT("the Curd Pistol as its sidearm"), Inventory->GetInSlot(EWeaponSlot::Sidearm) != nullptr && Inventory->GetInSlot(EWeaponSlot::Sidearm)->GetWeaponId() == CheeseStrikeWeapons::Pistol().Id);
			TestTrue(TEXT("and the knife"), Inventory->GetInSlot(EWeaponSlot::Knife) != nullptr && Inventory->GetInSlot(EWeaponSlot::Knife)->GetStats().IsMelee());
			TestFalse(TEXT("ready at once"), Character->GetWeapon()->IsDrawing());
			TestTrue(TEXT("only the one in hand shows"), OnlyHeldShows(Character));
		}
	});
	// 2 draws the pistol; a pull while it comes up fires once it is up, and holding it fires no more.
	Play(0, TEXT("tap Sidearm\nwait 0.05"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			TestEqual(TEXT("2 draws the Curd Pistol"), HeldId(Character), CheeseStrikeWeapons::Pistol().Id);
			TestTrue(TEXT("which takes a moment to come up"), Character->GetWeapon()->IsDrawing());
			TestTrue(TEXT("the rifle put away"), OnlyHeldShows(Character));
		}
	});
	Play(0, TEXT("tap Fire\nwait 0.6"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			const ACheeseStrikeWeapon* Pistol = Character->GetWeapon();
			TestEqual(TEXT("a pull while it came up fired once it was up"), Pistol->ShotsFired, 1);
			TestEqual(TEXT("from its own magazine"), Pistol->GetMagazine(), Pistol->GetStats().MagazineSize - 1);
			TestEqual(TEXT("with rounds without end"), Pistol->DescribeRounds(), FString::Printf(TEXT("%d"), Pistol->GetStats().MagazineSize - 1));
			const ACheeseStrikeWeapon* Blaster = Character->GetInventory()->GetInSlot(EWeaponSlot::Primary);
			TestEqual(TEXT("the rifle's magazine untouched"), Blaster->GetMagazine(), Blaster->GetStats().MagazineSize);
		}
	});
	Play(0, TEXT("hold Fire\nwait 1\nrelease Fire\nwait 0.1"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			TestEqual(TEXT("the pistol fires a shot a pull, however long it is held"), Character->GetWeapon()->ShotsFired, 2);
			Before = Character->GetActorLocation();
		}
	});
	// 3 draws the knife; a stroke swipes, where the striker stands.
	Play(0, TEXT("tap Knife\nwait 0.4\ntap Fire\nwait 0.15"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			TestEqual(TEXT("3 draws the knife"), HeldId(Character), CheeseStrikeWeapons::Knife().Id);
			TestEqual(TEXT("which strikes"), Character->GetWeapon()->ShotsFired, 1);
			const float Swipe = Character->GetWeapon()->SwipePhase();
			TestTrue(FString::Printf(TEXT("swiping (%.2f of the way)"), Swipe), Swipe > 0.0f && Swipe < 1.0f);
			const float Moved = FVector::Dist2D(Character->GetActorLocation(), Before);
			TestTrue(FString::Printf(TEXT("without lunging (%.0f cm)"), Moved), Moved < 10.0f);
			TestEqual(TEXT("it shows no rounds"), Character->GetWeapon()->DescribeRounds(), FString());
			TestTrue(TEXT("and only the knife shows"), OnlyHeldShows(Character));
		}
	});
	// 2 draws the pistol; the wheel goes round.
	Play(0, TEXT("tap Sidearm\nwait 0.4"));
	Then([this]() { TestEqual(TEXT("2 draws the pistol"), HeldId(Pawn(0)), CheeseStrikeWeapons::Pistol().Id); });
	Play(0, TEXT("tap NextWeapon\nwait 0.4"));
	Then([this]() { TestEqual(TEXT("the wheel down goes on to the knife"), HeldId(Pawn(0)), CheeseStrikeWeapons::Knife().Id); });
	Play(0, TEXT("tap NextWeapon\nwait 0.4"));
	Then([this]() { TestEqual(TEXT("and round to the rifle"), HeldId(Pawn(0)), CheeseStrikeWeapons::Blaster().Id); });
	Play(0, TEXT("tap PreviousWeapon\nwait 0.4"));
	Then([this]() { TestEqual(TEXT("the wheel up goes back round to the knife"), HeldId(Pawn(0)), CheeseStrikeWeapons::Knife().Id); });
	Play(0, TEXT("tap Primary\nwait 0.5\nhold Fire\nwait 0.5\nrelease Fire"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			TestEqual(TEXT("1 draws the rifle"), HeldId(Character), CheeseStrikeWeapons::Blaster().Id);
			TestTrue(FString::Printf(TEXT("which fires as before (%d)"), Character->GetWeapon()->ShotsFired), Character->GetWeapon()->ShotsFired >= 3);
		}
	});
	// A primary chosen now is carried from the next life: this one has fired, and is no longer fresh.
	Play(0, TEXT("cmd CheeseStrikeLoadout shotgun\nwait 0.3"));
	Then([this]() { TestEqual(TEXT("choosing doesn't change what is carried in a fight"), HeldId(Pawn(0)), CheeseStrikeWeapons::Blaster().Id); });
	Play(0, TEXT("cmd CheeseStrikeDie\nwait 2.5"));
	WaitUntil(TEXT("coming back"), 5.0f, []() { return Character(0) != nullptr && Character(0)->IsAlive() && Character(0)->GetWeapon() != nullptr; });
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			TestEqual(TEXT("the next life carries the Swiss Shotgun"), HeldId(Character), CheeseStrikeWeapons::Shotgun().Id);
			TestTrue(TEXT("with the pistol and the knife still"), Character->GetInventory()->CanDraw(EWeaponSlot::Sidearm) && Character->GetInventory()->CanDraw(EWeaponSlot::Knife));
		}
	});
	PutAt(0, JoinerSpot, JoinerYaw);
	// Just come in, a choice is carried at once.
	Play(0, TEXT("cmd CheeseStrikeLoadout munster\nwait 0.5"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			TestEqual(TEXT("just come in, the Munster Machine Gun at once"), HeldId(Character), CheeseStrikeWeapons::Munster().Id);
			TestTrue(TEXT("which is heavy to carry"), Character->GetCheeseStrikeMovement()->IsCarryingHeavy());
		}
	});
	Play(0, TEXT("hold Move 0 1\nwait 1\nrelease Move"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Character = Pawn(0))
		{
			const float Top = Character->GetCheeseStrikeMovement()->GetMaxSpeed();
			TestTrue(FString::Printf(TEXT("and slows its carrier (%.0f cm/s)"), Top), FMath::IsNearlyEqual(Top, CheeseStrikeMovement::WalkSpeed * CheeseStrikeMovement::HeavyFactor, 1.0f));
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponsWorkOverTheNetwork, "CheeseStrike.Play.WeaponsWorkOverTheNetwork", WeaponPlayTests::Flags)

bool FWeaponsWorkOverTheNetwork::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace WeaponPlayTests;
	StartPlaying({2, TEXT("?RespawnDelay=1")});
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	// The joiner changes to the pistol: the server, and the host's view of them, follow.
	Play(1, TEXT("tap Sidearm\nwait 0.6"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* OnServer = CharacterOf(1, 0);
		const ACheeseStrikeCharacter* Own = Character(1);
		if (TestNotNull(TEXT("the server has the joiner"), OnServer) && TestNotNull(TEXT("and the joiner themselves"), Own))
		{
			TestEqual(TEXT("the joiner holds the pistol"), HeldId(Own), CheeseStrikeWeapons::Pistol().Id);
			TestEqual(TEXT("as the server has it"), HeldId(OnServer), CheeseStrikeWeapons::Pistol().Id);
			TestTrue(TEXT("and shows it, the rifle put away"), OnlyHeldShows(OnServer));
		}
		if (ACheeseStrikeCharacter* Host = Pawn(0))
		{
			Host->EndProtection();
			HealthBefore = Host->GetHealth();
		}
	});
	// A pistol shot hurts by the pistol's damage, the server's count.
	Play(1, TEXT("tap Fire\nwait 0.5"));
	Then([this]()
	{
		if (const ACheeseStrikeCharacter* Host = Pawn(0))
		{
			const float Lost = HealthBefore - Host->GetHealth();
			const FWeaponStats& Pistol = CheeseStrikeWeapons::Pistol();
			TestTrue(FString::Printf(TEXT("the pistol's shot did the pistol's damage (%.0f)"), Lost), FMath::IsNearlyEqual(Lost, Pistol.Damage, 0.5f) || FMath::IsNearlyEqual(Lost, Pistol.Damage * Pistol.HeadMultiplier, 0.5f));
			HealthBefore = Host->GetHealth();
		}
		const ACheeseStrikeCharacter* OnServer = CharacterOf(1, 0);
		TestTrue(TEXT("from the pistol's own magazine, on the server"), OnServer != nullptr && OnServer->GetWeapon()->GetMagazine() == CheeseStrikeWeapons::Pistol().MagazineSize - 1);
	});
	// The knife, from the front: 55.
	PutAt(1, BesideHost, JoinerYaw);
	Play(1, TEXT("tap Knife\nwait 0.4\ntap Fire\nwait 0.4"));
	Then([this]()
	{
		if (ACheeseStrikeCharacter* Host = Pawn(0))
		{
			TestTrue(FString::Printf(TEXT("a stroke from the front does 55 (%.0f)"), HealthBefore - Host->GetHealth()), FMath::IsNearlyEqual(HealthBefore - Host->GetHealth(), CheeseStrikeWeapons::Knife().Damage, 0.5f));
			TestTrue(TEXT("and leaves them standing"), Host->IsAlive());
		}
	});
	// And in the back, it kills. The joiner's records (on the joiner's machine) count the kill and its badge.
	const TSharedRef<TPair<int32, int32>> RecordsBefore = MakeShared<TPair<int32, int32>>(0, 0);
	Then([RecordsBefore]()
	{
		const UWorld* JoinerWorld = World(1);
		if (const UCheeseStrikeRecordsSubsystem* Records = JoinerWorld != nullptr && JoinerWorld->GetGameInstance() != nullptr ? JoinerWorld->GetGameInstance()->GetSubsystem<UCheeseStrikeRecordsSubsystem>() : nullptr)
		{
			*RecordsBefore = TPair<int32, int32>(Records->GetStat(CheeseStrikeAchievements::Stat::Kills), Records->GetStat(CheeseStrikeAchievements::Stat::Assassinations));
		}
	});
	PutAt(0, HostSpot, JoinerYaw);
	PutAt(1, BesideHost, JoinerYaw);
	Play(1, TEXT("wait 0.5\ntap Fire\nwait 0.4"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		TestTrue(TEXT("a stroke in the back kills"), Host == nullptr || !Host->IsAlive());
		if (Host != nullptr && Joiner != nullptr)
		{
			TestEqual(TEXT("by the joiner"), Host->GetKilledBy(), Joiner->DisplayName());
		}
		// An ASSASSINATION, in the feed on both machines and counted for the joiner.
		for (int32 Machine = 0; Machine < 2; ++Machine)
		{
			const ACheeseStrikeGameState* Match = GameState(Machine);
			const uint8 Badges = Match != nullptr && Match->GetKillFeed().Num() > 0 ? Match->GetKillFeed().Last().Badges : 0;
			TestTrue(FString::Printf(TEXT("machine %d's feed calls it an assassination (badges %d)"), Machine, Badges), CheeseStrikeBadges::Has(Badges, CheeseStrikeBadges::EBadge::Assassination));
		}
		const ACheeseStrikePlayerState* Theirs = Joiner != nullptr ? Joiner->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
		TestTrue(TEXT("and the joiner's player state counts it"), Theirs != nullptr && Theirs->GetBadgeCount(CheeseStrikeBadges::EBadge::Assassination) == 1);
	});
	WaitUntil(TEXT("the joiner's records counting the kill and the assassination"), 3.0f, [RecordsBefore]()
	{
		const UWorld* JoinerWorld = World(1);
		const UCheeseStrikeRecordsSubsystem* Records = JoinerWorld != nullptr && JoinerWorld->GetGameInstance() != nullptr ? JoinerWorld->GetGameInstance()->GetSubsystem<UCheeseStrikeRecordsSubsystem>() : nullptr;
		return Records != nullptr && Records->GetStat(CheeseStrikeAchievements::Stat::Kills) == RecordsBefore->Key + 1 && Records->GetStat(CheeseStrikeAchievements::Stat::Assassinations) == RecordsBefore->Value + 1;
	});
	// The modes' calls, on the server: one slot only (Execution's sidearm), then one weapon only (Gun Game's).
	Then([this]()
	{
		if (ACheeseStrikeCharacter* OnServer = CharacterOf(1, 0))
		{
			OnServer->GetInventory()->RestrictTo(EWeaponSlot::Sidearm);
		}
	});
	Play(1, TEXT("wait 0.3\ntap Primary\nwait 0.3\ntap NextWeapon\nwait 0.3"));
	Then([this]()
	{
		TestEqual(TEXT("restricted to the sidearm, the joiner holds it"), HeldId(Character(1)), CheeseStrikeWeapons::Pistol().Id);
		TestEqual(TEXT("and can't draw another"), HeldId(CharacterOf(1, 0)), CheeseStrikeWeapons::Pistol().Id);
		if (ACheeseStrikeCharacter* OnServer = CharacterOf(1, 0))
		{
			OnServer->GetInventory()->GiveOnly(CheeseStrikeWeapons::Roquefort().Id);
		}
	});
	Wait(0.5f);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Own = Character(1);
		TestEqual(TEXT("given one weapon, the joiner holds it at once"), HeldId(Own), CheeseStrikeWeapons::Roquefort().Id);
		TestTrue(TEXT("ready to fire"), Own != nullptr && Own->GetWeapon() != nullptr && !Own->GetWeapon()->IsDrawing());
		TestTrue(TEXT("and carries nothing else"), Own != nullptr && !Own->GetInventory()->CanDraw(EWeaponSlot::Sidearm) && !Own->GetInventory()->CanDraw(EWeaponSlot::Knife));
		TestTrue(TEXT("with every slot open again"), Own != nullptr && Own->GetInventory()->IsAllowed(EWeaponSlot::Primary));
	});
	Play(1, TEXT("tap Knife\nwait 0.3"));
	Then([this]() { TestEqual(TEXT("a slot with nothing in it can't be drawn"), HeldId(Character(1)), CheeseStrikeWeapons::Roquefort().Id); });
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHeadIsWhereTheShooterSawIt, "CheeseStrike.Play.AHeadIsWhereTheShooterSawIt", WeaponPlayTests::Flags)

namespace WeaponPlayTests
{
	/** The middle of a sample's head. */
	FVector HeadOf(const FHitHistory::FSample& Sample)
	{
		return (Sample.HeadLow + Sample.HeadHigh) * 0.5f;
	}
	/** For each way back the server might look (0 to 0.2 s more), the sum of how far its head was from the one the joiner saw. */
	TArray<float> HeadMisses;
	int32 HeadSamples = 0;
	float StrafeStartedAt = 0.0f;
}

bool FAHeadIsWhereTheShooterSawIt::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace WeaponPlayTests;
	StartPlaying({2, TEXT("?RespawnDelay=1")});
	// 40 ms each way, as a player across a country has: the joiner's clock and what it sees of the host both lag by it.
	Then([]()
	{
		GEngine->Exec(World(0), TEXT("NetEmulation.PktLag 40"));
		GEngine->Exec(World(1), TEXT("NetEmulation.PktLag 40"));
	});
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Wait(3.0f);
	Then([]()
	{
		HeadMisses.Init(0.0f, 16);
		HeadSamples = 0;
		StrafeStartedAt = Now(0);
		// The joiner's copy of the host is posed every frame, as the server's is, though nothing draws it here.
		if (ACheeseStrikeCharacter* Seen = CharacterOf(0, 1))
		{
			Seen->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
	});
	// The host runs from side to side across the joiner's view; every frame, where the joiner sees the host's head at the
	// server's time as the joiner has it (what a shot fired then says), against where the server had it at that time.
	WaitUntil(TEXT("the host strafed"), 10.0f, []()
	{
		ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikeCharacter* Seen = CharacterOf(0, 1);
		if (Host == nullptr || Seen == nullptr)
		{
			return false;
		}
		const float Since = Now(0) - StrafeStartedAt;
		const float Way = FMath::Fmod(Since, 1.4f) < 0.7f ? 1.0f : -1.0f;
		Host->AddMovementInput(FVector(Way, 0.0f, 0.0f), 1.0f);
		const float Then = Now(1);
		if (Since > 0.5f)
		{
			const FVector Saw = HeadOf(Seen->SampleNow(Then));
			for (int32 Back = 0; Back < HeadMisses.Num(); ++Back)
			{
				HeadMisses[Back] += FVector::Dist(Saw, HeadOf(Host->GetHitHistory().At(Then - (Back - 5) * 0.02f)));
			}
			++HeadSamples;
		}
		return Since > 4.0f;
	});
	Then([this]()
	{
		FString Line;
		int32 Best = 0;
		for (int32 Back = 0; Back < HeadMisses.Num(); ++Back)
		{
			Line += FString::Printf(TEXT(" %.0f ms: %.1f cm;"), (Back - 5) * 20.0f, HeadMisses[Back] / FMath::Max(1, HeadSamples));
			Best = HeadMisses[Back] < HeadMisses[Best] ? Back : Best;
		}
		AddInfo(FString::Printf(TEXT("the head the joiner saw against the server's, looking back further by:%s (%d frames)"), *Line, HeadSamples));
		GEngine->Exec(World(0), TEXT("NetEmulation.PktLag 0"));
		GEngine->Exec(World(1), TEXT("NetEmulation.PktLag 0"));
		const float Miss = HeadMisses[5] / FMath::Max(1, HeadSamples);
		TestTrue(TEXT("measured"), HeadSamples > 30);
		TestTrue(FString::Printf(TEXT("the server's head is within a head's radius of the one the joiner saw at the time its shot names (%.1f cm on average)"), Miss), Miss < CheeseStrikeWeapons::HeadRadius);
	});
	// Standing still, a shot at the head the joiner sees takes the head's damage, and the joiner's hit marker says head; one at
	// the chest takes the body's, and the marker doesn't.
	const auto AimAt = [](bool bHead)
	{
		ACheeseStrikeCharacter* Joiner = Character(1);
		const ACheeseStrikeCharacter* Seen = CharacterOf(0, 1);
		if (Joiner == nullptr || Seen == nullptr || Controller(1) == nullptr)
		{
			return;
		}
		const FVector Eye = Joiner->GetFirstPersonCameraComponent()->GetComponentLocation();
		const FVector Target = bHead ? HeadOf(Seen->SampleNow(Now(1))) : Seen->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
		Controller(1)->SetControlRotation((Target - Eye).Rotation());
		if (ACheeseStrikeCharacter* Host = Character(0))
		{
			Host->EndProtection();
			HealthBefore = Host->GetHealth();
		}
	};
	PutAt(0, HostSpot, HostYaw);
	Wait(1.0f);
	Then([AimAt]() { AimAt(true); });
	Play(1, TEXT("wait 0.2\ntap Fire\nwait 0.6"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikeCharacter* Joiner = Character(1);
		const FWeaponStats& Held = Joiner != nullptr && Joiner->GetWeapon() != nullptr ? Joiner->GetWeapon()->GetStats() : CheeseStrikeWeapons::Blaster();
		const float Head = CheeseStrikeWeapons::DamageAt(Held, 400.0f, true);
		if (Host != nullptr && Joiner != nullptr && Joiner->GetWeapon() != nullptr)
		{
			TestTrue(FString::Printf(TEXT("a shot at the head takes the head's damage (%.1f, the head's %.1f)"), HealthBefore - Host->GetHealth(), Head), FMath::IsNearlyEqual(HealthBefore - Host->GetHealth(), Head, 0.5f));
			TestTrue(TEXT("and the joiner's marker says head"), Joiner->GetWeapon()->HitsConfirmed > 0 && Joiner->GetWeapon()->bLastHitHead);
		}
	});
	Then([AimAt]() { AimAt(false); });
	Play(1, TEXT("wait 0.4\ntap Fire\nwait 0.6"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikeCharacter* Joiner = Character(1);
		if (Host != nullptr && Joiner != nullptr && Joiner->GetWeapon() != nullptr)
		{
			const float Body = CheeseStrikeWeapons::DamageAt(Joiner->GetWeapon()->GetStats(), 400.0f, false);
			TestTrue(FString::Printf(TEXT("a shot at the chest takes the body's damage (%.1f, the body's %.1f)"), HealthBefore - Host->GetHealth(), Body), FMath::IsNearlyEqual(HealthBefore - Host->GetHealth(), Body, 0.5f));
			TestFalse(TEXT("and the marker doesn't say head"), Joiner->GetWeapon()->bLastHitHead);
		}
	});
	StopPlaying();
	return true;
}

#endif
