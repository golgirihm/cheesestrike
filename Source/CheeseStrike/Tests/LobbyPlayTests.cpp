// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The lobby's usual rules and the order it offers the modes in, and where the
// sides come in round by round, as plain rules; and, played in the editor, a
// lobby that keeps still while the host changes the rules, the host's Back to
// Lobby taking everyone back mid-match, and the sides swapping ends each round.

#include "Misc/AutomationTest.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeBots.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"
#include "UI/CheeseStrikeMenu.h"
#include "UI/CheeseStrikePostGame.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace LobbyPlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLobbyStartsWithTheUsualRules, "CheeseStrike.Rules.TheLobbyStartsWithTheUsualRules", LobbyPlayTests::Flags)

bool FTheLobbyStartsWithTheUsualRules::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	// Hosting from the front end: Gruyère Grotto, Deathmatch to 50, back 5 s after a death, seven bots of normal skill.
	const FString Options = CheeseStrikeRules::HostOptions();
	const int32 Map = CheeseStrikeRules::MapFrom(Options, INDEX_NONE);
	TestTrue(FString::Printf(TEXT("Gruyère Grotto (%s)"), *Options), Map >= 0 && FString(CheeseStrikeRules::Maps[Map].Level).EndsWith(TEXT("Lvl_Grotto")));
	TestEqual(TEXT("which is the usual map"), Map, CheeseStrikeRules::DefaultMap());
	TestTrue(TEXT("Deathmatch"), ModeFrom(Options) == EMode::Deathmatch);
	TestEqual(TEXT("to 50 kills"), KillLimitFor(ModeFrom(Options), Options), 50);
	TestEqual(TEXT("back 5 s after a death"), CheeseStrikeRules::RespawnDelayFrom(Options), 5.0f);
	TestEqual(TEXT("seven bots"), CheeseStrikeRules::BotsFrom(Options), 7);
	const int32 Skill = CheeseStrikeBots::SkillFrom(Options, INDEX_NONE);
	TestTrue(TEXT("of normal skill"), Skill >= 0 && FString(CheeseStrikeBots::Skill(Skill).Name) == TEXT("Normal"));

	// The modes in the lobby: every one once, the teams' first, free-for-all at the bottom.
	TSet<EMode> Seen;
	for (const EMode Each : LobbyOrder)
	{
		Seen.Add(Each);
	}
	TestEqual(TEXT("every mode is offered once"), Seen.Num(), NumModes);
	TestTrue(TEXT("Deathmatch first"), LobbyOrder[0] == EMode::Deathmatch);
	TestTrue(TEXT("free-for-all last"), LobbyOrder[NumModes - 1] == EMode::FreeForAll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheSidesSwapEndsEachRound, "CheeseStrike.Rules.TheSidesSwapEndsEachRound", LobbyPlayTests::Flags)

bool FTheSidesSwapEndsEachRound::RunTest(const FString& Parameters)
{
	using CheeseStrikeModes::StartSide;
	TestEqual(TEXT("before the first round, a side's own starts"), StartSide(0, 0, true), 0);
	TestEqual(TEXT("in the first, its own"), StartSide(1, 1, true), 1);
	TestEqual(TEXT("in the second, the other side's"), StartSide(0, 2, true), 1);
	TestEqual(TEXT("and the other side the first's"), StartSide(1, 2, true), 0);
	TestEqual(TEXT("in the third, its own again"), StartSide(0, 3, true), 0);
	TestEqual(TEXT("a mode not of rounds keeps its own"), StartSide(0, 2, false), 0);
	TestEqual(TEXT("no side keeps none"), StartSide(INDEX_NONE, 2, true), static_cast<int32>(INDEX_NONE));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace LobbyPlayTests
{
	constexpr EAutomationTestFlags PlayFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	void AllWidgets(const TSharedRef<SWidget>& Widget, TArray<TSharedRef<SWidget>>& Found)
	{
		Found.Add(Widget);
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
		{
			AllWidgets(Children->GetChildAt(Index), Found);
		}
	}

	/** Whether `Player`'s menu shows a line that reads `Text`. */
	bool Shows(int32 Player, const FString& Text)
	{
		const ACheeseStrikePlayerController* Local = PlayTesting::Controller(Player);
		const TSharedPtr<SCheeseStrikeMenu> Menu = Local != nullptr ? Local->GetMenu() : nullptr;
		if (!Menu.IsValid())
		{
			return false;
		}
		TArray<TSharedRef<SWidget>> Widgets;
		AllWidgets(Menu.ToSharedRef(), Widgets);
		for (const TSharedRef<SWidget>& Widget : Widgets)
		{
			if (Widget->GetType() == FName(TEXT("STextBlock")) && StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() == Text)
			{
				return true;
			}
		}
		return false;
	}

	/** How many of `Seats` are bots, or players. */
	int32 Count(const TArray<CheeseStrikeRules::FRosterEntry>& Seats, bool bBots)
	{
		int32 Found = 0;
		for (const CheeseStrikeRules::FRosterEntry& Seat : Seats)
		{
			Found += !Seat.bEmpty && Seat.bBot == bBots ? 1 : 0;
		}
		return Found;
	}

	int32 Builds(int32 Player)
	{
		const ACheeseStrikePlayerController* Local = PlayTesting::Controller(Player);
		return Local != nullptr && Local->GetMenu().IsValid() ? Local->GetMenu()->GetLobbyBuilds() : INDEX_NONE;
	}

	bool BothInTheLobby()
	{
		for (int32 Player = 0; Player < 2; ++Player)
		{
			const ACheeseStrikePlayerController* Local = PlayTesting::Controller(Player);
			const ACheeseStrikeGameState* Match = PlayTesting::GameState(Player);
			if (Local == nullptr || !Local->IsLobbyReady() || Match == nullptr || Match->HasMatchStarted() || Match->PlayerArray.Num() < 2)
			{
				return false;
			}
		}
		return true;
	}

	/** The pause menu's items as `Player` sees them, asking or not about Back to Lobby. */
	TArray<FString> PauseItems(int32 Player, bool bAsking)
	{
		const TSharedRef<SCheeseStrikeMenu> Menu = SNew(SCheeseStrikeMenu).Controller(PlayTesting::Controller(Player));
		Menu->Show(EMenuPage::Pause);
		if (bAsking)
		{
			Menu->ConfirmBackToLobby(true);
		}
		return Menu->GetFocusableNames();
	}

	/** Which side's starts the start nearest `At` in `World` is for: by the map's tags, or else by its halves. */
	int32 SideOfStartNear(UWorld* World, const FVector& At)
	{
		TArray<APlayerStart*> Starts;
		TArray<FVector> Spots;
		bool bTagged = false;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Starts.Add(*It);
			Spots.Add(It->GetActorLocation());
			bTagged |= It->PlayerStartTag == FName(CheeseStrikeModes::TeamStartTags[0]) || It->PlayerStartTag == FName(CheeseStrikeModes::TeamStartTags[1]);
		}
		int32 Nearest = INDEX_NONE;
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			Nearest = Nearest == INDEX_NONE || FVector::DistSquared2D(Spots[Index], At) < FVector::DistSquared2D(Spots[Nearest], At) ? Index : Nearest;
		}
		if (Nearest == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		if (bTagged)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				if (Starts[Nearest]->PlayerStartTag == FName(CheeseStrikeModes::TeamStartTags[Side]))
				{
					return Side;
				}
			}
			return INDEX_NONE;
		}
		return CheeseStrikeModes::SidesOf(Spots)[Nearest];
	}

	/** Where each player's character came in, by the side of the starts it came in at, and each player's own side: the host's world. */
	struct FCameIn
	{
		int32 Team[2] = {INDEX_NONE, INDEX_NONE};
		int32 StartSide[2] = {INDEX_NONE, INDEX_NONE};
	};
	FCameIn CameIn()
	{
		FCameIn Seen;
		for (int32 Player = 0; Player < 2; ++Player)
		{
			const ACheeseStrikeCharacter* Body = PlayTesting::CharacterOf(Player, 0);
			const ACheeseStrikePlayerState* State = Body != nullptr ? Body->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
			Seen.Team[Player] = State != nullptr ? State->GetTeam() : INDEX_NONE;
			Seen.StartSide[Player] = Body != nullptr ? SideOfStartNear(PlayTesting::World(0), Body->GetActorLocation()) : INDEX_NONE;
		}
		return Seen;
	}

	int32 BuildsBefore[2] = {0, 0};
	int32 MapPicked = INDEX_NONE;
	TWeakObjectPtr<ACheeseStrikeCharacter> RoundOneBody;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHostTakesEveryoneBackToTheLobby, "CheeseStrike.Play.TheHostTakesEveryoneBackToTheLobby", LobbyPlayTests::PlayFlags)

bool FTheHostTakesEveryoneBackToTheLobby::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace LobbyPlayTests;
	// A host and a joiner in the lobby of a Deathmatch, with a bot.
	StartPlaying({2, TEXT("?Mode=Deathmatch?Bots=1?KillLimit=50?RespawnDelay=5?BotSkill=1?Map=0"), false, true});
	WaitUntil(TEXT("both lobbies ready"), 15.0f, []() { return BothInTheLobby(); });
	Wait(1.0f);
	// The host changes the rules: neither lobby is drawn afresh, and the joiner's shows the new ones.
	Then([this]()
	{
		BuildsBefore[0] = Builds(0);
		BuildsBefore[1] = Builds(1);
		MapPicked = CheeseStrikeRules::NumMaps() - 2;
		Controller(0)->SetRulesFromMenu(30, 3.0f, MapPicked);
		Controller(0)->SetBotSkillFromMenu(2);
		Controller(0)->SetRulesFromMenu(10, 1.0f);
	});
	WaitUntil(TEXT("the joiner seeing the new rules"), 5.0f, []()
	{
		const ACheeseStrikeGameState* Match = GameState(1);
		return Match != nullptr && Match->GetMapIndex() == MapPicked && Match->GetBotSkill() == 2 && Match->GetKillLimit() == 10 && FMath::IsNearlyEqual(Match->GetRespawnDelay(), 1.0f);
	});
	Wait(0.5f);
	Then([this]()
	{
		TestEqual(TEXT("the host's lobby wasn't drawn afresh for the rules"), Builds(0), BuildsBefore[0]);
		TestEqual(TEXT("nor the joiner's"), Builds(1), BuildsBefore[1]);
		const FString MapName = FText::FromString(CheeseStrikeRules::Maps[MapPicked].Name).ToUpper().ToString();
		for (int32 Player = 0; Player < 2; ++Player)
		{
			const FString Who = Player == 0 ? TEXT("the host's") : TEXT("the joiner's");
			TestTrue(FString::Printf(TEXT("%s lobby shows the new map"), *Who), Shows(Player, MapName));
			TestTrue(FString::Printf(TEXT("%s shows the new kill limit"), *Who), Shows(Player, TEXT("10")));
			TestTrue(FString::Printf(TEXT("%s shows the new skill"), *Who), Shows(Player, TEXT("HARD")));
			TestTrue(FString::Printf(TEXT("%s shows the new wait"), *Who), Shows(Player, TEXT("1 s")));
		}
		// Back to Deathmatch's usual numbers, and the match begins on this level.
		Controller(0)->SetRulesFromMenu(50, 5.0f, 0);
		World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->AskToStart(true);
	});
	WaitUntil(TEXT("the match on, with both players in"), 20.0f, []() { return GameState(0) != nullptr && GameState(0)->HasMatchStarted() && GameState(1) != nullptr && GameState(1)->HasMatchStarted() && Character(0) != nullptr && Character(1) != nullptr; });
	Then([this]()
	{
		TestTrue(TEXT("the host may go back to the lobby"), Controller(0)->CanGoBackToLobby());
		TestFalse(TEXT("a joiner may not"), Controller(1)->CanGoBackToLobby());
		const TArray<FString> Host = PauseItems(0, false);
		TestTrue(TEXT("the host's pause menu has Back to Lobby"), Host.Contains(TEXT("BACK TO LOBBY")) && !Host.Contains(TEXT("LEAVE THE MATCH")));
		TestTrue(TEXT("and Settings"), Host.Contains(TEXT("SETTINGS")) && !Host.Contains(TEXT("OPTIONS")));
		const TArray<FString> Joiner = PauseItems(1, false);
		TestTrue(TEXT("the joiner's has Leave the Match"), Joiner.Contains(TEXT("LEAVE THE MATCH")) && !Joiner.Contains(TEXT("BACK TO LOBBY")));
		// The host's asks first, the focus on keeping on playing.
		const TSharedRef<SCheeseStrikeMenu> Asking = SNew(SCheeseStrikeMenu).Controller(Controller(0));
		Asking->Show(EMenuPage::Pause);
		Asking->ConfirmBackToLobby(true);
		TestEqual(TEXT("Back to Lobby asks first"), Asking->GetFocusableNames(), TArray<FString>({TEXT("END THE MATCH"), TEXT("KEEP PLAYING")}));
		TestEqual(TEXT("the focus on keeping on playing"), Asking->GetFocusName(), FString(TEXT("KEEP PLAYING")));
		Asking->ConfirmBackToLobby(false);
		TestTrue(TEXT("and can be thought better of"), Asking->GetFocusableNames().Contains(TEXT("BACK TO LOBBY")));
		// The host goes back: the match ends for everyone.
		Controller(0)->TogglePause();
		Controller(0)->BackToLobbyFromMenu();
	});
	WaitUntil(TEXT("host and joiner back in the lobby"), 25.0f, []() { return BothInTheLobby(); });
	Then([this]()
	{
		for (int32 Player = 0; Player < 2; ++Player)
		{
			const FString Who = Player == 0 ? TEXT("the host") : TEXT("the joiner");
			const ACheeseStrikeGameState* Match = GameState(Player);
			TestFalse(FString::Printf(TEXT("%s's match waits in the lobby again"), *Who), Match->HasMatchStarted());
			TestTrue(FString::Printf(TEXT("%s's lobby has both players and the bot"), *Who), Count(Match->Seats(), false) == 2 && Count(Match->Seats(), true) == 1);
			TestTrue(FString::Printf(TEXT("%s's is still Deathmatch"), *Who), Match->GetMode() == CheeseStrikeModes::EMode::Deathmatch);
			const UCheeseStrikePostGameSubsystem* PostGame = Controller(Player)->GetGameInstance()->GetSubsystem<UCheeseStrikePostGameSubsystem>();
			TestFalse(FString::Printf(TEXT("%s holds no results (there were none)"), *Who), PostGame != nullptr && PostGame->IsHeld());
		}
		TestFalse(TEXT("the host isn't paused"), Controller(0)->IsPaused());
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheSidesSwapStartsBetweenRounds, "CheeseStrike.Play.TheSidesSwapStartsBetweenRounds", LobbyPlayTests::PlayFlags)

bool FTheSidesSwapStartsBetweenRounds::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace LobbyPlayTests;
	// Elimination on The Creamery, whose starts are tagged for each side, a player a side.
	StartPlaying({2, TEXT("?Mode=Elimination?Rounds=3"), false, false, TEXT("/Game/CheeseStrike/Maps/Lvl_Creamery")});
	Then([this]()
	{
		const FCameIn Seen = CameIn();
		TestTrue(FString::Printf(TEXT("a player a side (%d, %d)"), Seen.Team[0], Seen.Team[1]), Seen.Team[0] != INDEX_NONE && Seen.Team[1] != INDEX_NONE && Seen.Team[0] != Seen.Team[1]);
		// (The joiner is put by the host to begin with, for the tests; the host comes in where the game puts it.)
		TestEqual(TEXT("round 1: the host at their own side's starts"), Seen.StartSide[0], Seen.Team[0]);
		RoundOneBody = Character(0);
		Character(0)->CheeseStrikeDie();
	});
	WaitUntil(TEXT("round 2, the host back"), CheeseStrikeModes::RoundBreak + 8.0f, []()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikeCharacter* Host = Character(0);
		return Match != nullptr && Match->GetRound() == 2 && Host != nullptr && Host != RoundOneBody.Get() && Host->IsAlive() && CharacterOf(1, 0) != nullptr;
	});
	Then([this]()
	{
		const FCameIn Seen = CameIn();
		for (int32 Player = 0; Player < 2; ++Player)
		{
			TestEqual(FString::Printf(TEXT("round 2: player %d at the other side's starts"), Player), Seen.StartSide[Player], 1 - Seen.Team[Player]);
		}
		RoundOneBody = Character(0);
		Character(0)->CheeseStrikeDie();
	});
	WaitUntil(TEXT("round 3, the host back"), CheeseStrikeModes::RoundBreak + 8.0f, []()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikeCharacter* Host = Character(0);
		return Match != nullptr && Match->GetRound() == 3 && Host != nullptr && Host != RoundOneBody.Get() && Host->IsAlive() && CharacterOf(1, 0) != nullptr;
	});
	Then([this]()
	{
		const FCameIn Seen = CameIn();
		for (int32 Player = 0; Player < 2; ++Player)
		{
			TestEqual(FString::Printf(TEXT("round 3: player %d at their own side's starts again"), Player), Seen.StartSide[Player], Seen.Team[Player]);
		}
	});
	StopPlaying();
	return true;
}

#endif
