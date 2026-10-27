// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The lobby and the menus: how the seats are dealt, the countdown's tones,
// when a match the lobby sent everyone to begins, and the options, as plain
// rules; and, played in the editor, a host and a joiner going from the
// lobby into a match on another map together.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeModes.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/UserInterfaceSettings.h"
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
#include "UI/CheeseStrikeMusic.h"
#include "UI/CheeseStrikeControls.h"
#include "Input/CheeseStrikeInput.h"
#include "InputAction.h"
#include "UI/CheeseStrikePostGame.h"
#include "Sessions/CheeseStrikeSessions.h"
#include "Sessions/CheeseStrikeSessionSubsystem.h"
#include "Sound/SoundWave.h"
#include "CheeseStrikePlayerState.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"

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
	TestEqual(TEXT("a hosted match starts with no bots"), DefaultBots, 0);
	TestEqual(TEXT("unless the URL says otherwise"), BotsFrom(TEXT("?KillLimit=5")), DefaultBots);
	TestEqual(TEXT("sixteen seats in all"), SeatCount, 16);
	TestEqual(TEXT("alone, the host may have fifteen bots, every other seat"), MostBots(1), 15);
	TestEqual(TEXT("two players leave fourteen"), MostBots(2), 14);
	TestEqual(TEXT("a full match none"), MostBots(16), 0);

	const TArray<FString> Names = BotNames(3, SeatCount);
	TArray<FRosterEntry> Host;
	Host.Add({TEXT("Host"), 0, false});
	// Alone with seven bots: four a side, the host and three on the first, four on the second, and the rest of each column empty.
	const TArray<FRosterEntry> First = WithEmptySeats(TeamColumn(0, Host, Names, 7), TeamSize, 0);
	const TArray<FRosterEntry> Second = WithEmptySeats(TeamColumn(1, Host, Names, 7), TeamSize, 1);
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
	TestEqual(TEXT("with a joiner, the first side has five"), TeamColumn(0, Two, Names, 7).Num(), 5);
	TestEqual(TEXT("the second four"), TeamColumn(1, Two, Names, 7).Num(), 4);
	TestEqual(TEXT("still eight seats a side"), WithEmptySeats(TeamColumn(1, Two, Names, 7), TeamSize, 1).Num(), TeamSize);
	// Every seat: fifteen bots, none empty.
	TestEqual(TEXT("fifteen bots fill every seat"), LobbyTests::Empty(WithEmptySeats(TeamColumn(0, Host, Names, 15), TeamSize, 0)) + LobbyTests::Empty(WithEmptySeats(TeamColumn(1, Host, Names, 15), TeamSize, 1)), 0);
	TestEqual(TEXT("and no more seats than the column has"), WithEmptySeats(TeamColumn(0, Host, Names, 15), 4, 0).Num(), 4);
	// A free-for-all's one column holds every seat.
	TestEqual(TEXT("one column of sixteen"), WithEmptySeats(Host, SeatCount, INDEX_NONE).Num(), SeatCount);

	// Each side has a colour of its own, and a free-for-all a third.
	const FLinearColor Gouda = CheeseStrikeModes::TeamColour(0);
	const FLinearColor Mozza = CheeseStrikeModes::TeamColour(1);
	TestTrue(TEXT("the sides' colours differ"), !Gouda.Equals(Mozza, 0.1f) && !Gouda.Equals(CheeseStrikeModes::TeamColour(INDEX_NONE), 0.05f));
	TestTrue(TEXT("and are bright enough to read on the dark tile"), Gouda.GetLuminance() > 0.25f && Mozza.GetLuminance() > 0.25f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLobbySeatsInTheOrderTheyCame, "CheeseStrike.Rules.TheLobbySeatsInTheOrderTheyCame", LobbyTests::Flags)

bool FTheLobbySeatsInTheOrderTheyCame::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeRules;
	TestEqual(TEXT("the killed are back after five seconds unless the host says otherwise"), RespawnDelay, 5.0f);
	TestEqual(TEXT("which is the default on the URL too"), RespawnDelayFrom(TEXT("?KillLimit=5")), 5.0f);

	const TArray<FString> Names = BotNames(3, SeatCount);
	TArray<FRosterEntry> Players;
	Players.Add({TEXT("Host"), 0, false});
	Players.Add({TEXT("Joiner"), 1, false});
	// Every seat in the order it was taken: the players as they joined, then the bots as they were added.
	const TArray<FRosterEntry> Three = Seats(Players, Names, 3);
	TestEqual(TEXT("two players and three bots"), Three.Num(), 5);
	TestTrue(TEXT("the players first, as they joined"), Three[0].Name == TEXT("Host") && Three[1].Name == TEXT("Joiner"));
	TestTrue(TEXT("then the bots, as they were added"), Three[2].Bot == 0 && Three[3].Bot == 1 && Three[4].Bot == 2 && Three[2].Name == Names[0] && Three[4].Name == Names[2]);
	// A bot added goes at the end, whichever side it is dealt to, and the others stay where they were.
	const TArray<FRosterEntry> Four = Seats(Players, Names, 4);
	bool bSame = Four.Num() == 6;
	for (int32 Index = 0; bSame && Index < Three.Num(); ++Index)
	{
		bSame = Four[Index].Name == Three[Index].Name && Four[Index].Team == Three[Index].Team;
	}
	TestTrue(TEXT("a new bot comes last, the rest as they were"), bSame && Four[5].Bot == 3 && Four[5].Name == Names[3]);
	TestTrue(TEXT("the sides dealt evenly, the first among equals first: the first, the second, the first"), Three[2].Team == 0 && Three[3].Team == 1 && Three[4].Team == 0);
	// A side's column keeps that order.
	const TArray<FRosterEntry> First = TeamColumn(0, Players, Names, 4);
	TestTrue(TEXT("a side's column too: its player, then its bots as they came"), First.Num() == 3 && First[0].Name == TEXT("Host") && First[1].Bot == 0 && First[2].Bot == 2);

	// Going over: a player may go to a side with fewer than eight players, the bots making way.
	TArray<FRosterEntry> Full;
	for (int32 Index = 0; Index < TeamSize; ++Index)
	{
		Full.Add({FString::Printf(TEXT("P%d"), Index), 0, false});
	}
	TestFalse(TEXT("not to a side of eight players"), HasRoomForAPlayer(Full, 0));
	TestTrue(TEXT("but to one with bots in it"), HasRoomForAPlayer(Players, 1) && HasRoomForAPlayer(Players, 0));
	TArray<FRosterEntry> Over = Players;
	Over[1].Team = 0;
	const TArray<FRosterEntry> AllBots = Seats(Over, Names, -1);
	TestEqual(TEXT("with both players on the first side, every seat still filled"), Taken(AllBots), SeatCount);
	TestEqual(TEXT("the first side's eight: the two and six bots"), Taken(AllBots, 0), TeamSize);

	// The host moves a bot: it goes over, and the others keep their sides rather than being dealt again.
	TArray<int32> Sides;
	TestTrue(TEXT("the host moves the first bot to the second side"), MoveBot(Players, Names, 3, Sides, 0));
	const TArray<FRosterEntry> Moved = Seats(Players, Names, 3, Sides);
	TestTrue(TEXT("over it goes"), Moved[2].Bot == 0 && Moved[2].Team == 1);
	TestTrue(TEXT("and the others stay where they were"), Moved[3].Team == 1 && Moved[4].Team == 0);
	TestEqual(TEXT("one side of two, the other of three"), Taken(Moved, 0) * 10 + Taken(Moved, 1), 23);
	const TArray<FRosterEntry> Added = Seats(Players, Names, 4, Sides);
	TestTrue(TEXT("a bot added afterwards is dealt to the side with fewer"), Added[5].Team == 0);
	TArray<int32> Kept = Sides;
	TArray<FRosterEntry> Alone;
	Alone.Add({TEXT("Host"), 0, false});
	TArray<int32> FullSides;
	TestFalse(TEXT("nothing moves to a side with no open seat"), MoveBot(Alone, Names, 15, FullSides, 0));
	TestTrue(TEXT("and nothing is pinned for it"), FullSides.Num() == 0);
	TestFalse(TEXT("nor a bot that isn't there"), MoveBot(Players, Names, 3, Kept, 7));
	TestTrue(TEXT("the sides ride on the URL"), BotSidesFrom(TEXT("?Bots=3?BotSides=") + BotSidesOption(Sides)) == Sides && BotSidesOption(Sides) == TEXT("110"));
	TestTrue(TEXT("and none is none"), BotSidesFrom(TEXT("?Bots=3")).Num() == 0);
	// A player who is a bot (the match's own, once in) keeps their seat and takes a bot's place.
	TArray<FRosterEntry> InMatch = Players;
	InMatch.Add({Names[0], 0, true});
	TestEqual(TEXT("bots already in count among those asked for"), Seats(InMatch, Names, 3).Num(), 5);

	// Nobody plays alone, and a match of sides needs someone on each.
	TestFalse(TEXT("the host alone can't start"), WhyNotStart(Seats(Alone, Names, 0), false).IsEmpty());
	TestTrue(TEXT("the host and a bot can"), WhyNotStart(Seats(Alone, Names, 1), false).IsEmpty());
	TestTrue(TEXT("two players can"), WhyNotStart(Seats(Players, Names, 0), false).IsEmpty());
	TestFalse(TEXT("but not as sides with one empty"), WhyNotStart(Seats(Over, Names, 0), true).IsEmpty());
	TestTrue(TEXT("which a bot fills"), WhyNotStart(Seats(Over, Names, 1), true).IsEmpty());

	// A long name, Steam's 32 letters, is written smaller in its seat.
	TestEqual(TEXT("a short name at the full size"), CheeseStrikeMenu::SeatNameSize(TEXT("Gorgon Zola")), 22);
	TestTrue(TEXT("a longer one smaller"), CheeseStrikeMenu::SeatNameSize(TEXT("Count Roquefort the Magnificent")) < 22);
	TestTrue(TEXT("and smaller still, never larger, as it grows"), CheeseStrikeMenu::SeatNameSize(FString::ChrN(CheeseStrikeMenu::LongestName, TEXT('W'))) <= CheeseStrikeMenu::SeatNameSize(FString::ChrN(24, TEXT('W'))));
	TestEqual(TEXT("Steam's longest"), CheeseStrikeMenu::LongestName, 32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAJoinerHearsWhyTheyWereLeftBehind, "CheeseStrike.Sessions.AJoinerHearsWhyTheyWereLeftBehind", LobbyTests::Flags)

bool FAJoinerHearsWhyTheyWereLeftBehind::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeSessions;
	TestTrue(TEXT("a connection that closes or times out is lost"), FarewellFor(ENetworkFailure::ConnectionLost) == EFarewell::ConnectionLost && FarewellFor(ENetworkFailure::ConnectionTimeout) == EFarewell::ConnectionLost);
	TestTrue(TEXT("one that never came is a match not joined"), FarewellFor(ENetworkFailure::PendingConnectionFailure) == EFarewell::CouldNotJoin);
	for (const EFarewell Each : {EFarewell::HostClosedTheLobby, EFarewell::HostLeftTheMatch, EFarewell::ConnectionLost, EFarewell::CouldNotJoin})
	{
		TestFalse(TEXT("each is said, with a title"), FarewellText(Each).IsEmpty() || FarewellTitle(Each).IsEmpty());
	}
	TestTrue(TEXT("and nothing is said for nothing"), FarewellText(EFarewell::None).IsEmpty());
	// The host's word stands over the lost connection that follows it.
	UCheeseStrikeSessionSubsystem* Sessions = NewObject<UCheeseStrikeSessionSubsystem>(NewObject<UGameInstance>(GetTransientPackage()));
	Sessions->SayFarewell(EFarewell::HostClosedTheLobby);
	Sessions->SayFarewell(EFarewell::ConnectionLost);
	TestTrue(TEXT("the host closed the lobby, though the connection then closed"), Sessions->GetFarewell() == EFarewell::HostClosedTheLobby);
	Sessions->DismissFarewell();
	Sessions->SayFarewell(EFarewell::ConnectionLost);
	TestTrue(TEXT("with no word, the connection was lost"), Sessions->GetFarewell() == EFarewell::ConnectionLost);
	Sessions->DismissFarewell();
	TestTrue(TEXT("until it is dismissed"), Sessions->GetFarewell() == EFarewell::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMenuHasItsMusic, "CheeseStrike.Rules.TheMenuHasItsMusic", LobbyTests::Flags)

bool FTheMenuHasItsMusic::RunTest(const FString& Parameters)
{
	using CheeseStrikeMusic::IsWanted;
	TestTrue(TEXT("the music plays under the front end"), IsWanted(EMenuPage::Main, false, false, false, false) && IsWanted(EMenuPage::Find, false, false, false, false) && IsWanted(EMenuPage::Credits, false, false, false, false));
	TestTrue(TEXT("its options"), IsWanted(EMenuPage::Options, false, false, false, false));
	TestTrue(TEXT("and the lobby"), IsWanted(EMenuPage::Lobby, false, false, false, false));
	TestFalse(TEXT("but fades out over the countdown, into the match"), IsWanted(EMenuPage::Lobby, false, true, false, false));
	TestFalse(TEXT("and is gone while the match leaves for its level"), IsWanted(EMenuPage::Lobby, false, false, true, false));
	TestFalse(TEXT("over the match"), IsWanted(EMenuPage::None, false, false, false, false));
	TestFalse(TEXT("its pause menu"), IsWanted(EMenuPage::Pause, true, false, false, false));
	TestFalse(TEXT("and the options opened from it"), IsWanted(EMenuPage::Options, true, false, false, false));
	TestFalse(TEXT("and while the results are held up on the way back"), IsWanted(EMenuPage::Lobby, false, false, false, true));
	// The songs: the six of Fab's "FREE Cyber Metal Music Pack", each a whole song of minutes.
	TestEqual(TEXT("six songs"), CheeseStrikeMusic::NumSongs(), 6);
	for (int32 Index = 0; Index < CheeseStrikeMusic::NumSongs(); ++Index)
	{
		const USoundWave* Wave = LoadObject<USoundWave>(nullptr, CheeseStrikeMusic::Songs[Index]);
		if (TestNotNull(FString::Printf(TEXT("%s is there"), CheeseStrikeMusic::Songs[Index]), Wave))
		{
			TestTrue(FString::Printf(TEXT("and is a song (%.0f s)"), Wave->Duration), Wave->Duration > 90.0f && Wave->Duration < 300.0f);
		}
	}
	// Chosen at random, one after another, never the one just played; every one can come.
	{
		using CheeseStrikeMusic::NextSong;
		TSet<int32> First;
		for (int32 Roll = 0; Roll < 6; ++Roll)
		{
			First.Add(NextSong(6, -1, Roll));
		}
		TestEqual(TEXT("any song can come first"), First.Num(), 6);
		bool bRepeats = false;
		bool bOutside = false;
		for (int32 Last = 0; Last < 6; ++Last)
		{
			TSet<int32> After;
			for (int32 Roll = 0; Roll < 5; ++Roll)
			{
				const int32 Next = NextSong(6, Last, Roll);
				bRepeats |= Next == Last;
				bOutside |= Next < 0 || Next >= 6;
				After.Add(Next);
			}
			TestEqual(FString::Printf(TEXT("after song %d, any of the other five"), Last), After.Num(), 5);
		}
		TestFalse(TEXT("never the same song twice running"), bRepeats);
		TestFalse(TEXT("and always one of the songs"), bOutside);
		TestEqual(TEXT("one song only repeats"), NextSong(1, 0, 0), 0);
	}
	// Its volume is its own, kept with the others.
	UCheeseStrikeSettings* Settings = NewObject<UCheeseStrikeSettings>(GetTransientPackage());
	Settings->SetToDefaults();
	TestEqual(TEXT("under the sounds to begin with"), Settings->GetMusicVolume(), CheeseStrikeSettings::DefaultMusicVolume);
	TestTrue(TEXT("which is quieter than they are"), CheeseStrikeSettings::DefaultMusicVolume < CheeseStrikeSettings::DefaultVolume);
	Settings->SetMusicVolume(3.0f);
	TestEqual(TEXT("and keeps to its range"), Settings->GetMusicVolume(), 1.0f);
	TestTrue(TEXT("in a class of its own, which the effects volume leaves alone"), UCheeseStrikeSettings::MusicSoundClass() != nullptr && UCheeseStrikeSettings::MusicSoundClass() != UCheeseStrikeSettings::MenuSoundClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheControlsPageSaysWhatEveryKeyDoes, "CheeseStrike.Rules.TheControlsPageSaysWhatEveryKeyDoes", LobbyTests::Flags)

bool FTheControlsPageSaysWhatEveryKeyDoes::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeControls;
	const UCheeseStrikeInputSet* Set = UCheeseStrikeInputSet::Build(GetTransientPackage());
	const TArray<FRow> Rows = CheeseStrikeControls::Rows(*Set);
	// Every action with a key is on the page, once, with every key it has.
	for (const UInputAction* Action : AllActions(*Set))
	{
		if (Set->KeysFor(Action).Num() == 0)
		{
			continue;
		}
		const int32 Showing = Rows.FilterByPredicate([Action](const FRow& Row) { return Row.Actions.Contains(Action); }).Num();
		TestEqual(FString::Printf(TEXT("%s is on the page once"), *Action->GetName()), Showing, 1);
		for (const FKey& Key : Set->KeysFor(Action))
		{
			const FRow* Row = Rows.FindByPredicate([Action](const FRow& Each) { return Each.Actions.Contains(Action); });
			const bool bShown = Row != nullptr && (Key.IsGamepadKey() ? Row->Xbox.Contains(PadLabel(Key, EPad::Xbox)) && Row->Nintendo.Contains(PadLabel(Key, EPad::Nintendo)) : Row->Keyboard.Contains(KeyboardLabel(Key)));
			TestTrue(FString::Printf(TEXT("%s's %s is shown"), *Action->GetName(), *Key.ToString()), bShown);
		}
	}
	const auto Find = [&Rows](const UInputAction* Action) { const FRow* Row = Rows.FindByPredicate([Action](const FRow& Each) { return Each.Actions.Contains(Action); }); return Row != nullptr ? *Row : FRow(); };
	TestEqual(TEXT("moving is WASD and the arrows"), Find(Set->Move).Keyboard, FString(TEXT("WASD / ↑←↓→")));
	TestEqual(TEXT("and the left stick"), Find(Set->Move).Xbox, FString(TEXT("LEFT STICK")));
	TestEqual(TEXT("looking, the mouse"), Find(Set->MouseLook).Keyboard, FString(TEXT("MOUSE")));
	TestTrue(TEXT("or the right stick, on one row"), Find(Set->MouseLook).Xbox == TEXT("RIGHT STICK") && Find(Set->StickLook).Action == Find(Set->MouseLook).Action);
	TestTrue(TEXT("jumping, the space bar, an Xbox pad's A, a Nintendo pad's B (the same button)"), Find(Set->Jump).Keyboard == TEXT("SPACE") && Find(Set->Jump).Xbox == TEXT("A") && Find(Set->Jump).Nintendo == TEXT("B"));
	TestTrue(TEXT("firing, the left button, RT or ZR"), Find(Set->Fire).Keyboard == TEXT("LMB") && Find(Set->Fire).Xbox == TEXT("RT") && Find(Set->Fire).Nintendo == TEXT("ZR"));
	TestTrue(TEXT("pausing, Escape, Menu or +"), Find(Set->Menu).Keyboard == TEXT("ESC") && Find(Set->Menu).Xbox == TEXT("MENU") && Find(Set->Menu).Nintendo == TEXT("+"));
	TestTrue(TEXT("a held key and a pressed button on one row: sprinting"), Find(Set->Sprint).Action == Find(Set->SprintToggle).Action && Find(Set->Sprint).Keyboard == TEXT("SHIFT") && Find(Set->Sprint).Xbox == TEXT("PRESS LS"));
	TestTrue(TEXT("the wheel is written as such"), Find(Set->NextWeapon).Keyboard.Contains(TEXT("WHEEL DOWN")));
	TestEqual(TEXT("a Nintendo pad's face buttons are the Xbox's swapped round"), PadLabel(EKeys::Gamepad_FaceButton_Right, EPad::Nintendo) + PadLabel(EKeys::Gamepad_FaceButton_Left, EPad::Nintendo) + PadLabel(EKeys::Gamepad_FaceButton_Top, EPad::Nintendo), FString(TEXT("AYX")));
	TestEqual(TEXT("its bumpers L and R"), PadLabel(EKeys::Gamepad_LeftShoulder, EPad::Nintendo) + PadLabel(EKeys::Gamepad_RightShoulder, EPad::Nintendo), FString(TEXT("LR")));

	// The table is the settings' first tab, which open from the front end and from the pause menu.
	using CheeseStrikeMenu::PageFor;
	TestTrue(TEXT("the settings open from the front end"), PageFor(false, false, false, false, EMenuPage::Options) == EMenuPage::Options);
	TestTrue(TEXT("and from the pause menu"), PageFor(true, false, true, true, EMenuPage::Options) == EMenuPage::Options);
	TestTrue(TEXT("not over a match not paused"), PageFor(true, false, true, false, EMenuPage::Options) == EMenuPage::None);
	TestEqual(TEXT("the controls are the settings' first tab"), static_cast<int32>(EOptionsTab::Controls), 0);

	// The space bar presses nothing in the menu (Slate would take it for Enter); Enter still does.
	const TSharedRef<SCheeseStrikeMenu> Menu = SNew(SCheeseStrikeMenu);
	const FKeyEvent Space(EKeys::SpaceBar, FModifierKeysState(), 0, false, 0, 0);
	const FKeyEvent Enter(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0);
	TestTrue(TEXT("the menu keeps the space bar from its buttons"), Menu->OnPreviewKeyDown(FGeometry(), Space).IsEventHandled() && Menu->OnKeyUp(FGeometry(), Space).IsEventHandled());
	TestFalse(TEXT("but lets Enter through to them"), Menu->OnPreviewKeyDown(FGeometry(), Enter).IsEventHandled());

	// A page that wouldn't fit is shrunk, the same both ways; one that fits is left as it is.
	TestEqual(TEXT("a page that fits isn't shrunk"), CheeseStrikeMenu::ShrinkToFit(FVector2D(1600.0, 900.0), FVector2D(1920.0, 1080.0)), 1.0f);
	TestEqual(TEXT("one too wide is, to the room's width"), CheeseStrikeMenu::ShrinkToFit(FVector2D(2000.0, 900.0), FVector2D(1600.0, 1080.0)), 0.8f);
	TestEqual(TEXT("the smallest window is the lobby's smallest screen"), CheeseStrikeMenu::MinWindowWidth * 10000 + CheeseStrikeMenu::MinWindowHeight, 1280 * 10000 + 720);

	// Full screen, a resolution picked is what the game is drawn at.
	using CheeseStrikeSettings::ScreenPercentageFor;
	TestTrue(TEXT("full screen at 1280 x 720 on a 1920 x 1080 screen, two thirds"), FMath::IsNearlyEqual(ScreenPercentageFor(FIntPoint(1280, 720), FIntPoint(1920, 1080)), 66.67f, 0.01f));
	TestEqual(TEXT("the screen's own, all of it"), ScreenPercentageFor(FIntPoint(1920, 1080), FIntPoint(1920, 1080)), 100.0f);
	TestEqual(TEXT("none picked, all of it"), ScreenPercentageFor(FIntPoint::ZeroValue, FIntPoint(1920, 1080)), 100.0f);
	TestEqual(TEXT("never more than the screen"), ScreenPercentageFor(FIntPoint(3840, 2160), FIntPoint(1920, 1080)), 100.0f);
	UCheeseStrikeSettings* Settings = NewObject<UCheeseStrikeSettings>(GetTransientPackage());
	Settings->SetFullscreenMode(EWindowMode::Fullscreen);
	Settings->SetFullscreenResolution(FIntPoint(1600, 900));
	TestEqual(TEXT("full screen, the resolution shown is the one picked"), Settings->GetResolutionShown(), FIntPoint(1600, 900));
	Settings->SetFullscreenMode(EWindowMode::Windowed);
	Settings->SetScreenResolution(FIntPoint(1280, 720));
	TestEqual(TEXT("in a window, the window's"), Settings->GetResolutionShown(), FIntPoint(1280, 720));
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
	TestTrue(TEXT("at the lowest resolution only + can be pressed"), !CanStepDown(0, Offered.Num()) && CanStepUp(0, Offered.Num()));
	TestTrue(TEXT("at the highest only -"), CanStepDown(Offered.Num() - 1, Offered.Num()) && !CanStepUp(Offered.Num() - 1, Offered.Num()));
	TestTrue(TEXT("between, both"), CanStepDown(1, Offered.Num()) && CanStepUp(1, Offered.Num()));
	TestTrue(TEXT("with one resolution, neither"), !CanStepDown(0, 1) && !CanStepUp(0, 1));

	// The settings object keeps each to its range, and the engine keeps the game's.
	// (Starting from the defaults: a settings file a previous run left must not decide what these check.)
	UCheeseStrikeSettings* Settings = NewObject<UCheeseStrikeSettings>(GetTransientPackage());
	Settings->SetToDefaults();
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

	/** Every widget under `Widget`, itself first. */
	void AllWidgets(const TSharedRef<SWidget>& Widget, TArray<TSharedRef<SWidget>>& Found)
	{
		Found.Add(Widget);
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
		{
			AllWidgets(Children->GetChildAt(Index), Found);
		}
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
		// The lobby fits the screen, the host's and the joiner's, with no scrolling: its rules are dropdowns.
		for (int32 Each = 0; Each < 2; ++Each)
		{
			const TSharedRef<SCheeseStrikeMenu> Menu = SNew(SCheeseStrikeMenu).Controller(Controller(Each));
			Menu->Show(EMenuPage::Lobby);
			Menu->SlatePrepass(1.0f);
			const FVector2D Wants = Menu->LobbyWants();
			const FString Who = Each == 0 ? TEXT("the host's") : TEXT("the joiner's");
			AddInfo(FString::Printf(TEXT("%s lobby wants %.0f x %.0f"), *Who, Wants.X, Wants.Y));
			TestTrue(FString::Printf(TEXT("%s lobby has a size (%.0f x %.0f)"), *Who, Wants.X, Wants.Y), Wants.X > 1000.0 && Wants.Y > 600.0);
			// The smallest window allowed first.
			for (const FIntPoint Screen : {FIntPoint(CheeseStrikeMenu::MinWindowWidth, CheeseStrikeMenu::MinWindowHeight), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1080)})
			{
				// The screen in the menu's own units: Slate scales the menu with the screen's shorter side (the engine's DPI curve).
				const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Screen);
				const FVector2D Room(Screen.X / Scale, Screen.Y / Scale);
				TestTrue(FString::Printf(TEXT("%s lobby (%.0f x %.0f) fits %d x %d (%.0f x %.0f)"), *Who, Wants.X, Wants.Y, Screen.X, Screen.Y, Room.X, Room.Y), Wants.X <= Room.X + 1.0 && Wants.Y <= Room.Y + 1.0);
			}
			// A window of the smallest width but taller (squarer) has less room across than the lobby wants: it is shrunk
			// to fit, not clipped, and stays readable.
			for (const FIntPoint Screen : {FIntPoint(CheeseStrikeMenu::MinWindowWidth, 960), FIntPoint(CheeseStrikeMenu::MinWindowWidth, 1024)})
			{
				const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Screen);
				const FVector2D Room(Screen.X / Scale, Screen.Y / Scale);
				const float Shrink = CheeseStrikeMenu::ShrinkToFit(Wants, Room);
				TestTrue(FString::Printf(TEXT("%s lobby is shrunk at %d x %d, by %.2f, not by much"), *Who, Screen.X, Screen.Y, Shrink), Shrink >= 0.8f && Wants.X * Shrink <= Room.X + 1.0);
			}
			TArray<TSharedRef<SWidget>> Widgets;
			LobbyTests::AllWidgets(Menu, Widgets);
			bool bScrolls = false;
			bool bRules = false;
			bool bOldNote = false;
			for (const TSharedRef<SWidget>& Widget : Widgets)
			{
				bScrolls |= Widget->GetType() == FName(TEXT("SScrollBox"));
				if (Widget->GetType() == FName(TEXT("STextBlock")))
				{
					const FString Text = StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
					bRules |= Text == TEXT("Bot skill");
					bOldNote |= Text.Contains(TEXT("sees the rules"));
				}
			}
			TestFalse(FString::Printf(TEXT("%s lobby has nothing to scroll"), *Who), bScrolls);
			TestTrue(FString::Printf(TEXT("%s lobby shows the rules"), *Who), bRules);
			TestFalse(TEXT("and doesn't say everyone sees them"), bOldNote);
		}
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
		// Paused, the game behind the menu is blurred, under the options too.
		const TSharedRef<SCheeseStrikeMenu> Menu = SNew(SCheeseStrikeMenu).Controller(Controller(0));
		Menu->Show(EMenuPage::Options);
		TestTrue(TEXT("the options opened from the pause menu blur the game"), Menu->IsBlurringTheGame());
		Menu->Show(EMenuPage::Pause);
		TestTrue(TEXT("as the pause menu does"), Menu->IsBlurringTheGame());
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
		const TSharedRef<SCheeseStrikeMenu> Menu = SNew(SCheeseStrikeMenu).Controller(Controller(0));
		Menu->Show(EMenuPage::Options);
		TestFalse(TEXT("options not opened from a pause (the front end's) leave the view as it is"), Menu->IsBlurringTheGame());
	});
	StopPlaying();
	return true;
}

namespace LobbyTests
{
	UCheeseStrikePostGameSubsystem* PostGameOf(int32 Player)
	{
		UWorld* Theirs = PlayTesting::World(Player);
		return Theirs != nullptr && Theirs->GetGameInstance() != nullptr ? Theirs->GetGameInstance()->GetSubsystem<UCheeseStrikePostGameSubsystem>() : nullptr;
	}

	UCheeseStrikeSessionSubsystem* SessionsOf(int32 Player)
	{
		UWorld* Theirs = PlayTesting::World(Player);
		return Theirs != nullptr && Theirs->GetGameInstance() != nullptr ? Theirs->GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
	}

	/** The side the server has `Player` on, or INDEX_NONE. */
	int32 SideOf(int32 Player)
	{
		const ACheeseStrikePlayerController* Local = PlayTesting::Controller(Player);
		const ACheeseStrikeGameState* Server = PlayTesting::GameState(0);
		if (Local == nullptr || Local->PlayerState == nullptr || Server == nullptr)
		{
			return INDEX_NONE;
		}
		for (const APlayerState* Each : Server->PlayerArray)
		{
			const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Each);
			if (Ours != nullptr && Ours->GetPlayerName() == Local->PlayerState->GetPlayerName())
			{
				return Ours->GetTeam();
			}
		}
		return INDEX_NONE;
	}

	/** Every text in `Menu`, joined. */
	FString TextsOf(const TSharedRef<SWidget>& Menu)
	{
		TArray<TSharedRef<SWidget>> Widgets;
		AllWidgets(Menu, Widgets);
		FString All;
		for (const TSharedRef<SWidget>& Widget : Widgets)
		{
			if (Widget->GetType() == FName(TEXT("STextBlock")))
			{
				All += StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() + TEXT("|");
			}
		}
		return All;
	}

	/** What each player saw on the way back to the lobby after the results. */
	struct FBack
	{
		bool bHeld[2] = {false, false};
		bool bSawAMadeUpLobby[2] = {false, false};
	};
	FBack Back;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLobbyKeepsItsSeatsAndTheResultsHoldUntilItIsBack, "CheeseStrike.Play.TheLobbyKeepsItsSeatsAndTheResultsHoldUntilItIsBack", LobbyTests::PlayFlags)

bool FTheLobbyKeepsItsSeatsAndTheResultsHoldUntilItIsBack::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	// A host and a joiner in the lobby of a Deathmatch with two bots.
	StartPlaying({2, TEXT("?Mode=Deathmatch?Bots=2"), false, true});
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		if (!TestNotNull(TEXT("the match's state"), Match))
		{
			return;
		}
		const TArray<CheeseStrikeRules::FRosterEntry> Seats = Match->Seats();
		TestEqual(TEXT("two players and two bots"), Seats.Num(), 4);
		TestTrue(TEXT("the host first, then the joiner, in the order they came"), Seats.Num() == 4 && Seats[0].Name == Match->GetHostName() && !Seats[1].bBot && Seats[1].Name != Match->GetHostName());
		TestTrue(TEXT("then the bots, in the order they were added"), Seats.Num() == 4 && Seats[2].Bot == 0 && Seats[3].Bot == 1);
		TestEqual(TEXT("the joiner on the second side"), LobbyTests::SideOf(1), 1);
		// The lobby's words: the host closes it, a joiner leaves it; either may go over to the other side.
		const TSharedRef<SCheeseStrikeMenu> Hosts = SNew(SCheeseStrikeMenu).Controller(Controller(0));
		Hosts->Show(EMenuPage::Lobby);
		const FString HostSees = LobbyTests::TextsOf(Hosts);
		TestTrue(TEXT("the host's Leave closes the lobby"), HostSees.Contains(TEXT("CLOSE LOBBY|")));
		TestTrue(TEXT("and the host may join the other side"), HostSees.Contains(TEXT("JOIN MOZZA RADICALS|")));
		TestTrue(TEXT("and start the match, there being four to play"), HostSees.Contains(TEXT("START THE MATCH|")) && !HostSees.Contains(TEXT("ADD A BOT")));
		Controller(1)->SwitchSideFromMenu();
	});
	WaitUntil(TEXT("the joiner going over to the first side"), 5.0f, []() { return LobbyTests::SideOf(1) == 0; });
	WaitUntil(TEXT("the joiner seeing it"), 5.0f, []() { return GameState(1) != nullptr && GameState(1)->TeamColumn(0).Num() == 2 && GameState(1)->TeamColumn(0)[1].Name == GameState(0)->TeamColumn(0)[1].Name; });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const TArray<CheeseStrikeRules::FRosterEntry> First = Match->TeamColumn(0);
		const TArray<CheeseStrikeRules::FRosterEntry> Second = Match->TeamColumn(1);
		TestTrue(TEXT("the first side is the two players"), First.Num() == 2 && !First[0].bBot && !First[1].bBot);
		TestTrue(TEXT("and the bots make the second"), Second.Num() == 2 && Second[0].bBot && Second[1].bBot);
		const TSharedRef<SCheeseStrikeMenu> Joiners = SNew(SCheeseStrikeMenu).Controller(Controller(1));
		Joiners->Show(EMenuPage::Lobby);
		const FString JoinerSees = LobbyTests::TextsOf(Joiners);
		TestTrue(TEXT("a joiner leaves the lobby"), JoinerSees.Contains(TEXT("LEAVE|")) && !JoinerSees.Contains(TEXT("CLOSE LOBBY")));
		TestTrue(TEXT("and may go back"), JoinerSees.Contains(TEXT("JOIN MOZZA RADICALS|")));
		// The host moves the second bot over to the first side.
		Controller(0)->MoveBotFromMenu(1);
		TestTrue(TEXT("the host moves a bot to the first side"), Match->TeamColumn(0).Num() == 3 && Match->TeamColumn(0)[2].Bot == 1);
		TestTrue(TEXT("and the other bot stays"), Match->TeamColumn(1).Num() == 1 && Match->TeamColumn(1)[0].Bot == 0);
		// Once the countdown starts, nothing in the lobby changes.
		ACheeseStrikeGameMode* Mode = World(0)->GetAuthGameMode<ACheeseStrikeGameMode>();
		Mode->AskToStart(false);
		TestTrue(TEXT("the countdown runs"), Match->IsCountingDown());
		Controller(0)->SetBotsFromMenu(5);
		Controller(0)->SetRulesFromMenu(5, 1.0f);
		Controller(0)->MoveBotFromMenu(0);
		TestEqual(TEXT("the bots can't be changed during it"), Mode->GetBotsWanted(), 2);
		TestTrue(TEXT("nor the rules"), Mode->GetKillLimit() != 5);
		TestEqual(TEXT("nor the bots' sides"), Match->TeamColumn(1).Num(), 1);
		const TSharedRef<SCheeseStrikeMenu> Counting = SNew(SCheeseStrikeMenu).Controller(Controller(0));
		Counting->Show(EMenuPage::Lobby);
		TestFalse(TEXT("the host's lobby offers no dropdown to change them"), Counting->GetFocusableNames().ContainsByPredicate([](const FString& Name) { return Name.StartsWith(TEXT("Dropdown")); }));
		TestFalse(TEXT("nor another side"), LobbyTests::TextsOf(Counting).Contains(TEXT("JOIN ")));
		Controller(1)->SwitchSideFromMenu();
	});
	Wait(1.0f);
	Then([this]()
	{
		TestEqual(TEXT("nor may a joiner go over"), LobbyTests::SideOf(1), 0);
		World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->CancelStart();
		// The match on this level, at once, with the sides as the lobby has them.
		World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->AskToStart(true);
	});
	WaitUntil(TEXT("the match on, with both players in"), 20.0f, []() { return GameState(0) != nullptr && GameState(0)->HasMatchStarted() && Character(0) != nullptr && Character(1) != nullptr; });
	Then([this]()
	{
		TestEqual(TEXT("the joiner plays on the side they chose"), LobbyTests::SideOf(1), 0);
		const FString Moved = GameState(0)->GetBotNames()[1];
		int32 MovedSide = INDEX_NONE;
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			const ACheeseStrikePlayerState* Bot = It->GetPlayerState<ACheeseStrikePlayerState>();
			MovedSide = Bot != nullptr && Bot->GetPlayerName() == Moved ? Bot->GetTeam() : MovedSide;
			It->StopMovement();
			It->SetActorTickEnabled(false);
		}
		TestEqual(TEXT("and the bot the host moved on the side it was moved to"), MovedSide, 0);
		LobbyTests::Back = LobbyTests::FBack();
		World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->EndMatch();
	});
	// After the results, back to the lobby: the results stay up until each player's lobby is there, never a lobby made up.
	WaitUntil(TEXT("both players back in a lobby ready to play in"), CheeseStrikeRules::ResultsTime + 30.0f, []()
	{
		bool bBoth = true;
		for (int32 Player = 0; Player < 2; ++Player)
		{
			const ACheeseStrikePlayerController* Local = Controller(Player);
			const UCheeseStrikePostGameSubsystem* PostGame = LobbyTests::PostGameOf(Player);
			const bool bHeld = PostGame != nullptr && PostGame->IsHeld();
			LobbyTests::Back.bHeld[Player] |= bHeld;
			if (Local != nullptr && Local->GetMenuPage() == EMenuPage::Lobby && !Local->IsLobbyReady() && !bHeld)
			{
				LobbyTests::Back.bSawAMadeUpLobby[Player] = true;
			}
			const ACheeseStrikeGameState* Match = GameState(Player);
			bBoth &= Local != nullptr && Local->IsLobbyReady() && !bHeld && Match != nullptr && !Match->HasMatchStarted() && Match->PlayerArray.Num() == 2;
		}
		return bBoth;
	});
	Then([this]()
	{
		for (int32 Player = 0; Player < 2; ++Player)
		{
			const FString Who = Player == 0 ? TEXT("the host") : TEXT("the joiner");
			TestTrue(FString::Printf(TEXT("%s's results were held up on the way back"), *Who), LobbyTests::Back.bHeld[Player]);
			TestFalse(FString::Printf(TEXT("%s never saw a lobby still being made"), *Who), LobbyTests::Back.bSawAMadeUpLobby[Player]);
		}
		const ACheeseStrikeGameState* Match = GameState(0);
		const TArray<CheeseStrikeRules::FRosterEntry> Seats = Match->Seats();
		TestTrue(TEXT("back in the lobby, the host is first and the joiner second, as they came"), Seats.Num() == 4 && Seats[0].Name == Match->GetHostName() && !Seats[1].bBot);
		TestEqual(TEXT("the joiner is still on the side they chose"), LobbyTests::SideOf(1), 0);
		TestTrue(TEXT("and the bots on theirs"), Match->TeamColumn(0).Num() == 3 && Match->TeamColumn(1).Num() == 1);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAJoinerIsToldWhenTheHostCloses, "CheeseStrike.Play.AJoinerIsToldWhenTheHostCloses", LobbyTests::PlayFlags)

bool FAJoinerIsToldWhenTheHostCloses::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	// The engine logs the joiner's lost connection as an error.
	AddExpectedError(TEXT("BroadcastNetworkFailure"), EAutomationExpectedErrorFlags::Contains, 0);
	StartPlaying({2, TEXT("?Bots=0"), false, true});
	Then([this]()
	{
		// Alone with nobody else, the host couldn't start; with a joiner, they can.
		TestTrue(TEXT("a host and a joiner may start"), World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->WhyNotStart().IsEmpty());
		Controller(0)->LeaveFromMenu();
	});
	WaitUntil(TEXT("the joiner back on the front end"), 20.0f, []()
	{
		const ACheeseStrikePlayerController* Joiner = Controller(1);
		return Joiner != nullptr && Joiner->GetWorld()->GetNetMode() == NM_Standalone && Joiner->GetMenuPage() == EMenuPage::Main;
	});
	Then([this]()
	{
		const UCheeseStrikeSessionSubsystem* Sessions = LobbyTests::SessionsOf(1);
		TestTrue(TEXT("told the host closed the lobby"), Sessions != nullptr && Sessions->GetFarewell() == CheeseStrikeSessions::EFarewell::HostClosedTheLobby);
		const TSharedRef<SCheeseStrikeMenu> Menu = SNew(SCheeseStrikeMenu).Controller(Controller(1));
		Menu->Show(EMenuPage::Main);
		TestTrue(TEXT("which the front end says"), LobbyTests::TextsOf(Menu).Contains(CheeseStrikeSessions::FarewellText(CheeseStrikeSessions::EFarewell::HostClosedTheLobby)));
		const UCheeseStrikeSessionSubsystem* Hosts = LobbyTests::SessionsOf(0);
		TestTrue(TEXT("and the host is told nothing"), Hosts != nullptr && Hosts->GetFarewell() == CheeseStrikeSessions::EFarewell::None);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAJoinerIsToldWhenTheConnectionIsLost, "CheeseStrike.Play.AJoinerIsToldWhenTheConnectionIsLost", LobbyTests::PlayFlags)

bool FAJoinerIsToldWhenTheConnectionIsLost::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	AddExpectedError(TEXT("BroadcastNetworkFailure"), EAutomationExpectedErrorFlags::Contains, 0);
	StartPlaying({2, TEXT("?Bots=0"), false, true});
	Then([this]()
	{
		// The host's end goes with no word: the connection simply closes.
		UNetDriver* Driver = World(0)->GetNetDriver();
		if (TestTrue(TEXT("the host has the joiner's connection"), Driver != nullptr && Driver->ClientConnections.Num() == 1))
		{
			Driver->ClientConnections[0]->Close();
		}
	});
	WaitUntil(TEXT("the joiner back on the front end"), 30.0f, []()
	{
		const ACheeseStrikePlayerController* Joiner = Controller(1);
		return Joiner != nullptr && Joiner->GetWorld()->GetNetMode() == NM_Standalone && Joiner->GetMenuPage() == EMenuPage::Main;
	});
	Then([this]()
	{
		const UCheeseStrikeSessionSubsystem* Sessions = LobbyTests::SessionsOf(1);
		TestTrue(TEXT("told the connection was lost"), Sessions != nullptr && Sessions->GetFarewell() == CheeseStrikeSessions::EFarewell::ConnectionLost);
		const TSharedRef<SCheeseStrikeMenu> Menu = SNew(SCheeseStrikeMenu).Controller(Controller(1));
		Menu->Show(EMenuPage::Main);
		TestTrue(TEXT("which the front end says"), LobbyTests::TextsOf(Menu).Contains(CheeseStrikeSessions::FarewellText(CheeseStrikeSessions::EFarewell::ConnectionLost)));
		// The host, alone now with no bots, can't start the match from the lobby; a script or a test still can, at once.
		ACheeseStrikeGameMode* Mode = World(0)->GetAuthGameMode<ACheeseStrikeGameMode>();
		TestFalse(TEXT("the host alone has nobody to play"), Mode->WhyNotStart().IsEmpty());
		const TSharedRef<SCheeseStrikeMenu> Hosts = SNew(SCheeseStrikeMenu).Controller(Controller(0));
		Hosts->Show(EMenuPage::Lobby);
		TestTrue(TEXT("and the start button says so"), LobbyTests::TextsOf(Hosts).Contains(TEXT("ADD A BOT OR WAIT FOR A PLAYER")));
		Mode->AskToStart(false);
		TestFalse(TEXT("so the countdown doesn't start"), GameState(0)->IsCountingDown());
		Mode->AskToStart(true);
	});
	WaitUntil(TEXT("the match started at once all the same"), 10.0f, []() { return GameState(0) != nullptr && GameState(0)->HasMatchStarted(); });
	StopPlaying();
	return true;
}

#endif
