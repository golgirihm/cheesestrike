// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Weapon pickups as plain rules (Rules/CheeseStrikePickups.h): the weak
// primary everyone comes in with, what pressing Interact over a weapon on a
// stand or the floor does by what is carried (swaps it, tops up its rounds,
// or nothing), who may take one, the reach, the seconds until a
// stand is back, how long a dropped weapon lies and how many, which modes
// have them, what bots go for, and how the stands are spread over a map;
// and the HUD's prompt.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikePickups.h"
#include "UI/CheeseStrikePickupHUD.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeShot.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PickupTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryoneComesInWithTheWeakestPrimary, "CheeseStrike.Pickups.EveryoneComesInWithTheWeakestPrimary", PickupTests::Flags)

bool FEveryoneComesInWithTheWeakestPrimary::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikePickups;
	const FWeaponStats* Spawn = CheeseStrikeArsenal::Find(SpawnPrimary());
	if (!TestNotNull(TEXT("the spawn primary is a weapon"), Spawn))
	{
		return false;
	}
	TestEqual(TEXT("the Colby Carbine"), Spawn->Id, CheeseStrikeWeapons::Carbine().Id);
	TestTrue(TEXT("a primary"), Spawn->Slot == EWeaponSlot::Primary);
	TestFalse(TEXT("which never lies on a stand"), CanLieOnAStand(Spawn->Id));
	TestEqual(TEXT("and is worth nothing more"), Worth(Spawn->Id), 0);
	for (const FName& Id : CheeseStrikeArsenal::InSlot(EWeaponSlot::Primary))
	{
		if (Id == Spawn->Id)
		{
			continue;
		}
		const FWeaponStats& Other = *CheeseStrikeArsenal::Find(Id);
		TestTrue(FString::Printf(TEXT("the %s lies on stands"), *Other.DisplayName.ToString()), CanLieOnAStand(Id));
		TestTrue(FString::Printf(TEXT("and is worth more (%d)"), Worth(Id)), Worth(Id) > 0);
		// Every other gun kills faster, close and at the body, but the scoped rifle, whose kill is a head shot.
		if (!Other.bScoped)
		{
			TestTrue(FString::Printf(TEXT("the %s kills faster (%.2f s against %.2f s)"), *Other.DisplayName.ToString(), CheeseStrikeWeapons::TimeToKill(Other), CheeseStrikeWeapons::TimeToKill(*Spawn)),
				CheeseStrikeWeapons::TimeToKill(Other) < CheeseStrikeWeapons::TimeToKill(*Spawn));
		}
	}
	TestEqual(TEXT("the Roquefort is worth the most"), Worth(CheeseStrikeWeapons::Roquefort().Id), 3);
	TestTrue(TEXT("the Blaster and the Munster more than the Grater and the Shotgun"),
		Worth(CheeseStrikeWeapons::Blaster().Id) == 2 && Worth(CheeseStrikeWeapons::Munster().Id) == 2 && Worth(CheeseStrikeWeapons::Grater().Id) == 1 && Worth(CheeseStrikeWeapons::Shotgun().Id) == 1 && Worth(CheeseStrikeWeapons::Burratas().Id) == 1);
	TestFalse(TEXT("no sidearm on a stand"), CanLieOnAStand(CheeseStrikeArsenal::Sidearm()));
	TestFalse(TEXT("nor the knife"), CanLieOnAStand(CheeseStrikeArsenal::Knife()));
	TestFalse(TEXT("nor anything that isn't a weapon"), CanLieOnAStand(TEXT("bazooka")));
	// The Gouda Overload's Gatling: never on a stand or the floor, and nothing is taken while it is carried.
	const FName Gatling = CheeseStrikeWeapons::Gatling().Id;
	TestFalse(TEXT("nor the Gouda Gatling"), CanLieOnAStand(Gatling) || CanBeDropped(Gatling));
	TestTrue(TEXT("carrying it, nothing is taken from a stand"), HowToTake(Gatling, CheeseStrikeWeapons::Roquefort().Id, false) == ETake::None);
	TestTrue(TEXT("or the floor"), HowToTake(Gatling, CheeseStrikeWeapons::Blaster().Id, false, true) == ETake::None);
	TestEqual(TEXT("and a bot carrying it wants nothing better"), BotWants(Gatling, CheeseStrikeWeapons::Roquefort().Id, 300.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAWeaponIsTakenByWhatIsCarried, "CheeseStrike.Pickups.AWeaponIsTakenByWhatIsCarried", PickupTests::Flags)

bool FAWeaponIsTakenByWhatIsCarried::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikePickups;
	const FName Carbine = CheeseStrikeWeapons::Carbine().Id;
	const FName Blaster = CheeseStrikeWeapons::Blaster().Id;
	const FName Roquefort = CheeseStrikeWeapons::Roquefort().Id;
	// The swap rules: every weapon is pressed for, whatever is carried (nothing is taken by walking over it).
	TestTrue(TEXT("with only the spawn primary, a weapon is swapped for"), HowToTake(Carbine, Roquefort, false) == ETake::Swap);
	TestTrue(TEXT("its rounds full or not"), HowToTake(Carbine, Roquefort, true) == ETake::Swap);
	TestTrue(TEXT("with no primary at all, too"), HowToTake(NAME_None, Blaster, false) == ETake::Swap);
	TestTrue(TEXT("with a better one"), HowToTake(Blaster, Roquefort, false) == ETake::Swap);
	TestTrue(TEXT("and for a worse one"), HowToTake(Roquefort, Blaster, false) == ETake::Swap);
	TestTrue(TEXT("one's own weapon tops up its rounds"), HowToTake(Blaster, Blaster, false) == ETake::TopUp);
	TestTrue(TEXT("unless they are full"), HowToTake(Blaster, Blaster, true) == ETake::None);
	TestTrue(TEXT("whatever the case of its name"), HowToTake(TEXT("BLASTER"), Blaster, false) == ETake::TopUp);
	TestTrue(TEXT("a stand of the spawn primary offers nothing"), HowToTake(Blaster, Carbine, false) == ETake::None);
	TestTrue(TEXT("nor one of a sidearm"), HowToTake(Carbine, CheeseStrikeArsenal::Sidearm(), false) == ETake::None);
	// On the floor: the spawn primary too, as it was dropped.
	TestTrue(TEXT("the spawn primary can be dropped"), CanBeDropped(Carbine));
	TestTrue(TEXT("and any other primary"), CanBeDropped(Roquefort) && CanBeDropped(CheeseStrikeWeapons::Burratas().Id));
	TestFalse(TEXT("but not the sidearm or the knife, which are kept"), CanBeDropped(CheeseStrikeArsenal::Sidearm()) || CanBeDropped(CheeseStrikeArsenal::Knife()));
	TestTrue(TEXT("a Colby Carbine on the floor is swapped for"), HowToTake(Roquefort, Carbine, false, true) == ETake::Swap);
	TestTrue(TEXT("or its rounds taken, by someone with one"), HowToTake(Carbine, Carbine, false, true) == ETake::TopUp);
	TestTrue(TEXT("never a sidearm"), HowToTake(Carbine, CheeseStrikeArsenal::Sidearm(), false, true) == ETake::None);
	TestEqual(TEXT("a stand tops up its weapon's reserve"), TopUpRounds(false, 40, 7, 9), 40);
	TestEqual(TEXT("a weapon on the floor everything it has"), TopUpRounds(true, 40, 7, 9), 16);

	// Dropped weapons: twenty seconds, at most twelve, blinking before they go.
	TestEqual(TEXT("twenty seconds on the floor"), DroppedSeconds, 20.0f);
	TestEqual(TEXT("at most twelve"), MostDropped, 12);
	TestEqual(TEXT("none cleared with room"), ToClearForDrop(5), 0);
	TestEqual(TEXT("the oldest goes for the thirteenth"), ToClearForDrop(12), 1);
	TestEqual(TEXT("and as many as it takes past that"), ToClearForDrop(14), 3);
	TestTrue(TEXT("shown while fresh"), DroppedShown(0.0f) && DroppedShown(DroppedSeconds - DroppedBlink - 0.1f));
	int32 Hidden = 0;
	for (float Age = DroppedSeconds - DroppedBlink; Age < DroppedSeconds; Age += 0.02f)
	{
		Hidden += DroppedShown(Age) ? 0 : 1;
	}
	TestTrue(FString::Printf(TEXT("blinking in its last seconds (%d of 150 frames hidden)"), Hidden), Hidden > 30 && Hidden < 90);

	// Who may take one.
	TestTrue(TEXT("anyone alive and free to act"), MayTake(true, true, false, true));
	TestFalse(TEXT("not the dead"), MayTake(false, true, false, true));
	TestFalse(TEXT("not while the count holds everyone"), MayTake(true, false, false, true));
	TestFalse(TEXT("not someone down"), MayTake(true, true, true, true));
	TestFalse(TEXT("not someone kept to their sidearm"), MayTake(true, true, false, false));

	// The reach: a bot goes over it; pressing Interact reaches further.
	const FVector Stand(1000.0f, 0.0f, 400.0f);
	TestTrue(TEXT("over it"), InReach(Stand, Stand + FVector(30.0f, 30.0f, 0.0f), Reach));
	TestFalse(TEXT("not a step off it"), InReach(Stand, Stand + FVector(Reach + 20.0f, 0.0f, 0.0f), Reach));
	TestTrue(TEXT("but in reach to press for"), InReach(Stand, Stand + FVector(Reach + 20.0f, 0.0f, 0.0f), PressReach));
	TestFalse(TEXT("nor from the floor under it"), InReach(Stand, Stand - FVector(0.0f, 0.0f, ReachUp + 10.0f), Reach));
	TestTrue(TEXT("a jump over it does"), InReach(Stand, Stand + FVector(0.0f, 0.0f, 60.0f), Reach));

	// Back after a minute (27 Oct 2026; half a minute before), the HUD's seconds rounded up.
	TestEqual(TEXT("a minute"), RespawnSeconds, 60.0f);
	TestEqual(TEXT("there: none to go"), SecondsLeft(100.0f, 0.0f), 0.0f);
	TestEqual(TEXT("taken at 100: 55 to go at 105"), SecondsLeft(105.0f, 100.0f + RespawnSeconds), 55.0f);
	TestEqual(TEXT("and none once it is back"), SecondsLeft(170.0f, 100.0f + RespawnSeconds), 0.0f);
	TestEqual(TEXT("24.2 s shows 25"), ShownSeconds(24.2f), 25);
	TestEqual(TEXT("and 0.1 shows 1"), ShownSeconds(0.1f), 1);

	// Every mode but Gun Game.
	for (int32 Index = 0; Index < CheeseStrikeModes::NumModes; ++Index)
	{
		const CheeseStrikeModes::EMode Mode = CheeseStrikeModes::ModeAt(Index);
		TestEqual(FString::Printf(TEXT("%s has pickups"), CheeseStrikeModes::Info(Mode).Name), InMode(Mode), Mode != CheeseStrikeModes::EMode::GunGame);
	}

	// The prompt names the key and both weapons.
	TestEqual(TEXT("the prompt"), CheeseStrikePickupHUD::PromptLine(TEXT("E"), TEXT("Roquefort Rifle")), FString(TEXT("PRESS E  ·  TAKE THE ROQUEFORT RIFLE")));
	TestEqual(TEXT("and the pad's button"), CheeseStrikePickupHUD::PromptLine(TEXT("LB"), TEXT("Cheddar Blaster")), FString(TEXT("PRESS LB  ·  TAKE THE CHEDDAR BLASTER")));
	TestEqual(TEXT("or the rounds of one's own"), CheeseStrikePickupHUD::PromptLine(TEXT("E"), TEXT("Cheddar Blaster"), true), FString(TEXT("PRESS E  ·  TAKE THE CHEDDAR BLASTER'S ROUNDS")));
	TestEqual(TEXT("what it is swapped for"), CheeseStrikePickupHUD::PromptUnder(TEXT("Munster Machine Gun")), FString(TEXT("for your Munster Machine Gun")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsGoForBetterWeaponsNearThem, "CheeseStrike.Pickups.BotsGoForBetterWeaponsNearThem", PickupTests::Flags)

bool FBotsGoForBetterWeaponsNearThem::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikePickups;
	const FName Carbine = CheeseStrikeWeapons::Carbine().Id;
	const FName Blaster = CheeseStrikeWeapons::Blaster().Id;
	const FName Grater = CheeseStrikeWeapons::Grater().Id;
	const FName Roquefort = CheeseStrikeWeapons::Roquefort().Id;
	TestTrue(TEXT("a bot with the spawn primary wants anything better"), BotWants(Carbine, Grater, 1000.0f) > 0.0f);
	TestTrue(TEXT("the better the more"), BotWants(Carbine, Roquefort, 1000.0f) > BotWants(Carbine, Blaster, 1000.0f));
	TestTrue(TEXT("the nearer the more"), BotWants(Carbine, Blaster, 500.0f) > BotWants(Carbine, Blaster, 2000.0f));
	TestTrue(TEXT("a better one further off before a lesser one near"), BotWants(Carbine, Roquefort, 2000.0f) > BotWants(Carbine, Grater, 300.0f));
	TestEqual(TEXT("nothing too far off"), BotWants(Carbine, Roquefort, BotRange + 1.0f), 0.0f);
	TestEqual(TEXT("nothing no better than its own"), BotWants(Blaster, Grater, 300.0f), 0.0f);
	TestEqual(TEXT("nor its own again"), BotWants(Blaster, Blaster, 300.0f), 0.0f);
	TestEqual(TEXT("nor the spawn primary"), BotWants(NAME_None, Carbine, 300.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheStandsAreFairToBothSides, "CheeseStrike.Pickups.TheStandsAreFairToBothSides", PickupTests::Flags)

bool FTheStandsAreFairToBothSides::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikePickups;
	const FName Blaster = CheeseStrikeWeapons::Blaster().Id;
	const FName Grater = CheeseStrikeWeapons::Grater().Id;
	const FName Roquefort = CheeseStrikeWeapons::Roquefort().Id;
	// A map mirrored across x = 0, each side's starts at its end.
	const TArray<FVector> Gouda = {FVector(-5000.0f, -500.0f, 0.0f), FVector(-5000.0f, 500.0f, 0.0f)};
	const TArray<FVector> Mozza = {FVector(5000.0f, -500.0f, 0.0f), FVector(5000.0f, 500.0f, 0.0f)};
	FString Why;
	const TArray<FSpot> Mirrored = {{FVector(0.0f, 0.0f, 400.0f), Roquefort}, {FVector(-3000.0f, 1000.0f, 0.0f), Blaster}, {FVector(3000.0f, 1000.0f, 0.0f), Blaster},
		{FVector(-4000.0f, -1000.0f, 0.0f), Grater}, {FVector(4000.0f, -1000.0f, 0.0f), Grater}};
	TestTrue(TEXT("pairs mirrored and one in the middle are fair"), AreFair(Mirrored, Gouda, Mozza, Why));
	TestTrue(FString::Printf(TEXT("and the better lie riskier (%s)"), *Why), BetterLieRiskier(Mirrored, Gouda, Mozza, Why));
	TestTrue(TEXT("the middle is the most contested"), Contest(FVector::ZeroVector, Gouda, Mozza) > 0.99f);
	TestTrue(TEXT("a side's own end the least"), Contest(FVector(-4900.0f, 0.0f, 0.0f), Gouda, Mozza) < 0.15f);

	TArray<FSpot> OneSided = Mirrored;
	OneSided[2].Weapon = Grater;
	TestFalse(TEXT("a better weapon nearer one side than its twin is not fair"), AreFair(OneSided, Gouda, Mozza, Why));
	TestTrue(FString::Printf(TEXT("and is named (%s)"), *Why), Why.Contains(TEXT("blaster")));
	TArray<FSpot> Moved = Mirrored;
	Moved[2].At = FVector(1500.0f, 1000.0f, 0.0f);
	TestFalse(TEXT("nor is one twin nearer the middle than the other"), AreFair(Moved, Gouda, Mozza, Why));

	TArray<FSpot> Backwards = Mirrored;
	Backwards[0].Weapon = Grater;
	Backwards[3].Weapon = Roquefort;
	Backwards[4].Weapon = Roquefort;
	TestFalse(TEXT("the best by the starts and the least in the middle is backwards"), BetterLieRiskier(Backwards, Gouda, Mozza, Why));

	// A map turned half round (x, y to -x, -y): its twins are turned too.
	const TArray<FVector> TurnedGouda = {FVector(-3000.0f, -3000.0f, 0.0f)};
	const TArray<FVector> TurnedMozza = {FVector(3000.0f, 3000.0f, 0.0f)};
	const TArray<FSpot> Turned = {{FVector(0.0f, 0.0f, 340.0f), Roquefort}, {FVector(-1700.0f, -600.0f, 0.0f), Blaster}, {FVector(1700.0f, 600.0f, 0.0f), Blaster}};
	TestTrue(TEXT("twins turned half round are fair"), AreFair(Turned, TurnedGouda, TurnedMozza, Why));
	return true;
}

#endif
