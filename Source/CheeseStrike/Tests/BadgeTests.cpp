// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// A kill's badges (Rules/CheeseStrikeBadges.h): which facts earn which.
// That they reach the feed on every machine and are counted is checked by
// CheeseStrike.Play.WeaponsWorkOverTheNetwork (an assassination).

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeBadges.h"
#include "Weapons/CheeseStrikeArsenal.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAKillEarnsItsBadges, "CheeseStrike.Badges.AKillEarnsItsBadges", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAKillEarnsItsBadges::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBadges;
	const auto Shot = [](const FWeaponStats& Weapon, float Distance, bool bHead = false)
	{
		FKillFacts Facts;
		Facts.Weapon = Weapon.Id;
		Facts.Distance = Distance;
		Facts.bHead = bHead;
		return Facts;
	};
	TestEqual(TEXT("a plain kill earns nothing"), BadgesFor(Shot(CheeseStrikeWeapons::Blaster(), 1000.0f)), static_cast<uint8>(0));
	TestTrue(TEXT("a kill on the head is a HEADSHOT"), Has(BadgesFor(Shot(CheeseStrikeWeapons::Blaster(), 1000.0f, true)), EBadge::Headshot));

	FKillFacts Knife = Shot(CheeseStrikeWeapons::Knife(), 100.0f);
	TestEqual(TEXT("the knife from the front earns nothing"), BadgesFor(Knife), static_cast<uint8>(0));
	Knife.bFromBehind = true;
	Knife.bOneHit = true;
	TestEqual(TEXT("from behind, an ASSASSINATION only (not ONE SHOT)"), BadgesFor(Knife), static_cast<uint8>(1 << static_cast<uint8>(EBadge::Assassination)));

	FKillFacts Executed;
	Executed.bExecuted = true;
	TestTrue(TEXT("an execution is an EXECUTION"), Has(BadgesFor(Executed), EBadge::Execution));

	TestTrue(TEXT("the shotgun within 2 m is POINT BLANK"), Has(BadgesFor(Shot(CheeseStrikeWeapons::Shotgun(), PointBlankRange)), EBadge::PointBlank));
	TestFalse(TEXT("not beyond"), Has(BadgesFor(Shot(CheeseStrikeWeapons::Shotgun(), PointBlankRange + 10.0f)), EBadge::PointBlank));
	TestFalse(TEXT("nor another gun"), Has(BadgesFor(Shot(CheeseStrikeWeapons::Pistol(), 50.0f)), EBadge::PointBlank));

	TestTrue(TEXT("40 m away is a LONGSHOT"), Has(BadgesFor(Shot(CheeseStrikeWeapons::Roquefort(), LongshotRange)), EBadge::Longshot));
	TestFalse(TEXT("39 m isn't"), Has(BadgesFor(Shot(CheeseStrikeWeapons::Roquefort(), LongshotRange - 100.0f)), EBadge::Longshot));

	FKillFacts Revenge = Shot(CheeseStrikeWeapons::Blaster(), 500.0f);
	Revenge.bRevenge = true;
	TestTrue(TEXT("killing who last killed you is REVENGE"), Has(BadgesFor(Revenge), EBadge::Revenge));

	FKillFacts One = Shot(CheeseStrikeWeapons::Roquefort(), 2000.0f);
	One.bOneHit = true;
	TestTrue(TEXT("full health to dead in one hit is ONE SHOT"), Has(BadgesFor(One), EBadge::OneShot));
	One.bHead = true;
	const uint8 RoquefortHead = BadgesFor(One);
	TestTrue(TEXT("but not the Roquefort's headshot, which always is: a HEADSHOT"), Has(RoquefortHead, EBadge::Headshot) && !Has(RoquefortHead, EBadge::OneShot));
	FKillFacts Pellets = Shot(CheeseStrikeWeapons::Shotgun(), 150.0f, true);
	Pellets.bOneHit = true;
	const uint8 Close = BadgesFor(Pellets);
	TestTrue(TEXT("a shotgun blast to the head up close earns all three"), Has(Close, EBadge::Headshot) && Has(Close, EBadge::PointBlank) && Has(Close, EBadge::OneShot));
	TestEqual(TEXT("listed in order"), List(Close).Num(), 3);
	TestEqual(TEXT("HEADSHOT first"), FString(Word(List(Close)[0])), FString(TEXT("HEADSHOT")));

	FKillFacts Own = Shot(CheeseStrikeWeapons::Blaster(), 10.0f, true);
	Own.bByAnother = false;
	TestEqual(TEXT("killing oneself earns nothing"), BadgesFor(Own), static_cast<uint8>(0));
	FKillFacts Streak;
	Streak.bOneHit = true;
	Streak.Distance = 5000.0f;
	TestEqual(TEXT("a streak's kill earns no gun's badges"), BadgesFor(Streak), static_cast<uint8>(0));
	for (int32 Each = 0; Each < NumBadges; ++Each)
	{
		TestFalse(TEXT("every badge has a word"), FString(Word(static_cast<EBadge>(Each))).IsEmpty());
	}
	return true;
}

#endif
