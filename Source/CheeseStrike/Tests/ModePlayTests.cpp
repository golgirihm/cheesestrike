// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the modes and the streaks in the editor (see
// PlayTesting.h): a team winning Deathmatch with friendly fire off, a round
// of Elimination ending and the next beginning, and the streaks called in
// and scoring for their callers.

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "Streaks/CheeseStrikeAerialCheese.h"
#include "Streaks/CheeseStrikeBombs.h"
#include "Weapons/CheeseStrikeShot.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace ModePlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** Two spots facing each other across 4 m of clear ground on the shooter level, and somewhere out of the way. */
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	constexpr float HostYaw = -90.0f;
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);
	constexpr float JoinerYaw = 90.0f;
	const FVector AsideSpot(1150.0f, -1450.0f, 100.0f);
	constexpr float AsideYaw = 117.0f;
	const TCHAR* const Burst = TEXT("hold Fire\nwait 1.2\nrelease Fire");

	/** Player `Of`'s state as world `Seen` has it. */
	ACheeseStrikePlayerState* StateOf(int32 Of, int32 Seen)
	{
		const ACheeseStrikePlayerController* Own = PlayTesting::Controller(Of);
		const ACheeseStrikeGameState* Match = PlayTesting::GameState(Seen);
		if (Own == nullptr || Own->PlayerState == nullptr || Match == nullptr)
		{
			return nullptr;
		}
		for (APlayerState* Each : Match->PlayerArray)
		{
			if (Each != nullptr && Each->GetPlayerId() == Own->PlayerState->GetPlayerId())
			{
				return Cast<ACheeseStrikePlayerState>(Each);
			}
		}
		return nullptr;
	}

	ACheeseStrikeGameMode* Mode()
	{
		UWorld* Host = PlayTesting::World(0);
		return Host != nullptr ? Host->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	}

	/** How far `At` is from the nearest player start. */
	float FromNearestStart(UWorld* World, const FVector& At)
	{
		float Nearest = TNumericLimits<float>::Max();
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist2D(It->GetActorLocation(), At)));
		}
		return Nearest;
	}

	template <typename T>
	T* First(UWorld* World)
	{
		TActorIterator<T> It(World);
		return It ? *It : nullptr;
	}

	bool Dead(int32 Player)
	{
		const ACheeseStrikeCharacter* Body = PlayTesting::Character(Player);
		return Body != nullptr && !Body->IsAlive();
	}

	TWeakObjectPtr<ACheeseStrikeCharacter> Remembered;
	int32 ShotsBefore = 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATeamWinsDeathmatchAtItsKillLimit, "CheeseStrike.Play.ATeamWinsDeathmatchAtItsKillLimit", ModePlayTests::Flags)

bool FATeamWinsDeathmatchAtItsKillLimit::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ModePlayTests;
	StartPlaying({3, TEXT("?Mode=Deathmatch?KillLimit=1?RespawnDelay=1")});
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		if (!TestNotNull(TEXT("the match"), Match))
		{
			return;
		}
		TestTrue(TEXT("Deathmatch is on"), Match->GetMode() == CheeseStrikeModes::EMode::Deathmatch && Match->HasTeams());
		TestEqual(TEXT("to the host's kill limit"), Match->GetKillLimit(), 1);
		const ACheeseStrikePlayerState* Host = StateOf(0, 0);
		const ACheeseStrikePlayerState* First = StateOf(1, 0);
		const ACheeseStrikePlayerState* Second = StateOf(2, 0);
		if (TestNotNull(TEXT("the host"), Host) && TestNotNull(TEXT("the first joiner"), First) && TestNotNull(TEXT("the second"), Second))
		{
			TestTrue(TEXT("the first joiner is on the other side"), Match->AreEnemies(Host, First));
			TestTrue(TEXT("the second on the host's"), Match->AreTeammates(Host, Second));
		}
		const ACheeseStrikeGameState* Seen = GameState(2);
		TestTrue(TEXT("a joiner knows the mode"), Seen != nullptr && Seen->HasTeams() && Seen->GetMode() == CheeseStrikeModes::EMode::Deathmatch);
	});
	PutAt(0, HostSpot, HostYaw);
	PutAt(2, JoinerSpot, JoinerYaw);
	PutAt(1, AsideSpot, AsideYaw);
	Wait(CheeseStrikeRules::SpawnProtection);
	// A teammate's shots do nothing.
	Play(2, Burst);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikeCharacter* Teammate = Character(2);
		if (TestNotNull(TEXT("the host"), Host) && TestNotNull(TEXT("the teammate"), Teammate))
		{
			TestTrue(FString::Printf(TEXT("the teammate fired (%d shots)"), Teammate->GetWeapon()->ShotsFired), Teammate->GetWeapon()->ShotsFired > 3);
			TestEqual(TEXT("but the host is unhurt: friendly fire is off"), Host->GetHealth(), CheeseStrikeWeapons::MaxHealth);
		}
		TestEqual(TEXT("and nobody has scored"), GameState(0)->GetTeamScore(0) + GameState(0)->GetTeamScore(1), 0);
	});
	PutAt(2, AsideSpot + FVector(300.0f, 0.0f, 0.0f), AsideYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Play(1, Burst);
	WaitUntil(TEXT("the match ending"), 6.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && Match->HasMatchEnded(); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestEqual(TEXT("the other side has its kill"), Match->GetTeamScore(1), 1);
		TestEqual(TEXT("and the host's none"), Match->GetTeamScore(0), 0);
		TestEqual(TEXT("so it wins"), Match->GetWinningTeam(), 1);
		TestEqual(TEXT("by name"), Match->GetWinnerName(), FString(CheeseStrikeRules::TeamNames[1]));
		const ACheeseStrikePlayerState* Killer = StateOf(1, 0);
		TestTrue(TEXT("and the joiner has the kill"), Killer != nullptr && Killer->GetKills() == 1);
	});
	WaitUntil(TEXT("a joiner hearing who won"), 5.0f, []() { const ACheeseStrikeGameState* Match = GameState(2); return Match != nullptr && Match->HasMatchEnded() && Match->GetWinningTeam() == 1; });
	Then([this]()
	{
		const ACheeseStrikeGameState* Seen = GameState(2);
		TestEqual(TEXT("with the score"), Seen->GetTeamScore(1), 1);
		TestEqual(TEXT("and the team's name"), Seen->GetWinnerName(), FString(CheeseStrikeRules::TeamNames[1]));
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnEliminationRoundEndsWhenASideIsOut, "CheeseStrike.Play.AnEliminationRoundEndsWhenASideIsOut", ModePlayTests::Flags)

bool FAnEliminationRoundEndsWhenASideIsOut::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ModePlayTests;
	StartPlaying({2, TEXT("?Mode=Elimination?Rounds=2")});
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		if (TestNotNull(TEXT("the match"), Match))
		{
			TestTrue(TEXT("Elimination is on"), Match->GetMode() == CheeseStrikeModes::EMode::Elimination);
			TestEqual(TEXT("round 1"), Match->GetRound(), 1);
			TestEqual(TEXT("of the first to 2"), Match->GetRoundLimit(), 2);
			TestTrue(FString::Printf(TEXT("with its time running (%.0f s left)"), Match->SecondsLeftInRound()), Match->SecondsLeftInRound() > CheeseStrikeModes::RoundTime - 30.0f);
		}
	});
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Wait(CheeseStrikeRules::SpawnProtection);
	Play(1, Burst);
	WaitUntil(TEXT("the host dying"), 5.0f, []() { return Dead(0); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikePlayerState* Host = StateOf(0, 0);
		TestTrue(TEXT("the host is out"), Host != nullptr && Host->IsOut());
		TestTrue(TEXT("the round is over"), Match->IsRoundBreak());
		TestEqual(TEXT("to the side left standing"), Match->GetLastRoundWinner(), 1);
		TestEqual(TEXT("which has a round"), Match->GetTeamScore(1), 1);
		TestFalse(TEXT("and the match goes on"), Match->HasMatchEnded());
		Remembered = Character(0);
	});
	WaitUntil(TEXT("the joiner hearing the host is out"), 5.0f, []() { const ACheeseStrikePlayerState* Host = StateOf(0, 1); return Host != nullptr && Host->IsOut(); });
	// Past the respawn delay, between rounds, nobody comes back yet.
	Wait(CheeseStrikeRules::RespawnDelay + 0.5f);
	Then([this]()
	{
		TestTrue(TEXT("the host stays dead between rounds"), Dead(0) && Character(0) == Remembered.Get());
		TestTrue(FString::Printf(TEXT("as the next round is coming (in %.1f s)"), GameState(0)->SecondsToNextRound()), GameState(0)->IsRoundBreak() && GameState(0)->SecondsToNextRound() > 0.0f);
	});
	WaitUntil(TEXT("the next round"), CheeseStrikeModes::RoundBreak + 3.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && Match->GetRound() == 2; });
	WaitUntil(TEXT("the host back"), 3.0f, []() { const ACheeseStrikeCharacter* Host = Character(0); return Host != nullptr && Host->IsAlive(); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikePlayerState* HostState = StateOf(0, 0);
		TestFalse(TEXT("the break is over"), Match->IsRoundBreak());
		TestTrue(TEXT("the host is in again"), HostState != nullptr && !HostState->IsOut());
		TestEqual(TEXT("with full health"), Host->GetHealth(), CheeseStrikeWeapons::MaxHealth);
		TestTrue(TEXT("a new character"), Host != Remembered.Get());
		TestTrue(FString::Printf(TEXT("at a start (%.0f cm from one)"), FromNearestStart(World(0), Host->GetActorLocation())), FromNearestStart(World(0), Host->GetActorLocation()) < 150.0f);
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		TestTrue(TEXT("and the joiner too, at full health"), Joiner != nullptr && Joiner->IsAlive() && Joiner->GetHealth() == CheeseStrikeWeapons::MaxHealth);
	});
	WaitUntil(TEXT("the joiner seeing round 2"), 5.0f, []() { const ACheeseStrikeGameState* Match = GameState(1); const ACheeseStrikePlayerState* Host = StateOf(0, 1); return Match != nullptr && Match->GetRound() == 2 && Host != nullptr && !Host->IsOut(); });
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Wait(CheeseStrikeRules::SpawnProtection);
	Play(1, Burst);
	WaitUntil(TEXT("the match ending"), 6.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && Match->HasMatchEnded(); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestEqual(TEXT("two rounds to the joiner's side"), Match->GetTeamScore(1), 2);
		TestEqual(TEXT("which wins"), Match->GetWinningTeam(), 1);
		TestEqual(TEXT("by name"), Match->GetWinnerName(), FString(CheeseStrikeRules::TeamNames[1]));
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreaksAreCalledInAndScoreForTheirCallers, "CheeseStrike.Play.StreaksAreCalledInAndScoreForTheirCallers", ModePlayTests::Flags)

bool FStreaksAreCalledInAndScoreForTheirCallers::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ModePlayTests;
	using CheeseStrikeStreaks::EStreak;
	StartPlaying({2, TEXT("?RespawnDelay=4")});
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Wait(CheeseStrikeRules::SpawnProtection);

	// The Unmanned Aerial Cheese: the host's own, in free-for-all.
	Then([]() { Mode()->CheeseStrikeGiveStreak(1); });
	Play(0, TEXT("tap Streak\nwait 0.3"));
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestTrue(TEXT("the drone shows the host the enemy"), Match->IsRevealedTo(StateOf(0, 0)));
		TestFalse(TEXT("but not the joiner"), Match->IsRevealedTo(StateOf(1, 0)));
		TestNotNull(TEXT("and it flies over the map"), First<ACheeseStrikeAerialCheese>(World(0)));
		TestTrue(TEXT("the streak is used"), StateOf(0, 0)->NextStreak() == EStreak::None);
		TestTrue(TEXT("everyone is told"), Match->GetAnnouncements().Num() > 0 && Match->GetAnnouncements().Last().Text.Contains(TEXT("called in an Unmanned Aerial Cheese")));
	});
	WaitUntil(TEXT("the joiner hearing of the drone"), 5.0f, []()
	{
		const ACheeseStrikeGameState* Match = GameState(1);
		return Match != nullptr && Match->GetAnnouncements().Num() > 0 && Match->IsRevealedTo(StateOf(0, 1)) && First<ACheeseStrikeAerialCheese>(World(1)) != nullptr;
	});

	// The Cheese Strike: marked on the ground just past the joiner, it kills them, as the host's kill.
	Then([]()
	{
		Mode()->CheeseStrikeGiveStreak(2);
		ShotsBefore = Character(0)->GetWeapon()->ShotsFired;
	});
	WaitUntil(TEXT("the host hearing of the strike"), 3.0f, []() { const ACheeseStrikePlayerState* Host = StateOf(0, 0); return Host != nullptr && Host->NextStreak() == EStreak::CheeseStrike; });
	Play(0, TEXT("tap Streak\nwait 0.2\nlook 0 -20\nwait 0.2\ntap Fire\nwait 0.3"));
	Then([this]()
	{
		const ACheeseStrikeBombs* Bombs = First<ACheeseStrikeBombs>(World(0));
		if (TestNotNull(TEXT("the strike is coming"), Bombs))
		{
			TestTrue(TEXT("a Cheese Strike"), Bombs->GetKind() == EStreak::CheeseStrike);
			TestEqual(TEXT("of seven wheels"), Bombs->GetDrops().Num(), CheeseStrikeStreaks::StrikeWheels);
		}
		TestFalse(TEXT("the host no longer marks"), StateOf(0, 0)->IsMarkingStrike());
		TestEqual(TEXT("and the shot marked rather than fired"), Character(0)->GetWeapon()->ShotsFired, ShotsBefore);
	});
	WaitUntil(TEXT("the strike landing on the joiner"), CheeseStrikeStreaks::StrikeWarning + 3.0f, []() { return Dead(1); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		const ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikePlayerState* HostState = StateOf(0, 0);
		TestEqual(TEXT("killed by the host"), Joiner->GetKilledBy(), Host->DisplayName());
		if (TestTrue(TEXT("in the feed"), Match->GetKillFeed().Num() > 0))
		{
			TestEqual(TEXT("as the host's"), Match->GetKillFeed().Last().Killer, Host->DisplayName());
			TestEqual(TEXT("by the Cheese Strike"), Match->GetKillFeed().Last().How, FString(TEXT("Cheese Strike")));
		}
		TestEqual(TEXT("the host has the kill"), HostState->GetKills(), 1);
		TestEqual(TEXT("which doesn't run on the streak"), HostState->GetStreak(), 0);
		TestTrue(TEXT("the host's own wheels don't hurt them"), Host->IsAlive() && Host->GetHealth() == CheeseStrikeWeapons::MaxHealth);
	});

	// The Gouda Overload, from the joiner's machine: it rains on the host, standing in the open, and kills them.
	WaitUntil(TEXT("the joiner back"), 4.0f + 4.0f, []() { const ACheeseStrikeCharacter* Joiner = Character(1); return Joiner != nullptr && Joiner->IsAlive(); });
	Then([]() { StateOf(1, 0)->GiveStreak(EStreak::GoudaOverload); });
	WaitUntil(TEXT("the joiner hearing of the Overload"), 3.0f, []() { const ACheeseStrikePlayerState* Joiner = StateOf(1, 1); return Joiner != nullptr && Joiner->NextStreak() == EStreak::GoudaOverload; });
	Play(1, TEXT("tap Streak\nwait 0.3"));
	WaitUntil(TEXT("the Overload coming"), 3.0f, []() { const ACheeseStrikeBombs* Bombs = First<ACheeseStrikeBombs>(World(0)); return Bombs != nullptr && Bombs->GetKind() == EStreak::GoudaOverload; });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestTrue(TEXT("with a warning to everyone"), Match->GetAnnouncements().Last().Text.Contains(TEXT("called in the Gouda Overload")));
		TestTrue(TEXT("and the host still standing, for now"), Character(0)->IsAlive());
	});
	WaitUntil(TEXT("the Overload killing the host"), CheeseStrikeStreaks::OverloadWarning + CheeseStrikeStreaks::OverloadRain + 2.0f, []() { return Dead(0); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		TestEqual(TEXT("the host killed by the joiner"), Character(0)->GetKilledBy(), Joiner->DisplayName());
		TestEqual(TEXT("by the Gouda Overload"), Match->GetKillFeed().Last().How, FString(TEXT("Gouda Overload")));
		const ACheeseStrikeBombs* Bombs = First<ACheeseStrikeBombs>(World(0));
		TestTrue(TEXT("its wheels landing round the host"), Bombs != nullptr && Bombs->GetLanded() > 0 && Bombs->GetHits() > 0);
	});
	StopPlaying();
	return true;
}

#endif
