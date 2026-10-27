// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The podium after a match: without a world, who stands on it in each mode
// (as the scoreboard orders them), the scores it writes, its timing and its
// places; then every hero's emotes; then, played, the podium on a host and a
// joiner with a bot on it, its countdown back to the lobby and the host's skip.

#include "Misc/AutomationTest.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Animation/CheeseStrikeEmote.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "Rules/CheeseStrikePodiums.h"
#include "Rules/CheeseStrikeRules.h"
#include "UI/CheeseStrikePodium.h"
#include "UI/CheeseStrikePodiumHUD.h"
#include "UI/CheeseStrikePostGame.h"
#include "Tests/PlayTesting.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PodiumTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
	constexpr EAutomationTestFlags PlayFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	CheeseStrikePodiums::FContender Contender(const TCHAR* Name, int32 Kills, int32 Deaths, int32 Order, int32 Team = INDEX_NONE, int32 GunLevel = 0, int32 HillPoints = 0)
	{
		CheeseStrikePodiums::FContender Made;
		Made.Standing.Name = Name;
		Made.Standing.Kills = Kills;
		Made.Standing.Deaths = Deaths;
		Made.Standing.Order = Order;
		Made.Team = Team;
		Made.GunLevel = GunLevel;
		Made.HillPoints = HillPoints;
		return Made;
	}

	/** The names of who `Choose` puts on the podium, first first, joined by commas. */
	FString Chosen(const TArray<CheeseStrikePodiums::FContender>& Contenders, CheeseStrikeModes::EWinBy WinBy, bool bTeams, int32 WinningTeam)
	{
		TArray<FString> Names;
		for (const int32 Index : CheeseStrikePodiums::Choose(Contenders, WinBy, bTeams, WinningTeam))
		{
			Names.Add(Contenders[Index].Standing.Name);
		}
		return FString::Join(Names, TEXT(","));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePodiumRanksAsTheScoreboard, "CheeseStrike.Podium.ThePodiumRanksAsTheScoreboard", PodiumTests::Flags)

bool FThePodiumRanksAsTheScoreboard::RunTest(const FString& Parameters)
{
	using namespace PodiumTests;
	using CheeseStrikeModes::EWinBy;
	// Free-for-all (and the kills of any mode): the most kills first, then the fewest deaths, then who came first.
	const TArray<CheeseStrikePodiums::FContender> Free = {
		Contender(TEXT("Brie"), 4, 3, 0), Contender(TEXT("Feta"), 9, 1, 1), Contender(TEXT("Edam"), 4, 1, 2), Contender(TEXT("Gouda"), 4, 1, 3), Contender(TEXT("Colby"), 0, 0, 4)};
	TestEqual(TEXT("free-for-all goes by the kills, then the deaths, then the order"), Chosen(Free, EWinBy::PlayerKills, false, CheeseStrikeModes::NoTeam), FString(TEXT("Feta,Edam,Gouda")));
	TestEqual(TEXT("three at most"), CheeseStrikePodiums::Choose(Free, EWinBy::PlayerKills, false, CheeseStrikeModes::NoTeam).Num(), CheeseStrikePodiums::Places);
	TestEqual(TEXT("fewer with fewer players"), Chosen({Free[0], Free[4]}, EWinBy::PlayerKills, false, CheeseStrikeModes::NoTeam), FString(TEXT("Brie,Colby")));
	TestEqual(TEXT("and nobody with nobody"), CheeseStrikePodiums::Choose({}, EWinBy::PlayerKills, false, CheeseStrikeModes::NoTeam).Num(), 0);
	// Gun Game: the step first, whatever the kills.
	const TArray<CheeseStrikePodiums::FContender> Ladder = {
		Contender(TEXT("Brie"), 12, 0, 0, INDEX_NONE, 5), Contender(TEXT("Feta"), 3, 4, 1, INDEX_NONE, 17), Contender(TEXT("Edam"), 7, 2, 2, INDEX_NONE, 5), Contender(TEXT("Gouda"), 9, 9, 3, INDEX_NONE, 11)};
	TestEqual(TEXT("Gun Game goes by the step reached, then the kills"), Chosen(Ladder, EWinBy::Ladder, false, CheeseStrikeModes::NoTeam), FString(TEXT("Feta,Gouda,Brie")));
	TestEqual(TEXT("and writes the step, from 1"), CheeseStrikePodiums::ScoreWords(CheeseStrikePodiums::ScoreOf(Ladder[1], EWinBy::Ladder, false), EWinBy::Ladder, false, 18), FString(TEXT("STEP 18 OF 18")));
	// The solo King of the Hill: the points first.
	const TArray<CheeseStrikePodiums::FContender> Hill = {
		Contender(TEXT("Brie"), 12, 0, 0, INDEX_NONE, 0, 40), Contender(TEXT("Feta"), 1, 4, 1, INDEX_NONE, 0, 100), Contender(TEXT("Edam"), 7, 2, 2, INDEX_NONE, 0, 40)};
	TestEqual(TEXT("the solo hill goes by the points, then the kills"), Chosen(Hill, EWinBy::Hill, false, CheeseStrikeModes::NoTeam), FString(TEXT("Feta,Brie,Edam")));
	TestEqual(TEXT("and writes them"), CheeseStrikePodiums::ScoreWords(CheeseStrikePodiums::ScoreOf(Hill[1], EWinBy::Hill, false), EWinBy::Hill, false), FString(TEXT("100 POINTS")));
	// Sides: the winners' best, by their kills (as each side's part of the scoreboard is ordered), however the losers did.
	const TArray<CheeseStrikePodiums::FContender> Sides = {
		Contender(TEXT("Brie"), 20, 0, 0, 1), Contender(TEXT("Feta"), 3, 4, 1, 0), Contender(TEXT("Edam"), 7, 2, 2, 0), Contender(TEXT("Gouda"), 1, 5, 3, 0), Contender(TEXT("Colby"), 5, 1, 4, 0), Contender(TEXT("Swiss"), 2, 2, 5, 1)};
	TestEqual(TEXT("a mode of sides puts the winning side's best on it"), Chosen(Sides, EWinBy::TeamKills, true, 0), FString(TEXT("Edam,Colby,Feta")));
	TestEqual(TEXT("only theirs, fewer if they are fewer"), Chosen(Sides, EWinBy::Rounds, true, 1), FString(TEXT("Brie,Swiss")));
	TestEqual(TEXT("King of the Hill in sides goes by the kills too"), Chosen(Sides, EWinBy::Hill, true, 1), FString(TEXT("Brie,Swiss")));
	TestEqual(TEXT("with no side the winner, everyone's best"), Chosen(Sides, EWinBy::TeamKills, true, CheeseStrikeModes::NoTeam), FString(TEXT("Brie,Edam,Colby")));
	TestEqual(TEXT("kills are written as kills"), CheeseStrikePodiums::ScoreWords(CheeseStrikePodiums::ScoreOf(Sides[0], EWinBy::TeamKills, true), EWinBy::TeamKills, true), FString(TEXT("20 KILLS")));
	TestEqual(TEXT("one kill as one"), CheeseStrikePodiums::ScoreWords(1, EWinBy::PlayerKills, false), FString(TEXT("1 KILL")));
	// The scoreboard orders by the same standing.
	TestEqual(TEXT("the scoreboard's standing puts Gun Game's step first"), CheeseStrikePodiums::RankedStanding(Ladder[1], EWinBy::Ladder, false).Kills > CheeseStrikePodiums::RankedStanding(Ladder[0], EWinBy::Ladder, false).Kills, true);
	TestEqual(TEXT("and leaves the kills alone in Deathmatch"), CheeseStrikePodiums::RankedStanding(Sides[0], EWinBy::TeamKills, true).Kills, 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePodiumComesAfterTheResultsAndCountsDown, "CheeseStrike.Podium.ThePodiumComesAfterTheResultsAndCountsDown", PodiumTests::Flags)

bool FThePodiumComesAfterTheResultsAndCountsDown::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikePodiums;
	TestEqual(TEXT("the results, then the podium, then the lobby"), CheeseStrikeRules::ResultsTime, After + Seconds);
	TestTrue(TEXT("the podium lasts about fifteen seconds"), Seconds >= 12.0f && Seconds <= 18.0f);
	const float LobbyAt = 100.0f;
	TestFalse(TEXT("not during the match"), IsShowing(false, LobbyAt, 99.0f));
	TestFalse(TEXT("not while the results show"), IsShowing(true, LobbyAt, LobbyAt - Seconds - 0.5f));
	TestTrue(TEXT("then until the lobby"), IsShowing(true, LobbyAt, LobbyAt - Seconds + 0.1f) && IsShowing(true, LobbyAt, LobbyAt - 0.1f));
	TestFalse(TEXT("nor without a time for the lobby"), IsShowing(true, 0.0f, 5.0f));
	TestEqual(TEXT("its countdown from fifteen"), SecondsLeft(LobbyAt, LobbyAt - Seconds), 15);
	TestEqual(TEXT("in whole seconds, up"), SecondsLeft(LobbyAt, LobbyAt - 4.5f), 5);
	TestEqual(TEXT("to nothing just before the lobby"), SecondsLeft(LobbyAt, LobbyAt - 0.2f), 0);
	TestEqual(TEXT("its words"), CheeseStrikePodiumHUD::CountdownWords(12), FString(TEXT("BACK TO THE LOBBY IN 12")));
	TestEqual(TEXT("and none at the end"), CheeseStrikePodiumHUD::CountdownWords(0), FString());
	TestEqual(TEXT("the countdown shows whole until the last second"), CheeseStrikePodiumHUD::CountdownAlpha(1.5f), 1.0f);
	TestTrue(TEXT("then fades"), CheeseStrikePodiumHUD::CountdownAlpha(0.7f) > 0.0f && CheeseStrikePodiumHUD::CountdownAlpha(0.7f) < 1.0f);
	TestEqual(TEXT("gone before the last frame is held, a quarter second before the lobby"), CheeseStrikePodiumHUD::CountdownAlpha(0.3f), 0.0f);
	TestTrue(TEXT("a skip leaves a moment for everyone to hear of it"), SkipSeconds > 0.2f && SkipSeconds < 2.0f);
	// The places: the first in the middle and highest, the second on the left, the third on the right.
	TestEqual(TEXT("the first in the middle"), SlotOf(0), 1);
	TestEqual(TEXT("the second to its left"), SlotOf(1), 0);
	TestEqual(TEXT("the third to its right"), SlotOf(2), 2);
	TestTrue(TEXT("the first highest, then the second"), PedestalHeight(0) > PedestalHeight(1) && PedestalHeight(1) > PedestalHeight(2) && PedestalHeight(2) > 0.0f);
	// Who won, as the top says it.
	TestEqual(TEXT("a side wins"), CheeseStrikePodiumHUD::Heading(TEXT("The Gouda Guys"), true), FString(TEXT("THE GOUDA GUYS WIN")));
	TestEqual(TEXT("a player wins"), CheeseStrikePodiumHUD::Heading(TEXT("Sir Cheddar"), false), FString(TEXT("SIR CHEDDAR WINS")));
	TestEqual(TEXT("nobody"), CheeseStrikePodiumHUD::Heading(FString(), false), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryHeroHasItsOwnEmoteForEachPlace, "CheeseStrike.Podium.EveryHeroHasItsOwnEmoteForEachPlace", PodiumTests::Flags)

bool FEveryHeroHasItsOwnEmoteForEachPlace::RunTest(const FString& Parameters)
{
	for (int32 Body = 0; Body < CheeseStrikeBodies::All().Num(); ++Body)
	{
		const CheeseStrikeBodies::FBody& Look = CheeseStrikeBodies::All()[Body];
		const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Look.Mesh);
		TSet<FString> Seen;
		for (int32 Place = 0; Place < CheeseStrikePodiums::Places; ++Place)
		{
			const TCHAR* Path = CheeseStrikePodiums::Emote(Body, Place);
			const FString What = FString::Printf(TEXT("%s's emote for place %d"), Look.Name, Place + 1);
			if (!TestNotNull(*What, Path))
			{
				continue;
			}
			TestFalse(*FString::Printf(TEXT("%s is not another place's"), *What), Seen.Contains(Path));
			Seen.Add(Path);
			const UAnimSequenceBase* Emote = LoadObject<UAnimSequenceBase>(nullptr, Path);
			if (TestNotNull(*FString::Printf(TEXT("%s loads"), *What), Emote))
			{
				TestTrue(*FString::Printf(TEXT("%s is for its skeleton"), *What), Mesh != nullptr && Emote->GetSkeleton() == Mesh->GetSkeleton());
				// Not a held pose (Wraith's VictoryPose_1 is a single frame, and stood still).
				TestTrue(*FString::Printf(TEXT("%s is a movement, not a pose"), *What), Emote->GetPlayLength() > 2.0f && Emote->GetNumberOfSampledKeys() > 30);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnEmoteLoopsWithoutASeam, "CheeseStrike.Podium.AnEmoteLoopsWithoutASeam", PodiumTests::Flags)

bool FAnEmoteLoopsWithoutASeam::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeEmote;
	// Where the two players are, and what shows, at every moment of every hero's emotes (and of lengths between): the picture
	// is a blend of the two, so the timeline "First, Second, how much of the second" has to carry on across the seam
	// where a pass ends and the next begins, and never jump within one.
	TArray<float> Lengths = {0.5f, 1.0f, 2.5f, 4.0f, 7.3f, 12.0f};
	for (int32 Body = 0; Body < CheeseStrikeBodies::All().Num(); ++Body)
	{
		for (int32 Place = 0; Place < CheeseStrikePodiums::Places; ++Place)
		{
			if (const UAnimSequenceBase* Emote = LoadObject<UAnimSequenceBase>(nullptr, CheeseStrikePodiums::Emote(Body, Place)))
			{
				Lengths.Add(Emote->GetPlayLength());
			}
		}
	}
	for (const float Length : Lengths)
	{
		const float Overlap = OverlapFor(Length);
		const float Period = Length - Overlap;
		TestTrue(*FString::Printf(TEXT("a %.2f s emote is blended over %.2f s, at most %.0f%% of it"), Length, Overlap, MostOverlapShare * 100.0f), Overlap > 0.0f && Overlap <= Length * MostOverlapShare + 0.0001f);
		// The seam: just before a pass ends and just after the next begins show the same.
		const float Eps = 0.0005f;
		const FLoop Before = LoopAt(Period - Eps, Length, Overlap);
		const FLoop After = LoopAt(Period + Eps, Length, Overlap);
		const auto Shown = [](const FLoop& At) { return At.Second * At.SecondShows + At.First * (1.0f - At.SecondShows); };
		TestTrue(*FString::Printf(TEXT("%.2f s: the seam shows the same picture (%.4f to %.4f)"), Length, Shown(Before), Shown(After)), FMath::Abs(Shown(Before) - Shown(After)) < 0.01f);
		TestTrue(*FString::Printf(TEXT("%.2f s: the second player is all that shows at the seam, at the first's next start"), Length), Before.SecondShows > 0.99f && FMath::Abs(Before.Second - After.First) < 0.01f && After.SecondShows == 0.0f);
		// Neither player is ever put past either end of the sequence, and the blend stays between none and all.
		bool bInside = true;
		for (float Time = 0.0f; Time < Period * 3.0f; Time += 0.01f)
		{
			const FLoop At = LoopAt(Time, Length, Overlap);
			bInside &= At.First >= 0.0f && At.First <= Length + 0.0001f && At.Second >= 0.0f && At.Second <= Length + 0.0001f && At.SecondShows >= 0.0f && At.SecondShows <= 1.0f;
		}
		TestTrue(*FString::Printf(TEXT("%.2f s: the players stay inside the sequence"), Length), bInside);
	}
	TestEqual(TEXT("a sequence too short to blend is played plainly"), OverlapFor(0.1f), 0.0f);
	TestEqual(TEXT("round in its own length"), LoopAt(0.25f, 0.5f, 0.0f).First, 0.25f);
	return true;
}

#if WITH_EDITOR

namespace PodiumTests
{
	/** Gives `Player` (on the server) `Kills` kills. */
	void GiveKills(ACheeseStrikePlayerState* Player, int32 Kills)
	{
		for (int32 Kill = 0; Player != nullptr && Kill < Kills; ++Kill)
		{
			Player->AddKill(false);
		}
	}

	ACheeseStrikePlayerState* BotState(UWorld* World)
	{
		for (TActorIterator<ACheeseStrikeBotController> It(World); It; ++It)
		{
			return It->GetPlayerState<ACheeseStrikePlayerState>();
		}
		return nullptr;
	}

	/** The names on `Player`'s podium, first first, joined by commas; empty without one. */
	FString OnPodium(int32 Player)
	{
		const ACheeseStrikePlayerController* Local = PlayTesting::Controller(Player);
		const ACheeseStrikePodium* Podium = Local != nullptr ? Local->GetPodium() : nullptr;
		TArray<FString> Names;
		for (const FPodiumPlace& Placed : Podium != nullptr ? Podium->GetPlaces() : TArray<FPodiumPlace>())
		{
			Names.Add(Placed.Name);
		}
		return FString::Join(Names, TEXT(","));
	}

	bool BothInTheLobby()
	{
		for (int32 Player = 0; Player < 2; ++Player)
		{
			const ACheeseStrikePlayerController* Local = PlayTesting::Controller(Player);
			const ACheeseStrikeGameState* Match = PlayTesting::GameState(Player);
			if (Local == nullptr || !Local->IsLobbyReady() || Match == nullptr || Match->HasMatchStarted() || Local->GetPodium() != nullptr)
			{
				return false;
			}
		}
		return true;
	}

	/** How far each place's hero's hands and head have gone since the first sample, at most, in centimetres. */
	float Moved[3] = {0.0f, 0.0f, 0.0f};
	FVector First[3][3];

	/** Samples the joiner's podium's heroes' hands and head: the first of a round, or how far from it. */
	void SamplePoses(bool bFirst)
	{
		const ACheeseStrikePlayerController* Local = PlayTesting::Controller(1);
		const ACheeseStrikePodium* Podium = Local != nullptr ? Local->GetPodium() : nullptr;
		static const FName Bones[3] = {TEXT("hand_r"), TEXT("hand_l"), TEXT("head")};
		for (int32 Place = 0; Place < 3; ++Place)
		{
			for (int32 Bone = 0; Bone < 3; ++Bone)
			{
				const FVector Now = Podium != nullptr ? Podium->BoneOf(Place, Bones[Bone]) : FVector::ZeroVector;
				if (bFirst)
				{
					First[Place][Bone] = Now;
					Moved[Place] = 0.0f;
				}
				else
				{
					Moved[Place] = FMath::Max(Moved[Place], static_cast<float>(FVector::Dist(Now, First[Place][Bone])));
				}
			}
		}
	}

	/** What the test saw: when the match ended, what the host's names were, and whether each held a picture of their own. */
	struct FSeen
	{
		double EndedAt = 0.0;
		FString Expected;
		bool bHeld[2] = {false, false};
	};
	FSeen Seen;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePodiumShowsTheTopThreeAndGoesBackToTheLobby, "CheeseStrike.Play.ThePodiumShowsTheTopThreeAndGoesBackToTheLobby", PodiumTests::PlayFlags)

bool FThePodiumShowsTheTopThreeAndGoesBackToTheLobby::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	// A host and a joiner, and a bot, in the lobby of a free-for-all; started at once.
	StartPlaying({2, TEXT("?Mode=FreeForAll?Bots=1"), false, true});
	Then([this]() { World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->AskToStart(true); });
	WaitUntil(TEXT("the match on, with both players in"), 20.0f, []() { return GameState(0) != nullptr && GameState(0)->HasMatchStarted() && Character(0) != nullptr && Character(1) != nullptr && PodiumTests::BotState(World(0)) != nullptr; });
	Then([this]()
	{
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			It->StopMovement();
			It->SetActorTickEnabled(false);
		}
		// The joiner first, the bot second, the host third.
		ACheeseStrikePlayerState* Bot = PodiumTests::BotState(World(0));
		PodiumTests::GiveKills(Controller(0)->GetPlayerState<ACheeseStrikePlayerState>(), 1);
		ACheeseStrikePlayerState* JoinerOnHost = nullptr;
		for (APlayerState* Each : World(0)->GetGameState<ACheeseStrikeGameState>()->PlayerArray)
		{
			JoinerOnHost = Each != nullptr && !Each->IsABot() && Each != Controller(0)->PlayerState ? Cast<ACheeseStrikePlayerState>(Each) : JoinerOnHost;
		}
		PodiumTests::GiveKills(JoinerOnHost, 3);
		PodiumTests::GiveKills(Bot, 2);
		const FString Joiner = Controller(1)->PlayerState != nullptr ? Controller(1)->PlayerState->GetPlayerName() : FString();
		PodiumTests::Seen = PodiumTests::FSeen();
		PodiumTests::Seen.Expected = FString::Printf(TEXT("%s,%s,%s"), *Joiner, *Bot->GetPlayerName(), *Controller(0)->PlayerState->GetPlayerName());
		TestNotEqual(TEXT("the bot has a player id of its own"), Bot->GetPlayerId(), Controller(0)->PlayerState->GetPlayerId());
		World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->CheeseStrikeEndMatch();
		PodiumTests::Seen.EndedAt = FPlatformTime::Seconds();
	});
	Wait(CheeseStrikePodiums::After - 1.0f);
	Then([this]() { TestTrue(TEXT("the results show first, without the podium"), Controller(0)->GetPodium() == nullptr && Controller(1)->GetPodium() == nullptr); });
	WaitUntil(TEXT("the podium on both machines"), 5.0f, []() { return Controller(0)->GetPodium() != nullptr && Controller(1)->GetPodium() != nullptr && PodiumTests::OnPodium(1) == PodiumTests::Seen.Expected; });
	Wait(0.5f);
	Then([this]()
	{
		TestEqual(TEXT("the host's podium: the joiner first, the bot second, the host third"), PodiumTests::OnPodium(0), PodiumTests::Seen.Expected);
		TestEqual(TEXT("the joiner's alike"), PodiumTests::OnPodium(1), PodiumTests::Seen.Expected);
		for (int32 Player = 0; Player < 2; ++Player)
		{
			const ACheeseStrikePlayerController* Local = Controller(Player);
			const ACheeseStrikePodium* Podium = Local->GetPodium();
			const FString Who = Player == 0 ? TEXT("the host") : TEXT("the joiner");
			TestTrue(*FString::Printf(TEXT("%s sees it through its camera"), *Who), Local->GetViewTarget() == Podium);
			TestEqual(*FString::Printf(TEXT("%s's first has the most kills"), *Who), Podium->GetPlaces()[0].Score, FString(TEXT("3 KILLS")));
			TestTrue(*FString::Printf(TEXT("%s's bot is marked a bot"), *Who), Podium->GetPlaces()[1].bBot);
			TestTrue(*FString::Printf(TEXT("%s's own place is marked theirs"), *Who), Podium->GetPlaces()[Player == 0 ? 2 : 0].bMine);
			for (int32 Place = 0; Place < 3; ++Place)
			{
				TestTrue(*FString::Printf(TEXT("%s's place %d plays its emote"), *Who, Place + 1), Podium->IsEmoting(Place));
			}
		}
		TestTrue(TEXT("the host may go back sooner"), Controller(0)->CanSkipPodium());
		TestFalse(TEXT("a joiner may not"), Controller(1)->CanSkipPodium());
		const float LobbyAt = GameState(0)->GetLobbyAt();
		Controller(1)->SkipPodium();
		TestEqual(TEXT("nor does a joiner's skip do anything"), GameState(0)->GetLobbyAt(), LobbyAt);
	});
	// Every hero on every place moves: its bones somewhere else a moment later, never a held pose.
	for (int32 Round = 0; Round < CheeseStrikeBodies::All().Num(); ++Round)
	{
		Then([Round]()
		{
			const int32 Count = CheeseStrikeBodies::All().Num();
			Controller(1)->SetPodiumBodies({Round, (Round + 1) % Count, (Round + 2) % Count});
		});
		Wait(0.3f);
		Then([]() { PodiumTests::SamplePoses(true); });
		for (int32 Step = 0; Step < 4; ++Step)
		{
			Wait(0.25f);
			Then([]() { PodiumTests::SamplePoses(false); });
		}
		Then([this]()
		{
			const ACheeseStrikePodium* Podium = Controller(1)->GetPodium();
			for (int32 Place = 0; Podium != nullptr && Place < Podium->GetPlaces().Num(); ++Place)
			{
				const FString What = FString::Printf(TEXT("%s on place %d"), CheeseStrikeBodies::All()[Podium->GetPlaces()[Place].Body].Name, Place + 1);
				TestTrue(*FString::Printf(TEXT("%s plays its emote"), *What), Podium->IsEmoting(Place));
				TestTrue(*FString::Printf(TEXT("%s moves (%.1f cm)"), *What, PodiumTests::Moved[Place]), PodiumTests::Moved[Place] > 3.0f);
			}
		});
	}
	Then([]() { Controller(1)->SetPodiumBodies({}); });
	// Its countdown runs out, and everyone is back in the lobby, the podium held up until then.
	WaitUntil(TEXT("both players back in the lobby after the countdown"), CheeseStrikeRules::ResultsTime + 30.0f, []()
	{
		for (int32 Player = 0; Player < 2; ++Player)
		{
			const UCheeseStrikePostGameSubsystem* PostGame = Controller(Player) != nullptr ? Controller(Player)->GetGameInstance()->GetSubsystem<UCheeseStrikePostGameSubsystem>() : nullptr;
			PodiumTests::Seen.bHeld[Player] |= PostGame != nullptr && PostGame->IsHeld();
		}
		return PodiumTests::BothInTheLobby();
	});
	Then([this]()
	{
		TestTrue(TEXT("not before the countdown was out"), FPlatformTime::Seconds() - PodiumTests::Seen.EndedAt >= CheeseStrikeRules::ResultsTime - 0.5f);
		TestTrue(TEXT("the host's podium was held up on the way"), PodiumTests::Seen.bHeld[0]);
		TestTrue(TEXT("and the joiner's"), PodiumTests::Seen.bHeld[1]);
		// Again, in Deathmatch: the winning side's podium, and the host goes back sooner.
		Controller(0)->SetModeFromMenu(static_cast<int32>(CheeseStrikeModes::EMode::Deathmatch));
		World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->AskToStart(true);
	});
	WaitUntil(TEXT("the second match on"), 20.0f, []() { return GameState(0) != nullptr && GameState(0)->HasMatchStarted() && GameState(0)->HasTeams() && Character(0) != nullptr && Character(1) != nullptr; });
	Then([this]()
	{
		ACheeseStrikeGameState* Match = World(0)->GetGameState<ACheeseStrikeGameState>();
		const int32 HostSide = Controller(0)->GetPlayerState<ACheeseStrikePlayerState>()->GetTeam();
		Match->AddTeamScore(HostSide, 5);
		World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->CheeseStrikeEndMatch();
		PodiumTests::Seen.EndedAt = FPlatformTime::Seconds();
	});
	WaitUntil(TEXT("the second podium on both machines"), CheeseStrikePodiums::After + 5.0f, []() { return Controller(0)->GetPodium() != nullptr && Controller(1)->GetPodium() != nullptr && !PodiumTests::OnPodium(1).IsEmpty(); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const int32 Winners = Match->GetWinningTeam();
		TestEqual(TEXT("the host's side won"), Winners, Controller(0)->GetPlayerState<ACheeseStrikePlayerState>()->GetTeam());
		int32 OnTheSide = 0;
		for (const APlayerState* Each : Match->PlayerArray)
		{
			OnTheSide += Match->TeamOf(Each) == Winners ? 1 : 0;
		}
		const TArray<FPodiumPlace>& Places = Controller(0)->GetPodium()->GetPlaces();
		TestEqual(TEXT("its players on the podium, as many as there are"), Places.Num(), FMath::Min(OnTheSide, CheeseStrikePodiums::Places));
		for (const FPodiumPlace& Placed : Places)
		{
			TestEqual(*FString::Printf(TEXT("%s is on the winning side, in its colours"), *Placed.Name), Placed.Team, Winners);
		}
		TestEqual(TEXT("the joiner's podium alike"), PodiumTests::OnPodium(1), PodiumTests::OnPodium(0));
		Controller(0)->SkipPodium();
	});
	WaitUntil(TEXT("both players back in the lobby soon after the host's skip"), 20.0f, []() { return PodiumTests::BothInTheLobby(); });
	Then([this]()
	{
		TestTrue(TEXT("well before the countdown would have run out"), FPlatformTime::Seconds() - PodiumTests::Seen.EndedAt < CheeseStrikeRules::ResultsTime);
	});
	StopPlaying();
	return true;
}

#endif

#endif
