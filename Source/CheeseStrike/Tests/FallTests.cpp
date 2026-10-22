// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Fall damage: its rules as plain functions (Rules/CheeseStrikeFalls.h), and
// players landing hard in the editor (see PlayTesting.h): hurt, killed by
// the fall (credited as a fall into the fondue is), put down by it in
// Execution, and a bot that won't step off a drop that would hurt it.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeFalls.h"
#include "Movement/CheeseStrikeMovement.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace FallTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFallsHurt, "CheeseStrike.Rules.FallsHurt", FallTests::Flags)

bool FFallsHurt::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFalls;
	const float Jump = CheeseStrikeMovement::JumpHeight();
	TestEqual(TEXT("a landing's speed tells the drop"), DropFor(FMath::Sqrt(2.0f * 980.0f * 400.0f), 980.0f), 400.0f, 0.5f);
	TestEqual(TEXT("going up, no drop"), DropFor(-300.0f, 980.0f), 0.0f);
	TestEqual(TEXT("a step down doesn't hurt"), DamageFor(50.0f), 0.0f);
	TestEqual(TEXT("nor a jump down from the Fondue's gantry (3.2 m)"), DamageFor(320.0f + Jump), 0.0f);
	TestEqual(TEXT("nor from a 4 m level (the Grotto's, the Creamery's mezzanine)"), DamageFor(400.0f + Jump), 0.0f);
	TestEqual(TEXT("nor from the Kitchen's worktop (4.5 m)"), DamageFor(450.0f + Jump), 0.0f);
	const float Belfry = DamageFor(700.0f);
	TestTrue(FString::Printf(TEXT("from the Market's belfry (7 m), a fifth or so (%.0f)"), Belfry), Belfry >= 12.0f && Belfry <= 28.0f);
	const float Gallery = DamageFor(800.0f);
	TestTrue(FString::Printf(TEXT("from the Grotto's gallery (8 m), a third or so (%.0f)"), Gallery), Gallery >= 22.0f && Gallery <= 40.0f);
	TestTrue(TEXT("rising steeply with the drop"), DamageFor(1200.0f) > 2.0f * Gallery);
	TestEqual(TEXT("a very long fall kills"), DamageFor(DeadlyDrop), DeadlyDamage);
	TestEqual(TEXT("and longer, no more"), DamageFor(5000.0f), DeadlyDamage);
	TestEqual(TEXT("whole numbers"), DamageFor(777.0f), FMath::RoundToFloat(DamageFor(777.0f)));
	TestTrue(TEXT("a bot steps off where it can't be hurt"), BotMayStepOff(SafeDrop));
	TestFalse(TEXT("but not off a drop that hurts"), BotMayStepOff(SafeDrop + 50.0f));
	TestEqual(TEXT("the feed says it fell"), HardLandingWords(), FString(TEXT("fell")));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "GameFramework/CharacterMovementComponent.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"

namespace FallTests
{
	/** Open floor on the shooter level, its ground about 100 cm under the capsule's middle. */
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);

	/** The host comes down onto the floor as from a drop of `Drop` cm: set falling at the speed such a drop ends in. */
	void Drop(float Drop)
	{
		using namespace PlayTesting;
		PutAt(0, HostSpot + FVector(0.0f, 0.0f, 20.0f), 0.0f);
		Then([Drop]()
		{
			ACheeseStrikeCharacter* Host = CharacterOf(0, 0);
			Host->EndProtection();
			UCharacterMovementComponent* Moving = Host->GetCharacterMovement();
			Moving->SetMovementMode(MOVE_Falling);
			Moving->Velocity = FVector(0.0f, 0.0f, -FMath::Sqrt(2.0f * -Moving->GetGravityZ() * Drop));
		});
		Wait(0.5f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FALongFallHurts, "CheeseStrike.Play.ALongFallHurts", FallTests::Flags)

bool FALongFallHurts::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace FallTests;
	static TWeakObjectPtr<ACheeseStrikeCharacter> Fallen;
	static FString Host;
	static FString Joiner;
	StartPlaying({2});
	PutAt(1, JoinerSpot, 90.0f);
	// A jump down off the Kitchen's worktop: nothing.
	FallTests::Drop(450.0f + CheeseStrikeMovement::JumpHeight());
	Then([this]()
	{
		TestEqual(TEXT("a jump down off a worktop doesn't hurt"), CharacterOf(0, 0)->GetHealth(), 100.0f);
	});
	// Off the Grotto's gallery: hurt, not killed.
	FallTests::Drop(800.0f);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = CharacterOf(0, 0);
		const float Expected = 100.0f - CheeseStrikeFalls::DamageFor(800.0f);
		TestTrue(FString::Printf(TEXT("a drop of 8 m hurts (%.0f health, about %.0f)"), Me->GetHealth(), Expected), Me->IsAlive() && FMath::Abs(Me->GetHealth() - Expected) <= 4.0f);
		const ACheeseStrikeCharacter* Seen = CharacterOf(0, 1);
		TestTrue(TEXT("the joiner sees the host hurt"), Seen != nullptr && Seen->GetHealth() < 100.0f);
	});
	// Hurt by the joiner, then a deadly fall: the joiner's kill.
	Then([this]()
	{
		ACheeseStrikeCharacter* Me = CharacterOf(0, 0);
		ACheeseStrikeCharacter* Them = CharacterOf(1, 0);
		Host = Me->DisplayName();
		Joiner = Them->DisplayName();
		Fallen = Me;
	});
	PutAt(0, HostSpot + FVector(0.0f, 0.0f, 20.0f), 0.0f);
	Then([this]()
	{
		ACheeseStrikeCharacter* Me = CharacterOf(0, 0);
		Me->EndProtection();
		TestTrue(TEXT("the joiner hurts the host"), Me->TakeShot(5.0f, CharacterOf(1, 0), false));
		UCharacterMovementComponent* Moving = Me->GetCharacterMovement();
		Moving->SetMovementMode(MOVE_Falling);
		Moving->Velocity = FVector(0.0f, 0.0f, -FMath::Sqrt(2.0f * -Moving->GetGravityZ() * CheeseStrikeFalls::DeadlyDrop));
	});
	WaitUntil(TEXT("the host dying of the fall"), 3.0f, []() { return Fallen.IsValid() && !Fallen->IsAlive(); });
	Then([this]()
	{
		TestEqual(TEXT("killed by whoever hurt them just before"), Fallen->GetKilledBy(), Joiner);
		const TArray<FKillFeedEntry>& Feed = GameState(0)->GetKillFeed();
		if (TestTrue(TEXT("in the kill feed"), Feed.Num() > 0))
		{
			TestEqual(TEXT("the feed has the joiner's kill"), Feed.Last().Killer, Joiner);
			TestEqual(TEXT("of the host"), Feed.Last().Victim, Host);
		}
		const ACheeseStrikePlayerState* Theirs = CharacterOf(1, 0)->GetPlayerState<ACheeseStrikePlayerState>();
		TestEqual(TEXT("the joiner scores it"), Theirs != nullptr ? Theirs->GetKills() : -1, 1);
	});
	WaitUntil(TEXT("the host back"), 10.0f, []() { return CharacterOf(0, 0) != nullptr && CharacterOf(0, 0) != Fallen.Get() && CharacterOf(0, 0)->IsAlive(); });
	// A deadly fall with nobody to blame: it fell.
	Then([]() { Fallen = CharacterOf(0, 0); });
	FallTests::Drop(CheeseStrikeFalls::DeadlyDrop + 200.0f);
	WaitUntil(TEXT("the host dying of the fall again"), 3.0f, []() { return Fallen.IsValid() && !Fallen->IsAlive(); });
	Wait(0.5f);
	Then([this]()
	{
		TestEqual(TEXT("nobody killed them"), Fallen->GetKilledBy(), FString());
		TestEqual(TEXT("they fell"), Fallen->GetDiedHow(), FString(TEXT("fell")));
		const TArray<FKillFeedEntry>& Feed = GameState(0)->GetKillFeed();
		if (TestTrue(TEXT("in the kill feed"), Feed.Num() > 0))
		{
			TestEqual(TEXT("with no killer"), Feed.Last().Killer, FString());
			TestEqual(TEXT("fell"), Feed.Last().How, FString(TEXT("fell")));
		}
		const ACheeseStrikeCharacter* Seen = CharacterOf(0, 1);
		TestTrue(TEXT("the joiner sees the host fell"), Seen != nullptr && Seen->GetDiedHow() == TEXT("fell"));
		TestEqual(TEXT("and the joiner has no more kills"), CharacterOf(1, 0)->GetPlayerState<ACheeseStrikePlayerState>()->GetKills(), 1);
	});
	// A bot looks before it steps off: over the floor it may, from high over it it may not.
	Then([this]()
	{
		ACheeseStrikeCharacter* Me = CharacterOf(0, 0);
		const UCheeseStrikeMovementComponent* Moving = Cast<UCheeseStrikeMovementComponent>(Me->GetCharacterMovement());
		if (!TestNotNull(TEXT("the game's movement"), Moving))
		{
			return;
		}
		Me->SetActorLocation(HostSpot + FVector(0.0f, 0.0f, 30.0f));
		TestTrue(TEXT("a step down onto the floor is safe"), Moving->IsDropAheadSafe(FVector(1.0f, 0.0f, 0.0f)));
		Me->SetActorLocation(HostSpot + FVector(0.0f, 0.0f, 300.0f));
		TestTrue(TEXT("so is a drop of three metres"), Moving->IsDropAheadSafe(FVector(1.0f, 0.0f, 0.0f)));
		Me->SetActorLocation(HostSpot + FVector(0.0f, 0.0f, CheeseStrikeFalls::SafeDrop + 300.0f));
		TestFalse(TEXT("but not one that would hurt"), Moving->IsDropAheadSafe(FVector(1.0f, 0.0f, 0.0f)));
		Me->SetActorLocation(HostSpot);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFallPutsDownInExecution, "CheeseStrike.Play.AFallPutsDownInExecution", FallTests::Flags)

bool FAFallPutsDownInExecution::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace FallTests;
	StartPlaying({2, TEXT("?Mode=Execution?Rounds=1")});
	PutAt(1, JoinerSpot, 90.0f);
	Wait(CheeseStrikeRules::SpawnProtection);
	// A fall that would kill puts them down, as a shot would.
	FallTests::Drop(CheeseStrikeFalls::DeadlyDrop + 200.0f);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = CharacterOf(0, 0);
		TestTrue(TEXT("a deadly fall puts the host down"), Me->IsDowned());
		TestTrue(TEXT("alive"), Me->IsAlive());
	});
	// Down, another fall doesn't finish them: only an enemy's hand up close does.
	FallTests::Drop(CheeseStrikeFalls::DeadlyDrop + 200.0f);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = CharacterOf(0, 0);
		TestTrue(TEXT("down, a fall doesn't finish them"), Me->IsDowned() && Me->IsAlive());
	});
	StopPlaying();
	return true;
}

#endif
