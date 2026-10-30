// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// How a match begins: the count before it (3, 2, 1, FIGHT), which holds
// everyone where they stand and reaches a joiner, and the protection of a
// fresh spawn, which ends with time or an attack, on every machine, and
// shows as a ghostly look while it lasts. The rules, then two plays.

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStart.h"
#include "UI/CheeseStrikeTones.h"
#include "Weapons/CheeseStrikeShot.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMatchCountsInAndProtectionEndsOnAttackOrTime, "CheeseStrike.Rules.TheMatchCountsInAndProtectionEndsOnAttackOrTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTheMatchCountsInAndProtectionEndsOnAttackOrTime::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeStart;
	// The count: three seconds unless the URL says, never more than ten; a later round's is two, none if the match has none.
	TestEqual(TEXT("a match counts three seconds"), CountdownFrom(TEXT("?Mode=Elimination")), 3.0f);
	TestEqual(TEXT("or as the URL says"), CountdownFrom(TEXT("?StartNow?Countdown=5")), 5.0f);
	TestEqual(TEXT("none, for the tests"), CountdownFrom(TEXT("?Countdown=0")), 0.0f);
	TestEqual(TEXT("never more than ten"), CountdownFrom(TEXT("?Countdown=60")), MostCountdown);
	TestEqual(TEXT("nothing that isn't a number"), CountdownFrom(TEXT("?Countdown=soon")), Countdown);
	TestEqual(TEXT("a later round counts two"), RoundCountdownFor(Countdown), 2.0f);
	TestEqual(TEXT("or the match's, if shorter"), RoundCountdownFor(1.0f), 1.0f);
	TestEqual(TEXT("none if it has none"), RoundCountdownFor(0.0f), 0.0f);

	// Held while it runs, from nothing after it, and never without one.
	TestTrue(TEXT("held while it counts"), IsFrozen(100.0f, 103.0f));
	TestFalse(TEXT("free once it is over"), IsFrozen(103.0f, 103.0f));
	TestFalse(TEXT("and with none"), IsFrozen(100.0f, 0.0f));
	// Nobody acts while it counts, nor once the round or the match is over; the dead never.
	TestTrue(TEXT("a living character acts while play is on"), MayAct(true, false, false, false));
	TestFalse(TEXT("not while it counts"), MayAct(true, true, false, false));
	TestFalse(TEXT("not once its round is over"), MayAct(true, false, true, false));
	TestFalse(TEXT("nor the match"), MayAct(true, false, false, true));
	TestFalse(TEXT("the dead never"), MayAct(false, false, false, false));
	// For a moment after a player is held the server takes their machine's word for where they are, then its own again.
	TestFalse(TEXT("not before it is held"), TakesClientWord(-1.0f));
	TestTrue(TEXT("as it is held"), TakesClientWord(0.0f) && TakesClientWord(HoldGrace * 0.9f));
	TestFalse(TEXT("and no longer after the grace"), TakesClientWord(HoldGrace));
	// The round's clock stands still through it.
	TestEqual(TEXT("the round's clock stands at its time through the count"), RoundLeft(100.0f, 102.0f, 222.0f), 120.0f);
	TestEqual(TEXT("and runs after"), RoundLeft(132.0f, 102.0f, 222.0f), 90.0f);
	TestEqual(TEXT("with no round, nothing"), RoundLeft(100.0f, 0.0f, 0.0f), 0.0f);

	// What the screen shows: 3, 2, 1 as each second begins, then FIGHT for a moment, then nothing.
	TestEqual(TEXT("three"), Shown(100.0f, 103.0f).Text, FString(TEXT("3")));
	TestEqual(TEXT("two"), Shown(101.5f, 103.0f).Text, FString(TEXT("2")));
	TestEqual(TEXT("one"), Shown(102.9f, 103.0f).Text, FString(TEXT("1")));
	TestTrue(TEXT("each second comes in large"), Shown(101.0f, 103.0f).Scale > 1.3f);
	TestEqual(TEXT("and settles"), Shown(101.5f, 103.0f).Scale, 1.0f);
	TestEqual(TEXT("fully seen"), Shown(101.5f, 103.0f).Alpha, 1.0f);
	TestEqual(TEXT("FIGHT as it ends"), Shown(103.0f, 103.0f).Text, FString(TEXT("FIGHT")));
	TestTrue(TEXT("as the word to go"), Shown(103.2f, 103.0f).bGo);
	TestTrue(TEXT("fading"), Shown(103.0f + GoShown * 0.9f, 103.0f).Alpha < 0.5f);
	TestTrue(TEXT("then nothing"), Shown(103.0f + GoShown, 103.0f).Text.IsEmpty());
	TestTrue(TEXT("nor with no count"), Shown(100.0f, 0.0f).Text.IsEmpty());

	// Protection: SpawnProtection seconds from coming in, or from the count's end if it is still running.
	TestEqual(TEXT("a fresh spawn's protection is three seconds"), CheeseStrikeRules::SpawnProtection, 3.0f);
	TestEqual(TEXT("from coming in"), ProtectedUntil(50.0f, 0.0f), 53.0f);
	TestEqual(TEXT("or from the end of the count"), ProtectedUntil(100.0f, 103.0f), 106.0f);
	TestEqual(TEXT("a count long over doesn't matter"), ProtectedUntil(200.0f, 103.0f), 203.0f);
	TestTrue(TEXT("protected until then"), IsProtected(105.9f, 106.0f));
	TestFalse(TEXT("ends with time"), IsProtected(106.0f, 106.0f));
	TestFalse(TEXT("never without any"), IsProtected(1.0f, 0.0f));
	// An attack ends it at once.
	const float Attacked = AfterAttack(106.0f, 104.0f);
	TestFalse(TEXT("ends on an attack"), IsProtected(104.0f, Attacked));
	TestEqual(TEXT("an attack after it ended changes nothing"), AfterAttack(106.0f, 110.0f), 106.0f);

	// The ghostly look: whole while protected, fading over GhostFade after, however it ended.
	TestEqual(TEXT("ghostly while protected"), GhostAmount(105.0f, 106.0f), 1.0f);
	TestEqual(TEXT("half gone halfway through the fade"), GhostAmount(106.0f + GhostFade * 0.5f, 106.0f), 0.5f);
	TestEqual(TEXT("gone after"), GhostAmount(106.0f + GhostFade, 106.0f), 0.0f);
	TestEqual(TEXT("fading from an attack too"), GhostAmount(104.0f + GhostFade * 0.5f, Attacked), 0.5f);
	TestEqual(TEXT("never without protection"), GhostAmount(1.0f, 0.0f), 0.0f);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace StartTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** Two spots facing each other across 4 m of clear ground on the shooter level. */
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	constexpr float HostYaw = -90.0f;
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);
	constexpr float JoinerYaw = 90.0f;

	/** What the test saw at a moment, to compare with later. */
	FVector HostAt;
	FVector JoinerAt;
	int32 HostShots = 0;
	int32 JoinerShots = 0;
	int32 Seconds = 0;
	int32 Ends = 0;

	/** Whether `Body` wears the ghost's overlay: its body, and with `bOwn` its first-person arms, and its weapon in hand (both its models with `bOwn`, else the one others see). */
	bool LooksGhostly(const ACheeseStrikeCharacter* Body, bool bOwn, FString& Missing)
	{
		Missing.Reset();
		if (Body == nullptr)
		{
			Missing = TEXT("no character");
			return false;
		}
		const auto Check = [&Missing](const UMeshComponent* Part, const TCHAR* Name)
		{
			if (Part != nullptr && Part->GetOverlayMaterial() == nullptr)
			{
				Missing += FString(Missing.IsEmpty() ? TEXT("") : TEXT(", ")) + Name;
			}
		};
		Check(Body->GetMesh(), TEXT("the body"));
		if (bOwn)
		{
			Check(Body->GetFirstPersonMesh(), TEXT("the first-person arms"));
		}
		if (const ACheeseStrikeWeapon* Weapon = Body->GetWeapon())
		{
			// Its models, skinned or plain.
			Check(Cast<UMeshComponent>(Weapon->GetModel(false)), TEXT("the weapon others see"));
			if (bOwn)
			{
				Check(Cast<UMeshComponent>(Weapon->GetModel(true)), TEXT("the weapon in view"));
			}
		}
		else
		{
			Missing += TEXT(" (no weapon)");
		}
		return Missing.IsEmpty() && Body->GetGhostShown() > 0.0f;
	}

	/** Whether nothing of `Body` wears the overlay any more. */
	bool LooksNormal(const ACheeseStrikeCharacter* Body)
	{
		if (Body == nullptr)
		{
			return false;
		}
		const ACheeseStrikeWeapon* Weapon = Body->GetWeapon();
		for (const UMeshComponent* Part : {static_cast<const UMeshComponent*>(Body->GetMesh()), static_cast<const UMeshComponent*>(Body->GetFirstPersonMesh()),
			 Weapon != nullptr ? static_cast<const UMeshComponent*>(Cast<UMeshComponent>(Weapon->GetModel(true))) : nullptr,
			 Weapon != nullptr ? static_cast<const UMeshComponent*>(Cast<UMeshComponent>(Weapon->GetModel(false))) : nullptr})
		{
			if (Part != nullptr && Part->GetOverlayMaterial() != nullptr)
			{
				return false;
			}
		}
		return Body->GetGhostShown() <= 0.0f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCountHoldsEveryoneAndReachesTheJoiner, "CheeseStrike.Play.TheCountHoldsEveryoneAndReachesTheJoiner", StartTests::Flags)

bool FTheCountHoldsEveryoneAndReachesTheJoiner::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	// The longest count, so that the joiner is in well before it ends.
	StartPlaying({2, TEXT("?Countdown=10")});
	Then([this]()
	{
		const ACheeseStrikeGameState* Host = GameState(0);
		const ACheeseStrikeGameState* Joiner = GameState(1);
		if (!TestNotNull(TEXT("the host's match"), Host) || !TestNotNull(TEXT("the joiner's"), Joiner) || !TestNotNull(TEXT("both characters"), Character(0)) || !TestNotNull(TEXT("both"), Character(1)))
		{
			return;
		}
		AddInfo(FString::Printf(TEXT("The joiner is in with %.1f s of the count left"), Host->SecondsToGo()));
		TestTrue(TEXT("the match is counting in"), Host->IsFrozen() && Host->SecondsToGo() > 1.5f);
		TestEqual(TEXT("the joiner has the same count"), Joiner->GetGoAt(), Host->GetGoAt());
		TestTrue(TEXT("and is held by it"), Joiner->IsFrozen());
		TestFalse(TEXT("the host can't act"), Character(0)->CanAct());
		TestFalse(TEXT("nor the joiner, on their machine"), Character(1)->CanAct());
		TestFalse(TEXT("nor on the server's"), CharacterOf(1, 0) != nullptr && CharacterOf(1, 0)->CanAct());
		// Both see it counting in the middle of the screen.
		for (int32 Each = 0; Each < 2; ++Each)
		{
			const CheeseStrikeStart::FShown Shown = CheeseStrikeStart::Shown(Now(Each), GameState(Each)->GetGoAt());
			TestTrue(FString::Printf(TEXT("player %d sees a second counting (%s)"), Each, *Shown.Text), !Shown.Text.IsEmpty() && !Shown.bGo);
		}
		// Protected from the count's end, not from coming in.
		TestTrue(TEXT("both are protected"), Character(0)->IsProtected() && Character(1)->IsProtected());
		StartTests::HostAt = Character(0)->GetActorLocation();
		StartTests::JoinerAt = CharacterOf(1, 0)->GetActorLocation();
		StartTests::HostShots = Character(0)->GetWeapon()->ShotsFired;
		StartTests::JoinerShots = Character(1)->GetWeapon()->ShotsFired;
		StartTests::Seconds = CheeseStrikeTones::TimesPlayed(CheeseStrikeTones::ETone::Second);
		StartTests::Ends = CheeseStrikeTones::TimesPlayed(CheeseStrikeTones::ETone::End);
	});
	// Both try to run, jump, fire, change weapons and call a streak; they may look round.
	Play(0, TEXT("hold Move 0 1\nhold Fire\ntap Jump\nlook 40 0\nwait 1\nrelease Fire\nrelease Move\ntap Sidearm\nwait 0.3"));
	Play(1, TEXT("hold Move 1 0\nhold Fire\nwait 1\nrelease Fire\nrelease Move\ntap Knife\nwait 0.3"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		if (Host == nullptr || Joiner == nullptr || Character(1) == nullptr)
		{
			AddError(TEXT("a character went"));
			return;
		}
		TestTrue(TEXT("still counting"), GameState(0)->IsFrozen());
		TestTrue(FString::Printf(TEXT("the host didn't move (%.0f cm)"), FVector::Dist2D(Host->GetActorLocation(), StartTests::HostAt)), FVector::Dist2D(Host->GetActorLocation(), StartTests::HostAt) < 5.0f);
		TestTrue(FString::Printf(TEXT("nor the joiner (%.0f cm)"), FVector::Dist2D(Joiner->GetActorLocation(), StartTests::JoinerAt)), FVector::Dist2D(Joiner->GetActorLocation(), StartTests::JoinerAt) < 5.0f);
		TestEqual(TEXT("the host fired nothing"), Host->GetWeapon()->ShotsFired, StartTests::HostShots);
		TestEqual(TEXT("nor the joiner"), Character(1)->GetWeapon()->ShotsFired, StartTests::JoinerShots);
		TestEqual(TEXT("the host still holds the primary"), Host->GetWeapon()->GetStats().Slot, EWeaponSlot::Primary);
		TestEqual(TEXT("and the joiner"), Character(1)->GetWeapon()->GetStats().Slot, EWeaponSlot::Primary);
		TestTrue(TEXT("but the host looked round"), FMath::Abs(FRotator::NormalizeAxis(Host->GetControlRotation().Yaw - 90.0f)) > 1.0f);
		TestEqual(TEXT("both at full health"), Host->GetHealth() + Joiner->GetHealth(), 2.0f * CheeseStrikeWeapons::MaxHealth);
		// No streak while it counts.
		ACheeseStrikeGameMode* Mode = World(0)->GetAuthGameMode<ACheeseStrikeGameMode>();
		Mode->CheeseStrikeGiveStreak(1);
	});
	Play(0, TEXT("tap Streak\nwait 0.3"));
	Then([this]()
	{
		const ACheeseStrikePlayerState* Player = Character(0)->GetPlayerState<ACheeseStrikePlayerState>();
		TestTrue(TEXT("a streak can't be called in while it counts"), GameState(0)->IsFrozen() && Player->NextStreak() != CheeseStrikeStreaks::EStreak::None);
	});
	WaitUntil(TEXT("the count ending on both machines"), 12.0f, []() { return !GameState(0)->IsFrozen() && !GameState(1)->IsFrozen(); });
	Then([this]()
	{
		TestTrue(TEXT("both can act"), Character(0)->CanAct() && Character(1)->CanAct());
		TestEqual(TEXT("FIGHT, for the host"), CheeseStrikeStart::Shown(Now(0), GameState(0)->GetGoAt()).Text, FString(TEXT("FIGHT")));
		TestTrue(TEXT("both are protected for a while yet"), Character(0)->IsProtected() && Character(1)->IsProtected());
	});
	Wait(1.2f);
	Then([this]()
	{
		// Each machine heard a tone each second it saw (the joiner came in partway) and the chime at the end.
		const int32 Seconds = CheeseStrikeTones::TimesPlayed(CheeseStrikeTones::ETone::Second) - StartTests::Seconds;
		AddInfo(FString::Printf(TEXT("%d tones of a second after the joiner was in"), Seconds));
		TestTrue(TEXT("a tone each second"), Seconds >= 2);
		TestEqual(TEXT("and the chime once on each machine"), CheeseStrikeTones::TimesPlayed(CheeseStrikeTones::ETone::End) - StartTests::Ends, 2);
		StartTests::JoinerAt = CharacterOf(1, 0)->GetActorLocation();
		StartTests::JoinerShots = Character(1)->GetWeapon()->ShotsFired;
	});
	// Now they can.
	Play(1, TEXT("hold Move 1 0\nhold Fire\nwait 0.8\nrelease Fire\nrelease Move\nwait 0.3"));
	Then([this]()
	{
		TestTrue(TEXT("the joiner moves"), FVector::Dist2D(CharacterOf(1, 0)->GetActorLocation(), StartTests::JoinerAt) > 50.0f);
		TestTrue(TEXT("and fires"), Character(1)->GetWeapon()->ShotsFired > StartTests::JoinerShots);
	});
	Play(0, TEXT("tap Streak\nwait 0.5"));
	Then([this]()
	{
		const ACheeseStrikePlayerState* Player = Character(0)->GetPlayerState<ACheeseStrikePlayerState>();
		TestEqual(TEXT("the streak is called in once the count is over"), Player->NextStreak(), CheeseStrikeStreaks::EStreak::None);
		TestFalse(TEXT("which ends the caller's protection"), Character(0)->IsProtected());
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFreshSpawnIsGhostlyAndUnhurtUntilItAttacks, "CheeseStrike.Play.AFreshSpawnIsGhostlyAndUnhurtUntilItAttacks", StartTests::Flags)

bool FAFreshSpawnIsGhostlyAndUnhurtUntilItAttacks::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	StartPlaying({2});
	PutAt(0, StartTests::HostSpot, StartTests::HostYaw);
	PutAt(1, StartTests::JoinerSpot, StartTests::JoinerYaw);
	// The host came in before the joiner, so protect them afresh, for long enough to look.
	Then([]() { Character(0)->CheeseStrikeProtect(30.0f); });
	Wait(0.5f);
	Then([this]()
	{
		FString Missing;
		TestTrue(TEXT("the host is protected"), Character(0)->IsProtected());
		TestTrue(FString::Printf(TEXT("and looks ghostly on their own machine, arms and weapon too (not: %s)"), *Missing), StartTests::LooksGhostly(Character(0), true, Missing));
		TestTrue(TEXT("and is protected on the joiner's"), CharacterOf(0, 1) != nullptr && CharacterOf(0, 1)->IsProtected());
		TestTrue(FString::Printf(TEXT("where they look ghostly too (not: %s)"), *Missing), StartTests::LooksGhostly(CharacterOf(0, 1), false, Missing));
	});
	// The joiner shoots the protected host: nothing, and the joiner's own protection is over on every machine.
	Play(1, TEXT("hold Fire\nwait 1\nrelease Fire\nwait 0.8"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		TestTrue(TEXT("the joiner fired"), Character(1)->GetWeapon()->ShotsFired > 0);
		TestEqual(TEXT("the protected host isn't hurt"), Host->GetHealth(), CheeseStrikeWeapons::MaxHealth);
		TestTrue(TEXT("and stays ghostly"), Host->GetGhostShown() > 0.0f);
		TestFalse(TEXT("firing ended the joiner's protection on the server"), CharacterOf(1, 0)->IsProtected());
		TestFalse(TEXT("and on their own machine"), Character(1)->IsProtected());
		TestTrue(TEXT("and their ghostly look, there"), StartTests::LooksNormal(Character(1)));
		TestTrue(TEXT("and on the host's"), StartTests::LooksNormal(CharacterOf(1, 0)));
	});
	// The host's protection runs out: no longer ghostly, on either machine, and hurt by shots.
	Then([]() { Character(0)->CheeseStrikeProtect(0.5f); });
	WaitUntil(TEXT("the host's ghostly look fading on both machines"), 5.0f, []() { return StartTests::LooksNormal(Character(0)) && StartTests::LooksNormal(CharacterOf(0, 1)); });
	Then([this]() { TestFalse(TEXT("the host's protection ended with time"), Character(0)->IsProtected() || CharacterOf(0, 1)->IsProtected()); });
	Play(1, TEXT("tap Fire\nwait 0.6"));
	Then([this]() { TestTrue(FString::Printf(TEXT("then the host is hurt (%.0f)"), Character(0)->GetHealth()), Character(0)->IsAlive() && Character(0)->GetHealth() < CheeseStrikeWeapons::MaxHealth); });
	// The knife's stroke ends it too.
	Then([]() { Character(0)->CheeseStrikeProtect(30.0f); });
	Play(0, TEXT("tap Knife\nwait 1\ntap Fire\nwait 0.8"));
	Then([this]()
	{
		AddInfo(FString::Printf(TEXT("The host holds %s, fired %d"), *Character(0)->GetWeapon()->GetStats().Id.ToString(), Character(0)->GetWeapon()->ShotsFired));
		TestTrue(TEXT("the host struck"), Character(0)->GetWeapon()->ShotsFired > 0 && Character(0)->GetWeapon()->GetStats().IsMelee());
		TestFalse(TEXT("which ended their protection on the server"), Character(0)->IsProtected());
		TestFalse(TEXT("and on the joiner's machine"), CharacterOf(0, 1)->IsProtected());
		TestTrue(TEXT("and their ghostly look, on theirs"), StartTests::LooksNormal(Character(0)));
		TestTrue(TEXT("and the joiner's"), StartTests::LooksNormal(CharacterOf(0, 1)));
	});
	StopPlaying();
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEachRoundCountsIn, "CheeseStrike.Play.EachRoundCountsIn", StartTests::Flags)

bool FEachRoundCountsIn::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	// Elimination with a count of three before the match, so two before each later round.
	StartPlaying({2, TEXT("?Mode=Elimination?Countdown=3?RoundTime=60")});
	WaitUntil(TEXT("the first round's count ending"), 8.0f, []() { return !GameState(0)->IsFrozen() && !GameState(1)->IsFrozen(); });
	Then([this]()
	{
		TestEqual(TEXT("round one"), GameState(0)->GetRound(), 1);
		// The round's clock started at the count's end.
		TestTrue(FString::Printf(TEXT("its clock ran from the count's end (%.1f s left)"), GameState(0)->SecondsLeftInRound()), GameState(0)->SecondsLeftInRound() > 58.0f);
		// The joiner's side is out: the round is over, and after the break the next counts in.
		CharacterOf(1, 0)->CheeseStrikeDie();
	});
	WaitUntil(TEXT("the second round"), 15.0f, []() { return GameState(0)->GetRound() == 2 && GameState(1)->GetRound() == 2 && Character(0) != nullptr && Character(0)->IsAlive(); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Host = GameState(0);
		AddInfo(FString::Printf(TEXT("Round two counts %.1f s more"), Host->SecondsToGo()));
		TestTrue(TEXT("round two counts in"), Host->IsFrozen() && Host->SecondsToGo() <= CheeseStrikeStart::RoundCountdown + 0.1f);
		TestTrue(TEXT("on the joiner's machine too"), GameState(1)->IsFrozen() && FMath::IsNearlyEqual(GameState(1)->GetGoAt(), Host->GetGoAt()));
		TestEqual(TEXT("with the round's clock standing at its time"), Host->SecondsLeftInRound(), 60.0f);
		TestFalse(TEXT("holding everyone"), Character(0)->CanAct());
		TestTrue(TEXT("who are protected"), Character(0)->IsProtected());
	});
	WaitUntil(TEXT("round two's count ending"), 4.0f, []() { return !GameState(0)->IsFrozen(); });
	Then([this]()
	{
		TestTrue(TEXT("then everyone can act"), Character(0)->CanAct());
		TestTrue(TEXT("protected for a while yet"), Character(0)->IsProtected());
	});
	StopPlaying();
	return true;
}

#endif
