// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The loadout (Rules/CheeseStrikeLoadout.h): what is chosen and what to begin
// with, which modes have one, when it may be changed, the longer count-in it
// brings and the page it puts up; then a play: a joiner picks one in the
// lobby and comes in with it, one picked during the count-in is in hand when
// the count ends, and one picked mid-match is refused.

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Animation/CheeseStrikeBodies.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeLoadout.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeStart.h"
#include "UI/CheeseStrikeMenu.h"
#include "UI/CheeseStrikeMusic.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeGrenades.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLoadoutIsPickedBeforePlayAndLengthensTheCount, "CheeseStrike.Rules.TheLoadoutIsPickedBeforePlayAndLengthensTheCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTheLoadoutIsPickedBeforePlayAndLengthensTheCount::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeLoadout;
	using CheeseStrikeGrenades::EGrenadeKind;
	// The choices: the three weakest primaries, two kinds of grenade, the four heroes; and what a fresh install plays with.
	TestEqual(TEXT("the primaries offered are the three weakest"), Primaries(), TArray<FName>({CheeseStrikeWeapons::Carbine().Id, CheeseStrikeWeapons::Blaster().Id, CheeseStrikeWeapons::Munster().Id}));
	TestTrue(TEXT("each is offered"), IsPrimaryOffered(CheeseStrikeWeapons::Munster().Id) && IsPrimaryOffered(TEXT("blaster")));
	TestFalse(TEXT("a stand's weapon isn't"), IsPrimaryOffered(CheeseStrikeWeapons::Shotgun().Id) || IsPrimaryOffered(CheeseStrikeWeapons::Roquefort().Id));
	TestFalse(TEXT("nor the sidearm, the knife or a streak's"), IsPrimaryOffered(CheeseStrikeWeapons::Pistol().Id) || IsPrimaryOffered(CheeseStrikeWeapons::Knife().Id) || IsPrimaryOffered(CheeseStrikeWeapons::Gatling().Id));
	TestEqual(TEXT("the Carbine to begin with"), DefaultPrimary(), CheeseStrikeWeapons::Carbine().Id);
	TestEqual(TEXT("as the arsenal's own default"), DefaultPrimary(), CheeseStrikeArsenal::DefaultPrimary());
	TestTrue(TEXT("the cheese grenade"), DefaultGrenade == EGrenadeKind::Cheese);
	TestEqual(TEXT("and Lt. Belica"), DefaultHero(), CheeseStrikeBodies::DefaultHero);
	TestEqual(TEXT("a primary offered is kept"), PrimaryOrDefault(CheeseStrikeWeapons::Blaster().Id), CheeseStrikeWeapons::Blaster().Id);
	TestEqual(TEXT("any other is the default"), PrimaryOrDefault(CheeseStrikeWeapons::Shotgun().Id), DefaultPrimary());
	TestEqual(TEXT("none too"), PrimaryOrDefault(NAME_None), DefaultPrimary());
	TestEqual(TEXT("two kinds of grenade"), CheeseStrikeGrenades::NumKinds, 2);

	// Every mode but Gun Game, whose ladder gives the weapons.
	for (const CheeseStrikeModes::FModeInfo& Mode : CheeseStrikeModes::Modes)
	{
		TestEqual(FString::Printf(TEXT("%s has a loadout unless its ladder arms everyone"), Mode.Name), HasLoadout(Mode.Mode), Mode.WinBy != CheeseStrikeModes::EWinBy::Ladder);
	}
	TestFalse(TEXT("Gun Game has none"), HasLoadout(CheeseStrikeModes::EMode::GunGame));
	TestTrue(TEXT("Deathmatch has"), HasLoadout(CheeseStrikeModes::EMode::Deathmatch));

	// When it may be changed: on the front end, in the lobby and during a count-in; never once play is on, paused or not.
	TestTrue(TEXT("on the front end"), MayChange(EWhen::FrontEnd));
	TestTrue(TEXT("in the lobby"), MayChange(EWhen::Lobby));
	TestTrue(TEXT("during a count-in"), MayChange(EWhen::CountIn));
	TestFalse(TEXT("not while play is on"), MayChange(EWhen::Playing));
	TestFalse(TEXT("nor once it is over"), MayChange(EWhen::Over));
	TestTrue(TEXT("alone on a level is the front end"), WhenFor(false, true, false, false) == EWhen::FrontEnd && WhenFor(false, false, false, false) == EWhen::FrontEnd);
	TestTrue(TEXT("a match that waits is the lobby"), WhenFor(true, false, false, false) == EWhen::Lobby);
	TestTrue(TEXT("one started and held is a count-in"), WhenFor(true, true, false, true) == EWhen::CountIn);
	TestTrue(TEXT("one started and free is play"), WhenFor(true, true, false, false) == EWhen::Playing);
	TestTrue(TEXT("one over is over, held or not"), WhenFor(true, true, true, true) == EWhen::Over && WhenFor(true, true, true, false) == EWhen::Over);
	TestFalse(TEXT("so the pause menu changes nothing"), MayChange(WhenFor(true, true, false, false)));

	// The page comes up of itself as a count-in begins in a mode with a loadout, and stays away once put away for that count.
	TestTrue(TEXT("a count-in in a mode with a loadout offers the page"), OffersPage(true, true, 103.0f, 0.0f));
	TestFalse(TEXT("not in Gun Game"), OffersPage(false, true, 103.0f, 0.0f));
	TestFalse(TEXT("not without a count"), OffersPage(true, false, 103.0f, 0.0f) || OffersPage(true, true, 0.0f, 0.0f));
	TestFalse(TEXT("not once put away for this count"), OffersPage(true, true, 103.0f, 103.0f));
	TestTrue(TEXT("but again for the next"), OffersPage(true, true, 250.0f, 103.0f));
	TestEqual(TEXT("it says the seconds to the match"), CountText(false, 7.2f), FString(TEXT("THE MATCH BEGINS IN 8")));
	TestEqual(TEXT("or to the round"), CountText(true, 4.0f), FString(TEXT("THE ROUND BEGINS IN 4")));
	TestEqual(TEXT("never under one"), CountText(false, 0.0f), FString(TEXT("THE MATCH BEGINS IN 1")));

	// The count-in is longer where a loadout can be chosen: 8 s before a match and 5 before a round; Gun Game keeps 3 and 2.
	using namespace CheeseStrikeStart;
	TestEqual(TEXT("a mode with a loadout counts eight seconds"), CountdownFrom(TEXT("?Mode=Deathmatch")), LoadoutCountdown);
	TestEqual(TEXT("eight"), LoadoutCountdown, 8.0f);
	TestEqual(TEXT("free-for-all too (the URL's default mode)"), CountdownFrom(TEXT("?KillLimit=20")), LoadoutCountdown);
	TestEqual(TEXT("Gun Game three"), CountdownFrom(TEXT("?Mode=GunGame")), Countdown);
	TestEqual(TEXT("the URL still overrides"), CountdownFrom(TEXT("?Mode=Deathmatch?Countdown=0")), 0.0f);
	TestEqual(TEXT("a later round counts five with a loadout"), RoundCountdownFor(LoadoutCountdown, true), LoadoutRoundCountdown);
	TestEqual(TEXT("five"), LoadoutRoundCountdown, 5.0f);
	TestEqual(TEXT("two without"), RoundCountdownFor(Countdown, false), RoundCountdown);
	TestEqual(TEXT("none if the match has none"), RoundCountdownFor(0.0f, true), 0.0f);
	TestTrue(TEXT("the HUD's glyphs come only in the last three seconds"), Shown(100.0f, 108.0f).Text.IsEmpty() && Shown(104.9f, 108.0f).Text.IsEmpty() && Shown(105.1f, 108.0f).Text == TEXT("3"));

	// The menu: the page from the front end, over the lobby, and of itself over a match's count-in (unless paused); no music over the count-in's.
	using CheeseStrikeMenu::PageFor;
	using CheeseStrikeMenu::LobbyOverlay;
	TestTrue(TEXT("the loadout page is the front end's"), PageFor(false, false, false, false, EMenuPage::Loadout) == EMenuPage::Loadout);
	TestTrue(TEXT("over the lobby, which stays the page shown"), PageFor(false, false, true, false, EMenuPage::Loadout) == EMenuPage::Lobby && LobbyOverlay(EMenuPage::Lobby, EMenuPage::Loadout) == EMenuPage::Loadout);
	TestTrue(TEXT("as the settings are"), LobbyOverlay(EMenuPage::Lobby, EMenuPage::Options) == EMenuPage::Options && CheeseStrikeMenu::SettingsOverLobby(EMenuPage::Lobby, EMenuPage::Options));
	TestTrue(TEXT("and nothing else"), LobbyOverlay(EMenuPage::Lobby, EMenuPage::Main) == EMenuPage::None && LobbyOverlay(EMenuPage::Main, EMenuPage::Loadout) == EMenuPage::None);
	TestTrue(TEXT("Escape in the loadout over the lobby goes back to the lobby's menu, and from that to the lobby"), CheeseStrikeMenu::LobbyMenuKey(EMenuPage::Loadout) == EMenuPage::LobbyMenu && CheeseStrikeMenu::LobbyMenuKey(EMenuPage::LobbyMenu) == EMenuPage::Main);
	TestTrue(TEXT("a count-in that offers it shows it over the match"), PageFor(true, false, true, false, EMenuPage::Main, false, false, true) == EMenuPage::Loadout);
	TestTrue(TEXT("not when paused"), PageFor(true, false, true, true, EMenuPage::Main, false, false, true) == EMenuPage::Pause);
	TestTrue(TEXT("nor once put away"), PageFor(true, false, true, false, EMenuPage::Main, false, false, false) == EMenuPage::None);
	TestTrue(TEXT("the music plays under the front end's loadout page"), CheeseStrikeMusic::IsWanted(EMenuPage::Loadout, false, false, false, false, false));
	TestFalse(TEXT("but not under a count-in's"), CheeseStrikeMusic::IsWanted(EMenuPage::Loadout, false, false, false, false, true));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace LoadoutTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** Player `Of`'s state, as the server has it. */
	ACheeseStrikePlayerState* StateOf(int32 Of)
	{
		const ACheeseStrikeCharacter* Body = PlayTesting::CharacterOf(Of, 0);
		return Body != nullptr ? Body->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	}

	FName HeldOf(int32 Of)
	{
		const ACheeseStrikeCharacter* Body = PlayTesting::CharacterOf(Of, 0);
		return Body != nullptr && Body->GetWeapon() != nullptr ? Body->GetWeapon()->GetWeaponId() : NAME_None;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FALoadoutPickedInTheLobbyOrTheCountInIsCarried, "CheeseStrike.Play.ALoadoutPickedInTheLobbyOrTheCountInIsCarried", LoadoutTests::Flags)

bool FALoadoutPickedInTheLobbyOrTheCountInIsCarried::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace LoadoutTests;
	// A host and a joiner in the lobby, no bots, and the match counting eight seconds in once it starts (the loadout's count).
	StartPlaying({2, TEXT("?Mode=FreeForAll?Bots=0?Countdown=8"), false, true});
	WaitUntil(TEXT("the host's lobby"), 10.0f, []() { return Controller(0) != nullptr && Controller(0)->GetMenuPage() == EMenuPage::Lobby; });
	WaitUntil(TEXT("the joiner's lobby, with the host's match"), 10.0f, []() { return Controller(1) != nullptr && Controller(1)->GetMenu().IsValid() && Controller(1)->GetMenu()->IsLobbyReady(); });
	// The joiner picks in the lobby: the Munster, smoke, Wraith. The pick goes to the server (the lobby allows it).
	Play(1, TEXT("cmd CheeseStrikeLoadout munster smoke wraith"));
	WaitUntil(TEXT("the joiner's pick on the server"), 5.0f, []()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		for (const APlayerState* Each : Match != nullptr ? Match->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			const ACheeseStrikePlayerState* Player = Cast<ACheeseStrikePlayerState>(Each);
			if (Player != nullptr && Player->GetPlayerName() == Controller(1)->PlayerState->GetPlayerName() && Player->GetPrimary() == CheeseStrikeWeapons::Munster().Id)
			{
				return true;
			}
		}
		return false;
	});
	Then([this]()
	{
		TestTrue(TEXT("in the lobby the loadout may be changed"), Controller(1)->LoadoutWhen() == CheeseStrikeLoadout::EWhen::Lobby && CheeseStrikeLoadout::MayChange(Controller(1)->LoadoutWhen()));
		World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->AskToStart(false);
	});
	// The match starts after the lobby's countdown and counts in: both come in held, the joiner with what they picked.
	WaitUntil(TEXT("the match counting in with both characters armed"), CheeseStrikeRules::MatchCountdown + 15.0f, []()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		return Match != nullptr && Match->HasMatchStarted() && Match->IsFrozen() && HeldOf(0) != NAME_None && HeldOf(1) != NAME_None;
	});
	WaitUntil(TEXT("the loadout page up for the count-in"), 3.0f, []() { return Controller(0) != nullptr && Controller(0)->IsLoadoutOffered() && Controller(0)->GetMenuPage() == EMenuPage::Loadout; });
	Then([this]()
	{
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		TestEqual(TEXT("the joiner comes in with the Munster"), HeldOf(1), CheeseStrikeWeapons::Munster().Id);
		TestTrue(TEXT("and smoke grenades"), Joiner != nullptr && Joiner->GetGrenades() != nullptr && Joiner->GetGrenades()->GetKind() == CheeseStrikeGrenades::EGrenadeKind::Smoke);
		TestEqual(TEXT("two of them"), Joiner != nullptr && Joiner->GetGrenades() != nullptr ? Joiner->GetGrenades()->GetCount() : 0, CheeseStrikeGrenades::SpawnCount);
		TestEqual(TEXT("as Wraith"), Joiner != nullptr ? Joiner->GetBody() : INDEX_NONE, CheeseStrikeBodies::HeroFromKey(TEXT("Wraith")));
		TestEqual(TEXT("the host with the usual primary, the Cheddar Blaster the tests come in with"), HeldOf(0), CheeseStrikeWeapons::Blaster().Id);
		TestTrue(TEXT("during which the loadout may be changed"), Controller(0)->LoadoutWhen() == CheeseStrikeLoadout::EWhen::CountIn);
		// The host picks the Munster and Murdock during the count: taken up at once, while held.
		Controller(0)->DismissLoadout();
	});
	Play(0, TEXT("cmd CheeseStrikeLoadout munster murdock"));
	WaitUntil(TEXT("the host's pick taken up at once"), 4.0f, []() { return HeldOf(0) == CheeseStrikeWeapons::Munster().Id; });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestTrue(TEXT("while the count still runs"), Match != nullptr && Match->IsFrozen());
		TestEqual(TEXT("and the body swapped as it stands"), CharacterOf(0, 0) != nullptr ? CharacterOf(0, 0)->GetBody() : INDEX_NONE, CheeseStrikeBodies::HeroFromKey(TEXT("Murdock")));
		TestFalse(TEXT("the page put away stays away for this count"), Controller(0)->IsLoadoutOffered());
		TestTrue(TEXT("so the match shows, not the page"), Controller(0)->GetMenuPage() == EMenuPage::None);
	});
	WaitUntil(TEXT("the count ending"), 12.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && !Match->IsFrozen(); });
	Then([this]()
	{
		TestEqual(TEXT("the Munster is in hand as the count ends"), HeldOf(0), CheeseStrikeWeapons::Munster().Id);
		TestFalse(TEXT("play on, the loadout may not be changed"), CheeseStrikeLoadout::MayChange(Controller(0)->LoadoutWhen()));
	});
	// A pick made mid-match is refused by the server: the player state keeps what it had.
	Play(0, TEXT("cmd CheeseStrikeLoadout blaster\nwait 1.5"));
	Then([this]()
	{
		const ACheeseStrikePlayerState* Host = StateOf(0);
		TestEqual(TEXT("mid-match, the server keeps the loadout it took during the count"), Host != nullptr ? Host->GetPrimary() : NAME_None, CheeseStrikeWeapons::Munster().Id);
		TestEqual(TEXT("and the Munster stays in hand"), HeldOf(0), CheeseStrikeWeapons::Munster().Id);
	});
	StopPlaying();
	return true;
}

#endif
