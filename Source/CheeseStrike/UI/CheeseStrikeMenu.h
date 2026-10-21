// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ACheeseStrikePlayerController;
class SBox;
struct FButtonStyle;
struct FSlateBrush;

/** The pages of the menu that sits over the level. */
enum class EMenuPage : uint8
{
	/** No menu: the match is on. */
	None,
	/** The front end: host, find, quit. */
	Main,
	/** The matches found, to join one. */
	Find,
	/** Waiting for the host to start, with who is here and the rules to set. */
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
	EMenuPage PageFor(bool bMatchStarted, bool bMatchEnded, bool bInAMatch, bool bPaused, EMenuPage Chosen, bool bTravelling = false);

	/** How often a page that shows changing things (the lobby, the matches found) is redrawn, in seconds. */
	constexpr float RefreshEvery = 0.5f;
	/** Corners, in Slate units. */
	constexpr float Rounding = 6.0f;
	/** The lobby's picture of the map, wide (16:9), and its rules' column as wide. */
	constexpr float MapPictureWidth = 600.0f;
}

/**
 * The menu, drawn with Slate in code, over the level seen from the menu
 * camera: the game's name at the top and a dark panel on the left with the
 * page's choices as buttons, or the lobby across the whole view with the
 * rules on the left, the two sides on the right, and Leave and Start below.
 * The player controller shows it, tells it the page, and does what its
 * buttons ask (CheeseStrikePlayerController). Escape goes to the controller
 * too, so that a paused match resumes while only the menu takes input.
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

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	enum class EButtonKind : uint8 { Plain, Primary, Danger };

	TSharedRef<SWidget> MakePage();
	TSharedRef<SWidget> MakeMain();
	TSharedRef<SWidget> MakeFind();
	TSharedRef<SWidget> MakeLobby();
	TSharedRef<SWidget> MakeLobbyRules();
	TSharedRef<SWidget> MakePause();
	TSharedRef<SWidget> MakeTitle(const FString& Text);
	TSharedRef<SWidget> MakeLine(const FString& Text, bool bDim = false);
	TSharedRef<SWidget> MakeButton(const FString& Text, TFunction<void()> Do, EButtonKind Kind = EButtonKind::Plain, TSharedPtr<SWidget> Inside = nullptr);
	/** A row of choices, the chosen one lit; with `bCanPick` off, only the chosen one is shown lit and none can be pressed. */
	TSharedRef<SWidget> MakeChoices(const FString& Label, const TArray<FString>& Choices, int32 Chosen, bool bCanPick, TFunction<void(int32)> Pick);
	/** One team's column in the lobby: its name, then who is on it, each in a chip with an icon, the host crowned. */
	TSharedRef<SWidget> MakeTeamColumn(int32 Team);
	TSharedRef<SWidget> MakeNameChip(const FString& Name, bool bBot, bool bHost, int32 Team);
	/** A number with a button either side to make it less or more; with `bCanPick` off, the buttons can't be pressed. */
	TSharedRef<SWidget> MakeStepper(const FString& Label, const FString& Value, bool bCanPick, TFunction<void()> Less, TFunction<void()> More);
	/** The picture of map `MapIndex` (CheeseStrikeRules::Maps), its name across the bottom. */
	TSharedRef<SWidget> MakeMapPicture(int32 MapIndex);
	/** Everything the lobby shows but the seconds, as text: when it changes, the lobby is drawn again. */
	FString LobbyKey() const;
	const FButtonStyle& ButtonStyle(EButtonKind Kind) const;

	TWeakObjectPtr<ACheeseStrikePlayerController> Controller;
	/** The game's name over the front end, the panel on the left, and the lobby across the whole view. */
	TSharedPtr<SWidget> Masthead;
	TSharedPtr<SWidget> Panel;
	TSharedPtr<SBox> Content;
	TSharedPtr<SWidget> Lobby;
	TSharedPtr<SBox> LobbyContent;
	EMenuPage Page = EMenuPage::None;
	FString ShownLobbyKey;
};
