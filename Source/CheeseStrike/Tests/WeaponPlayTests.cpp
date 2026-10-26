// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the arsenal in the editor (see PlayTesting.h): carrying a
// primary, the sidearm and the knife, changing between them with their
// keys, each with its own rounds and its draw time; choosing a primary for
// the next life; and over the network, the server seeing the change, the
// sidearm's damage, the knife's stroke and its stroke in the back, and the
// modes' calls (one slot only, one weapon only).

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerController.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "PlayTesting.h"
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
	// And in the back, it kills.
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

#endif
