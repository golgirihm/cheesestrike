// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The cheese the players bleed (Rules/CheeseStrikeBlood.h): how much a hit
// sprays, how an execution's wound runs over its time, how a pool spreads
// and fades, and the caps that keep it cheap. That it reaches every machine
// is checked by the plays (CheeseStrike.Play.AKillIsScoredAndTheKilledComeBack
// for shots, CheeseStrike.Play.ExecutionAllDownIsNotOut for the stomp and
// the stab).

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeBlood.h"
#include "Rules/CheeseStrikeExecution.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHitSpraysCheeseByItsDamage, "CheeseStrike.Blood.AHitSpraysCheeseByItsDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAHitSpraysCheeseByItsDamage::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBlood;
	const FSpray Light = ShotSpray(12.0f);
	const FSpray Heavy = ShotSpray(100.0f);
	TestTrue(FString::Printf(TEXT("even a light hit throws a score of drops (%d)"), Light.Drops), Light.Drops >= 20);
	TestTrue(TEXT("a harder hit throws more"), Heavy.Drops > Light.Drops);
	TestTrue(TEXT("faster"), Heavy.Speed > Light.Speed);
	TestTrue(TEXT("a short spray, not a fountain"), Heavy.Drops <= 40 && Heavy.ConeDegrees < 45.0f);
	TestEqual(TEXT("a hit past any gun's damage throws no more than the most"), ShotSpray(1000.0f).Drops, Heavy.Drops);
	TestTrue(TEXT("gone in a moment"), Heavy.Life <= 0.4f && Light.Life <= 0.4f);
	// Fine drops, drawn out along their flight: at rest a drop is its own shape; flying, longer the faster, to a limit.
	TestEqual(TEXT("at rest, unstretched"), DropStretch(0.0f), 1.0f);
	TestTrue(TEXT("flying, stretched"), DropStretch(600.0f) > 1.5f && DropStretch(600.0f) < DropStretch(900.0f));
	TestEqual(TEXT("to a limit"), DropStretch(5000.0f), MostStretch);
	TestEqual(TEXT("or the limit its spray gives"), DropStretch(5000.0f, 4.0f), 4.0f);
	TestEqual(TEXT("which a spray's drops don't have unless it says"), ShotSpray(50.0f).Stretch, MostStretch);
	const float Biggest = Heavy.Size * (1.0f + SizeJitter);
	const float Fastest = Heavy.Speed * (1.0f + Heavy.SpeedJitter);
	TestTrue(FString::Printf(TEXT("a shot's drops are tiny: none longer than 2 cm (%.2f)"), DropLength(Biggest, Fastest)), DropLength(Biggest, Fastest) <= 2.0f);
	TestTrue(TEXT("and longer than wide as they fly"), DropLength(Light.Size, Light.Speed) > 2.0f * Light.Size * 1.5f);
	// A misty puff or two, a third of a second, growing and fading.
	const FMist Mist = ShotMist(30.0f);
	TestTrue(TEXT("one puff for a light hit (where it went in)"), ShotMistPuffs(12.0f) == 1);
	TestTrue(TEXT("two for a hard one"), ShotMistPuffs(100.0f) == 2);
	TestTrue(TEXT("a third of a second"), FMath::IsNearlyEqual(Mist.Life, 0.3f, 0.05f));
	TestTrue(TEXT("growing"), MistRadiusAt(Mist, Mist.Life * 0.5f) > MistRadiusAt(Mist, 0.0f) && FMath::IsNearlyEqual(MistRadiusAt(Mist, Mist.Life), Mist.To, 0.01f));
	TestTrue(TEXT("soft: never solid"), MistOpacityAt(Mist, 0.0f) <= 0.85f);
	TestTrue(TEXT("and fading away"), MistOpacityAt(Mist, Mist.Life * 0.5f) < MistOpacityAt(Mist, 0.0f) && FMath::IsNearlyZero(MistOpacityAt(Mist, Mist.Life)));
	// The caps.
	TestEqual(TEXT("drops are made while there is room"), DropsAllowed(20, 0), 20);
	TestEqual(TEXT("only up to the cap"), DropsAllowed(20, MostDrops - 5), 5);
	TestEqual(TEXT("and none past it"), DropsAllowed(20, MostDrops + 3), 0);
	int32 Splats = 0;
	for (int32 Index = 0; Index < 30; ++Index)
	{
		Splats += LeavesSplat(Index) ? 1 : 0;
	}
	TestEqual(TEXT("one landing drop in a few leaves a splat"), Splats, 30 / SplatEvery);
	TestTrue(TEXT("a splat is a few centimetres across"), SplatRadius(0.15f) >= 3.0f && SplatRadius(0.5f) <= 10.0f);
	TestEqual(TEXT("a splat stays"), SplatOpacityAt(SplatSeconds - 0.1f), 1.0f);
	TestEqual(TEXT("then fades away"), SplatOpacityAt(SplatSeconds + SplatFades), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnExecutionBleedsCheese, "CheeseStrike.Blood.AnExecutionBleedsCheese", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAnExecutionBleedsCheese::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBlood;
	using namespace CheeseStrikeExecution;
	// The stab: nothing until the knife is in; spurts while it is held there; a gush as it comes out, dying away.
	TestEqual(TEXT("nothing before the knife is in"), StabRate(StabLands - 0.05f), 0.0f);
	TestTrue(TEXT("a spurt as it goes in"), StabRate(StabLands + 0.01f) > 80.0f);
	const float Between = StabLands + 0.9f / PulsesPerSecond;
	TestTrue(TEXT("less between the spurts"), StabRate(Between) < 20.0f);
	TestTrue(TEXT("and the next spurt"), StabRate(StabLands + 1.0f / PulsesPerSecond + 0.01f) > 80.0f);
	TestTrue(TEXT("a gush as it comes out, more than any spurt"), StabRate(StabOut + 0.01f) > StabRate(StabLands + 0.01f));
	TestTrue(TEXT("faster too"), StabSpeed(StabOut + 0.01f) > StabSpeed(StabLands + 0.01f));
	TestEqual(TEXT("and over"), StabRate(StabOut + GushSeconds + 0.01f), 0.0f);
	// Drops over the whole of it: a fair stream, not a flood.
	float Drops = 0.0f;
	for (float Time = 0.0f; Time < StabSeconds; Time += 0.01f)
	{
		Drops += StabRate(Time) * 0.01f;
	}
	TestTrue(FString::Printf(TEXT("the stab bleeds a big spray: hundreds of drops (%.0f)"), Drops), Drops > 250.0f && Drops < 900.0f);
	TestTrue(TEXT("in a fan, widest as the knife comes out"), StabCone(StabOut + 0.1f) > StabCone(StabLands + 0.1f) && StabCone(StabLands + 0.1f) >= 20.0f);
	TestTrue(TEXT("of small drops"), StabDropSize(StabOut + 0.1f) <= 0.5f);
	int32 Puffs = 0;
	for (float Time = 0.0f; Time < StabSeconds; Time += 1.0f / 60.0f)
	{
		Puffs += StabMistDue(Time, Time + 1.0f / 60.0f) ? 1 : 0;
	}
	TestTrue(FString::Printf(TEXT("with a puff of mist at each spurt and the gush (%d)"), Puffs), Puffs >= 3 && Puffs <= 8);

	// The stomp: a fire-hydrant jet for HoseSeconds from the blow, one dense steady stream of small bright drops, far
	// more than the stab, and no chunks.
	TestEqual(TEXT("nothing before the boot lands"), StompRate(StompLands - 0.05f), 0.0f);
	float Hose = 0.0f;
	float Least = TNumericLimits<float>::Max();
	for (float Time = StompLands; Time < StompLands + HoseSeconds; Time += 0.01f)
	{
		Hose += StompRate(Time) * 0.01f;
		if (Time < StompLands + HoseSeconds - HoseSlackSeconds)
		{
			Least = FMath::Min(Least, StompRate(Time));
		}
	}
	TestTrue(FString::Printf(TEXT("a dense jet for two seconds (%.0f drops, never under %.0f a second until it slackens)"), Hose, Least), Hose > 2.0f * Drops && Hose >= 1000.0f && Least >= 590.0f);
	TestTrue(TEXT("one steady stream, not throbbing"), FMath::IsNearlyEqual(StompRate(StompLands + 0.4f), StompRate(StompLands + 1.0f), 1.0f) && FMath::IsNearlyEqual(StompRate(StompLands + 1.0f), HoseRate, 1.0f));
	TestTrue(TEXT("slackening only at its end"), StompRate(StompLands + HoseSeconds - 0.1f) < HoseRate * 0.5f);
	TestTrue(TEXT("within the cap at any moment (drops live at most DropLife)"), HoseRate * DropLife <= MostDrops);
	TestEqual(TEXT("then it stops"), StompRate(StompLands + HoseSeconds + 0.01f), 0.0f);
	const FSpray First = HoseSpray(StompLands);
	const FSpray Last = HoseSpray(StompLands + HoseSeconds);
	TestTrue(FString::Printf(TEXT("at 9 to 12 m/s (%.0f to %.0f cm/s)"), First.Speed, Last.Speed), First.Speed <= 1200.0f && First.Speed >= 900.0f && Last.Speed >= 900.0f && Last.Speed <= 1200.0f);
	TestTrue(TEXT("easing"), First.Speed > Last.Speed);
	TestTrue(TEXT("nearly all at one speed"), First.SpeedJitter <= 0.15f);
	// Crowded along the cone's line: half the drops within a tenth of the cone, a few out to its edge; a fan is even.
	TestTrue(TEXT("a jet crowds its line"), First.Core >= 0.9f && ConeShare(0.5f, First.Core) <= 0.1f && FMath::IsNearlyEqual(ConeShare(1.0f, First.Core), 1.0f));
	TestTrue(TEXT("a wound's fan is spread evenly"), ShotSpray(50.0f).Core == 0.0f && FMath::IsNearlyEqual(ConeShare(0.5f, 0.0f), 0.5f));
	TestTrue(FString::Printf(TEXT("in a cone 10 degrees wide (%.0f)"), First.ConeDegrees * 2.0f), First.ConeDegrees > 0.0f && First.ConeDegrees * 2.0f <= 10.0f);
	TestTrue(FString::Printf(TEXT("of small drops, about 1.5 cm at rest (%.2f), bigger than a shot's"), DropLength(First.Size, 0.0f, First.Stretch)), FMath::IsNearlyEqual(DropLength(First.Size, 0.0f, First.Stretch), 1.5f, 0.3f) && First.Size > ShotSpray(100.0f).Size);
	const float Slowest = Last.Speed * (1.0f - Last.SpeedJitter);
	TestTrue(FString::Printf(TEXT("drawn out three to five times along their flight (%.1f to %.1f)"), DropStretch(Slowest, Last.Stretch), DropStretch(First.Speed, First.Stretch)), DropStretch(Slowest, Last.Stretch) >= 3.0f && DropStretch(First.Speed, First.Stretch) <= 5.0f);
	TestTrue(TEXT("so longer than any shot's drop"), DropLength(First.Size, First.Speed, First.Stretch) > 2.0f);
	TestTrue(TEXT("bright cheese yellow, in a look of their own, which a wound's drops aren't"), First.bBright && Last.bBright && !ShotSpray(100.0f).bBright);
	TestTrue(TEXT("with a sheath of mist several times a second"), HoseMistEvery <= 0.1f && HoseMist().Life > HoseMistEvery);
	TestTrue(TEXT("far faster than the stab's spurts"), First.Speed > StabSpeed(StabOut));

	// Its pool: spreads quickly then slowly to its full size, stays, then fades and is gone.
	TestEqual(TEXT("a pool starts small"), PoolRadiusAt(0.0f), PoolStart);
	const float Early = PoolRadiusAt(PoolGrowSeconds * 0.25f) - PoolStart;
	const float Late = PoolRadiusAt(PoolGrowSeconds) - PoolRadiusAt(PoolGrowSeconds * 0.75f);
	TestTrue(TEXT("spreading quickly at first, then slowing"), Early > Late && Late > 0.0f);
	TestEqual(TEXT("to its full size"), PoolRadiusAt(PoolGrowSeconds + 1.0f), PoolRadius);
	TestTrue(TEXT("a pool a couple of metres across"), PoolRadius * 2.0f >= 200.0f && PoolRadius * 2.0f <= 320.0f);
	TestEqual(TEXT("it stays several seconds"), PoolOpacityAt(PoolGrowSeconds + PoolStays - 0.1f), 1.0f);
	TestTrue(TEXT("then fades"), PoolOpacityAt(PoolGrowSeconds + PoolStays + PoolFades * 0.5f) < 1.0f);
	TestFalse(TEXT("not gone while it shows"), IsPoolGone(PoolGrowSeconds + PoolStays + PoolFades - 0.1f));
	TestTrue(TEXT("and is gone"), IsPoolGone(PoolGrowSeconds + PoolStays + PoolFades) && PoolOpacityAt(PoolGrowSeconds + PoolStays + PoolFades) == 0.0f);
	return true;
}

#endif
