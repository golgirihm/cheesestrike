// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The lobby and the menus: how the seats are dealt, the countdown's tones,
// when a match the lobby sent everyone to begins, and the options, as plain
// rules; and, played in the editor, a host and a joiner going from the
// lobby into a match on another map together.

#include "Misc/AutomationTest.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"
#include "Settings/CheeseStrikeSettings.h"
#include "UI/CheeseStrikeCurtain.h"
#include "UI/CheeseStrikeMenu.h"
#include "UI/CheeseStrikeTones.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace LobbyTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	int32 Bots(const TArray<CheeseStrikeRules::FRosterEntry>& Seats)
	{
		int32 Count = 0;
		for (const CheeseStrikeRules::FRosterEntry& Seat : Seats)
		{
			Count += Seat.bBot ? 1 : 0;
		}
		return Count;
	}

	int32 Empty(const TArray<CheeseStrikeRules::FRosterEntry>& Seats)
	{
		int32 Count = 0;
		for (const CheeseStrikeRules::FRosterEntry& Seat : Seats)
		{
			Count += Seat.bEmpty ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLobbyDealsItsSeats, "CheeseStrike.Rules.TheLobbyDealsItsSeats", LobbyTests::Flags)

bool FTheLobbyDealsItsSeats::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeRules;
	TestEqual(TEXT("a hosted match starts with seven bots"), DefaultBots, 7);
	TestEqual(TEXT("unless the URL says otherwise"), BotsFrom(TEXT("?KillLimit=5")), DefaultBots);
	TestEqual(TEXT("sixteen seats in all"), SeatCount, 16);
	TestEqual(TEXT("alone, the host may have fifteen bots, every other seat"), MostBots(1), 15);
	TestEqual(TEXT("two players leave fourteen"), MostBots(2), 14);
	TestEqual(TEXT("a full match none"), MostBots(16), 0);

	const TArray<FString> Names = BotNames(3, SeatCount);
	TArray<FRosterEntry> Host;
	Host.Add({TEXT("Host"), 0, false});
	// Alone with seven bots: four a side, the host and three on the first, four on the second, and the rest of each column empty.
	const TArray<FRosterEntry> First = WithEmptySeats(TeamColumn(0, Host, Names, DefaultBots), TeamSize, 0);
	const TArray<FRosterEntry> Second = WithEmptySeats(TeamColumn(1, Host, Names, DefaultBots), TeamSize, 1);
	TestEqual(TEXT("eight seats a side"), First.Num(), TeamSize);
	TestEqual(TEXT("on both sides"), Second.Num(), TeamSize);
	TestTrue(TEXT("the host first"), !First[0].bBot && !First[0].bEmpty && First[0].Name == TEXT("Host"));
	TestEqual(TEXT("then three bots"), LobbyTests::Bots(First), 3);
	TestEqual(TEXT("four on the other side"), LobbyTests::Bots(Second), 4);
	TestEqual(TEXT("four empty seats on the first"), LobbyTests::Empty(First), 4);
	TestEqual(TEXT("and on the second"), LobbyTests::Empty(Second), 4);
	TestTrue(TEXT("players, then bots, then empty seats"), First[1].bBot && First[3].bBot && First[4].bEmpty && First[7].bEmpty && Second[0].bBot && Second[4].bEmpty);
	TestTrue(TEXT("an empty seat is its side's"), First[4].Team == 0 && Second[4].Team == 1);

	// A joiner on the second side: seven bots, five and four with the players.
	TArray<FRosterEntry> Two = Host;
	Two.Add({TEXT("Joiner"), 1, false});
	TestEqual(TEXT("with a joiner, the first side has five"), TeamColumn(0, Two, Names, DefaultBots).Num(), 5);
	TestEqual(TEXT("the second four"), TeamColumn(1, Two, Names, DefaultBots).Num(), 4);
	TestEqual(TEXT("still eight seats a side"), WithEmptySeats(TeamColumn(1, Two, Names, DefaultBots), TeamSize, 1).Num(), TeamSize);
	// Every seat: fifteen bots, none empty.
	TestEqual(TEXT("fifteen bots fill every seat"), LobbyTests::Empty(WithEmptySeats(TeamColumn(0, Host, Names, 15), TeamSize, 0)) + LobbyTests::Empty(WithEmptySeats(TeamColumn(1, Host, Names, 15), TeamSize, 1)), 0);
	TestEqual(TEXT("and no more seats than the column has"), WithEmptySeats(TeamColumn(0, Host, Names, 15), 4, 0).Num(), 4);
	// A free-for-all's one column holds every seat.
	TestEqual(TEXT("one column of sixteen"), WithEmptySeats(Host, SeatCount, INDEX_NONE).Num(), SeatCount);

	// Each side has a colour of its own, and a free-for-all a third.
	const FLinearColor Gouda = CheeseStrikeMenu::TeamColour(0);
	const FLinearColor Mozza = CheeseStrikeMenu::TeamColour(1);
	TestTrue(TEXT("the sides' colours differ"), !Gouda.Equals(Mozza, 0.1f) && !Gouda.Equals(CheeseStrikeMenu::TeamColour(INDEX_NONE), 0.05f));
	TestTrue(TEXT("and are bright enough to read on the dark tile"), Gouda.GetLuminance() > 0.25f && Mozza.GetLuminance() > 0.25f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCountdownSoundsEachSecond, "CheeseStrike.Rules.TheCountdownSoundsEachSecond", LobbyTests::Flags)

bool FTheCountdownSoundsEachSecond::RunTest(const FString& Parameters)
{
	using CheeseStrikeMenu::CountdownTone;
	using CheeseStrikeMenu::ECountdownTone;
	// Five seconds at thirty frames a second, as this machine's clock of the server's has them, wavering back by a tenth now
	// and then: a tone as each of 5, 4, 3, 2 and 1 begins, each once, and the end's at 0, once, though the server's word
	// that the match is under way comes after.
	int32 Seconds = 0;
	int32 Ends = 0;
	bool bEndLast = false;
	int32 Sounded = INDEX_NONE;
	int32 Frame = 0;
	for (float Left = CheeseStrikeRules::MatchCountdown - 0.01f; Left > -0.5f; Left -= 1.0f / 30.0f, ++Frame)
	{
		const float Wavering = Frame % 7 == 3 ? 0.1f : 0.0f;
		const ECountdownTone Tone = CountdownTone(Sounded, true, FMath::Max(0.0f, Left + Wavering), Left < -0.3f);
		Seconds += Tone == ECountdownTone::Second ? 1 : 0;
		Ends += Tone == ECountdownTone::End ? 1 : 0;
		bEndLast = Tone == ECountdownTone::End ? true : (Tone == ECountdownTone::Second ? false : bEndLast);
	}
	TestEqual(TEXT("a tone each second, once"), Seconds, 5);
	TestEqual(TEXT("and one at the end"), Ends, 1);
	TestTrue(TEXT("after the last second's"), bEndLast);
	Sounded = INDEX_NONE;
	TestTrue(TEXT("the first second sounds as the countdown starts"), CountdownTone(Sounded, true, 4.99f, false) == ECountdownTone::Second && Sounded == 5);
	TestTrue(TEXT("nothing within a second"), CountdownTone(Sounded, true, 4.5f, false) == ECountdownTone::None);
	Sounded = INDEX_NONE;
	TestTrue(TEXT("a countdown heard of late (a clock ahead of the server's) keeps quiet for the second nearly gone"), CountdownTone(Sounded, true, 4.1f, false) == ECountdownTone::None);
	TestTrue(TEXT("and sounds the next as it begins"), CountdownTone(Sounded, true, 3.99f, false) == ECountdownTone::Second);
	TestTrue(TEXT("called off, nothing"), CountdownTone(Sounded, false, 0.0f, false) == ECountdownTone::None && Sounded == INDEX_NONE);
	Sounded = 1;
	TestTrue(TEXT("the server starting the match before this clock got to 0 sounds the end"), CountdownTone(Sounded, true, 0.4f, true) == ECountdownTone::End);
	TestTrue(TEXT("but not twice"), CountdownTone(Sounded, true, 0.0f, true) == ECountdownTone::None && CountdownTone(Sounded, false, 0.0f, true) == ECountdownTone::None);
	Sounded = INDEX_NONE;
	TestTrue(TEXT("nor with no countdown at all, as when going back to the lobby"), CountdownTone(Sounded, false, 0.0f, true) == ECountdownTone::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheTonesAreSoft, "CheeseStrike.Rules.TheTonesAreSoft", LobbyTests::Flags)

bool FTheTonesAreSoft::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeTones;
	for (ETone Tone : {ETone::Second, ETone::End})
	{
		const TCHAR* Name = Tone == ETone::End ? TEXT("the end's") : TEXT("a second's");
		const float Length = CheeseStrikeTones::Length(Tone);
		constexpr float Rate = 48000.0f;
		float Loudest = 0.0f;
		double Energy = 0.0;
		const int32 Frames = FMath::CeilToInt(Length * Rate);
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			const float Value = Sample(Tone, Frame / Rate);
			Loudest = FMath::Max(Loudest, FMath::Abs(Value));
			Energy += Value * Value;
		}
		const float Average = FMath::Sqrt(static_cast<float>(Energy / Frames));
		TestTrue(FString::Printf(TEXT("%s tone is short (%.2f s)"), Name, Length), Length > 0.1f && Length < 1.0f);
		TestTrue(FString::Printf(TEXT("%s tone is soft, never near full scale (%.2f)"), Name, Loudest), Loudest <= Peak + 0.001f && Loudest > 0.05f);
		TestTrue(FString::Printf(TEXT("%s tone is heard, not a click (%.3f)"), Name, Average), Average > 0.01f);
		TestTrue(FString::Printf(TEXT("%s tone starts from silence"), Name), FMath::Abs(Sample(Tone, 0.0f)) < 0.01f);
		TestTrue(FString::Printf(TEXT("%s tone ends in silence"), Name), FMath::Abs(Sample(Tone, Length - 1.0f / Rate)) < 0.01f && Sample(Tone, Length) == 0.0f && Sample(Tone, -0.1f) == 0.0f);
	}
	TestTrue(TEXT("the end rings longer than a second"), Length(ETone::End) > Length(ETone::Second));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAMatchBeginsWhenEveryoneIsReady, "CheeseStrike.Rules.AMatchBeginsWhenEveryoneIsReady", LobbyTests::Flags)

bool FAMatchBeginsWhenEveryoneIsReady::RunTest(const FString& Parameters)
{
	using CheeseStrikeRules::ShouldBegin;
	using CheeseStrikeRules::WaitForPlayers;
	TestFalse(TEXT("one of two ready waits"), ShouldBegin(2, 1, 2, 1.0f));
	TestFalse(TEXT("however many are in"), ShouldBegin(2, 1, 2, WaitForPlayers - 1.0f));
	TestTrue(TEXT("two of two begins"), ShouldBegin(2, 2, 2, 0.5f));
	TestTrue(TEXT("anyone who never comes is waited for so long, then not"), ShouldBegin(3, 2, 2, WaitForPlayers + 0.1f));
	TestFalse(TEXT("nobody in, nothing begins"), ShouldBegin(2, 0, 0, WaitForPlayers + 10.0f));
	TestTrue(TEXT("a joiner of the lobby's coming in late is welcome"), ShouldBegin(2, 3, 3, 0.5f));
	TestTrue(TEXT("a match waits for players only while it waits to begin"), !CheeseStrikeRules::PlayersFrom(TEXT("?Players=0")) && CheeseStrikeRules::PlayersFrom(TEXT("?Players=2")) == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOptionsOpenAndKeepToTheirRanges, "CheeseStrike.Rules.TheOptionsOpenAndKeepToTheirRanges", LobbyTests::Flags)

bool FTheOptionsOpenAndKeepToTheirRanges::RunTest(const FString& Parameters)
{
	using CheeseStrikeMenu::PageFor;
	TestTrue(TEXT("the options open from the front end"), PageFor(false, false, false, false, EMenuPage::Options) == EMenuPage::Options);
	TestTrue(TEXT("and from the pause menu, the match going on"), PageFor(true, false, true, true, EMenuPage::Options) == EMenuPage::Options);
	TestTrue(TEXT("but not over the match when it isn't paused"), PageFor(true, false, true, false, EMenuPage::Options) == EMenuPage::None);
	TestTrue(TEXT("nor in the lobby"), PageFor(false, false, true, false, EMenuPage::Options) == EMenuPage::Lobby);
	TestTrue(TEXT("a joiner following the host to another level sees the lobby, not the front end"), PageFor(false, false, true, false, EMenuPage::Find, true) == EMenuPage::Lobby);

	using namespace CheeseStrikeSettings;
	TestEqual(TEXT("the design's field of view"), DefaultFieldOfView, 75.0f);
	TestTrue(FString::Printf(TEXT("75 top to bottom is about 107 across a 16:9 screen (%.1f)"), HorizontalFieldOfView(75.0f, 16.0f / 9.0f)), FMath::IsNearlyEqual(HorizontalFieldOfView(75.0f, 16.0f / 9.0f), 107.5f, 0.5f));
	TestTrue(TEXT("and the same across a square one"), FMath::IsNearlyEqual(HorizontalFieldOfView(75.0f, 1.0f), 75.0f, 0.01f));
	TestTrue(TEXT("the usual look turns as it always has"), LookScale(DefaultSensitivity, DefaultAimedSensitivity, false, false).Equals(FVector2D(1.0, 1.0)));
	TestTrue(TEXT("twice as sensitive turns twice as far"), LookScale(2.0f, 1.0f, false, false).Equals(FVector2D(2.0, 2.0)));
	TestTrue(TEXT("aimed, by the aimed share of that"), LookScale(2.0f, 0.5f, false, true).Equals(FVector2D(1.0, 1.0)));
	TestTrue(TEXT("but not unaimed"), LookScale(2.0f, 0.5f, false, false).Equals(FVector2D(2.0, 2.0)));
	TestTrue(TEXT("inverted, up is down"), LookScale(1.0f, 1.0f, true, false).Equals(FVector2D(1.0, -1.0)));
	TestTrue(TEXT("and the sensitivity keeps to its range"), LookScale(50.0f, 1.0f, false, false).X == MaxSensitivity && LookScale(0.0f, 1.0f, false, false).X == MinSensitivity);
	TestEqual(TEXT("no frame rate limit is the first choice"), FrameRateLimits[FrameRateLimitIndex(0.0f)], 0);
	TestEqual(TEXT("a limit is found among them"), FrameRateLimits[FrameRateLimitIndex(144.0f)], 144);
	TestEqual(TEXT("or the nearest"), FrameRateLimits[FrameRateLimitIndex(59.0f)], 60);
	const TArray<FIntPoint> Supported = {{800, 600}, {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
	const TArray<FIntPoint> Offered = Resolutions(Supported, FIntPoint(2560, 1440));
	TestTrue(TEXT("the common resolutions the screen has, up to its own"), Offered == TArray<FIntPoint>({{1280, 720}, {1920, 1080}, {2560, 1440}}));
	TestTrue(TEXT("and the screen's own, whatever it is"), Resolutions(Supported, FIntPoint(1680, 1050)).Contains(FIntPoint(1680, 1050)));
	TestEqual(TEXT("the one set is found"), ResolutionIndex(Offered, FIntPoint(1920, 1080)), 1);
	TestEqual(TEXT("or the biggest below it"), ResolutionIndex(Offered, FIntPoint(2000, 1200)), 1);

	// The settings object keeps each to its range, and the engine keeps the game's.
	UCheeseStrikeSettings* Settings = NewObject<UCheeseStrikeSettings>(GetTransientPackage());
	TestEqual(TEXT("seventy-five to begin with"), Settings->GetFieldOfView(), 75.0f);
	Settings->SetFieldOfView(500.0f);
	TestEqual(TEXT("the field of view keeps to its range"), Settings->GetFieldOfView(), MaxFieldOfView);
	Settings->SetMouseSensitivity(-1.0f);
	TestEqual(TEXT("so does the sensitivity"), Settings->GetMouseSensitivity(), MinSensitivity);
	Settings->SetToDefaults();
	TestEqual(TEXT("and the defaults are the defaults"), Settings->GetFieldOfView(), DefaultFieldOfView);
	TestNotNull(TEXT("the engine keeps the game's settings (Config/DefaultEngine.ini)"), UCheeseStrikeSettings::Get());
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace LobbyTests
{
	constexpr EAutomationTestFlags PlayFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** What each player saw, frame by frame, between the lobby and the match. */
	struct FWatched
	{
		bool bSawFrontEnd[2] = {false, false};
		bool bUncoveredBeforeStart[2] = {false, false};
		bool bSaidWaiting = false;
		bool bStarted = false;
		bool bAllReadyAtStart = false;
		bool bBotsBeforeStart = false;
		int32 BotsAtStart = -1;
		int32 Seconds = 0;
		int32 Ends = 0;
	};
	FWatched Watched;

	int32 BotsIn(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ACheeseStrikeBotController> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}

	bool OnTheCreamery(int32 Player)
	{
		UWorld* Theirs = PlayTesting::World(Player);
		return Theirs != nullptr && CheeseStrikeRules::MapIndexFor(Theirs->GetOutermost()->GetName()) == 0;
	}

	UCheeseStrikeCurtainSubsystem* CurtainOf(int32 Player)
	{
		UWorld* Theirs = PlayTesting::World(Player);
		return Theirs != nullptr && Theirs->GetGameInstance() != nullptr ? Theirs->GetGameInstance()->GetSubsystem<UCheeseStrikeCurtainSubsystem>() : nullptr;
	}

	/** Looks at both players this frame; true once both are in the match on The Creamery, seeing it. */
	bool LookAtEveryone()
	{
		using namespace PlayTesting;
		bool bBothIn = true;
		for (int32 Player = 0; Player < 2; ++Player)
		{
			const ACheeseStrikePlayerController* Local = Controller(Player);
			if (Local != nullptr && (Local->GetMenuPage() == EMenuPage::Main || Local->GetMenuPage() == EMenuPage::Find))
			{
				Watched.bSawFrontEnd[Player] = true;
			}
			const UCheeseStrikeCurtainSubsystem* Curtain = CurtainOf(Player);
			const ACheeseStrikeGameState* Match = GameState(Player);
			if (OnTheCreamery(Player) && Match != nullptr && !Match->HasMatchStarted() && Curtain != nullptr)
			{
				Watched.bUncoveredBeforeStart[Player] |= !Curtain->IsUp();
				Watched.bSaidWaiting |= Curtain->GetSaying().StartsWith(TEXT("Waiting for players ("));
			}
			bBothIn &= OnTheCreamery(Player) && Match != nullptr && Match->HasMatchStarted() && Character(Player) != nullptr && Curtain != nullptr && !Curtain->IsUp();
		}
		// The server's side: nobody plays, bots included, until everyone is ready.
		if (OnTheCreamery(0) && GameState(0) != nullptr)
		{
			if (!GameState(0)->HasMatchStarted())
			{
				Watched.bBotsBeforeStart |= BotsIn(World(0)) > 0;
			}
			else if (!Watched.bStarted)
			{
				Watched.bStarted = true;
				Watched.BotsAtStart = BotsIn(World(0));
				int32 Ready = 0;
				int32 Players = 0;
				for (FConstPlayerControllerIterator It = World(0)->GetPlayerControllerIterator(); It; ++It)
				{
					const ACheeseStrikePlayerController* Each = Cast<ACheeseStrikePlayerController>(It->Get());
					Players += Each != nullptr ? 1 : 0;
					Ready += Each != nullptr && Each->HasSaidReady() ? 1 : 0;
				}
				Watched.bAllReadyAtStart = Players == 2 && Ready == 2;
			}
		}
		return bBothIn;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryoneEntersTheMatchTogether, "CheeseStrike.Play.EveryoneEntersTheMatchTogether", LobbyTests::PlayFlags)

bool FEveryoneEntersTheMatchTogether::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using LobbyTests::Watched;
	// A host and a joiner in the lobby, with the bots a hosted match starts with, and The Creamery picked.
	StartPlaying({2, TEXT("?KillLimit=50?Map=0"), false, true});
	Then([this]()
	{
		Watched = LobbyTests::FWatched();
		const ACheeseStrikeGameState* Match = GameState(0);
		if (!TestNotNull(TEXT("the match's state"), Match))
		{
			return;
		}
		TestEqual(TEXT("the lobby starts with seven bots"), CheeseStrikeRules::BotsIn(2, Match->GetBotsWanted()), CheeseStrikeRules::DefaultBots);
		TestEqual(TEXT("seven in the columns"), LobbyTests::Bots(Match->TeamColumn(0)) + LobbyTests::Bots(Match->TeamColumn(1)), CheeseStrikeRules::DefaultBots);
		TestEqual(TEXT("no bot plays in the lobby"), LobbyTests::BotsIn(World(0)), 0);
		Watched.Seconds = CheeseStrikeTones::TimesPlayed(CheeseStrikeTones::ETone::Second);
		Watched.Ends = CheeseStrikeTones::TimesPlayed(CheeseStrikeTones::ETone::End);
		World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->AskToStart(false);
	});
	WaitUntil(TEXT("everyone in the match on The Creamery, seeing it"), CheeseStrikeRules::MatchCountdown + 90.0f, &LobbyTests::LookAtEveryone);
	Then([this]()
	{
		TestFalse(TEXT("the host never sees the front end on the way"), Watched.bSawFrontEnd[0]);
		TestFalse(TEXT("nor does the joiner"), Watched.bSawFrontEnd[1]);
		TestFalse(TEXT("the host's view stays covered until the match begins"), Watched.bUncoveredBeforeStart[0]);
		TestFalse(TEXT("so does the joiner's"), Watched.bUncoveredBeforeStart[1]);
		TestTrue(TEXT("the cover says it waits for the players"), Watched.bSaidWaiting);
		TestTrue(TEXT("the match begins when both are ready"), Watched.bAllReadyAtStart);
		TestFalse(TEXT("no bot plays before"), Watched.bBotsBeforeStart);
		TestEqual(TEXT("the bots come as it begins"), Watched.BotsAtStart, CheeseStrikeRules::DefaultBots);
		// Both machines sounded the countdown: five seconds and the end, each.
		TestEqual(TEXT("each player heard a tone a second"), CheeseStrikeTones::TimesPlayed(CheeseStrikeTones::ETone::Second) - Watched.Seconds, 2 * 5);
		TestEqual(TEXT("and the end's"), CheeseStrikeTones::TimesPlayed(CheeseStrikeTones::ETone::End) - Watched.Ends, 2);
		// The bots stand still from here: what follows is the host's options, not a fight.
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			It->StopMovement();
			It->SetActorTickEnabled(false);
		}
	});
	// The view is as wide as the options say, and the look as fast.
	Then([this]()
	{
		ACheeseStrikePlayerController* Host = Controller(0);
		UCheeseStrikeSettings* Settings = UCheeseStrikeSettings::Get();
		if (Host == nullptr || Settings == nullptr || Host->PlayerCameraManager == nullptr)
		{
			AddError(TEXT("no host, settings or camera"));
			return;
		}
		FVector2D Size(16.0, 9.0);
		if (Host->GetLocalPlayer() != nullptr && Host->GetLocalPlayer()->ViewportClient != nullptr)
		{
			Host->GetLocalPlayer()->ViewportClient->GetViewportSize(Size);
		}
		const float Aspect = Size.Y > 0.0 ? static_cast<float>(Size.X / Size.Y) : 16.0f / 9.0f;
		const float Wanted = CheeseStrikeSettings::HorizontalFieldOfView(UCheeseStrikeSettings::FieldOfViewNow(), Aspect);
		TestTrue(FString::Printf(TEXT("the field of view is the settings' (%.1f, %.1f wanted)"), Host->PlayerCameraManager->GetFOVAngle(), Wanted), FMath::IsNearlyEqual(Host->PlayerCameraManager->GetFOVAngle(), Wanted, 1.0f));
		const float Sensitivity = Settings->GetMouseSensitivity();
		const bool bInverted = Settings->IsLookInverted();
		Settings->SetMouseSensitivity(2.0f);
		Settings->SetLookInverted(true);
		TestTrue(TEXT("the look turns as fast as the settings say, and up and down the other way"), Host->GetPlayScript() != nullptr || Host->LookScale().Equals(FVector2D(2.0, -2.0)));
		Settings->SetMouseSensitivity(Sensitivity);
		Settings->SetLookInverted(bInverted);
		// The options from the pause menu: the match goes on, and the character takes no input.
		Host->TogglePause();
		Host->ChooseMenu(EMenuPage::Options);
	});
	WaitUntil(TEXT("the host's options over the match"), 3.0f, []() { return Controller(0) != nullptr && Controller(0)->GetMenuPage() == EMenuPage::Options; });
	Then([this]()
	{
		TestTrue(TEXT("the match goes on"), GameState(0) != nullptr && GameState(0)->IsMatchInProgress());
		TestTrue(TEXT("paused still"), Controller(0)->IsPaused());
		TestFalse(TEXT("so the character takes no input"), Character(0) != nullptr && Character(0)->TakesInput());
		Controller(0)->MenuPressed();
	});
	WaitUntil(TEXT("Escape going back to the pause menu"), 3.0f, []() { return Controller(0) != nullptr && Controller(0)->GetMenuPage() == EMenuPage::Pause; });
	Then([this]()
	{
		TestTrue(TEXT("still paused"), Controller(0)->IsPaused());
		Controller(0)->MenuPressed();
	});
	WaitUntil(TEXT("Escape resuming the match"), 3.0f, []() { return Controller(0) != nullptr && Controller(0)->GetMenuPage() == EMenuPage::None; });
	Then([this]()
	{
		TestTrue(TEXT("and the character takes input again"), Character(0) != nullptr && Character(0)->TakesInput());
	});
	StopPlaying();
	return true;
}

#endif
