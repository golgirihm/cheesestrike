// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The melted gouda under the Gouda Overload's slowed foes (Rules/CheeseStrikeGoo.h):
// when a puddle is laid, how big, how it spreads, stays and fades, and the cap.
// That it is laid under a slowed foe on every machine is checked by
// CheeseStrike.Play.StreaksAreCalledInAndScoreForTheirCallers.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeGoo.h"
#include "Rules/CheeseStrikeStreaks.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASlowedFoeLeavesMeltedGoudaUnderfoot, "CheeseStrike.Goo.ASlowedFoeLeavesMeltedGoudaUnderfoot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FASlowedFoeLeavesMeltedGoudaUnderfoot::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeGoo;
	// A fresh puddle every few steps, or now and then where they stand; never every frame.
	TestFalse(TEXT("nothing new a moment later on the spot"), PuddleDue(0.0f, 0.1f));
	TestTrue(TEXT("a fresh one a few steps on"), PuddleDue(PuddleEvery, 0.2f) && PuddleEvery >= 20.0f && PuddleEvery <= 60.0f);
	TestTrue(TEXT("and every second or so standing"), PuddleDue(0.0f, PuddleRenew) && PuddleRenew >= 0.5f && PuddleRenew <= 2.0f);
	// Big as the gouda lands, then smaller as the goo thins, so that the trail dwindles as they come back to speed.
	TestTrue(TEXT("the first is the biggest, wider than a body"), FirstRadius > ThickRadius && FirstRadius * 2.0f >= 120.0f);
	TestTrue(TEXT("thick goo leaves bigger ones than thin"), RadiusFor(1.0f) > RadiusFor(0.5f) && RadiusFor(0.5f) > RadiusFor(0.0f));
	TestEqual(TEXT("as thick as it gets"), RadiusFor(2.0f), ThickRadius);
	const float Just = CheeseStrikeStreaks::GooWash(1.0f, 1.0f, 1.0f + CheeseStrikeStreaks::GooSeconds);
	const float Late = CheeseStrikeStreaks::GooWash(1.0f + CheeseStrikeStreaks::GooSeconds * 0.9f, 1.0f, 1.0f + CheeseStrikeStreaks::GooSeconds);
	TestTrue(FString::Printf(TEXT("late in the goo the puddles are small (%.0f cm against %.0f)"), RadiusFor(Late), RadiusFor(Just)), RadiusFor(Late) < RadiusFor(Just) * 0.7f);
	// Each spreads out: quick at first, then slowing, to its full size, and no further.
	TestTrue(TEXT("it starts smaller"), FMath::IsNearlyEqual(RadiusAt(100.0f, 0.0f), 100.0f * PuddleStart) && PuddleStart < 0.6f);
	TestTrue(TEXT("spreads quickly at first"), RadiusAt(100.0f, PuddleGrowSeconds * 0.25f) - RadiusAt(100.0f, 0.0f) > RadiusAt(100.0f, PuddleGrowSeconds) - RadiusAt(100.0f, PuddleGrowSeconds * 0.75f));
	TestTrue(TEXT("to its size, then stays"), FMath::IsNearlyEqual(RadiusAt(100.0f, PuddleGrowSeconds), 100.0f) && FMath::IsNearlyEqual(RadiusAt(100.0f, PuddleGrowSeconds * 3.0f), 100.0f));
	// Whole for a while, then fading, then gone: a few seconds after the goo is.
	TestTrue(TEXT("whole while it stays"), OpacityAt(0.0f) == 1.0f && OpacityAt(PuddleStays) == 1.0f);
	TestTrue(TEXT("fading after"), OpacityAt(PuddleStays + PuddleFades * 0.5f) > 0.3f && OpacityAt(PuddleStays + PuddleFades * 0.5f) < 0.7f);
	TestTrue(TEXT("gone at the end"), OpacityAt(PuddleStays + PuddleFades) == 0.0f && IsGone(PuddleStays + PuddleFades) && !IsGone(PuddleStays));
	TestTrue(TEXT("gone within ten seconds of being laid"), PuddleStays + PuddleFades <= 10.0f);
	// Wave 17: each is a glob with volume, as tall as a thick liquid's puddle is for its width: landing as a heap, slumping
	// as it spreads, melting flat as it fades.
	const float Wide = FirstRadius / BlobRadius;
	TestTrue(FString::Printf(TEXT("the first glob stands %.0f cm at its humps once it has spread"), 20.0f * Wide * BlobThick), 20.0f * Wide * BlobThick >= 12.0f && 20.0f * Wide * BlobThick <= 25.0f);
	TestTrue(TEXT("a heap as it lands"), BlobHeightAt(0.0f) == BlobHeap && BlobHeap > 1.0f);
	TestTrue(TEXT("slumping as it spreads"), BlobHeightAt(PuddleGrowSeconds * 0.5f) < BlobHeightAt(0.0f) && FMath::IsNearlyEqual(BlobHeightAt(PuddleGrowSeconds), 1.0f));
	TestEqual(TEXT("its height while it stays"), BlobHeightAt(PuddleStays), 1.0f);
	TestTrue(TEXT("melting down as it fades"), BlobHeightAt(PuddleStays + PuddleFades * 0.5f) < 0.7f && BlobHeightAt(PuddleStays + PuddleFades) == 0.0f);
	TestTrue(TEXT("lying in a wet film a little wider than itself"), FilmShare > 1.0f && FilmShare < 1.5f);
	// Capped: a walk through the whole goo leaves no more than the cap (and the oldest go first past it).
	const int32 Walk = FMath::CeilToInt32(CheeseStrikeStreaks::GooSeconds / PuddleRenew + 1.0f);
	TestTrue(FString::Printf(TEXT("a foe standing in it the whole time leaves %d, within the cap"), Walk), Walk <= MostPuddles);
	return true;
}

#endif
