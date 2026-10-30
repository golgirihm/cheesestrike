// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The bots: their rules as plain functions (Rules/CheeseStrikeBots.h), and
// bots playing in the editor (see PlayTesting.h): one noticing a player in
// front of it and shooting them, and a full match of them on The Creamery.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeBots.h"
#include "Weapons/CheeseStrikeArsenal.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BotTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotSkillsAreTheDesigns, "CheeseStrike.Bots.SkillsAreTheDesigns", BotTests::Flags)

bool FBotSkillsAreTheDesigns::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBots;
	TestEqual(TEXT("three skills"), NumSkills, 3);
	TestEqual(TEXT("easy reacts in 0.75 s"), Skill(0).Reaction, 0.75f);
	TestEqual(TEXT("normal aims 4 degrees off at first"), Skill(1).AimError, 4.0f);
	TestEqual(TEXT("hard fires bursts of 8"), Skill(2).Burst, 8);
	TestEqual(TEXT("hard turns at 8 rad/s"), Skill(2).TurnRate, 8.0f);
	TestEqual(TEXT("normal unless the host says"), SkillFrom(TEXT("?listen")), 1);
	TestEqual(TEXT("as the URL says"), SkillFrom(TEXT("?BotSkill=2")), 2);
	TestEqual(TEXT("within the table"), SkillFrom(TEXT("?BotSkill=7")), 2);
	TestEqual(TEXT("a skill past the table is its last"), FString(Skill(9).Name), FString(TEXT("Hard")));
	// The Practice Range's dummies: not a skill of the table (the lobby never offers it), but a word of their own.
	TestTrue(TEXT("?BotSkill=dummy makes the bots dummies"), DummiesFrom(TEXT("?Bots=4?BotSkill=dummy")));
	TestFalse(TEXT("a skill doesn't"), DummiesFrom(TEXT("?BotSkill=2")) || DummiesFrom(TEXT("?listen")));
	TestEqual(TEXT("and the dummies' skill is the usual, for anything that asks"), SkillFrom(TEXT("?BotSkill=dummy")), DefaultSkill);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotAimSteadiesAsItTracks, "CheeseStrike.Bots.AimSteadiesAsItTracks", BotTests::Flags)

bool FBotAimSteadiesAsItTracks::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBots;
	const FSkill& Normal = Skill(1);
	TestEqual(TEXT("its first aim is off by the skill's error"), AimError(Normal, 0.0f), 4.0f);
	TestEqual(TEXT("halfway through steadying"), AimError(Normal, 0.75f), 2.8f, 0.001f);
	TestEqual(TEXT("steady after 1.5 s"), AimError(Normal, 1.5f), 1.6f, 0.001f);
	TestEqual(TEXT("and no steadier after"), AimError(Normal, 10.0f), 1.6f, 0.001f);
	TestTrue(TEXT("a hard bot steadies better than an easy one"), AimError(Skill(2), 2.0f) < AimError(Skill(0), 2.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotTurnsNoFasterThanItsSkill, "CheeseStrike.Bots.TurnsNoFasterThanItsSkill", BotTests::Flags)

bool FBotTurnsNoFasterThanItsSkill::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBots;
	const FRotator Turned = TurnToward(FRotator(0, 0, 0), FRotator(0, 90, 0), 5.0f, 0.1f);
	TestEqual(TEXT("5 rad/s for a tenth of a second is about 29 degrees"), float(Turned.Yaw), FMath::RadiansToDegrees(0.5f), 0.01f);
	const FRotator Round = TurnToward(FRotator(0, 170, 0), FRotator(0, -170, 0), 8.0f, 1.0f);
	TestEqual(TEXT("the short way round, past 180"), float(FRotator::NormalizeAxis(Round.Yaw)), -170.0f, 0.01f);
	const FRotator Small = TurnToward(FRotator(0, 0, 0), FRotator(0, 10, 0), 8.0f, 0.01f);
	TestEqual(TEXT("part of the way"), float(Small.Yaw), FMath::RadiansToDegrees(0.08f), 0.01f);
	const FRotator Both = TurnToward(FRotator(0, 0, 0), FRotator(30, 40, 0), 3.0f, 0.1f);
	TestEqual(TEXT("up and across together: both a share of the way"), float(Both.Pitch / Both.Yaw), 0.75f, 0.01f);
	const FRotator There = TurnToward(FRotator(0, 0, 0), FRotator(-5, 3, 0), 8.0f, 0.1f);
	TestEqual(TEXT("arrives when near"), There, FRotator(-5, 3, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotSeesWhatIsInFront, "CheeseStrike.Bots.SeesWhatIsInFront", BotTests::Flags)

bool FBotSeesWhatIsInFront::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBots;
	const FVector Facing(1, 0, 0);
	TestTrue(TEXT("ahead"), InSight(Facing, FVector(3000, 0, 0)));
	TestTrue(TEXT("ahead and above"), InSight(Facing, FVector(1000, 0, 800)));
	TestFalse(TEXT("behind"), InSight(Facing, FVector(-3000, 0, 0)));
	TestTrue(TEXT("behind but right there"), InSight(Facing, FVector(-300, 0, 0)));
	TestFalse(TEXT("too far"), InSight(Facing, FVector(SightRange + 100, 0, 0)));
	const FVector At59 = FRotator(0, 59, 0).Vector() * 2000.0f;
	const FVector At61 = FRotator(0, 61, 0).Vector() * 2000.0f;
	TestTrue(TEXT("just inside the cone"), InSight(Facing, At59));
	TestFalse(TEXT("just outside it"), InSight(Facing, At61));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotKeepsItsDistance, "CheeseStrike.Bots.KeepsItsDistance", BotTests::Flags)

bool FBotKeepsItsDistance::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBots;
	// The Cheddar Blaster reaches 50 m: a bot fights from about 22 m.
	TestTrue(TEXT("far off: closes in"), MoveFor(4000.0f, 5000.0f, 100.0f, true) == EMove::Approach);
	TestTrue(TEXT("about its range: holds"), MoveFor(2200.0f, 5000.0f, 100.0f, true) == EMove::Hold);
	TestTrue(TEXT("a little past it: still holds"), MoveFor(2600.0f, 5000.0f, 100.0f, true) == EMove::Hold);
	TestTrue(TEXT("inside 6 m: backs off"), MoveFor(500.0f, 5000.0f, 100.0f, true) == EMove::BackOff);
	TestTrue(TEXT("hurt: gets away, however far"), MoveFor(2200.0f, 5000.0f, 40.0f, true) == EMove::Retreat);
	TestTrue(TEXT("at 45 it still fights"), MoveFor(2200.0f, 5000.0f, 45.0f, true) == EMove::Hold);
	TestTrue(TEXT("hurt with no wedge to go for: fights on"), MoveFor(2200.0f, 5000.0f, 40.0f, false) == EMove::Hold);
	// It opens fire within 30 m with the Blaster, further only at someone shooting it, and never beyond the weapon's reach.
	TestTrue(TEXT("fires at 25 m"), WouldFireAt(2500.0f, 5000.0f, false));
	TestFalse(TEXT("holds fire at 40 m"), WouldFireAt(4000.0f, 5000.0f, false));
	TestTrue(TEXT("unless they shoot it"), WouldFireAt(4000.0f, 5000.0f, true));
	TestFalse(TEXT("never past its reach"), WouldFireAt(5500.0f, 5000.0f, true));
	// A weapon for close quarters is fought with close: the Swiss Shotgun reaches 13 m, and a bot holds at 4 m rather than backing off.
	const float Shotgun = CheeseStrikeWeapons::Shotgun().MaxRange;
	TestTrue(TEXT("with the shotgun, closes in from 10 m"), MoveFor(1000.0f, Shotgun, 100.0f, true) == EMove::Approach);
	TestTrue(TEXT("holds at 4 m"), MoveFor(400.0f, Shotgun, 100.0f, true) == EMove::Hold);
	TestTrue(TEXT("backs off only from someone on top of it"), MoveFor(250.0f, Shotgun, 100.0f, true) == EMove::BackOff);
	// The BabyBomb: never fired at anyone nearer than 6 m, whose burst would reach the bot too.
	const float Least = CheeseStrikeWeapons::BabyBomb().BotLeast;
	TestEqual(TEXT("the BabyBomb's nearest is 6 m"), Least, 600.0f);
	TestFalse(TEXT("not at 5 m"), FarEnoughFor(500.0f, Least));
	TestTrue(TEXT("at 8 m"), FarEnoughFor(800.0f, Least));
	TestTrue(TEXT("any other weapon at any distance"), FarEnoughFor(10.0f, CheeseStrikeWeapons::Blaster().BotLeast));
	// The Cheese Stick: right up to them, a swing in its reach.
	const float StickReach = CheeseStrikeWeapons::Stick().Reach;
	TestTrue(TEXT("with the stick, closes in from 4 m"), MoveFor(400.0f, CheeseStrikeWeapons::Stick().MaxRange, 100.0f, true) == EMove::Approach);
	TestTrue(TEXT("swings at 2.5 m"), WouldStrikeAt(250.0f, StickReach));
	// The knife: right up to them, and a stroke once they are in reach.
	const float Reach = CheeseStrikeWeapons::Knife().Reach;
	TestTrue(TEXT("with the knife, closes in from 2 m"), MoveFor(200.0f, Reach, 100.0f, true) == EMove::Approach);
	TestTrue(TEXT("strikes in reach"), WouldStrikeAt(120.0f, Reach));
	TestFalse(TEXT("not out of it"), WouldStrikeAt(Reach + 20.0f, Reach));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotChoosesItsWeapons, "CheeseStrike.Bots.ChoosesItsWeapons", BotTests::Flags)

bool FBotChoosesItsWeapons::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBots;
	// Its primary: one of the primaries, suited to its skill.
	FRandomStream Random(99);
	for (int32 SkillIndex = 0; SkillIndex < NumSkills; ++SkillIndex)
	{
		TSet<FName> Chosen;
		for (int32 Bot = 0; Bot < 200; ++Bot)
		{
			const FName Primary = PrimaryFor(SkillIndex, Random);
			const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Primary);
			if (!TestTrue(TEXT("a primary"), Stats != nullptr && Stats->Slot == EWeaponSlot::Primary))
			{
				return false;
			}
			Chosen.Add(Primary);
		}
		AddInfo(FString::Printf(TEXT("%s bots carry %d kinds of primary"), Skill(SkillIndex).Name, Chosen.Num()));
		TestTrue(FString::Printf(TEXT("%s bots carry several kinds"), Skill(SkillIndex).Name), Chosen.Num() >= 4);
		TestEqual(FString::Printf(TEXT("easy bots never the scoped rifle; others may (%s)"), Skill(SkillIndex).Name), Chosen.Contains(CheeseStrikeWeapons::Roquefort().Id), SkillIndex > 0);
	}

	// What it holds.
	FArmed Armed;
	Armed.bPrimary = true;
	Armed.bSidearm = true;
	Armed.bKnife = true;
	Armed.PrimaryMagazine = 10;
	Armed.bPrimaryReserve = true;
	TestTrue(TEXT("its primary, in a fight at range"), ChooseWeapon(Armed, true, 2000.0f, EWeaponSlot::Primary) == EWeaponSlot::Primary);
	TestTrue(TEXT("and out of one"), ChooseWeapon(Armed, false, 0.0f, EWeaponSlot::Knife) == EWeaponSlot::Primary);
	TestTrue(TEXT("the knife for someone close"), ChooseWeapon(Armed, true, 200.0f, EWeaponSlot::Primary) == EWeaponSlot::Knife);
	TestTrue(TEXT("kept out as they step back a little"), ChooseWeapon(Armed, true, 300.0f, EWeaponSlot::Knife) == EWeaponSlot::Knife);
	TestTrue(TEXT("put away when they get away"), ChooseWeapon(Armed, true, 500.0f, EWeaponSlot::Knife) == EWeaponSlot::Primary);
	Armed.PrimaryMagazine = 0;
	TestTrue(TEXT("its magazine empty with an enemy near: the sidearm, quicker than reloading"), ChooseWeapon(Armed, true, 1200.0f, EWeaponSlot::Primary) == EWeaponSlot::Sidearm);
	TestTrue(TEXT("kept to the end of the fight"), ChooseWeapon(Armed, true, 2500.0f, EWeaponSlot::Sidearm) == EWeaponSlot::Sidearm);
	TestTrue(TEXT("with the enemy far off, it reloads"), ChooseWeapon(Armed, true, 3000.0f, EWeaponSlot::Primary) == EWeaponSlot::Primary);
	TestTrue(TEXT("after the fight, back to the primary to reload it"), ChooseWeapon(Armed, false, 0.0f, EWeaponSlot::Sidearm) == EWeaponSlot::Primary);
	Armed.bPrimaryReserve = false;
	TestTrue(TEXT("out of rounds for it altogether: the sidearm"), ChooseWeapon(Armed, false, 0.0f, EWeaponSlot::Primary) == EWeaponSlot::Sidearm);
	// Only what the mode lets it draw.
	FArmed Execution;
	Execution.bSidearm = true;
	TestTrue(TEXT("with only the sidearm, the sidearm"), ChooseWeapon(Execution, true, 100.0f, EWeaponSlot::Sidearm) == EWeaponSlot::Sidearm);
	FArmed KnifeOnly;
	KnifeOnly.bKnife = true;
	TestTrue(TEXT("with only the knife, the knife, near or far"), ChooseWeapon(KnifeOnly, true, 3000.0f, EWeaponSlot::Knife) == EWeaponSlot::Knife);
	// The Cheese Stick: up close it, further off the pistol, out of a fight the stick.
	FArmed Stick;
	Stick.bPrimary = true;
	Stick.bSidearm = true;
	Stick.bKnife = true;
	Stick.bPrimaryMelee = true;
	Stick.PrimaryReach = CheeseStrikeWeapons::Stick().Reach;
	TestTrue(TEXT("the stick for someone close, not the knife"), ChooseWeapon(Stick, true, 200.0f, EWeaponSlot::Primary) == EWeaponSlot::Primary);
	TestTrue(TEXT("drawn as they come within a few metres"), ChooseWeapon(Stick, true, 400.0f, EWeaponSlot::Sidearm) == EWeaponSlot::Primary);
	TestTrue(TEXT("kept out as they step back a little"), ChooseWeapon(Stick, true, 600.0f, EWeaponSlot::Primary) == EWeaponSlot::Primary);
	TestTrue(TEXT("the pistol for someone far off"), ChooseWeapon(Stick, true, 2000.0f, EWeaponSlot::Primary) == EWeaponSlot::Sidearm);
	TestTrue(TEXT("and the stick again after the fight"), ChooseWeapon(Stick, false, 0.0f, EWeaponSlot::Sidearm) == EWeaponSlot::Primary);
	Stick.bSidearm = false;
	TestTrue(TEXT("with no pistol, the stick even far off, to close in with"), ChooseWeapon(Stick, true, 2000.0f, EWeaponSlot::Primary) == EWeaponSlot::Primary);
	// The BabyBomb: the pistol for someone too close for its burst.
	FArmed Bomb = Armed;
	Bomb.PrimaryMagazine = 2;
	Bomb.bPrimaryReserve = true;
	Bomb.PrimaryLeast = CheeseStrikeWeapons::BabyBomb().BotLeast;
	TestTrue(TEXT("the BabyBomb at 10 m"), ChooseWeapon(Bomb, true, 1000.0f, EWeaponSlot::Primary) == EWeaponSlot::Primary);
	TestTrue(TEXT("the pistol at 4 m"), ChooseWeapon(Bomb, true, 400.0f, EWeaponSlot::Primary) == EWeaponSlot::Sidearm);
	TestTrue(TEXT("and the knife on top of them"), ChooseWeapon(Bomb, true, 150.0f, EWeaponSlot::Primary) == EWeaponSlot::Knife);
	// No bot comes in with a power weapon.
	for (int32 SkillIndex = 0; SkillIndex < NumSkills; ++SkillIndex)
	{
		for (const TPair<FName, float>& Each : PrimaryWeights(SkillIndex))
		{
			if (Each.Key == CheeseStrikeWeapons::BabyBomb().Id || Each.Key == CheeseStrikeWeapons::Stick().Id)
			{
				TestEqual(FString::Printf(TEXT("no %s bot comes in with the %s"), Skill(SkillIndex).Name, *Each.Key.ToString()), Each.Value, 0.0f);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotFiresInBurstsAndRests, "CheeseStrike.Bots.FiresInBurstsAndRests", BotTests::Flags)

bool FBotFiresInBurstsAndRests::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBots;
	const FSkill& Normal = Skill(1);
	const float Interval = 0.12f;
	FBursts Bursts;
	// Wanting to fire for three seconds, a step at a time: count the bursts and how long the trigger is held.
	int32 BurstsFired = 0;
	float Held = 0.0f;
	float FirstBurst = 0.0f;
	bool bWas = false;
	for (float Time = 0.0f; Time < 3.0f; Time += 0.01f)
	{
		const bool bHolds = HoldsTrigger(Bursts, Normal, true, Interval, Time);
		if (bHolds && !bWas)
		{
			++BurstsFired;
		}
		if (bHolds)
		{
			Held += 0.01f;
			if (BurstsFired == 1)
			{
				FirstBurst += 0.01f;
			}
		}
		bWas = bHolds;
	}
	// A burst of 5 at 0.12 s is held 0.54 s, then 0.55 s of rest: a little under three bursts in 3 s.
	TestEqual(TEXT("a burst is held for its shots"), FirstBurst, Interval * (Normal.Burst - 0.5f), 0.02f);
	TestEqual(TEXT("bursts with rests between"), BurstsFired, 3);
	TestTrue(TEXT("held about half the time"), Held > 1.2f && Held < 1.8f);
	// Off target, it lets go at once, and waits out a rest before the next.
	FBursts Off;
	TestTrue(TEXT("on target: fires"), HoldsTrigger(Off, Normal, true, Interval, 10.0f));
	TestFalse(TEXT("off target: lets go"), HoldsTrigger(Off, Normal, false, Interval, 10.1f));
	TestFalse(TEXT("back on target, resting"), HoldsTrigger(Off, Normal, true, Interval, 10.2f));
	TestTrue(TEXT("fires again after its rest"), HoldsTrigger(Off, Normal, true, Interval, 10.1f + Normal.Rest + 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsKeepOutOfTheDownedsAim, "CheeseStrike.Bots.KeepOutOfTheDownedsAim", BotTests::Flags)

bool FBotsKeepOutOfTheDownedsAim::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBots;
	// Someone down at the origin, facing +X: they aim within 45 degrees of it.
	const FVector Downed(0.0f, 0.0f, 0.0f);
	TestTrue(TEXT("straight ahead of them is in their aim"), InDownedAim(FVector(800, 0, 0), Downed, 0.0f));
	TestFalse(TEXT("beside them isn't"), InDownedAim(FVector(0, 800, 0), Downed, 0.0f));
	TestFalse(TEXT("nor behind their head"), InDownedAim(FVector(-800, 0, 0), Downed, 0.0f));
	TestTrue(TEXT("the aim turns with the body"), InDownedAim(FVector(0, 800, 0), Downed, 90.0f));

	// Out of their aim: straight in, to stand beside them on that side, out of it still.
	const FVector Beside = FinishApproach(FVector(0, 1000, 0), Downed, 0.0f, 0.0f);
	TestTrue(FString::Printf(TEXT("from the side, it goes to stand beside them (%s)"), *Beside.ToCompactString()), FVector::Dist2D(Beside, Downed) <= FlankStandOff + 1.0f && Beside.Y > 0.0f);
	TestFalse(TEXT("out of their aim"), InDownedAim(Beside, Downed, 0.0f));

	// In their aim, far off: never straight at them; round to the nearer edge, swerving from side to side.
	const FVector Ahead(1000.0f, 150.0f, 0.0f);
	const FVector ToThem = (Downed - Ahead).GetSafeNormal2D();
	TArray<FVector> Steps;
	for (float Time = 0.0f; Time < ZigzagPeriod; Time += ZigzagPeriod / 8.0f)
	{
		Steps.Add(FinishApproach(Ahead, Downed, 0.0f, Time));
	}
	bool bLeft = false;
	bool bRight = false;
	const FVector Straight = (FinishApproach(Ahead, Downed, 0.0f, 0.0f) + FinishApproach(Ahead, Downed, 0.0f, ZigzagPeriod * 0.75f)) / 2.0f - Ahead;
	for (const FVector& Step : Steps)
	{
		const FVector Way = (Step - Ahead).GetSafeNormal2D();
		const float Off = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Way, ToThem)));
		TestTrue(FString::Printf(TEXT("in their aim, it heads %.0f degrees off straight at them"), Off), Off > 8.0f);
		TestTrue(TEXT("moving round toward its nearer edge, their left"), BearingFromDowned(Step, Downed, 0.0f) > BearingFromDowned(Ahead, Downed, 0.0f) + 2.0f);
		const float Across = FVector::CrossProduct(Straight.GetSafeNormal2D(), Way).Z;
		bLeft |= Across > 0.1f;
		bRight |= Across < -0.1f;
		TestTrue(TEXT("a step at a time"), FVector::Dist2D(Step, Ahead) < ZigzagStep + ZigzagWidth + 1.0f);
	}
	TestTrue(TEXT("swerving one way and the other as it goes"), bLeft && bRight);
	// Going on, it leaves the aim well before it is near them, and comes to stand beside them.
	FVector At = Ahead;
	float Time = 0.0f;
	float OutAt = -1.0f;
	for (int32 Tick = 0; Tick < 300 && FVector::Dist2D(At, Downed) > FlankStandOff + 20.0f; ++Tick)
	{
		const FVector Toward = FinishApproach(At, Downed, 0.0f, Time) - At;
		At += Toward.GetSafeNormal2D() * FMath::Min(40.0f, static_cast<float>(Toward.Size2D()));
		Time += 0.1f;
		if (InDownedAim(At, Downed, 0.0f))
		{
			TestTrue(FString::Printf(TEXT("never in their aim close up (%.0f cm off)"), FVector::Dist2D(At, Downed)), FVector::Dist2D(At, Downed) > 350.0f);
		}
		else if (OutAt < 0.0f)
		{
			OutAt = FVector::Dist2D(At, Downed);
		}
	}
	TestTrue(FString::Printf(TEXT("out of their aim %.0f cm from them"), OutAt), OutAt > 350.0f);
	TestTrue(FString::Printf(TEXT("and it gets to them in the end (%.0f cm off)"), FVector::Dist2D(At, Downed)), FVector::Dist2D(At, Downed) <= FlankStandOff + 25.0f);
	TestFalse(TEXT("beside them"), InDownedAim(At, Downed, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsSwerveOnlyWhereThereIsRoom, "CheeseStrike.Bots.SwerveOnlyWhereThereIsRoom", BotTests::Flags)

bool FBotsSwerveOnlyWhereThereIsRoom::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBots;
	// The swerve fits the floor either side: the whole width in the open, less in a narrower place, none in a hallway.
	TestEqual(TEXT("in the open, the whole swerve"), SwerveWidth(TNumericLimits<float>::Max()), ZigzagWidth);
	TestEqual(TEXT("with just room for it, the whole swerve"), SwerveWidth(ZigzagWidth + SwerveClearance), ZigzagWidth);
	const float Some = SwerveWidth(SwerveClearance + 70.0f);
	TestTrue(FString::Printf(TEXT("a little room, a little swerve (%.0f)"), Some), Some > 0.0f && Some <= 70.0f);
	TestEqual(TEXT("a hallway, none"), SwerveWidth(SwerveClearance + LeastSwerve - 1.0f), 0.0f);
	TestEqual(TEXT("no room, none"), SwerveWidth(0.0f), 0.0f);

	// In someone's aim in a hallway 1.2 m wide (60 cm either side), it comes on with no step to the side at all.
	const FVector Downed(0.0f, 0.0f, 0.0f);
	const FVector Ahead(1000.0f, 150.0f, 0.0f);
	const FVector Straight = FinishApproach(Ahead, Downed, 0.0f, 0.0f, 1.0f, 60.0f);
	const FVector Later = FinishApproach(Ahead, Downed, 0.0f, ZigzagPeriod * 0.75f, 1.0f, 60.0f);
	TestTrue(FString::Printf(TEXT("the same step whichever way the swerve is (%s, %s)"), *Straight.ToCompactString(), *Later.ToCompactString()), FVector::Dist2D(Straight, Later) < 1.0f);
	TestTrue(TEXT("and a step, not a stand"), FVector::Dist2D(Straight, Ahead) > 100.0f);
	// In the open it swerves one way and the other, as wide as the swerve.
	const FVector Open = FinishApproach(Ahead, Downed, 0.0f, 0.0f);
	const FVector OpenLater = FinishApproach(Ahead, Downed, 0.0f, ZigzagPeriod * 0.75f);
	TestTrue(TEXT("in the open, it swerves"), FVector::Dist2D(Open, OpenLater) > ZigzagWidth);

	// A path to the same moving goal is asked for no more than every RepathEvery seconds.
	TestFalse(TEXT("not again at once, though it has moved"), ShouldRepath(true, 500.0f, 0.1f));
	TestFalse(TEXT("not again at once, though not following one"), ShouldRepath(false, 0.0f, RepathEvery * 0.5f));
	TestTrue(TEXT("again once the goal has moved and a moment has gone"), ShouldRepath(true, 500.0f, RepathEvery));
	TestFalse(TEXT("not while following one to where it still is"), ShouldRepath(true, 50.0f, 5.0f));
	TestTrue(TEXT("when it has none"), ShouldRepath(false, 0.0f, RepathEvery));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsExecuteAndSpreadOut, "CheeseStrike.Bots.ExecuteAndSpreadOut", BotTests::Flags)

bool FBotsExecuteAndSpreadOut::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBots;
	// Left down, an enemy gets up again: a bot goes to execute them from far off, and even mid-fight when near.
	TestTrue(TEXT("nobody in sight: to an enemy down far off"), GoesToExecute(4000.0f, false, 0.0f));
	TestFalse(TEXT("but not across the map"), GoesToExecute(ExecuteRange + 1.0f, false, 0.0f));
	TestTrue(TEXT("in a fight, to one down nearer than the enemy it fights"), GoesToExecute(800.0f, true, 2000.0f));
	TestFalse(TEXT("but not past the enemy it fights"), GoesToExecute(800.0f, true, 500.0f));
	TestFalse(TEXT("nor far off mid-fight"), GoesToExecute(ExecuteWhileFighting + 1.0f, true, 5000.0f));
	TestTrue(TEXT("further than it went to revive, before"), ExecuteRange > 2500.0f);
	// Roaming, it keeps away from its teammates: a spot far from them beats a spot as far from here near them.
	TestTrue(TEXT("alone beats with a teammate"), RoamScore(2000.0f, AloneAt) > RoamScore(2000.0f, 200.0f));
	TestTrue(TEXT("being alone counts for more than going further"), RoamScore(1000.0f, AloneAt) > RoamScore(RoamFar, 300.0f));
	TestEqual(TEXT("and further than far is no better"), RoamScore(RoamFar * 2.0f, AloneAt * 2.0f), RoamScore(RoamFar, AloneAt));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "GameFramework/PawnMovementComponent.h"
#include "PlayTesting.h"
#include "Modes/CheeseStrikeLastStand.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeExecution.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikeWeapon.h"

namespace BotTests
{
	TArray<ACheeseStrikeBotController*> Bots()
	{
		TArray<ACheeseStrikeBotController*> Found;
		if (UWorld* World = PlayTesting::World(0))
		{
			for (TActorIterator<ACheeseStrikeBotController> It(World); It; ++It)
			{
				Found.Add(*It);
			}
		}
		return Found;
	}

	ACheeseStrikeCharacter* BodyOf(const ACheeseStrikeBotController* Bot)
	{
		return Bot != nullptr ? Cast<ACheeseStrikeCharacter>(Bot->GetPawn()) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FABotNoticesAndShoots, "CheeseStrike.Play.ABotNoticesAndShoots", BotTests::Flags)

bool FABotNoticesAndShoots::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace BotTests;
	// One player and one hard bot, on the shooter level's open ground.
	StartPlaying({1, TEXT("?Bots=1?BotSkill=2")});
	WaitUntil(TEXT("a bot in the match"), 10.0f, []() { return Bots().Num() == 1 && BodyOf(Bots()[0]) != nullptr; });
	// It waits for the navigation mesh, then stands 9 m from the player along the level's long clear run, facing them.
	WaitUntil(TEXT("the navigation mesh"), 15.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		FNavLocation Spot;
		return Navigation != nullptr && Navigation->ProjectPointToNavigation(FVector(1150, -1450, 100), Spot, FVector(100, 100, 300));
	});
	PutAt(0, FVector(1150, -1450, 100), 117.0f);
	Then([this]()
	{
		ACheeseStrikeBotController* Bot = Bots()[0];
		ACheeseStrikeCharacter* Body = BodyOf(Bot);
		TestEqual(TEXT("the bot plays at the skill the host picked"), Bot->GetSkill(), 2);
		Bot->StopMovement();
		Body->GetMovementComponent()->StopMovementImmediately();
		Body->SetActorLocation(FVector(1150, -1450, 100) + FRotator(0, 117, 0).Vector() * 900.0f);
		Bot->FaceTo(FRotator(0, 117 - 180, 0));
		Body->EndProtection();
		Character(0)->EndProtection();
	});
	WaitUntil(TEXT("the bot shooting the player"), 8.0f, []() { return Character(0) == nullptr || Character(0)->GetHealth() < 100.0f; });
	Then([this]()
	{
		ACheeseStrikeBotController* Bot = Bots()[0];
		const ACheeseStrikeCharacter* Body = BodyOf(Bot);
		AddInfo(FString::Printf(TEXT("bot at %s facing %s, %s, target %s (sees %d); player at %s, health %.0f, protected %d"),
			Body != nullptr ? *Body->GetActorLocation().ToCompactString() : TEXT("-"), *Bot->GetControlRotation().ToCompactString(), Bot->Doing(),
			Bot->GetTarget() != nullptr ? *Bot->GetTarget()->DisplayName() : TEXT("none"), Bot->SeesTarget() ? 1 : 0,
			Character(0) != nullptr ? *Character(0)->GetActorLocation().ToCompactString() : TEXT("-"), Character(0) != nullptr ? Character(0)->GetHealth() : -1.0f, Character(0) != nullptr && Character(0)->IsProtected() ? 1 : 0));
		TestTrue(TEXT("it fought the player"), Bot->GetTarget() == Character(0) || Character(0) == nullptr || !Character(0)->IsAlive());
		const float Reaction = Bot->GetLastReaction();
		TestTrue(FString::Printf(TEXT("not before its reaction time (%.2f s)"), Reaction), Reaction >= CheeseStrikeBots::Skill(2).Reaction - 0.05f);
		TestTrue(FString::Printf(TEXT("but soon after (%.2f s)"), Reaction), Reaction < 1.5f);
		// With whichever of its weapons it chose.
		int32 Shots = 0;
		const UCheeseStrikeInventory* Inventory = BodyOf(Bot) != nullptr ? BodyOf(Bot)->GetInventory() : nullptr;
		for (const EWeaponSlot Slot : {EWeaponSlot::Primary, EWeaponSlot::Sidearm, EWeaponSlot::Knife})
		{
			const ACheeseStrikeWeapon* Weapon = Inventory != nullptr ? Inventory->GetInSlot(Slot) : nullptr;
			Shots += Weapon != nullptr ? Weapon->ShotsFired : 0;
		}
		TestTrue(FString::Printf(TEXT("its shots were the server's: fired (%d), and hit"), Shots), Shots > 0);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FABotFinishesFromOutsideTheAim, "CheeseStrike.Play.ABotFinishesFromOutsideTheAim", BotTests::Flags)

bool FABotFinishesFromOutsideTheAim::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace BotTests;
	// Execution: the host and the second joiner on one side, the first joiner and a hard bot on the other.
	static float InAimNear = 0.0f;
	static float LastSample = -1.0f;
	static float Nearest = 0.0f;
	static float StartedInAim = 0.0f;
	InAimNear = 0.0f;
	LastSample = -1.0f;
	Nearest = TNumericLimits<float>::Max();
	StartedInAim = 0.0f;
	// The level's long clear run (as ABotNoticesAndShoots), the others well away.
	const FVector Down(1150.0f, -1450.0f, 100.0f);
	constexpr float DownYaw = 117.0f;
	StartPlaying({3, TEXT("?Mode=Execution?Rounds=1?Bots=1?BotSkill=2")});
	WaitUntil(TEXT("a bot in the match"), 10.0f, []() { return Bots().Num() == 1 && BodyOf(Bots()[0]) != nullptr; });
	// The joiners stay out of it: kept from the bot's fight by spawn protection. Their own runs out three seconds after
	// they come in, which on a busy PC is before the bot is set going, and a bot fights anyone standing in sight before
	// it tends someone down (it would shoot the enemy joiner down, leaving the host's side all down and out).
	Then([]()
	{
		CharacterOf(1, 0)->CheeseStrikeProtect(300.0f);
		CharacterOf(2, 0)->CheeseStrikeProtect(300.0f);
	});
	WaitUntil(TEXT("the navigation mesh"), 15.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		FNavLocation Spot;
		return Navigation != nullptr && Navigation->ProjectPointToNavigation(FVector(1150.0f, -1450.0f, 100.0f), Spot, FVector(100, 100, 300));
	});
	PutAt(0, Down, DownYaw);
	PutAt(1, FVector(-231.0f, -700.0f, 100.0f), 90.0f);
	PutAt(2, FVector(-231.0f, -300.0f, 100.0f), -90.0f);
	Then([this, Down, DownYaw]()
	{
		ACheeseStrikeBotController* Bot = Bots()[0];
		ACheeseStrikeCharacter* Body = BodyOf(Bot);
		const ACheeseStrikePlayerState* Theirs = Bot->GetPlayerState<ACheeseStrikePlayerState>();
		const ACheeseStrikePlayerState* Host = CharacterOf(0, 0)->GetPlayerState<ACheeseStrikePlayerState>();
		TestTrue(TEXT("the bot is on the other side from the host"), Theirs != nullptr && Host != nullptr && Theirs->GetTeam() != Host->GetTeam());
		// The bot 8 m straight down the host's aim, facing them.
		Bot->StopMovement();
		Body->GetMovementComponent()->StopMovementImmediately();
		Body->SetActorLocation(Down + FRotator(0.0f, DownYaw, 0.0f).Vector() * 800.0f);
		Bot->FaceTo(FRotator(0.0f, DownYaw + 180.0f, 0.0f));
		Body->EndProtection();
		CharacterOf(0, 0)->GetLastStand()->GoDown(Bot->GetPlayerState<ACheeseStrikePlayerState>());
	});
	// It comes to execute them, keeping out of their aim when it is near.
	WaitUntil(TEXT("the bot executing the host"), 25.0f, [Down]()
	{
		const ACheeseStrikeCharacter* Host = CharacterOf(0, 0);
		const ACheeseStrikeCharacter* Body = Bots().Num() > 0 ? BodyOf(Bots()[0]) : nullptr;
		if (Host == nullptr || !Host->IsAlive())
		{
			return true;
		}
		const float Time = Now(0);
		if (Body != nullptr && Host->IsDowned() && LastSample >= 0.0f)
		{
			const float Apart = FVector::Dist2D(Body->GetActorLocation(), Host->GetActorLocation());
			const bool bInAim = CheeseStrikeBots::InDownedAim(Body->GetActorLocation(), Host->GetActorLocation(), Host->GetActorRotation().Yaw);
			Nearest = FMath::Min(Nearest, Apart);
			if (bInAim && Apart < 400.0f)
			{
				InAimNear += Time - LastSample;
			}
			if (bInAim && Apart > 600.0f)
			{
				StartedInAim += Time - LastSample;
			}
		}
		LastSample = Time;
		return false;
	});
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = CharacterOf(0, 0);
		ACheeseStrikeBotController* Bot = Bots()[0];
		AddInfo(FString::Printf(TEXT("bot %s, %s; host at %s"), *BodyOf(Bot)->GetActorLocation().ToCompactString(), Bot->Doing(), *Host->GetActorLocation().ToCompactString()));
		TestFalse(TEXT("the host is finished"), Host->IsAlive());
		TestEqual(TEXT("by the bot"), Host->GetKilledBy(), BodyOf(Bot)->DisplayName());
		TestTrue(FString::Printf(TEXT("it came close (%.0f cm)"), Nearest), Nearest <= CheeseStrikeModes::FinishRange);
		TestTrue(FString::Printf(TEXT("from outside the host's aim: %.2f s in it within 4 m"), InAimNear), InAimNear < 0.5f);
		TestTrue(TEXT("executed, with a press"), Host->GetLastStand()->WasExecuted());
	});
	// The execution plays out before it does anything else.
	WaitUntil(TEXT("the bot done executing"), CheeseStrikeExecution::StabSeconds + 1.0f, []() { return Bots().Num() > 0 && !BodyOf(Bots()[0])->GetLastStand()->InExecution(); });
	// Its own teammate (whichever joiner is on its side) goes down a few metres off: it goes and revives them.
	static int32 Mate = 1;
	Then([this, Down]()
	{
		const ACheeseStrikePlayerState* Theirs = Bots()[0]->GetPlayerState<ACheeseStrikePlayerState>();
		Mate = CharacterOf(1, 0)->GetPlayerState<ACheeseStrikePlayerState>()->GetTeam() == Theirs->GetTeam() ? 1 : 2;
		TestEqual(TEXT("one joiner is on the bot's side"), CharacterOf(Mate, 0)->GetPlayerState<ACheeseStrikePlayerState>()->GetTeam(), Theirs->GetTeam());
		CharacterOf(Mate, 0)->SetActorLocation(Down + FRotator(0.0f, DownYaw, 0.0f).Vector() * 600.0f);
	});
	Wait(0.5f);
	Then([this]()
	{
		CharacterOf(Mate, 0)->GetLastStand()->GoDown(nullptr);
	});
	WaitUntil(TEXT("the bot reviving its teammate"), 15.0f, []()
	{
		const ACheeseStrikeCharacter* Lying = CharacterOf(Mate, 0);
		return Lying != nullptr && Lying->IsAlive() && !Lying->IsDowned();
	});
	Then([this]()
	{
		TestEqual(TEXT("revived, with the revived's health"), CharacterOf(Mate, 0)->GetHealth(), CheeseStrikeModes::ReviveHealth);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsPlayTheCreamery, "CheeseStrike.Play.BotsPlayTheCreamery", BotTests::Flags)

bool FBotsPlayTheCreamery::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace BotTests;
	// A player and fifteen bots: a full match.
	static TMap<FString, FVector> Spawned;
	static TMap<FString, float> Furthest;
	static TSet<FString> Fought;
	static TSet<FString> Lanes;
	static float StartedAt = 0.0f;
	static TMap<FString, int32> Samples;
	Samples.Reset();
	Spawned.Reset();
	Furthest.Reset();
	Fought.Reset();
	Lanes.Reset();
	FSession Session;
	Session.Level = TEXT("/Game/CheeseStrike/Maps/Lvl_Creamery");
	Session.Options = TEXT("?Bots=15?KillLimit=50");
	StartPlaying(Session);
	WaitUntil(TEXT("fifteen bots"), 10.0f, []() { return Bots().Num() == 15; });
	// Every lane can be walked to from a dock, by the navigation mesh the bots go by.
	WaitUntil(TEXT("the navigation mesh"), 20.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		FNavLocation Spot;
		return Navigation != nullptr && !Navigation->IsNavigationBeingBuilt(World(0)) && Navigation->ProjectPointToNavigation(FVector(-6250, 0, 220), Spot, FVector(100, 100, 300));
	});
	Then([this]()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		const FVector Dock(-6250, 0, 220);
		struct FPlace { const TCHAR* Name; FVector At; };
		for (const FPlace& Place : {FPlace{TEXT("the yard by the containers"), FVector(-1500, 3550, 100)}, FPlace{TEXT("the yard behind the containers"), FVector(-1200, 4200, 100)},
			FPlace{TEXT("the Squeeze"), FVector(-1500, 2200, 100)}, FPlace{TEXT("the hall"), FVector(-1500, 0, 100)}, FPlace{TEXT("the mezzanine"), FVector(-1000, -1550, 500)},
			FPlace{TEXT("the canal's floor"), FVector(-2000, -3200, -200)}, FPlace{TEXT("Tank Row"), FVector(-2200, -4600, 100)}, FPlace{TEXT("the other dock"), FVector(6250, 0, 220)},
			FPlace{TEXT("the Curing House's upper floor"), FVector(-4000, -1300, 450)}, FPlace{TEXT("the Rind Tunnel"), FVector(-2200, -2075, 100)}, FPlace{TEXT("the gallery"), FVector(1000, 1550, 500)},
			FPlace{TEXT("a catwalk"), FVector(900, 0, 500)}})
		{
			UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(World(0), Dock, Place.At);
			const bool bWhole = Path != nullptr && Path->IsValid() && !Path->IsPartial();
			TestTrue(FString::Printf(TEXT("%s can be walked to from a dock (%s)"), Place.Name, Path == nullptr ? TEXT("no path") : Path->IsPartial() ? TEXT("partly") : TEXT("whole")), bWhole);
		}
		StartedAt = Now(0);
	});
	// Half a minute at least: long enough to leave the docks, spread through the lanes and fight there.
	WaitUntil(TEXT("the bots roaming the lanes, finding each other and killing"), 90.0f, []()
	{
		int32 Kills = 0;
		for (ACheeseStrikeBotController* Bot : Bots())
		{
			const ACheeseStrikePlayerState* State = Bot->GetPlayerState<ACheeseStrikePlayerState>();
			const ACheeseStrikeCharacter* Body = BodyOf(Bot);
			if (State == nullptr)
			{
				continue;
			}
			Kills += State->GetKills();
			if (Body == nullptr || !Body->IsAlive())
			{
				continue;
			}
			const FString Name = State->GetPlayerName();
			if (!Spawned.Contains(Name))
			{
				Spawned.Add(Name, Body->GetActorLocation());
			}
			Furthest.FindOrAdd(Name) = FMath::Max(Furthest.FindRef(Name), FVector::Dist2D(Spawned[Name], Body->GetActorLocation()));
			if (Bot->SeesTarget())
			{
				Fought.Add(Name);
			}
			// Which lane it is in: the yard north of the hall, the hall, the canal and the tanks south of it.
			const FVector At = Body->GetActorLocation();
			const FString Where = FMath::Abs(At.X) > 5300.0f ? TEXT("docks") : FMath::Abs(At.X) > 2400.0f ? TEXT("plazas") : At.Y > 2600.0f ? TEXT("yard") : At.Y < -2800.0f ? TEXT("canal") : FMath::Abs(At.Y) < 1800.0f ? TEXT("hall") : TEXT("walkways");
			Samples.FindOrAdd(Where + TEXT(" ") + Bot->Doing())++;
			if (FMath::Abs(At.X) < 2400.0f)
			{
				Lanes.Add(At.Y > 2600.0f ? TEXT("the yard") : At.Y < -2800.0f ? TEXT("the canal") : FMath::Abs(At.Y) < 1800.0f ? TEXT("the hall") : TEXT(""));
			}
		}
		int32 Moved = 0;
		for (const TPair<FString, float>& Each : Furthest)
		{
			Moved += Each.Value > 1000.0f ? 1 : 0;
		}
		return Now(0) - StartedAt >= 30.0f && Moved == 15 && Kills >= 10 && Fought.Num() >= 12 && Lanes.Contains(TEXT("the yard")) && Lanes.Contains(TEXT("the hall")) && Lanes.Contains(TEXT("the canal"));
	});
	Then([this]()
	{
		int32 Kills = 0;
		for (ACheeseStrikeBotController* Bot : Bots())
		{
			if (const ACheeseStrikePlayerState* State = Bot->GetPlayerState<ACheeseStrikePlayerState>())
			{
				Kills += State->GetKills();
				AddInfo(FString::Printf(TEXT("%s: %d kills, %d deaths, went %.0f m, %s"), *State->GetPlayerName(), State->GetKills(), State->GetDeaths(), Furthest.FindRef(State->GetPlayerName()) / 100.0f, Bot->Doing()));
			}
		}
		Samples.KeySort([](const FString& A, const FString& B) { return A < B; });
		for (const TPair<FString, int32>& Each : Samples)
		{
			AddInfo(FString::Printf(TEXT("seen %d times: %s"), Each.Value, *Each.Key));
		}
		TestTrue(FString::Printf(TEXT("the bots killed one another (%d)"), Kills), Kills >= 10);
		TestTrue(FString::Printf(TEXT("most bots found someone to fight (%d)"), Fought.Num()), Fought.Num() >= 12);
		for (const TCHAR* Lane : {TEXT("the yard"), TEXT("the hall"), TEXT("the canal")})
		{
			TestTrue(FString::Printf(TEXT("bots went into %s"), Lane), Lanes.Contains(Lane));
		}
		int32 Moved = 0;
		for (const TPair<FString, float>& Each : Furthest)
		{
			Moved += Each.Value > 1000.0f ? 1 : 0;
		}
		TestEqual(TEXT("every bot went somewhere, 10 m or more from where it came in"), Moved, 15);
	});
	StopPlaying();
	return true;
}

#endif
