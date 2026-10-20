// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ACheeseStrikePlayerController;
class SBox;

/** The pages of the menu that sits over the level. */
enum class EMenuPage : uint8
{
	/** No menu: the match is on. */
	None,
	/** The front end: play alone, host, find, quit. */
	Main,
	/** The host's choices for a match. */
	Host,
	/** The matches found, to join one. */
	Find,
	/** Waiting for the host to start, with who is here. */
	Lobby,
	/** The pause menu during a match. */
	Pause,
};

namespace CheeseStrikeMenu
{
	/**
	 * Which page shows, from where the match is: none once it is on (or the
	 * pause menu, if the player paused), none once it is over (the scoreboard
	 * is up), the lobby while a hosted or joined match waits, and otherwise
	 * the page the player chose among the front end's.
	 */
	EMenuPage PageFor(bool bMatchStarted, bool bMatchEnded, bool bInAMatch, bool bPaused, EMenuPage Chosen);

	/** How often a page that shows changing things (the lobby, the matches found) is redrawn, in seconds. */
	constexpr float RefreshEvery = 0.5f;
}

/**
 * The menu, drawn with Slate in code: a dark panel on the left of the view
 * with the page's title and its choices as buttons, over the level seen
 * from the menu camera. The player controller shows it, tells it the page,
 * and does what its buttons ask (CheeseStrikePlayerController).
 */
class SCheeseStrikeMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCheeseStrikeMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ACheeseStrikePlayerController>, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Shows `Page`, drawn afresh. */
	void Show(EMenuPage Page);
	/** Draws the page again, for what has changed. */
	void Refresh();
	EMenuPage GetPage() const { return Page; }
	/** The host's choices on the Host page, kept while the menu lives. */
	int32 GetChosenKillLimit() const { return ChosenKillLimit; }
	float GetChosenRespawnDelay() const { return ChosenRespawnDelay; }

private:
	TSharedRef<SWidget> MakePage();
	TSharedRef<SWidget> MakeMain();
	TSharedRef<SWidget> MakeHost();
	TSharedRef<SWidget> MakeFind();
	TSharedRef<SWidget> MakeLobby();
	TSharedRef<SWidget> MakePause();
	TSharedRef<SWidget> MakeTitle(const FString& Text);
	TSharedRef<SWidget> MakeLine(const FString& Text, bool bDim = false);
	TSharedRef<SWidget> MakeButton(const FString& Text, TFunction<void()> Do, bool bChosen = false);
	/** A row of choices, the chosen one lit. */
	TSharedRef<SWidget> MakeChoices(const FString& Label, const TArray<FString>& Choices, int32 Chosen, TFunction<void(int32)> Pick);
	/** One team's column in the lobby: its name, then who is on it, bots dimmer. */
	TSharedRef<SWidget> MakeTeamColumn(int32 Team);

	TWeakObjectPtr<ACheeseStrikePlayerController> Controller;
	/** The game's name over the front end, the panel on the left, and the lobby across the whole view. */
	TSharedPtr<SWidget> Masthead;
	TSharedPtr<SWidget> Panel;
	TSharedPtr<SBox> Content;
	TSharedPtr<SWidget> Lobby;
	TSharedPtr<SBox> LobbyContent;
	EMenuPage Page = EMenuPage::None;
	int32 ChosenKillLimit = 0;
	float ChosenRespawnDelay = 0.0f;
};
