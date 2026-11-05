// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveSequence.h"
#include "Layout/SlateRect.h"
#include "Types/SlateEnums.h"
#include "Types/SlateStructs.h"
#include "Widgets/SCompoundWidget.h"
#include "Rules/CheeseStrikeFriends.h"
#include "Rules/CheeseStrikeRules.h"

class ACheeseStrikePlayerController;
class UCheeseStrikeSessionSubsystem;
class APlayerState;
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
	/**
	 * The loadout (Rules/CheeseStrikeLoadout.h): the hero, the primary and the grenade, picked from the front end, over
	 * the lobby (an overlay on it, as the settings are), and of itself as a count-in begins in a mode with a loadout
	 * (ACheeseStrikePlayerController::IsLoadoutOffered: the HUD's count shows through, the page says the seconds).
	 */
	Loadout,
	/** TEMPORARY: the sampler of UI themes (UI/CheeseStrikeThemes.h), a dev page of the front end's, to pick a look from. */
	Themes,
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
	 * UCheeseStrikePostGameSubsystem) or the settings opened over it with
	 * Escape (LobbyMenuKey), and otherwise the page the player chose among
	 * the front end's. Once a match is on, the loadout page while a count-in
	 * offers it (`bLoadoutOffered`, CheeseStrikeLoadout::OffersPage), unless
	 * paused.
	 */
	EMenuPage PageFor(bool bMatchStarted, bool bMatchEnded, bool bInAMatch, bool bPaused, EMenuPage Chosen, bool bTravelling = false, bool bResultsHeld = false, bool bLoadoutOffered = false);

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
	/**
	 * Whether the host's Close Lobby can be pressed with the lobby's bottom
	 * saying `Status`: not once the countdown has begun (nothing in the lobby
	 * can change then, ACheeseStrikeGameMode::CanChangeLobby), nor while it
	 * goes to another level; it shows faded, takes no click and no focus.
	 */
	bool CanCloseLobby(ELobbyStatus Status);
	/**
	 * The page chosen after the menu key (Escape, a pad's Menu or B) in the
	 * lobby, from the one chosen now: the settings over the lobby, and from
	 * them (or from the loadout over it) back to it (the lobby's own page,
	 * which PageFor shows for anything chosen; the settings and the loadout
	 * are overlays on it, LobbyOverlay).
	 */
	EMenuPage LobbyMenuKey(EMenuPage Chosen);
	/**
	 * What shows over the lobby: the settings or the loadout, when the page
	 * `Shown` is the lobby and the player `Chosen` one of them (Escape,
	 * LobbyMenuKey; the lobby's LOADOUT button); None otherwise. They are an
	 * overlay within the lobby's page, the lobby under them blurred and
	 * darkened and taking no press, so that nothing of the lobby is drawn
	 * afresh for them; Escape, B or Back puts them away.
	 */
	EMenuPage LobbyOverlay(EMenuPage Shown, EMenuPage Chosen);
	/** Whether the settings show over the lobby (LobbyOverlay is the settings). */
	bool SettingsOverLobby(EMenuPage Shown, EMenuPage Chosen);
	/** The loadout page's pictures: a hero's portrait (Content/UI/Heroes, tools/art/hero_portraits.py), a weapon's icon and a grenade's (Content/UI/Loadout), in Slate units. */
	constexpr float PortraitSize = 108.0f;
	constexpr float WeaponIconWidth = 150.0f;
	constexpr float GrenadeIconSize = 72.0f;
	/** Corners, in Slate units. */
	constexpr float Rounding = 6.0f;
	/** The front end's masthead: the game's name and under it the subtitle, their font sizes, and how far down from the top. */
	constexpr int32 TitleSize = 84;
	constexpr int32 SubtitleSize = 40;
	constexpr float MastheadTop = 24.0f;
	/** Seconds a new page (not one drawn again) and a dialog take to fade in, and how big a dialog starts (it grows to its size meanwhile). Only drawn so: they take presses at once. */
	constexpr float FadeSeconds = 0.15f;
	constexpr float DialogFromScale = 0.94f;
	/** A confirmation dialog's card: how wide, and how far its content is in from its sides. */
	constexpr float DialogWidth = 480.0f;
	constexpr float DialogPadding = 32.0f;
	/** The lobby's picture of the map, wide (16:9), and its rules' column as wide; the gap between it and the seats. */
	constexpr float MapPictureWidth = 680.0f;
	constexpr float MapPictureRounding = 12.0f;
	constexpr float LobbyGap = 44.0f;
	/** The friends panel's width when it is collapsed to its tab: the lobby leaves this much room at its right, so nothing is under it. */
	constexpr float FriendsTabWidth = 120.0f;
	/**
	 * The pages' panel on the left: how far its content is in from its sides, how wide its content is on the front end, so
	 * how wide the panel is; the friends panel open on the right is as wide (the one width for both), drawn with the same
	 * brush, colour and opacity, its corners the pages' panel's mirrored: the two touching the screen's edge square, the
	 * two inner ones rounded by PanelRounding.
	 */
	constexpr float PanelPadding = 48.0f;
	constexpr float PanelContentWidth = 400.0f;
	constexpr float PanelWidth = PanelContentWidth + 2.0f * PanelPadding;
	constexpr float PanelRounding = 18.0f;
	/** The menu's tooltips: how long the mouse rests on an item before its tooltip shows, how far from the mouse it is put, and how wide its words wrap at. */
	constexpr float TooltipDelay = 0.3f;
	constexpr float TooltipOffset = 18.0f;
	constexpr float TooltipWrap = 340.0f;
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
	/** Shows `Tab` of the options when they show (on their page, or over the lobby). */
	void SetOptionsTab(EOptionsTab Tab) { OptionsTab = Tab; ShownPageKey.Reset(); ShownLobbyOverlayKey.Reset(); }
	EOptionsTab GetOptionsTab() const { return OptionsTab; }
	/** Opens the lobby's dropdown `Index` (in the order they show), shutting any other; out of range, shuts them all. */
	void OpenDropdown(int32 Index);
	/** Whether the game behind the menu is blurred: under the pause menu, and the options opened from it. */
	bool IsBlurringTheGame() const;
	/** What shows over the lobby (CheeseStrikeMenu::LobbyOverlay: the settings, the loadout, or None), the lobby under it blurred and taking no press. */
	EMenuPage GetLobbyOverlay() const { return bLobbyOverlayShown ? LobbyOverlayPage : EMenuPage::None; }
	bool AreSettingsOverLobby() const { return GetLobbyOverlay() == EMenuPage::Options; }

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
	/**
	 * The friends panel, open (its tall content, shown too while it slides open or shut) or folded to its tab; a friend's row
	 * in it, in `Section` (JOIN and INVITE in Joinable, INVITE in Others, nothing in the party); and a player of our game's
	 * row in the party (ourself, or another with the host's KICK).
	 */
	TSharedRef<SWidget> MakeFriendsPanel(bool bOpen);
	/** Pins the friends panel open or shut (a click on its chevron): it then stays so whatever the mouse does, for as long as the menu lasts. */
	void PinFriends(bool bOpen);
	/** Whether the friends panel is pinned, and which way. */
	const TOptional<bool>& GetFriendsPin() const { return FriendsPinned; }
	/** Whether the friends panel is open, or on its way open (CheeseStrikeFriends::PanelOpen). */
	bool IsFriendsPanelOpen() const { return bFriendsWantOpen; }
	/** The friends panel's width now, from its curve (CheeseStrikeFriends::PanelWidthAt): the tab's shut, PanelWidth open, between on the way. */
	FOptionalSize FriendsWidthNow() const;
	TSharedRef<SWidget> MakeFriendRow(int32 Index, CheeseStrikeFriends::ESection Section);
	TSharedRef<SWidget> MakePartyRow(const APlayerState* Who, bool bSelf, bool bHost);
	/** The host's choice of who can join the party (read-only for everyone else). */
	TSharedRef<SWidget> MakePartyPermission(bool bHost, UCheeseStrikeSessionSubsystem* Sessions);
	/** A friends section's header: a chevron, its name and a count, which a click folds away or opens again (remembered while the menu lasts). */
	TSharedRef<SWidget> MakeSectionHeader(CheeseStrikeFriends::ESection Section, int32 Count);
	void AskBefore(EMenuAsk Question);
	bool IsConfirmingBackToLobby() const { return bConfirmingBackToLobby; }
	/** How many times the lobby has been built afresh: not when a rule changes, which its rows show as they are. */
	int32 GetLobbyBuilds() const { return LobbyBuilds; }
	/** Whether the lobby shows the host's match, ready to be played in: not joining it, nor on the way to another level. */
	bool IsLobbyReady() const;
	/** The room the lobby needs, laid out in full with its margins: in less (a small window), it is shrunk to fit, never clipped. */
	FVector2D LobbyWants() const;
	/** The room the front end's masthead (the game's name and its subtitle) needs, and the pages' panel on the left. */
	FVector2D MastheadWants() const;
	FVector2D PanelWants() const;
	/** Shuts any dropdown open, giving the focus back to its button; true if one was. */
	bool CloseDropdowns();
	/** Whether the item with the focus is ringed: not while the player uses the mouse, which has its hover. */
	static bool AreFocusRingsShown();
	/** TEMPORARY: the theme sampler shows theme `Index` (0 to 4), or all of them stacked for any index past them. */
	void ShowTheme(int32 Index);

private:
	enum class EButtonKind : uint8 { Plain, Primary, Danger, Quiet };

	TSharedRef<SWidget> MakePage();
	TSharedRef<SWidget> MakeMain();
	/**
	 * The loadout page (EMenuPage::Loadout, Rules/CheeseStrikeLoadout.h): HERO as the four heroes' portraits, PRIMARY as
	 * the three weapons' icons with their names, GRENADE as the two kinds', the one picked of each ringed in cheese, each
	 * kept in the settings as it is picked (and sent to the server by the controller); on the front end the figure wears and
	 * holds the picks at once. During a count-in it says the seconds left and OK puts it away; otherwise Back.
	 */
	TSharedRef<SWidget> MakeLoadout();
	/** One choice of the loadout page: `Picture` (sized `PictureSize`) over `Name`, ringed in cheese when `bChosen`; `Pick` on a press. */
	TSharedRef<SWidget> MakeLoadoutChoice(const FString& Name, const FSlateBrush* Picture, const FVector2D& PictureSize, bool bChosen, TFunction<void()> Pick);
	/** A row of the loadout page: `Label` over `Choices`. */
	TSharedRef<SWidget> MakeLoadoutRow(const FString& Label, TSharedRef<SWidget> Choices);
	/** What the loadout page is made of (the picks, and whether a count-in offers it): drawn again when it changes. */
	FString LoadoutKey() const;
	TSharedRef<SWidget> MakeFind();
	/**
	 * TEMPORARY (UI/CheeseStrikeThemes.cpp): the sampler of UI themes: tabs across the top for each theme and SHOW ALL,
	 * then the theme's sample, or every sample stacked in a scroll box; each sample a title, the buttons, a dropdown, a
	 * slider, a toggle, a text field, tabs, a card with a tooltip and a chip in that theme's looks, with its own hover and
	 * click sounds. Its picks (ThemeSamples) are read live, so nothing is drawn afresh for them.
	 */
	TSharedRef<SWidget> MakeThemes();
	TSharedRef<SWidget> MakeThemeSample(int32 Theme);
	/** A dropdown in a theme's looks (as MakeDropdown, kept in Dropdowns likewise), its chosen one of `Choices` read live. */
	TSharedRef<SWidget> MakeThemedDropdown(int32 Theme, const TArray<FString>& Choices, TFunction<int32()> GetChosen, TFunction<void(int32)> Pick);
	/** What the themes page is made of: the theme shown, or all. */
	FString ThemesKey() const;
	TSharedRef<SWidget> MakeLobby();
	TSharedRef<SWidget> MakeLobbyRules();
	TSharedRef<SWidget> MakePause();
	TSharedRef<SWidget> MakeCredits();
	/** The settings: a tab for each kind (EOptionsTab), each setting on a row, applied as it changes and kept. */
	TSharedRef<SWidget> MakeOptions();
	/** The settings' Controls tab: a row an action, its keys on the keyboard and mouse, and its buttons on an Xbox, a PlayStation and a Nintendo pad (CheeseStrikeControls::Rows). */
	TSharedRef<SWidget> MakeControls();
	/** A row of the controls' table, or its heading. */
	TSharedRef<SWidget> ControlsRow(const FString& Action, const FString& Keys, const FString& Xbox, const FString& PlayStation, const FString& Nintendo, bool bHeading) const;
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
	/** The friends panel folded to its tab: a chevron, the friends' figure, the word and how many are online. */
	TSharedRef<SWidget> MakeFriendsTab(UCheeseStrikeSessionSubsystem* Sessions);
	/**
	 * The friends panel's chevron, a button: pointing right (to the edge it folds to) while the panel is open, left (the
	 * way it opens) on its tab, turning from the other's way as it is made afresh; a click pins the panel the other way
	 * (PinFriends). It takes no focus: a pad's stays on the page's items.
	 */
	TSharedRef<SWidget> MakeFriendsChevron(bool bOpen);
	/**
	 * A tooltip in the menu's own looks: `Text` (lines, wrapped at TooltipWrap) on a rounded box in the panel's colour, the
	 * menu's font, no border, with `Title` in cheese over it when there is one. Drawn by the menu itself, beside the mouse,
	 * while the widget it belongs to is hovered (WithTooltip): not in a window of the OS's, as Slate's tooltips are, whose
	 * corners can't be rounded, and which a screenshot never shows.
	 */
	TSharedRef<SWidget> MakeTooltip(const FString& Text, const FString& Title = FString());
	/** Gives `Widget` the tooltip MakeTooltip makes of `Text` and `Title` (none for an empty `Text`); returns `Widget`. */
	TSharedRef<SWidget> WithTooltip(TSharedRef<SWidget> Widget, const FString& Text, const FString& Title = FString());
	/** Gives `Widget` a tooltip of its own making (the theme sampler's, in its theme's looks), shown as MakeTooltip's are; returns `Widget`. */
	TSharedRef<SWidget> WithTooltipMade(TSharedRef<SWidget> Widget, TFunction<TSharedRef<SWidget>()> Make);
	/** Shows the tooltip of whichever widget with one is hovered (the innermost), after TooltipDelay, beside the mouse; run every frame. */
	void UpdateTooltip();
	/**
	 * A row of the friends panel: a rounded card, lit under the mouse, with `Picture` on the left, `Name` over `Under`
	 * (cut with an ellipsis, never wrapped) and `Action` on the right; `Hover` as its tooltip.
	 */
	TSharedRef<SWidget> MakeFriendCard(TSharedRef<SWidget> Picture, const FString& Name, TSharedRef<SWidget> Under, TSharedRef<SWidget> Action, const FString& Hover);
	/** A player's picture, rounded, `Size` across: Steam's (`Texture`) or their initials on a colour of their name's. */
	TSharedRef<SWidget> MakeAvatar(class UTexture2D* Texture, const FString& Name, float Size);
	/** A count in a pill, as a section's header has it. */
	TSharedRef<SWidget> MakeCountPill(int32 Count);
	/** A faint line between sections. */
	TSharedRef<SWidget> MakeDivider();
	/** A small button in the menu's looks (its kinds and rounding), for a row: Join, Invite, Kick, Refresh. */
	TSharedRef<SWidget> MakeSmallButton(const FString& Text, const FString& ItemName, TFunction<void()> Do, EButtonKind Kind = EButtonKind::Plain);
	/**
	 * A friend row's button, smaller than MakeSmallButton so that two fit beside a name: faded, with no click and no focus,
	 * when `WhyNot` says why it can't be pressed (its tooltip); else `Hover` is.
	 */
	TSharedRef<SWidget> MakeRowButton(const FString& Text, const FString& ItemName, TFunction<void()> Do, EButtonKind Kind, const FString& WhyNot, const FString& Hover);
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
	/** What the settings page is made of (its tab, and what is pressed rather than slid): drawn again when it changes, on the front end, the pause menu and over the lobby alike. */
	FString OptionsKey() const;
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
	/** The items of the overlay over the lobby (the settings or the loadout), likewise theirs alone while it shows (bLobbyOverlayShown); the lobby's are out of reach under it. */
	TArray<FFocusable> LobbyOverlayFocusables;
	bool bBuildingLobbyOverlay = false;
	const TArray<FFocusable>& Active() const { return Asking != EMenuAsk::None ? DialogFocusables : (bLobbyOverlayShown ? LobbyOverlayFocusables : Focusables); }
	TArray<FFocusable>& Active() { return Asking != EMenuAsk::None ? DialogFocusables : (bLobbyOverlayShown ? LobbyOverlayFocusables : Focusables); }
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
	/**
	 * TEMPORARY: the theme sampler's state (MakeThemes): which theme's sample shows, or all of them, and each sample's own
	 * picks (its tab, its dropdown's choice, its toggle and its slider), read live by the sample's widgets.
	 */
	int32 ThemeShown = 0;
	bool bAllThemes = false;
	struct FThemeSample
	{
		int32 Tab = 0;
		int32 Choice = 0;
		bool bOn = true;
		float Value = 0.7f;
	};
	TArray<FThemeSample> ThemeSamples;
	/** The page last built afresh: a page shown in its place fades in, the same one drawn again doesn't. */
	EMenuPage BuiltPage = EMenuPage::None;
	EOptionsTab OptionsTab = EOptionsTab::Controls;
	FString ShownLobbyKey;
	int32 ShownLobbyStatus = INDEX_NONE;
	/**
	 * The overlay over the lobby (CheeseStrikeMenu::LobbyOverlay: the settings, or the loadout): a blur over the lobby's
	 * page with the page drawn in a panel on it, shown while the player has it chosen, and drawn again as its tab or
	 * what it shows changes (OptionsKey, LoadoutKey); the lobby under it is drawn by its own rules meanwhile, never
	 * afresh for it.
	 */
	TSharedPtr<SWidget> LobbyOverlay;
	TSharedPtr<SBox> LobbyOverlayContent;
	FString ShownLobbyOverlayKey;
	bool bLobbyOverlayShown = false;
	EMenuPage LobbyOverlayPage = EMenuPage::None;
	/** The lobby's item that had the focus as the overlay came over it, to have it back as it goes. */
	FString FocusBeforeLobbyOverlay;
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
	/**
	 * The friends panel on the right: a tab, opened by hovering it on the front end and in the lobby (shut again half a
	 * second after the mouse leaves) or pinned open or shut by a click on its chevron, and open on the find page
	 * (CheeseStrikeFriends::PanelOpen). Its width is bound to a curve (FriendsCurve, CheeseStrikeFriends::PanelSeconds,
	 * eased in and out) played forward to open and backward to shut, read off it every frame; its content (FriendsInside)
	 * fades in over the second half of an expand and out over the first half of a collapse, and the tall content stays
	 * until the panel is shut, when it folds to its tab.
	 */
	TSharedPtr<SBox> FriendsBox;
	TSharedPtr<SBox> FriendsContent;
	TSharedPtr<SWidget> FriendsInside;
	FCurveSequence FriendsCurve;
	TOptional<bool> FriendsPinned;
	bool bFriendsWantOpen = false;
	bool bFriendsHovered = false;
	double FriendsLeftAt = -1000.0;
	/** What the panel was last drawn for. */
	FString ShownFriendsKey;
	/** When the friends were last asked for. */
	double FriendsReadAt = -1000.0;
	/** The friends sections folded shut (remembered while the menu lasts), and the one just folded or opened, whose chevron turns. */
	TSet<uint8> ClosedFriendSections;
	int32 TurningFriendSection = INDEX_NONE;
	/** The pictures of the friends in the rows drawn (kept while they are). */
	TArray<TSharedPtr<FSlateBrush>> FriendBrushes;
	int32 LobbyBuilds = 0;
	/** The widgets given a tooltip (WithTooltip), with its words; the dead are dropped as they are looked through. */
	struct FTooltipOwner
	{
		TWeakPtr<SWidget> Widget;
		FString Text;
		FString Title;
		/** Makes the tooltip itself, in place of MakeTooltip, when set (WithTooltipMade). */
		TFunction<TSharedRef<SWidget>()> Make;
	};
	TArray<FTooltipOwner> TooltipOwners;
	/**
	 * The box the tooltip is drawn in, over everything, put beside the mouse by its padding; whose tooltip it shows, which
	 * widget with one is hovered and since when, and when the last tooltip went (one hovered soon after shows at once).
	 */
	TSharedPtr<SBox> TooltipBox;
	TSharedPtr<SWidget> TooltipCard;
	bool bTooltipShown = false;
	TWeakPtr<SWidget> TooltipShownFor;
	TWeakPtr<SWidget> TooltipHoveredWidget;
	double TooltipHoveredAt = 0.0;
	double TooltipHiddenAt = -1000.0;
};
