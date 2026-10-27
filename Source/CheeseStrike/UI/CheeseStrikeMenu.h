// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Types/SlateEnums.h"
#include "Widgets/SCompoundWidget.h"
#include "Rules/CheeseStrikeFriends.h"
#include "Rules/CheeseStrikeRules.h"

class ACheeseStrikePlayerController;
class UCheeseStrikeSessionSubsystem;
class SBox;
class SScrollBox;
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
	/** The player's settings (UCheeseStrikeSettings), each kind on a tab (EOptionsTab): from the front end, or from the pause menu during a match. */
	Options,
};

/** The settings page's tabs. */
/** What the menu's dialog asks before it does something it can't take back: closing the host's lobby, or quitting the game. */
enum class EMenuAsk : uint8
{
	None,
	CloseLobby,
	Quit,
};

enum class EOptionsTab : uint8
{
	/** Every action's keys and buttons (CheeseStrikeControls), as a table. */
	Controls,
	/** The look's sensitivity, aiming's share of it, inverting it, and the gamepad's aim assist. */
	Aiming,
	Sound,
	Graphics,
};

namespace CheeseStrikeMenu
{
	/**
	 * Which page shows, from where the match is: none once it is on (or the
	 * pause menu, if the player paused, or the options from it), none once
	 * it is over (the scoreboard is up), the lobby while a hosted or joined
	 * match waits or goes to another level (but none while the results are
	 * held up on the way back to the lobby, `bResultsHeld`:
	 * UCheeseStrikePostGameSubsystem), and otherwise the page the player
	 * chose among the front end's.
	 */
	EMenuPage PageFor(bool bMatchStarted, bool bMatchEnded, bool bInAMatch, bool bPaused, EMenuPage Chosen, bool bTravelling = false, bool bResultsHeld = false);

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
	/**
	 * Whether the lobby is drawn again this frame: when what it shows has
	 * `bChanged`, but not while the host has a dropdown open (drawing it
	 * again would close it under the mouse) unless its status has changed
	 * (travelling, starting), which outweighs any choice being made.
	 */
	bool ShouldRedrawLobby(bool bChanged, bool bStatusChanged, bool bDropdownOpen);
	/** Corners, in Slate units. */
	constexpr float Rounding = 6.0f;
	/** A confirmation dialog's card: how wide, and how far its content is in from its sides. */
	constexpr float DialogWidth = 480.0f;
	constexpr float DialogPadding = 32.0f;
	/** The lobby's picture of the map, wide (16:9), and its rules' column as wide; the gap between it and the seats. */
	constexpr float MapPictureWidth = 680.0f;
	constexpr float MapPictureRounding = 12.0f;
	constexpr float LobbyGap = 44.0f;
	/** The friends panel's width when it is collapsed to its tab: the lobby leaves this much room at its right, so nothing is under it. */
	constexpr float FriendsTabWidth = 120.0f;
	/** The width of the names of the lobby's rules, left of their dropdowns. */
	constexpr float RuleLabelWidth = 220.0f;
	/** The smallest the game's window may be made (in a window), so that nothing in the menus is crowded out. */
	constexpr int32 MinWindowWidth = 1280;
	constexpr int32 MinWindowHeight = 720;
	/**
	 * How much a page that needs `Wants` is shrunk to fit `Room`: 1 when it
	 * fits, less (the same both ways) when it would otherwise be clipped.
	 */
	float ShrinkToFit(const FVector2D& Wants, const FVector2D& Room);
	/** The longest name a player may have (Steam's), which a seat in the lobby fits: smaller, and cut short with an ellipsis if it still doesn't. */
	constexpr int32 LongestName = 32;
	/** The size a seat writes `Name` at: smaller the longer it is. */
	int32 SeatNameSize(const FString& Name);
	/** Paused, how much the game behind the menu is blurred (Slate's blur strength) and darkened (black's opacity over it). */
	constexpr float PauseBlur = 12.0f;
	constexpr float PauseDarkening = 0.4f;
	/** How many columns a free-for-all's lobby lays its seats out in: as many seats to a column as a side has. */
	constexpr int32 FreeForAllColumns = 2;
	/**
	 * Which of a page's items has the focus after it is drawn again:
	 * `Names` being its items in order, the one called `Name` if it is
	 * there, else the one at `Index` (or the last), and the first on a page
	 * shown afresh (`Index` 0, `Name` empty). INDEX_NONE if it has none.
	 */
	int32 FocusAfterRedraw(const TArray<FString>& Names, const FString& Name, int32 Index);
	/**
	 * Where a move of the focus `Way` (up, down, left, right) goes from the
	 * item at `From`, among items at `Others` (on the screen): the nearest
	 * that way, its distance along the way counted once and how far it is
	 * off to the side three times, so that a row or a column is kept to.
	 * INDEX_NONE if there is none that way.
	 */
	int32 NextInDirection(const FSlateRect& From, const TArray<FSlateRect>& Others, EUINavigation Way);
}

/**
 * The menu, drawn with Slate in code, over the level seen from the menu
 * camera: the game's name at the top and a dark panel on the left with the
 * page's choices as buttons, or the lobby across the whole view with the
 * rules on the left, the two sides on the right, and Leave and Start below.
 * The player controller shows it, tells it the page, and does what its
 * buttons ask (CheeseStrikePlayerController). Escape goes to the controller
 * too, so that a paused match resumes while only the menu takes input.
 *
 * A gamepad or the keyboard works it too: each page puts the focus on its
 * first item (a button, a dropdown, a slider), the d-pad, the left stick
 * and the arrows move it (Slate's navigation), A and Enter press, a slider
 * moves with left and right, a dropdown opens with A and picks with A, and
 * B and Escape close an open dropdown or go back (as Escape always did).
 * The item focused is ringed in cheese, unless the player uses the mouse.
 * Each item is registered by name as it is made (Focusable), so that a page
 * drawn again (PageKey, LobbyKey) gives the focus back to the same item.
 */
struct FSlateBrush;

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
	/** Opens the lobby's dropdown `Index` (in the order they show), shutting any other; out of range, shuts them all. */
	void OpenDropdown(int32 Index);
	/** Whether the game behind the menu is blurred: under the pause menu, and the options opened from it. */
	bool IsBlurringTheGame() const;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	/** The space bar presses nothing in the menu (Slate takes it for Enter): only Enter, a click or a gamepad's A do. */
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent) override;

	/** The page's items that can take the focus, by name, in the order they show. */
	TArray<FString> GetFocusableNames() const;
	/** The item that has the focus, or is to have it as soon as it can (a page just drawn has none for a frame). */
	const FString& GetFocusName() const { return FocusName; }
	/** Puts the focus on the item called `Name`. */
	void FocusItem(const FString& Name);
	/** Whether one of the lobby's dropdowns is open. */
	bool HasDropdownOpen() const { return IsDropdownOpen(); }
	/** The host's pause menu asks whether to end the match and take everyone back to the lobby (`bAsk`), or stops asking. */
	void ConfirmBackToLobby(bool bAsk);
	/** The buttons that close the lobby or quit: the first click arms them, the second does it. */
	TSharedRef<SWidget> MakeCloseLobbyButton();
	TSharedRef<SWidget> MakeQuitButton();
	/** The dialog over the whole menu, for what `Asking` is: its question, and what to press. */
	TSharedRef<SWidget> MakeDialog();
	/** The friends panel, open or as its tab; and a friend's row in it, with Join or Invite. */
	TSharedRef<SWidget> MakeFriendsPanel(bool bOpen);
	TSharedRef<SWidget> MakeFriendRow(int32 Index);
	/** The host's choice of who can join the party (read-only for everyone else). */
	TSharedRef<SWidget> MakePartyPermission(bool bHost, UCheeseStrikeSessionSubsystem* Sessions);
	/** A friends section's header: its name and count, which a click folds away or opens again (remembered while the menu lasts). */
	TSharedRef<SWidget> MakeSectionHeader(CheeseStrikeFriends::ESection Section, int32 Count);
	void AskBefore(EMenuAsk Question);
	bool IsConfirmingBackToLobby() const { return bConfirmingBackToLobby; }
	/** How many times the lobby has been built afresh: not when a rule changes, which its rows show as they are. */
	int32 GetLobbyBuilds() const { return LobbyBuilds; }
	/** Whether the lobby shows the host's match, ready to be played in: not joining it, nor on the way to another level. */
	bool IsLobbyReady() const;
	/** The room the lobby needs, laid out in full with its margins: in less (a small window), it is shrunk to fit, never clipped. */
	FVector2D LobbyWants() const;
	/** Shuts any dropdown open, giving the focus back to its button; true if one was. */
	bool CloseDropdowns();

private:
	enum class EButtonKind : uint8 { Plain, Primary, Danger, Quiet };

	TSharedRef<SWidget> MakePage();
	TSharedRef<SWidget> MakeMain();
	TSharedRef<SWidget> MakeFind();
	TSharedRef<SWidget> MakeLobby();
	TSharedRef<SWidget> MakeLobbyRules();
	TSharedRef<SWidget> MakePause();
	TSharedRef<SWidget> MakeCredits();
	/** The settings: a tab for each kind (EOptionsTab), each setting on a row, applied as it changes and kept. */
	TSharedRef<SWidget> MakeOptions();
	/** The settings' Controls tab: a row an action, its keys on the keyboard and mouse, and its buttons on an Xbox and a Nintendo pad (CheeseStrikeControls::Rows). */
	TSharedRef<SWidget> MakeControls();
	/** A row of the controls' table, or its heading. */
	TSharedRef<SWidget> ControlsRow(const FString& Action, const FString& Keys, const FString& Xbox, const FString& Nintendo, bool bHeading) const;
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
	/** A line of text, wrapped at the width it was last given unless `bWrap` is off (in the lobby, which a frame of unwrapped words would shrink). */
	/**
	 * A line of text. Wrapped, it wraps at a width given, not at the one it happens to have been drawn at (AutoWrapText
	 * only knows that after a layout pass, so the text stood a word a line, and the page grew, for a frame or two):
	 * `WrapAt` if there is one, otherwise what the page has (WrapWidth).
	 */
	TSharedRef<SWidget> MakeLine(const FString& Text, bool bDim = false, bool bWrap = true, float WrapAt = 0.0f);
	/** How wide the current page's content is, less room for a scroll bar: what a line of text there wraps at. */
	float WrapWidth() const;
	TSharedRef<SWidget> MakeButton(const FString& Text, TFunction<void()> Do, EButtonKind Kind = EButtonKind::Plain, TSharedPtr<SWidget> Inside = nullptr);
	/**
	 * A dropdown: the chosen one of `Choices` (`GetChosen`, read as it is
	 * drawn and as its list opens) on a button that opens the
	 * list of them (each with its picture from `Pictures`, if any), the
	 * chosen one lit, `Pick` called with the one clicked. With `bCanPick`
	 * off (a joiner's lobby), only the chosen one, on nothing to press.
	 * Kept in Dropdowns, so that the lobby isn't drawn again while one is
	 * open.
	 */
	TSharedRef<SWidget> MakeDropdown(const TArray<FString>& Choices, TFunction<int32()> GetChosen, bool bCanPick, TFunction<void(int32)> Pick, TArray<const FSlateBrush*> Pictures = {});
	/** A rule's value with nothing to press, as a joiner sees what the host chose. */
	TSharedRef<SWidget> MakeReadOnly(TAttribute<FText> Value);
	/** A row of the lobby's rules: its name on the left, then what sets it. */
	TSharedRef<SWidget> MakeRuleRow(const FString& Label, TSharedRef<SWidget> Setter);
	/** Whether one of the lobby's dropdowns is open. */
	bool IsDropdownOpen() const;
	/**
	 * A column of the lobby's seats: `Title` over them in `Colour`, then
	 * `Seats` (players, bots, then empty seats; CheeseStrikeRules::
	 * WithEmptySeats), each in a chip of that colour with an icon, the host
	 * crowned, sharing the column's height. Two of these, one a side, make
	 * the lobby of a team match (MakeSeatColumns); one with every seat, a
	 * free-for-all's.
	 */
	TSharedRef<SWidget> MakeSeatColumn(const FString& Title, const FLinearColor& Colour, const TArray<CheeseStrikeRules::FRosterEntry>& Seats, int32 Team = INDEX_NONE);
	/** The lobby's columns: a team's column for each side when `bTeams`, or one column of every seat. */
	TSharedRef<SWidget> MakeSeatColumns(bool bTeams);
	/**
	 * One seat's chip: a player's (crowned if `bHost`, marked if `bMine`), a
	 * bot's, or an empty seat, in `Colour`. With `Click`, it is a button: a
	 * seat on the other side takes the player over to it, and the host's
	 * click on a bot moves the bot.
	 */
	TSharedRef<SWidget> MakeNameChip(const CheeseStrikeRules::FRosterEntry& Seat, bool bHost, const FLinearColor& Colour, bool bMine = false, TFunction<void()> Click = nullptr);
	/** The main page's word on why the player was put out of a match (the host left, the connection was lost), with OK to dismiss it. */
	TSharedRef<SWidget> MakeFarewell();
	/** A number with a button either side to make it less or more; with `bCanPick` off, the buttons can't be pressed. */
	TSharedRef<SWidget> MakeStepper(const FString& Label, const FString& Value, bool bCanLess, bool bCanMore, TFunction<void()> Less, TFunction<void()> More);
	/** The picture of the map `GetMap` says (CheeseStrikeRules::Maps), its name across the bottom, as it is now. */
	TSharedRef<SWidget> MakeMapPicture(TFunction<int32()> GetMap);
	/** What the lobby is made of, as text (not the seconds, nor the rules' values, which its rows show as they are): when it changes, the lobby is drawn again. */
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
	/**
	 * Registers `Widget` as an item of the page that can take the focus,
	 * called `Name` (made unique on the page by a number after it), and
	 * returns it.
	 */
	TSharedRef<SWidget> Focusable(const FString& Name, TSharedRef<SWidget> Widget);
	/** Notes which item has the focus, and puts it back on an item if none has (a page drawn again, a click outside). */
	void KeepFocus();
	/** Gives `Widget` the local player's focus, ringed; whether it took it. */
	bool GiveFocus(const TSharedPtr<SWidget>& Widget);
	/** Whether `Widget` and everything it is in are enabled, so that it can take the focus. */
	static bool IsReallyEnabled(const TSharedPtr<SWidget>& Widget);
	/** The local player's Slate user. */
	int32 UserIndex() const;

	struct FFocusable
	{
		FString Name;
		TWeakPtr<SWidget> Widget;
	};
	/** The items of the page as last drawn. */
	TArray<FFocusable> Focusables;
	/** The items of the dialog, which has the focus to itself while it is up (Asking): the page's are out of reach meanwhile. */
	TArray<FFocusable> DialogFocusables;
	bool bBuildingDialog = false;
	const TArray<FFocusable>& Active() const { return Asking != EMenuAsk::None ? DialogFocusables : Focusables; }
	TArray<FFocusable>& Active() { return Asking != EMenuAsk::None ? DialogFocusables : Focusables; }
	/** What had the focus before the dialog came up, to have it back as it goes. */
	FString FocusBeforeAsk;
	FString FocusName;
	int32 FocusIndex = 0;
	/** Whether the focus is still to be put on FocusName's item. */
	bool bFocusPending = true;
	/** The item each page had the focus on when last left, to come back to it. */
	TMap<uint8, FString> PageFocus;
	/** A dropdown's list just opened: its chosen item, to have the focus. */
	TWeakPtr<SWidget> DropFocus;
	/** The button of each dropdown, as Dropdowns, to give the focus back to when one shuts. */
	TArray<TWeakPtr<SWidget>> DropdownButtons;
	/** The credits' list, which up and down scroll (it has nothing to focus). */
	TWeakPtr<SScrollBox> CreditsScroll;

	EMenuPage Page = EMenuPage::None;
	EOptionsTab OptionsTab = EOptionsTab::Controls;
	FString ShownLobbyKey;
	int32 ShownLobbyStatus = INDEX_NONE;
	/** The lobby's dropdowns as last drawn. */
	TArray<TWeakPtr<class SMenuAnchor>> Dropdowns;
	/** The blur over the game behind the pause menu. */
	TSharedPtr<SWidget> Blur;
	FString ShownPageKey;
	/** Whether the host's pause menu is asking to confirm Back to Lobby. */
	bool bConfirmingBackToLobby = false;
	/** A first click on the host's Close Lobby or on Quit has armed it: the second click does it, so that a press too many doesn't close the lobby or the game. */
	EMenuAsk Asking = EMenuAsk::None;
	/** What the dialog was last drawn for, so that it is drawn again only when that changes. */
	EMenuAsk ShownAsk = EMenuAsk::None;
	TSharedPtr<SWidget> Dialog;
	TSharedPtr<SBox> DialogContent;
	/** The friends panel on the right: a tab, opened by hovering it in the lobby, and open on the find page. */
	TSharedPtr<SBox> FriendsBox;
	TSharedPtr<SBox> FriendsContent;
	/** What the panel was last drawn for. */
	FString ShownFriendsKey;
	/** When the friends were last asked for, the panel's width now and when it was last set (to ease it open and shut). */
	double FriendsReadAt = -1000.0;
	float FriendsWidth = 120.0f;
	/** The friends sections folded shut (remembered while the menu lasts). */
	TSet<uint8> ClosedFriendSections;
	double FriendsWidthAt = 0.0;
	/** The pictures of the friends in the rows drawn (kept while they are). */
	TArray<TSharedPtr<FSlateBrush>> FriendBrushes;
	int32 LobbyBuilds = 0;
};
