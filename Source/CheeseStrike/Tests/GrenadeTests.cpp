// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The cheese grenade as arithmetic: how many a player carries, its fuse and
// throw, the harm of its burst with distance, the arc a bot works out to
// land one, and when a bot throws.

#include "Misc/AutomationTest.h"
#include "Weapons/CheeseStrikeGrenadeRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GrenadeTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAGrenadeBurstsAndFallsOff, "CheeseStrike.Weapons.AGrenadeBurstsAndFallsOff", GrenadeTests::Flags)

bool FAGrenadeBurstsAndFallsOff::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeGrenades;
	TestTrue(TEXT("a player comes in with one or two"), SpawnCount >= 1 && SpawnCount <= 2 && SpawnCount <= MostCarried);
	TestTrue(TEXT("its fuse is about two and a half seconds"), Fuse >= 2.2f && Fuse <= 2.8f);
	TestTrue(TEXT("it leaves the hand before the throw is done"), ReleaseDelay > 0.0f && ReleaseDelay < ThrowTime);
	TestTrue(TEXT("and the throw is over in under a second"), ThrowTime <= 1.0f);
	// The burst: deadly at its middle, falling off in a straight line to its edge, nothing past it.
	TestTrue(TEXT("at its middle it kills"), DamageAt(0.0f) >= 100.0f);
	TestEqual(TEXT("its middle does the most"), DamageAt(0.0f), Most);
	TestEqual(TEXT("its edge the least"), DamageAt(Radius), Least);
	TestEqual(TEXT("halfway, halfway between"), DamageAt(Radius * 0.5f), (Most + Least) * 0.5f, 0.01f);
	TestEqual(TEXT("past its edge, nothing"), DamageAt(Radius + 1.0f), 0.0f);
	TestTrue(TEXT("nearer hurts more"), DamageAt(100.0f) > DamageAt(200.0f) && DamageAt(200.0f) > DamageAt(400.0f));
	TestTrue(TEXT("a kill reaches a metre out"), DamageAt(100.0f) >= 100.0f);
	TestTrue(TEXT("but not two"), DamageAt(200.0f) < 100.0f);
	TestTrue(TEXT("its reach is a few metres"), Radius >= 300.0f && Radius <= 600.0f);

	// The throw: a little above the aim, at its speed, with some of the thrower's own.
	const FVector Level = LaunchVelocity(FVector::ForwardVector, FVector::ZeroVector);
	TestEqual(TEXT("it leaves at its speed"), static_cast<float>(Level.Size()), ThrowSpeed, 0.5f);
	TestEqual(TEXT("lifted above the aim"), static_cast<float>(Level.Rotation().Pitch), ThrowLift, 0.01f);
	TestEqual(TEXT("straight ahead"), static_cast<float>(Level.Rotation().Yaw), 0.0f, 0.01f);
	const FVector Running = LaunchVelocity(FVector::ForwardVector, FVector(400.0f, 0.0f, 0.0f));
	TestEqual(TEXT("a running throw goes faster"), static_cast<float>(Running.X - Level.X), 400.0f * CarriedSpeedShare, 0.5f);
	const FVector Up = LaunchVelocity(FVector::UpVector, FVector::ZeroVector);
	TestTrue(TEXT("thrown straight up, it goes up"), Up.Z > ThrowSpeed * 0.99f);
	// How far a throw on flat ground carries before its first bounce: a good way across a room, short of a long sightline.
	const float G = 980.0f * GravityScale;
	const float FlatRange = ThrowSpeed * ThrowSpeed * FMath::Sin(FMath::DegreesToRadians(2.0f * (30.0f + ThrowLift))) / G;
	TestTrue(FString::Printf(TEXT("thrown up at 30 degrees it carries %.0f cm"), FlatRange), FlatRange > 1500.0f && FlatRange < 3000.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FABotThrowsAGrenadeSensibly, "CheeseStrike.Bots.ThrowsAGrenadeSensibly", GrenadeTests::Flags)

bool FABotThrowsAGrenadeSensibly::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeGrenades;
	// The arc: the lower of the two that land where wanted.
	float Pitch = 0.0f;
	TestTrue(TEXT("a spot 10 m off on the level is in reach"), PitchToReach(1000.0f, 0.0f, ThrowSpeed, 980.0f, Pitch));
	const float Flight = 1000.0f / (ThrowSpeed * FMath::Cos(FMath::DegreesToRadians(Pitch)));
	const float Height = ThrowSpeed * FMath::Sin(FMath::DegreesToRadians(Pitch)) * Flight - 0.5f * 980.0f * Flight * Flight;
	TestEqual(TEXT("and the arc found comes down there"), Height, 0.0f, 1.0f);
	TestTrue(FString::Printf(TEXT("the lower arc (%.1f degrees)"), Pitch), Pitch > 0.0f && Pitch < 45.0f);
	float Higher = 0.0f;
	TestTrue(TEXT("a spot up on a ledge is in reach"), PitchToReach(1000.0f, 300.0f, ThrowSpeed, 980.0f, Higher));
	TestTrue(TEXT("by a higher throw"), Higher > Pitch);
	TestFalse(TEXT("a spot 50 m off isn't"), PitchToReach(5000.0f, 0.0f, ThrowSpeed, 980.0f, Pitch));

	// When: an enemy just gone behind cover, in range, with a grenade and none thrown lately.
	FBotThrow Hidden;
	Hidden.Carried = 2;
	Hidden.SinceLastThrow = 30.0f;
	Hidden.SinceSeen = 1.0f;
	Hidden.Distance = 1200.0f;
	Hidden.bSeesThem = false;
	Hidden.Roll = 0.3f;
	TestTrue(TEXT("it throws at someone just gone behind cover"), BotShouldThrow(Hidden));
	FBotThrow Seen = Hidden;
	Seen.bSeesThem = true;
	TestFalse(TEXT("less often at someone in sight"), BotShouldThrow(Seen));
	Seen.Roll = 0.05f;
	TestTrue(TEXT("but now and then"), BotShouldThrow(Seen));
	FBotThrow None = Hidden;
	None.Carried = 0;
	TestFalse(TEXT("never with none left"), BotShouldThrow(None));
	FBotThrow Again = Hidden;
	Again.SinceLastThrow = 2.0f;
	TestFalse(TEXT("nor straight after the last"), BotShouldThrow(Again));
	FBotThrow Close = Hidden;
	Close.Distance = 300.0f;
	TestFalse(TEXT("nor at someone close enough to shoot"), BotShouldThrow(Close));
	FBotThrow Far = Hidden;
	Far.Distance = 4000.0f;
	TestFalse(TEXT("nor beyond a good throw"), BotShouldThrow(Far));
	FBotThrow Forgotten = Hidden;
	Forgotten.SinceSeen = 10.0f;
	TestFalse(TEXT("nor at where someone was long ago"), BotShouldThrow(Forgotten));
	TestTrue(TEXT("a bot's range reaches past the burst"), BotNearest > BotOwnMargin);
	for (const float Distance : {BotNearest, 1000.0f, BotFarthest})
	{
		const float Short = BotLandingShort(Distance);
		TestTrue(FString::Printf(TEXT("at %.0f cm it aims short (%.0f) but never within reach of itself"), Distance, Short), Short >= 0.0f && Distance - Short >= BotOwnMargin);
	}
	TestTrue(TEXT("farther off, it allows for more roll"), BotLandingShort(2000.0f) > BotLandingShort(1000.0f));

	// A grenade about to burst near it that can hurt it, it runs from: out of reach, and a little more.
	TestTrue(TEXT("it runs from a grenade in reach"), BotGetsAway(Radius - 50.0f, true, false));
	TestTrue(TEXT("and one just past it"), BotGetsAway(Radius + 50.0f, true, false));
	TestFalse(TEXT("not from one well away"), BotGetsAway(BotKeepsAway + 10.0f, true, false));
	TestFalse(TEXT("nor one that can't hurt it (a teammate's)"), BotGetsAway(100.0f, false, false));
	TestFalse(TEXT("nor one already burst"), BotGetsAway(100.0f, true, true));
	return true;
}

#endif
