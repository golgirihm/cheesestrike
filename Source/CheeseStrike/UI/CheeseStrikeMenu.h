// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Rules/CheeseStrikeRules.h"

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
	/** Who made what the game is made of (CheeseStrikeCredits). */
	Credits,
	/** The player's options (UCheeseStrikeSettings): from the front end, or from the pause menu during a match. */
	Options,
};

/** The options page's tabs. */
enum class EOptionsTab : uint8
{
	Controls,
	View,
	Sound,
	Graphics,
};

namespace CheeseStrikeMenu
{
	/**
	 * Which page shows, from where the match is: none once it is on (or the
	 * pause menu, if the player paused, or the options from it), none once
	 * it is over (the scoreboard is up), the lobby while a hosted or joined
	 * match waits or goes to another level, and otherwise the page the
	 * player chose among the front end's.
	 */
	EMenuPage PageFor(bool bMatchStarted, bool bMatchEnded, bool bInAMatch, bool bPaused, EMenuPage Chosen, bool bTravelling = false);

	/** The tones of the lobby's countdown (CheeseStrikeTones). */
	enum class ECountdownTone : uint8
	{
		None,
		/** A second has gone by: 5, 4, 3, 2, 1. */
		Second,
		/** The countdown is over. */
		End,
	};
	/**
	 * The tone this frame: from the countdown now (`bCounting`, `Left`
	 * seconds left) and whether it is `bOver` on the server (the match has
	 * begun, or is leaving for its level), and `Sounded`, kept between
	 * frames: the last whole second sounded, 0 once the end has, INDEX_NONE
	 * with no countdown. A tone as each second begins, each once however
	 * this machine's clock of the server's wavers (the first only if most of
	 * it is still to come), and the end's when the
	 * seconds run out or the countdown is over before this clock has got
	 * there; none when it is called off.
	 */
	ECountdownTone CountdownTone(int32& Sounded, bool bCounting, float Left, bool bOver);

	/**
	 * A side's colour, on its chips and over its column: The Gouda Guys'
	 * orange and the Mozza Radicals' sky blue (Okabe and Ito's, which eyes
	 * that see colour differently still tell apart), both clear on the
	 * lobby's dark tile; any other team, as in a free-for-all, a cheese
	 * yellow.
	 */
	FLinearColor TeamColour(int32 Team);

	/** What the bottom of the lobby says, right of Leave. */
	enum class ELobbyStatus : uint8
	{
		/** A joiner who hasn't got the host's match yet: "Joining the lobby", no seats. */
		Joining,
		/** Going to another level, the match's map or back to the lobby: "Going to ...". */
		Travelling,
		/** The countdown is over and the match about to begin: "Starting", nothing to press. */
		Starting,
		/** Counting down: the host's red Cancel with the seconds, or "Starting in N". */
		Counting,
		/** The host's blue Start the Match. */
		CanStart,
		/** A joiner's "Waiting for the host to start". */
		Waiting,
	};
	/**
	 * What the lobby's bottom says: `bHaveMatch` once the local player has
	 * the host's match (its state, the host and themself, for a joiner),
	 * `bTravelling` once the match is leaving the level, and while counting
	 * down, the seconds left.
	 */
	ELobbyStatus LobbyStatus(bool bHaveMatch, bool bTravelling, bool bCounting, float SecondsLeft, bool bHost);
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
	/** Draws the page again if what it shows has changed; cheap otherwise, so run every frame. */
	void Refresh();
	EMenuPage GetPage() const { return Page; }
	/** Shows `Tab` of the options when they show. */
	void SetOptionsTab(EOptionsTab Tab) { OptionsTab = Tab; ShownPageKey.Reset(); }
	EOptionsTab GetOptionsTab() const { return OptionsTab; }

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
	TSharedRef<SWidget> MakeCredits();
	/** The options: a tab for each kind (EOptionsTab), each setting on a row, applied as it changes and kept. */
	TSharedRef<SWidget> MakeOptions();
	TSharedRef<SWidget> MakeOptionsTab();
	/** A row of the options: its name on the left, then what sets it. */
	TSharedRef<SWidget> MakeOptionRow(const FString& Label, TSharedRef<SWidget> Setter);
	/** A slider from `Min` to `Max` by `Step`, its value written beside it by `Show`; `Set` as it moves, and the settings kept when it is let go. */
	TSharedRef<SWidget> MakeSlider(float Min, float Max, float Step, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FString(float)> Show);
	/** A row of choices without a label of its own, for an options row. */
	TSharedRef<SWidget> MakeChoiceButtons(const TArray<FString>& Choices, int32 Chosen, bool bCanPick, TFunction<void(int32)> Pick);
	/** A spinner, turning once a second. */
	TSharedRef<SWidget> MakeSpinner(float Radius = 10.0f) const;
	TSharedRef<SWidget> MakeTitle(const FString& Text);
	TSharedRef<SWidget> MakeLine(const FString& Text, bool bDim = false);
	TSharedRef<SWidget> MakeButton(const FString& Text, TFunction<void()> Do, EButtonKind Kind = EButtonKind::Plain, TSharedPtr<SWidget> Inside = nullptr);
	/** A row of choices, the chosen one lit; with `bCanPick` off, only the chosen one is shown lit and none can be pressed. */
	TSharedRef<SWidget> MakeChoices(const FString& Label, const TArray<FString>& Choices, int32 Chosen, bool bCanPick, TFunction<void(int32)> Pick);
	/**
	 * A column of the lobby's seats: `Title` over them in `Colour`, then
	 * `Seats` (players, bots, then empty seats; CheeseStrikeRules::
	 * WithEmptySeats), each in a chip of that colour with an icon, the host
	 * crowned, sharing the column's height. Two of these, one a side, make
	 * the lobby of a team match (MakeSeatColumns); one with every seat, a
	 * free-for-all's.
	 */
	TSharedRef<SWidget> MakeSeatColumn(const FString& Title, const FLinearColor& Colour, const TArray<CheeseStrikeRules::FRosterEntry>& Seats);
	/** The lobby's columns: a team's column for each side when `bTeams`, or one column of every seat. */
	TSharedRef<SWidget> MakeSeatColumns(bool bTeams);
	/** One seat's chip: a player's (crowned if `bHost`), a bot's, or an empty seat, in `Colour`. */
	TSharedRef<SWidget> MakeNameChip(const CheeseStrikeRules::FRosterEntry& Seat, bool bHost, const FLinearColor& Colour);
	/** A number with a button either side to make it less or more; with `bCanPick` off, the buttons can't be pressed. */
	TSharedRef<SWidget> MakeStepper(const FString& Label, const FString& Value, bool bCanPick, TFunction<void()> Less, TFunction<void()> More);
	/** The picture of map `MapIndex` (CheeseStrikeRules::Maps), its name across the bottom. */
	TSharedRef<SWidget> MakeMapPicture(int32 MapIndex);
	/** Everything the lobby shows but the seconds, as text: when it changes, the lobby is drawn again. */
	FString LobbyKey() const;
	/** The same for the other pages: the page, and on the Find page what was found. */
	FString PageKey() const;
	/** Whether the local player has the host's match yet: its state, with the host and themself in it. */
	bool HaveMatch() const;
	/** What the lobby's bottom says now (CheeseStrikeMenu::LobbyStatus). */
	CheeseStrikeMenu::ELobbyStatus LobbyStatusNow() const;
	const FButtonStyle& ButtonStyle(EButtonKind Kind) const;

	TWeakObjectPtr<ACheeseStrikePlayerController> Controller;
	/** The game's name over the front end, the panel on the left, and the lobby across the whole view. */
	TSharedPtr<SWidget> Masthead;
	TSharedPtr<SWidget> Panel;
	TSharedPtr<SBox> Content;
	TSharedPtr<SWidget> Lobby;
	TSharedPtr<SBox> LobbyContent;
	EMenuPage Page = EMenuPage::None;
	EOptionsTab OptionsTab = EOptionsTab::Controls;
	FString ShownLobbyKey;
	FString ShownPageKey;
};
