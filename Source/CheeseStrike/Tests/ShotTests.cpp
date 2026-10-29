// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// What a shot does, as arithmetic: damage with distance and on the head,
// time to kill, spread that blooms, recoil that climbs and settles, the
// ray tests a hit is decided by, and where a player was put back to.

#include "Misc/AutomationTest.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeShot.h"
#include "Rules/CheeseStrikeDowned.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ShotTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageFallsOffWithDistanceAndDoublesOnTheHead, "CheeseStrike.Shot.DamageFallsOffWithDistanceAndDoublesOnTheHead", ShotTests::Flags)

bool FDamageFallsOffWithDistanceAndDoublesOnTheHead::RunTest(const FString& Parameters)
{
	const FWeaponStats& Weapon = CheeseStrikeWeapons::Blaster();
	TestEqual(TEXT("close, the body takes the whole damage"), CheeseStrikeWeapons::DamageAt(Weapon, 100.0f, false), Weapon.Damage);
	TestEqual(TEXT("up to where falloff starts"), CheeseStrikeWeapons::DamageAt(Weapon, Weapon.FalloffStart, false), Weapon.Damage);
	TestEqual(TEXT("the floor from where it ends"), CheeseStrikeWeapons::DamageAt(Weapon, Weapon.FalloffEnd, false), Weapon.Damage * Weapon.FalloffFloor);
	TestEqual(TEXT("and no less further out"), CheeseStrikeWeapons::DamageAt(Weapon, Weapon.MaxRange, false), Weapon.Damage * Weapon.FalloffFloor);
	const float Halfway = (Weapon.FalloffStart + Weapon.FalloffEnd) / 2.0f;
	TestEqual(TEXT("halfway between"), CheeseStrikeWeapons::DamageAt(Weapon, Halfway, false), Weapon.Damage * (1.0f + Weapon.FalloffFloor) / 2.0f);
	TestEqual(TEXT("the head takes more"), CheeseStrikeWeapons::DamageAt(Weapon, 100.0f, true), Weapon.Damage * Weapon.HeadMultiplier);
	TestTrue(TEXT("five body shots to kill, in about half a second"), CheeseStrikeWeapons::TimeToKill(Weapon) > 0.4f && CheeseStrikeWeapons::TimeToKill(Weapon) < 0.6f);
	TestTrue(TEXT("falloff starts beyond a room and ends across a yard"), Weapon.FalloffStart > 1000.0f && Weapon.FalloffEnd < 4000.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHeadHitTakesTheHeadsShareAtAnyRange, "CheeseStrike.Shot.AHeadHitTakesTheHeadsShareAtAnyRange", ShotTests::Flags)

bool FAHeadHitTakesTheHeadsShareAtAnyRange::RunTest(const FString& Parameters)
{
	// Every gun's head hit is its body hit times its head multiplier, however far; only the Roquefort's takes a whole health
	// (100) everywhere: any other's head hit that leaves someone standing is the weapon's numbers, not a shot gone wrong.
	for (const FWeaponStats* Weapon : CheeseStrikeArsenal::All())
	{
		if (Weapon->IsMelee())
		{
			continue;
		}
		bool bMultiplied = true;
		float Least = TNumericLimits<float>::Max();
		for (float Distance = 0.0f; Distance <= Weapon->MaxRange; Distance += 100.0f)
		{
			const float Head = CheeseStrikeWeapons::DamageAt(*Weapon, Distance, true);
			bMultiplied &= FMath::IsNearlyEqual(Head, CheeseStrikeWeapons::DamageAt(*Weapon, Distance, false) * Weapon->HeadMultiplier, 0.01f);
			Least = FMath::Min(Least, Head * FMath::Max(Weapon->Pellets, 1));
		}
		TestTrue(FString::Printf(TEXT("the %s's head hit is its body hit times %.2f at every range"), *Weapon->DisplayName.ToString(), Weapon->HeadMultiplier), bMultiplied);
		const bool bRoquefort = Weapon->Id == CheeseStrikeWeapons::Roquefort().Id;
		TestEqual(FString::Printf(TEXT("the %s's head hit is a sure kill only for the Roquefort (the least %.0f)"), *Weapon->DisplayName.ToString(), Least), Least >= 100.0f, bRoquefort);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAScopedRifleSwaysWithTheBreath, "CheeseStrike.Shot.AScopedRifleSwaysWithTheBreath", ShotTests::Flags)

bool FAScopedRifleSwaysWithTheBreath::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	TestTrue(TEXT("only the Roquefort is scoped"), Roquefort().bScoped && !Blaster().bScoped);
	TestTrue(TEXT("a few tenths of a degree each way"), ScopeSwayYaw > 0.1f && ScopeSwayYaw < 0.6f && ScopeSwayPitch > 0.1f && ScopeSwayPitch < 0.6f);
	TestEqual(TEXT("it starts at the aim"), ScopeSway(0.0f), FVector2D::ZeroVector);
	// Over a period: within its amplitudes, centred, slow, and a figure-eight (the pitch goes twice for the yaw's once).
	FVector2D Sum = FVector2D::ZeroVector;
	FVector2D Most = FVector2D::ZeroVector;
	float Fastest = 0.0f;
	int32 PitchCrossings = 0;
	int32 YawCrossings = 0;
	constexpr float Step = 0.01f;
	FVector2D Last = ScopeSway(0.0f);
	for (float Time = Step; Time < ScopeSwayPeriod; Time += Step)
	{
		const FVector2D Now = ScopeSway(Time);
		Sum += Now;
		Most = FVector2D(FMath::Max(Most.X, FMath::Abs(Now.X)), FMath::Max(Most.Y, FMath::Abs(Now.Y)));
		Fastest = FMath::Max(Fastest, static_cast<float>((Now - Last).Size() / Step));
		PitchCrossings += (Now.Y > 0.0f) != (Last.Y > 0.0f) ? 1 : 0;
		YawCrossings += (Now.X > 0.0f) != (Last.X > 0.0f) ? 1 : 0;
		Last = Now;
	}
	TestTrue(FString::Printf(TEXT("it never goes past its amplitudes (%.3f, %.3f)"), Most.X, Most.Y), Most.X <= ScopeSwayYaw + 0.0001f && Most.Y <= ScopeSwayPitch + 0.0001f && Most.X > ScopeSwayYaw * 0.99f && Most.Y > ScopeSwayPitch * 0.99f);
	TestTrue(TEXT("and is centred on the aim"), FMath::Abs(Sum.X) / (ScopeSwayPeriod / Step) < 0.01f && FMath::Abs(Sum.Y) / (ScopeSwayPeriod / Step) < 0.01f);
	TestTrue(FString::Printf(TEXT("it is slow: under half a degree a second (%.2f)"), Fastest), Fastest < 0.5f);
	TestTrue(FString::Printf(TEXT("the pitch crosses the aim twice as often as the yaw (%d, %d)"), PitchCrossings, YawCrossings), PitchCrossings >= 2 * YawCrossings - 1 && PitchCrossings <= 2 * YawCrossings + 1);
	TestTrue(TEXT("and it comes back to where it began after a period"), ScopeSway(ScopeSwayPeriod * 3.0f).Equals(ScopeSway(0.0f), 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpreadBloomsAndRecoilClimbs, "CheeseStrike.Shot.SpreadBloomsAndRecoilClimbs", ShotTests::Flags)

bool FSpreadBloomsAndRecoilClimbs::RunTest(const FString& Parameters)
{
	const FWeaponStats& Weapon = CheeseStrikeWeapons::Blaster();
	TestEqual(TEXT("the first shot from the hip goes where it is aimed"), CheeseStrikeWeapons::SpreadFor(Weapon, 0, false), 0.0f);
	TestEqual(TEXT("halfway through the bloom, half the cone"), CheeseStrikeWeapons::SpreadFor(Weapon, Weapon.BloomShots / 2, false), Weapon.HipSpreadDegrees / 2.0f);
	TestEqual(TEXT("the whole cone after the bloom"), CheeseStrikeWeapons::SpreadFor(Weapon, Weapon.BloomShots, false), Weapon.HipSpreadDegrees);
	TestEqual(TEXT("and no more"), CheeseStrikeWeapons::SpreadFor(Weapon, Weapon.BloomShots * 3, false), Weapon.HipSpreadDegrees);
	TestEqual(TEXT("aimed, the tight cone"), CheeseStrikeWeapons::SpreadFor(Weapon, Weapon.BloomShots, true), Weapon.SpreadDegrees);
	TestTrue(TEXT("which is far tighter"), Weapon.SpreadDegrees * 10.0f < Weapon.HipSpreadDegrees);

	TestEqual(TEXT("the first shot has no kick"), CheeseStrikeWeapons::RecoilFor(Weapon, 0, false), FVector2D::ZeroVector);
	const FVector2D Second = CheeseStrikeWeapons::RecoilFor(Weapon, 1, false);
	TestTrue(TEXT("the second climbs"), Second.Y > 0.0f);
	TestTrue(TEXT("by about one shot's worth"), Second.Y < Weapon.RecoilUp && Second.Y > Weapon.RecoilUp * 0.4f);
	float Last = 0.0f;
	for (int32 Index = 1; Index < 40; ++Index)
	{
		const float Climb = CheeseStrikeWeapons::RecoilFor(Weapon, Index, false).Y;
		TestTrue(FString::Printf(TEXT("shot %d climbs on"), Index), Climb > Last);
		TestTrue(FString::Printf(TEXT("shot %d stays under the cap"), Index), Climb < Weapon.RecoilUp * Weapon.RecoilCap);
		Last = Climb;
	}
	TestTrue(TEXT("and settles near the cap"), Last > Weapon.RecoilUp * Weapon.RecoilCap * 0.9f);
	TestTrue(TEXT("it wanders to the side"), FMath::Abs(CheeseStrikeWeapons::RecoilFor(Weapon, 5, false).X) > 0.0f);
	TestTrue(TEXT("along the same pattern every time"), CheeseStrikeWeapons::RecoilFor(Weapon, 5, false) == CheeseStrikeWeapons::RecoilFor(Weapon, 5, false));
	TestEqual(TEXT("aimed, it is steadier"), CheeseStrikeWeapons::RecoilFor(Weapon, 5, true).Y, CheeseStrikeWeapons::RecoilFor(Weapon, 5, false).Y * CheeseStrikeWeapons::AimRecoilFactor);

	TestEqual(TEXT("a run begins at the first shot"), CheeseStrikeWeapons::NextInRun(Weapon, -1, 100.0f), 0);
	TestEqual(TEXT("and goes on while shots come quickly"), CheeseStrikeWeapons::NextInRun(Weapon, 3, Weapon.FireInterval), 4);
	TestEqual(TEXT("and starts over after a pause"), CheeseStrikeWeapons::NextInRun(Weapon, 3, Weapon.RecoilReset + 0.01f), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAShotIsTurnedAndSpread, "CheeseStrike.Shot.AShotIsTurnedAndSpread", ShotTests::Flags)

bool FAShotIsTurnedAndSpread::RunTest(const FString& Parameters)
{
	const FVector Ahead = FVector::ForwardVector;
	const FVector Up = CheeseStrikeWeapons::Turned(Ahead, FVector2D(0.0f, 90.0f));
	TestTrue(TEXT("90 degrees up is up"), Up.Equals(FVector::UpVector, 0.001f));
	const FVector Right = CheeseStrikeWeapons::Turned(Ahead, FVector2D(90.0f, 0.0f));
	TestTrue(TEXT("90 degrees right is right"), Right.Equals(FVector::RightVector, 0.001f));
	TestTrue(TEXT("nothing is nothing"), CheeseStrikeWeapons::Turned(Ahead, FVector2D::ZeroVector).Equals(Ahead, 0.001f));

	FRandomStream Random(7);
	TestTrue(TEXT("no cone, no spread"), CheeseStrikeWeapons::Spread(Ahead, 0.0f, Random).Equals(Ahead, 0.001f));
	float Widest = 0.0f;
	for (int32 Shot = 0; Shot < 50; ++Shot)
	{
		const FVector Way = CheeseStrikeWeapons::Spread(Ahead, 5.0f, Random);
		const float Off = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Way, Ahead), -1.0f, 1.0f)));
		// A cone 5 degrees wide: half of that either side of the aim.
		TestTrue(FString::Printf(TEXT("shot %d stays in the cone (%.2f off)"), Shot, Off), Off <= 2.5f + 0.01f);
		Widest = FMath::Max(Widest, Off);
	}
	TestTrue(FString::Printf(TEXT("and fills it (%.2f the furthest off)"), Widest), Widest > 1.5f);
	FRandomStream A(42);
	FRandomStream B(42);
	TestTrue(TEXT("the same seed gives the same shot, for the shooter and the server to agree"), CheeseStrikeWeapons::Spread(Ahead, 5.0f, A).Equals(CheeseStrikeWeapons::Spread(Ahead, 5.0f, B), 0.0001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARayHitsACapsuleAndAHead, "CheeseStrike.Shot.ARayHitsACapsuleAndAHead", ShotTests::Flags)

bool FARayHitsACapsuleAndAHead::RunTest(const FString& Parameters)
{
	const FVector Feet(1000.0f, 0.0f, 0.0f);
	const float Radius = 34.0f;
	const float Height = 192.0f;
	// Straight at the middle of the body.
	const float Middle = CheeseStrikeWeapons::RayHitsCapsule(FVector(0, 0, 96), FVector::ForwardVector, Feet, Radius, Height);
	TestTrue(TEXT("hits the near side"), FMath::IsNearlyEqual(Middle, 1000.0f - Radius, 0.01f));
	// Past the side.
	TestEqual(TEXT("misses past the side"), CheeseStrikeWeapons::RayHitsCapsule(FVector(0, Radius + 1.0f, 96), FVector::ForwardVector, Feet, Radius, Height), -1.0f);
	// Over the top, and under the feet.
	TestEqual(TEXT("misses over the top"), CheeseStrikeWeapons::RayHitsCapsule(FVector(0, 0, Height + 1.0f), FVector::ForwardVector, Feet, Radius, Height), -1.0f);
	TestEqual(TEXT("misses under the feet"), CheeseStrikeWeapons::RayHitsCapsule(FVector(0, 0, -1.0f), FVector::ForwardVector, Feet, Radius, Height), -1.0f);
	// The rounded top: a ray at the very top's height, off centre, hits the cap.
	const float Cap = CheeseStrikeWeapons::RayHitsCapsule(FVector(0, 0, Height - Radius), FVector::ForwardVector, Feet, Radius, Height);
	TestTrue(TEXT("hits the top's round"), FMath::IsNearlyEqual(Cap, 1000.0f - Radius, 0.01f));
	// Away from it, and from inside.
	TestEqual(TEXT("misses behind"), CheeseStrikeWeapons::RayHitsCapsule(FVector(0, 0, 96), FVector::BackwardVector, Feet, Radius, Height), -1.0f);
	TestEqual(TEXT("from inside doesn't count"), CheeseStrikeWeapons::RayHitsCapsule(Feet + FVector(0, 0, 96), FVector::ForwardVector, Feet, Radius, Height), -1.0f);
	// A crouched capsule is shorter.
	TestEqual(TEXT("a crouched body is missed over its head"), CheeseStrikeWeapons::RayHitsCapsule(FVector(0, 0, 100.0f), FVector::ForwardVector, Feet, Radius, 88.0f), -1.0f);

	const FVector Head(1000.0f, 0.0f, 160.0f);
	TestTrue(TEXT("a head shot"), FMath::IsNearlyEqual(CheeseStrikeWeapons::RayHitsSphere(FVector(0, 0, 160), FVector::ForwardVector, Head, CheeseStrikeWeapons::HeadRadius), 1000.0f - CheeseStrikeWeapons::HeadRadius, 0.01f));
	TestEqual(TEXT("just over the head"), CheeseStrikeWeapons::RayHitsSphere(FVector(0, 0, 160 + CheeseStrikeWeapons::HeadRadius + 1.0f), FVector::ForwardVector, Head, CheeseStrikeWeapons::HeadRadius), -1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAPlayerIsPutBackWhereTheyWere, "CheeseStrike.Shot.APlayerIsPutBackWhereTheyWere", ShotTests::Flags)

bool FAPlayerIsPutBackWhereTheyWere::RunTest(const FString& Parameters)
{
	FHitHistory History;
	TestEqual(TEXT("nothing yet"), History.At(1.0f).Feet, FVector::ZeroVector);
	for (int32 Tick = 0; Tick <= 20; ++Tick)
	{
		FHitHistory::FSample Sample;
		Sample.Time = Tick * 0.1f;
		Sample.Feet = FVector(Tick * 100.0f, 0.0f, 0.0f);
		Sample.Eyes = Sample.Feet + FVector(0, 0, 160.0f);
		Sample.HeadLow = Sample.Eyes;
		Sample.HeadHigh = Sample.Eyes + FVector(0, 0, 8.0f);
		// Turning, through the back: from 170 degrees round to -170 is 20 degrees, not 340.
		Sample.Yaw = Tick <= 15 ? 170.0f : -170.0f;
		History.Add(Sample);
	}
	TestTrue(FString::Printf(TEXT("only the last second is kept (%d samples)"), History.Num()), History.Num() >= 10 && History.Num() <= 12);
	TestTrue(TEXT("between two samples, between their places"), History.At(1.55f).Feet.Equals(FVector(1550.0f, 0.0f, 0.0f), 0.01f));
	TestTrue(TEXT("the eyes too"), History.At(1.55f).Eyes.Equals(FVector(1550.0f, 0.0f, 160.0f), 0.01f));
	TestTrue(TEXT("and the head"), History.At(1.55f).HeadHigh.Equals(FVector(1550.0f, 0.0f, 168.0f), 0.01f));
	TestTrue(FString::Printf(TEXT("and which way they faced, the short way round (%.1f)"), History.At(1.55f).Yaw), FMath::IsNearlyEqual(FRotator::NormalizeAxis(History.At(1.55f).Yaw), 180.0f, 0.1f) || FMath::IsNearlyEqual(FRotator::NormalizeAxis(History.At(1.55f).Yaw), -180.0f, 0.1f));
	TestTrue(TEXT("on a sample, that sample"), History.At(1.5f).Feet.Equals(FVector(1500.0f, 0.0f, 0.0f), 0.01f));
	TestTrue(TEXT("past the end, the latest"), History.At(5.0f).Feet.Equals(FVector(2000.0f, 0.0f, 0.0f), 0.01f));
	TestTrue(TEXT("before the start, the earliest kept"), History.At(0.0f).Feet.X < 1100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAShotFindsABodyLyingDown, "CheeseStrike.Shot.AShotFindsABodyLyingDown", ShotTests::Flags)

bool FAShotFindsABodyLyingDown::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	// A capsule lying any way: along X from 0 to 100, 10 thick.
	const FVector From(0.0f, 0.0f, 20.0f);
	const FVector To(100.0f, 0.0f, 20.0f);
	TestTrue(TEXT("a shot from the side hits its side"), FMath::IsNearlyEqual(RayHitsSegment(FVector(50, -500, 20), FVector(0, 1, 0), From, To, 10.0f), 490.0f, 0.01f));
	TestTrue(TEXT("a shot down the length hits its round end"), FMath::IsNearlyEqual(RayHitsSegment(FVector(-500, 0, 20), FVector(1, 0, 0), From, To, 10.0f), 490.0f, 0.01f));
	TestTrue(TEXT("a shot from above hits its top"), FMath::IsNearlyEqual(RayHitsSegment(FVector(30, 0, 520), FVector(0, 0, -1), From, To, 10.0f), 490.0f, 0.01f));
	TestEqual(TEXT("over it is a miss"), RayHitsSegment(FVector(50, -500, 31), FVector(0, 1, 0), From, To, 10.0f), -1.0f);
	TestEqual(TEXT("past its end is a miss"), RayHitsSegment(FVector(111, -500, 20), FVector(0, 1, 0), From, To, 10.0f), -1.0f);
	TestEqual(TEXT("from inside, no hit"), RayHitsSegment(FVector(50, 0, 20), FVector(0, 1, 0), From, To, 10.0f), -1.0f);
	// A player lying down, facing +X, the capsule's axis meeting the floor at the origin.
	const CheeseStrikeDowned::FShape Body = CheeseStrikeDowned::Shape(FVector::ZeroVector, 0.0f, 500.0f);
	FHitHistory::FSample Lying;
	Lying.bLying = true;
	Lying.Feet = FVector::ZeroVector;
	Lying.Eyes = Body.Head;
	Lying.HeadLow = Body.Head;
	Lying.HeadHigh = Body.Head;
	Lying.Hips = Body.Hips;
	Lying.Shoulders = Body.Shoulders;
	Lying.LegsTo = Body.Feet;
	Lying.Height = 88.0f;
	const FVector Side(0.0f, 1.0f, 0.0f);
	TestTrue(TEXT("lying, a shot at the hips from the side hits"), FHitHistory::RayHitsBody(FVector(Body.Hips.X, -500, Body.Hips.Z), Side, Lying) > 0.0f);
	TestTrue(TEXT("and at the shins, out ahead along the floor"), FHitHistory::RayHitsBody(FVector(Body.Feet.X - 10.0f, -500, 10.0f), Side, Lying) > 0.0f);
	TestEqual(TEXT("but not at the chest of someone standing there"), FHitHistory::RayHitsBody(FVector(0, -500, 120.0f), Side, Lying), -1.0f);
	TestEqual(TEXT("nor over the legs"), FHitHistory::RayHitsBody(FVector(Body.Feet.X - 20.0f, -500, 40.0f), Side, Lying), -1.0f);
	FHitHistory::FSample Standing = Lying;
	Standing.bLying = false;
	Standing.Height = 192.0f;
	TestTrue(TEXT("standing, the upright capsule as before"), FHitHistory::RayHitsBody(FVector(0, -500, 120.0f), Side, Standing) > 0.0f);
	// Put back between a sample standing and one lying, it is as the nearer was.
	FHitHistory History;
	FHitHistory::FSample Before = Standing;
	Before.Time = 0.0f;
	FHitHistory::FSample After = Lying;
	After.Time = 0.1f;
	History.Add(Before);
	History.Add(After);
	TestFalse(TEXT("nearer the standing sample, standing"), History.At(0.04f).bLying);
	TestTrue(TEXT("nearer the lying one, lying where it lay"), History.At(0.06f).bLying && History.At(0.06f).Hips.Equals(Body.Hips, 0.01f));
	// Lying, the head is where the raised head is: a shot at it from the side is to the head.
	bool bHead = false;
	TestTrue(TEXT("lying, a shot at the raised head hits"), FHitHistory::RayHitsPlayer(FVector(Body.Head.X, -500, Body.Head.Z), Side, Lying, bHead) > 0.0f);
	TestTrue(TEXT("and is to the head"), bHead);
	FHitHistory::RayHitsPlayer(FVector(Body.Hips.X, -500, Body.Hips.Z), Side, Lying, bHead);
	TestFalse(TEXT("one at the hips isn't"), bHead);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAShotToTheVisibleHeadIsAHeadshot, "CheeseStrike.Shot.AShotToTheVisibleHeadIsAHeadshot", ShotTests::Flags)

bool FAShotToTheVisibleHeadIsAHeadshot::RunTest(const FString& Parameters)
{
	// Lt. Belica standing 10 m ahead facing the shooter: her head bone 170 cm up (as her mesh has it), its X up the head and its
	// Y the way she faces; her head her row's capsule on it.
	const CheeseStrikeBodies::FBody& Belica = CheeseStrikeBodies::All()[CheeseStrikeBodies::IndexOf(TEXT("Lt. Belica"))];
	const FVector Feet(1000.0f, 0.0f, 0.0f);
	const FTransform HeadBone(FRotationMatrix::MakeFromXY(FVector::UpVector, FVector::BackwardVector).ToQuat(), Feet + FVector(0.0f, 0.0f, 170.0f));
	FHitHistory::FSample Standing;
	Standing.Feet = Feet;
	Standing.Radius = 34.0f;
	Standing.Height = 192.0f;
	Standing.Eyes = Feet + FVector(0.0f, 0.0f, 165.0f);
	Standing.HeadLow = HeadBone.TransformPosition(Belica.HeadLow);
	Standing.HeadHigh = HeadBone.TransformPosition(Belica.HeadHigh);
	Standing.HeadRadius = Belica.HeadRadius;
	const float Top = Standing.HeadHigh.Z + Standing.HeadRadius;
	TestTrue(FString::Printf(TEXT("the top of the head about her height (%.0f cm)"), Top), Top > 180.0f && Top < 195.0f);
	bool bHead = false;
	const auto Shot = [&Standing, &bHead](const FVector& From, const FVector& Along)
	{
		return FHitHistory::RayHitsPlayer(From, Along.GetSafeNormal(), Standing, bHead);
	};
	TestTrue(TEXT("a shot at the face hits"), Shot(FVector(0.0f, 0.0f, 165.0f), FVector::ForwardVector) > 0.0f);
	TestTrue(TEXT("and is to the head, though the body's capsule is touched first"), bHead);
	Shot(FVector(0.0f, 0.0f, Top - 3.0f), FVector::ForwardVector);
	TestTrue(TEXT("through the top of the head, to the head"), bHead);
	Shot(FVector(2000.0f, 0.0f, 172.0f), FVector::BackwardVector);
	TestTrue(TEXT("through the back of the head, to the head"), bHead);
	Shot(FVector(1000.0f, -800.0f, 176.0f), FVector::RightVector);
	TestTrue(TEXT("through its side, to the head"), bHead);
	Shot(FVector(0.0f, 0.0f, Top + 298.0f), FVector(1000.0f, 0.0f, -300.0f));
	TestTrue(TEXT("from above onto the crown, to the head"), bHead);
	TestTrue(TEXT("a shot through the shoulder hits"), Shot(FVector(0.0f, 20.0f, 150.0f), FVector::ForwardVector) > 0.0f);
	TestFalse(TEXT("but not the head"), bHead);
	TestTrue(TEXT("nor does one past the side of the head, over the shoulder"), Shot(FVector(0.0f, 16.0f, 172.0f), FVector::ForwardVector) > 0.0f && !bHead);
	Shot(FVector(0.0f, 0.0f, 120.0f), FVector::ForwardVector);
	TestFalse(TEXT("nor one at the chest"), bHead);
	TestEqual(TEXT("over the head is a miss"), Shot(FVector(0.0f, 0.0f, 200.0f), FVector::ForwardVector), -1.0f);
	return true;
}

#endif
