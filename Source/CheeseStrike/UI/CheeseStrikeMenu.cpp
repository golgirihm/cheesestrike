// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMenu.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "CheeseStrikeLoader.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "CheeseStrikeGameState.h"
#include "GameFramework/GameMode.h"
#include "CheeseStrikePlayerController.h"
#include "Input/CheeseStrikeInput.h"
#include "Rules/CheeseStrikeBots.h"
#include "Rules/CheeseStrikeHills.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeFriends.h"
#include "Styling/SlateBrush.h"
#include "Settings/CheeseStrikeSettings.h"
#include "UI/CheeseStrikeControls.h"
#include "UI/CheeseStrikeCredits.h"
#include "Rules/CheeseStrikeRules.h"
#include "Sessions/CheeseStrikeSessionSubsystem.h"
#include "CheeseStrikePlayerState.h"

namespace CheeseStrikeMenu
{
	EMenuPage PageFor(bool bMatchStarted, bool bMatchEnded, bool bInAMatch, bool bPaused, EMenuPage Chosen, bool bTravelling, bool bResultsHeld)
	{
		const bool bFrontEndPage = Chosen == EMenuPage::Find || Chosen == EMenuPage::Credits || Chosen == EMenuPage::Options;
		const EMenuPage FrontEnd = bFrontEndPage ? Chosen : EMenuPage::Main;
		// Going to the map the host picked: the lobby, saying so. Back to the lobby after the results: the results, held up
		// until the lobby is there.
		if (bTravelling)
		{
			return bResultsHeld ? EMenuPage::None : (bInAMatch ? EMenuPage::Lobby : FrontEnd);
		}
		if (bMatchEnded)
		{
			return EMenuPage::None;
		}
		if (bMatchStarted)
		{
			// The settings open from the pause menu, the match going on and the player still paused.
			return bPaused ? (Chosen == EMenuPage::Options ? Chosen : EMenuPage::Pause) : EMenuPage::None;
		}
		if (bInAMatch)
		{
			return EMenuPage::Lobby;
		}
		return FrontEnd;
	}

	ECountdownTone CountdownTone(int32& Sounded, bool bCounting, float Left, bool bOver)
	{
		// Over on the server: the end, unless this clock got there first and sounded it.
		if (bOver)
		{
			const bool bEnd = Sounded > 0;
			Sounded = Sounded == INDEX_NONE ? INDEX_NONE : 0;
			return bEnd ? ECountdownTone::End : ECountdownTone::None;
		}
		// Called off, or none.
		if (!bCounting)
		{
			Sounded = INDEX_NONE;
			return ECountdownTone::None;
		}
		// Each whole second once, going down only: this machine's clock of the server's may waver back across one.
		const int32 Now = FMath::Max(0, FMath::CeilToInt(Left));
		// The first second heard of, if most of it is still to come: a clock of the server's ahead of it, or the countdown
		// coming late, would otherwise sound two seconds at once.
		if (Sounded == INDEX_NONE)
		{
			Sounded = Now;
			return Now > 0 && Left > Now - 0.5f ? ECountdownTone::Second : ECountdownTone::None;
		}
		if (Now < Sounded)
		{
			Sounded = Now;
			return Now > 0 ? ECountdownTone::Second : ECountdownTone::End;
		}
		return ECountdownTone::None;
	}

	bool ShouldRedrawLobby(bool bChanged, bool bStatusChanged, bool bDropdownOpen)
	{
		// A dropdown the host has open would close as the lobby is drawn again: wait for it, unless the lobby itself has
		// moved on (travelling, starting), which nothing in the dropdown could matter to.
		return bChanged && (!bDropdownOpen || bStatusChanged);
	}

	int32 FocusAfterRedraw(const TArray<FString>& Names, const FString& Name, int32 Index)
	{
		if (Names.Num() == 0)
		{
			return INDEX_NONE;
		}
		const int32 Found = Name.IsEmpty() ? INDEX_NONE : Names.IndexOfByKey(Name);
		return Found != INDEX_NONE ? Found : FMath::Clamp(Index, 0, Names.Num() - 1);
	}

	int32 NextInDirection(const FSlateRect& From, const TArray<FSlateRect>& Others, EUINavigation Way)
	{
		const bool bAcross = Way == EUINavigation::Left || Way == EUINavigation::Right;
		const bool bForward = Way == EUINavigation::Down || Way == EUINavigation::Right;
		if (!bAcross && Way != EUINavigation::Up && Way != EUINavigation::Down)
		{
			return INDEX_NONE;
		}
		// How far apart two spans are along an axis: nothing if they overlap.
		const auto Apart = [](double LowA, double HighA, double LowB, double HighB) { return FMath::Max(0.0, FMath::Max(LowB - HighA, LowA - HighB)); };
		constexpr double Slack = 2.0;
		int32 Best = INDEX_NONE;
		double BestScore = 0.0;
		for (int32 Index = 0; Index < Others.Num(); ++Index)
		{
			const FSlateRect& Other = Others[Index];
			if (Other == From)
			{
				continue;
			}
			double Along = 0.0;
			double Aside = 0.0;
			if (bAcross)
			{
				Along = bForward ? Other.Left - From.Right : From.Left - Other.Right;
				Aside = Apart(From.Top, From.Bottom, Other.Top, Other.Bottom);
			}
			else
			{
				Along = bForward ? Other.Top - From.Bottom : From.Top - Other.Bottom;
				Aside = Apart(From.Left, From.Right, Other.Left, Other.Right);
			}
			if (Along < -Slack)
			{
				continue;
			}
			const double Score = FMath::Max(0.0, Along) + Aside * 3.0;
			if (Best == INDEX_NONE || Score < BestScore)
			{
				Best = Index;
				BestScore = Score;
			}
		}
		return Best;
	}

	float ShrinkToFit(const FVector2D& Wants, const FVector2D& Room)
	{
		if (Wants.X <= 0.0 || Wants.Y <= 0.0)
		{
			return 1.0f;
		}
		return static_cast<float>(FMath::Clamp(FMath::Min(Room.X / Wants.X, Room.Y / Wants.Y), 0.1, 1.0));
	}

	int32 SeatNameSize(const FString& Name)
	{
		// The seats are as wide as a sixteen-letter name at the full size; longer ones come down, a Steam name's 32 letters
		// to the smallest, and what still doesn't fit ends in an ellipsis.
		const int32 Length = Name.Len();
		return Length <= 16 ? 22 : (Length <= 22 ? 19 : (Length <= 27 ? 17 : 15));
	}

	ELobbyStatus LobbyStatus(bool bHaveMatch, bool bTravelling, bool bCounting, float SecondsLeft, bool bHost)
	{
		if (bTravelling)
		{
			return ELobbyStatus::Travelling;
		}
		if (!bHaveMatch)
		{
			return ELobbyStatus::Joining;
		}
		if (bCounting)
		{
			return SecondsLeft <= 0.0f ? ELobbyStatus::Starting : ELobbyStatus::Counting;
		}
		return bHost ? ELobbyStatus::CanStart : ELobbyStatus::Waiting;
	}
}

namespace
{
	const FLinearColor Ink(0.92f, 0.9f, 0.86f);
	const FLinearColor Muted(0.6f, 0.58f, 0.55f);
	const FLinearColor Cheese(0.96f, 0.72f, 0.2f);
	const FLinearColor PanelColour(0.04f, 0.04f, 0.045f, 0.82f);

	FSlateFontInfo Font(int32 Size, const FName& Style = FName("Regular"))
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	/** `Text` in capitals, accents and all (Gruyère's è, which FString's ToUpper leaves). */
	FString Upper(const TCHAR* Text)
	{
		return FText::FromString(Text).ToUpper().ToString();
	}

	/** `Colour` darkened (or lightened, above 1) by `Scale`, as opaque. */
	FLinearColor Shade(const FLinearColor& Colour, float Scale, float Alpha = 1.0f)
	{
		return FLinearColor(Colour.R * Scale, Colour.G * Scale, Colour.B * Scale, Alpha);
	}

	/** The menu's looks, made once: rounded buttons in three kinds, chips, the sliders, and the pictures from Content/UI. */
	struct FLooks
	{
		FButtonStyle Plain;
		FButtonStyle Primary;
		FButtonStyle Danger;
		FSlateRoundedBoxBrush BotChip;
		FSlateRoundedBoxBrush PrimaryFace;
		/** A dropdown's choices that aren't chosen: clear until the mouse is over them. */
		FButtonStyle Quiet;
		/** A dropdown's list, and a rule's value as a joiner sees it (nothing to press). */
		FSlateRoundedBoxBrush DropdownList;
		FSlateRoundedBoxBrush ReadOnlyValue;
		/** The ring round the item that has the focus, for a gamepad or the keyboard. */
		FSlateRoundedBoxBrush FocusRing;
		/** Behind a map's picture, and the bar with its name over the picture's foot: rounded to match the picture. */
		FSlateRoundedBoxBrush MapBack;
		FSlateRoundedBoxBrush MapNameBar;
		/** A confirmation dialog's card. */
		FSlateRoundedBoxBrush DialogCard;
		FSliderStyle Slider;
		TUniquePtr<FSlateDynamicImageBrush> PlayerIcon;
		TUniquePtr<FSlateDynamicImageBrush> BotIcon;
		TUniquePtr<FSlateDynamicImageBrush> CrownIcon;
		TUniquePtr<FSlateDynamicImageBrush> LobbyTile;
		TMap<FString, TUniquePtr<FSlateDynamicImageBrush>> MapPictures;
		TMap<FString, TUniquePtr<FSlateRoundedBoxBrush>> Chips;

		/** A map's picture, from Content/UI/Maps, loaded the first time it is asked for. */
		const FSlateBrush* MapPicture(const FString& File)
		{
			if (!MapPictures.Contains(File))
			{
				const FString Path = FPaths::ProjectContentDir() / TEXT("UI") / TEXT("Maps") / File;
				TUniquePtr<FSlateDynamicImageBrush> Picture;
				if (FPaths::FileExists(Path))
				{
					Picture = MakeUnique<FSlateDynamicImageBrush>(FName(*Path), FVector2D(CheeseStrikeMenu::MapPictureWidth, CheeseStrikeMenu::MapPictureWidth * 9.0f / 16.0f));
					// Drawn as a rounded box with the picture in it: the corners are rounded, whatever the picture.
					Picture->DrawAs = ESlateBrushDrawType::RoundedBox;
					Picture->OutlineSettings = FSlateBrushOutlineSettings(CheeseStrikeMenu::MapPictureRounding);
				}
				MapPictures.Add(File, MoveTemp(Picture));
			}
			return MapPictures[File].Get();
		}

		/** A seat that can be clicked: its chip, and brighter under the mouse; made the first time it is asked for. */
		TMap<FString, TUniquePtr<FButtonStyle>> SeatButtons;
		const FButtonStyle* SeatButton(const FLinearColor& Fill, const FLinearColor& Edge)
		{
			const FString Key = Fill.ToString() + Edge.ToString();
			if (!SeatButtons.Contains(Key))
			{
				TUniquePtr<FButtonStyle> Style = MakeUnique<FButtonStyle>();
				const FLinearColor Lit(FMath::Min(1.0f, Fill.R * 1.6f + 0.06f), FMath::Min(1.0f, Fill.G * 1.6f + 0.06f), FMath::Min(1.0f, Fill.B * 1.6f + 0.06f), FMath::Max(Fill.A, 0.35f));
				Style->SetNormal(FSlateRoundedBoxBrush(Fill, 8.0f, Edge, Edge.A > 0.0f ? 2.0f : 0.0f));
				Style->SetHovered(FSlateRoundedBoxBrush(Lit, 8.0f, Edge.A > 0.0f ? Edge : Lit, 2.0f));
				Style->SetPressed(FSlateRoundedBoxBrush(Fill, 8.0f, Edge.A > 0.0f ? Edge : Lit, 2.0f));
				Style->SetNormalPadding(FMargin(0.0f));
				Style->SetPressedPadding(FMargin(0.0f));
				Style->SetNormalForeground(Ink);
				Style->SetHoveredForeground(FLinearColor::White);
				Style->SetPressedForeground(FLinearColor::White);
				SeatButtons.Add(Key, MoveTemp(Style));
			}
			return SeatButtons[Key].Get();
		}

		/** A seat's chip, filled with `Fill` and edged with `Edge` (none if clear), made the first time it is asked for. */
		const FSlateBrush* Chip(const FLinearColor& Fill, const FLinearColor& Edge)
		{
			const FString Key = Fill.ToString() + Edge.ToString();
			if (!Chips.Contains(Key))
			{
				Chips.Add(Key, MakeUnique<FSlateRoundedBoxBrush>(Fill, 8.0f, Edge, Edge.A > 0.0f ? 2.0f : 0.0f));
			}
			return Chips[Key].Get();
		}

		FLooks()
			: BotChip(FLinearColor(0.1f, 0.1f, 0.11f, 1.0f), 8.0f)
			, PrimaryFace(FLinearColor(0.1f, 0.42f, 0.9f), CheeseStrikeMenu::Rounding)
			, DropdownList(FLinearColor(0.025f, 0.025f, 0.028f, 1.0f), CheeseStrikeMenu::Rounding, FLinearColor(0.22f, 0.22f, 0.24f), 1.0f)
			, ReadOnlyValue(FLinearColor(0.12f, 0.12f, 0.13f, 0.9f), CheeseStrikeMenu::Rounding)
			, FocusRing(FLinearColor::Transparent, CheeseStrikeMenu::Rounding, Cheese, 3.0f)
			, MapBack(FLinearColor(0.1f, 0.1f, 0.11f, 1.0f), CheeseStrikeMenu::MapPictureRounding)
			, MapNameBar(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), FVector4(0.0, 0.0, CheeseStrikeMenu::MapPictureRounding, CheeseStrikeMenu::MapPictureRounding))
			, DialogCard(FLinearColor(PanelColour.R, PanelColour.G, PanelColour.B, 1.0f), 16.0f)
		{
			const auto Make = [](FButtonStyle& Style, const FLinearColor& Rest, const FLinearColor& Hover, const FLinearColor& Press)
			{
				Style.SetNormal(FSlateRoundedBoxBrush(Rest, CheeseStrikeMenu::Rounding));
				Style.SetHovered(FSlateRoundedBoxBrush(Hover, CheeseStrikeMenu::Rounding));
				Style.SetPressed(FSlateRoundedBoxBrush(Press, CheeseStrikeMenu::Rounding));
				Style.SetNormalForeground(Ink);
				Style.SetHoveredForeground(FLinearColor::White);
				Style.SetPressedForeground(FLinearColor::White);
				Style.SetNormalPadding(FMargin(0.0f));
				Style.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
				// What can't be pressed now (the start before there is anyone to play): the same, faded.
				Style.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(Rest.R * 0.45f + 0.04f, Rest.G * 0.45f + 0.04f, Rest.B * 0.45f + 0.04f, Rest.A), CheeseStrikeMenu::Rounding));
				Style.SetDisabledForeground(Muted);
			};
			Make(Plain, FLinearColor(0.18f, 0.18f, 0.19f), FLinearColor(0.28f, 0.28f, 0.3f), FLinearColor(0.12f, 0.12f, 0.13f));
			Make(Primary, FLinearColor(0.1f, 0.42f, 0.9f), FLinearColor(0.2f, 0.52f, 1.0f), FLinearColor(0.08f, 0.32f, 0.72f));
			Make(Danger, FLinearColor(0.72f, 0.16f, 0.14f), FLinearColor(0.85f, 0.24f, 0.2f), FLinearColor(0.55f, 0.1f, 0.1f));
			Make(Quiet, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), FLinearColor(0.24f, 0.24f, 0.26f), FLinearColor(0.12f, 0.12f, 0.13f));
			// A slider: a thin rounded bar, and a cheese-coloured knob that lights under the mouse.
			Slider.SetNormalBarImage(FSlateRoundedBoxBrush(FLinearColor(0.22f, 0.22f, 0.24f), 3.0f));
			Slider.SetHoveredBarImage(FSlateRoundedBoxBrush(FLinearColor(0.3f, 0.3f, 0.32f), 3.0f));
			Slider.SetDisabledBarImage(FSlateRoundedBoxBrush(FLinearColor(0.12f, 0.12f, 0.13f), 3.0f));
			Slider.SetNormalThumbImage(FSlateRoundedBoxBrush(Cheese, 9.0f, FVector2f(18.0f, 18.0f)));
			Slider.SetHoveredThumbImage(FSlateRoundedBoxBrush(FLinearColor(1.0f, 0.82f, 0.36f), 9.0f, FVector2f(18.0f, 18.0f)));
			Slider.SetDisabledThumbImage(FSlateRoundedBoxBrush(Muted, 9.0f, FVector2f(18.0f, 18.0f)));
			Slider.SetBarThickness(6.0f);
			const FString Folder = FPaths::ProjectContentDir() / TEXT("UI");
			PlayerIcon = MakeUnique<FSlateDynamicImageBrush>(FName(*(Folder / TEXT("player.png"))), FVector2D(22.0, 22.0));
			BotIcon = MakeUnique<FSlateDynamicImageBrush>(FName(*(Folder / TEXT("bot.png"))), FVector2D(22.0, 22.0));
			CrownIcon = MakeUnique<FSlateDynamicImageBrush>(FName(*(Folder / TEXT("crown.png"))), FVector2D(22.0, 22.0));
			LobbyTile = MakeUnique<FSlateDynamicImageBrush>(FName(*(Folder / TEXT("lobby_tile.png"))), FVector2D(512.0, 512.0), FLinearColor::White, ESlateBrushTileType::Both);
		}
	};

	FLooks& Looks()
	{
		static FLooks Made;
		return Made;
	}

	/** A dropdown's chevron, drawn in the button's own colour: down, or up while its list is open. */
	class SCheeseStrikeDropArrow : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCheeseStrikeDropArrow) {}
			SLATE_ATTRIBUTE(bool, Open)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Open = InArgs._Open;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(16.0, 10.0);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const bool bUp = Open.Get(false);
			TArray<FVector2f> Points = bUp ? TArray<FVector2f>({{2.0f, 8.0f}, {8.0f, 2.0f}, {14.0f, 8.0f}}) : TArray<FVector2f>({{2.0f, 2.0f}, {8.0f, 8.0f}, {14.0f, 2.0f}});
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, InWidgetStyle.GetForegroundColor(), true, 3.0f);
			return LayerId;
		}

	private:
		TAttribute<bool> Open;
	};

	/**
	 * A page that fills the room it is given, as big as it needs at least:
	 * given less than it needs (a small window), it is laid out at what it
	 * needs and shrunk, the same both ways, to fit, so that nothing is ever
	 * clipped or crowded (CheeseStrikeMenu::ShrinkToFit).
	 */
	class SCheeseStrikeFitBox : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCheeseStrikeFitBox) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			ChildSlot [ InArgs._Content.Widget ];
		}

		virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override
		{
			const FVector2D Room = AllottedGeometry.GetLocalSize();
			const FVector2D Wants = ChildSlot.GetWidget()->GetDesiredSize();
			const float Scale = CheeseStrikeMenu::ShrinkToFit(Wants, Room);
			// Laid out at the room it has, or at what it needs where that is more, then shrunk into the room.
			const FVector2D Size(FMath::Max(Room.X, Wants.X * Scale) / Scale, FMath::Max(Room.Y, Wants.Y * Scale) / Scale);
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ChildSlot.GetWidget(), FVector2f(Size), FSlateLayoutTransform(Scale)));
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			// Whatever it is given: it shrinks rather than asking for more.
			return FVector2D::ZeroVector;
		}

		/** How much it shrinks its page now; 1 when it fits. */
		float GetShrink() const
		{
			return CheeseStrikeMenu::ShrinkToFit(ChildSlot.GetWidget()->GetDesiredSize(), GetCachedGeometry().GetLocalSize());
		}
	};

	/** Whether the item with the focus is ringed: not while the player uses the mouse, which has its hover. */
	bool MenuFocusRingsShown = true;

	/** The menu's button: Slate's, ringed in cheese when it has the focus from a gamepad or the keyboard. */
	class SCheeseStrikeButton : public SButton
	{
	public:
		virtual const FSlateBrush* GetFocusBrush() const override
		{
			return MenuFocusRingsShown ? &Looks().FocusRing : nullptr;
		}
	};

	/** The menu's slider, likewise. */
	class SCheeseStrikeSlider : public SSlider
	{
	public:
		virtual const FSlateBrush* GetFocusBrush() const override
		{
			return MenuFocusRingsShown ? &Looks().FocusRing : nullptr;
		}
	};

	/**
	 * A dropdown's open list: up and down move along its items (and stop at
	 * its ends), nothing leaves it sideways, and B or Escape shut it.
	 */
	class SCheeseStrikeDropList : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCheeseStrikeDropList) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
			SLATE_ARGUMENT(TArray<TSharedPtr<SWidget>>, Items)
			SLATE_EVENT(FSimpleDelegate, OnClose)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Items = InArgs._Items;
			OnClose = InArgs._OnClose;
			ChildSlot [ InArgs._Content.Widget ];
		}

		virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent) override
		{
			const EUINavigation Way = InNavigationEvent.GetNavigationType();
			if (Way == EUINavigation::Up || Way == EUINavigation::Down)
			{
				int32 At = INDEX_NONE;
				for (int32 Index = 0; Index < Items.Num(); ++Index)
				{
					At = Items[Index].IsValid() && Items[Index]->HasUserFocus(InNavigationEvent.GetUserIndex()).IsSet() ? Index : At;
				}
				const int32 To = FMath::Clamp(At + (Way == EUINavigation::Down ? 1 : -1), 0, Items.Num() - 1);
				if (Items.IsValidIndex(To) && To != At)
				{
					return FNavigationReply::Explicit(Items[To]);
				}
			}
			return FNavigationReply::Stop();
		}

		virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
		{
			// As in the menu: the space bar picks nothing.
			return InKeyEvent.GetKey() == EKeys::SpaceBar ? FReply::Handled() : FReply::Unhandled();
		}

		virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
		{
			return InKeyEvent.GetKey() == EKeys::SpaceBar ? FReply::Handled() : SCompoundWidget::OnKeyUp(MyGeometry, InKeyEvent);
		}

		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
		{
			if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
			{
				OnClose.ExecuteIfBound();
				return FReply::Handled();
			}
			return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
		}

	private:
		TArray<TSharedPtr<SWidget>> Items;
		FSimpleDelegate OnClose;
	};

	UCheeseStrikeSessionSubsystem* SessionsOf(const ACheeseStrikePlayerController* Player)
	{
		return Player != nullptr && Player->GetGameInstance() != nullptr ? Player->GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
	}

	/** The options' choices that are buttons, in the order they are offered. */
	const TCHAR* const QualityNames[] = {TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC")};
	const EWindowMode::Type WindowModes[] = {EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed};
	const TCHAR* const WindowModeNames[] = {TEXT("FULLSCREEN"), TEXT("BORDERLESS"), TEXT("WINDOWED")};
	/** The width of the options' names, left of what sets them. */
	constexpr float OptionLabelWidth = 250.0f;

	/** The resolutions the options offer on this screen. */
	TArray<FIntPoint> ResolutionsHere()
	{
		TArray<FIntPoint> Supported;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
		const UCheeseStrikeSettings* Settings = UCheeseStrikeSettings::Get();
		return CheeseStrikeSettings::Resolutions(Supported, Settings != nullptr ? Settings->GetDesktopResolution() : FIntPoint(1920, 1080));
	}

	/** Applies the window and resolution in the settings, unless in the editor, whose window isn't the game's to change; and keeps them. */
	void ApplyWindow(UCheeseStrikeSettings* Settings)
	{
		if (!GIsEditor)
		{
			Settings->ApplyResolutionSettings(false);
		}
		Settings->SaveSettings();
	}

	/** Applies the rest of the graphics settings, and keeps them. */
	void ApplyGraphics(UCheeseStrikeSettings* Settings)
	{
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
	}
}

void SCheeseStrikeMenu::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	// The pages' panel stands against the left edge of the screen, so its two right corners are the ones rounded.
	static const FSlateRoundedBoxBrush PagesPanelBrush(FLinearColor::White, FVector4(0.0, 18.0, 18.0, 0.0));
	ChildSlot
	[
		SNew(SOverlay)
		// Paused, the game behind the menu is blurred and a little darker: it goes on, but the player is out of it.
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SAssignNew(Blur, SBackgroundBlur)
			.BlurStrength(CheeseStrikeMenu::PauseBlur)
			.Padding(0.0f)
			.Visibility(EVisibility::Collapsed)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, CheeseStrikeMenu::PauseDarkening))
			]
		]
		// The lobby covers the whole view, over its tile.
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SAssignNew(Lobby, SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(PanelColour.R, PanelColour.G, PanelColour.B, 1.0f))
			]
			+ SOverlay::Slot()
			[
				SNew(SImage).Image(Looks().LobbyTile.Get()).ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
			]
			// The friends tab sits at the right edge, collapsed: its width is kept free, so that it never lies over the seats.
			+ SOverlay::Slot().Padding(FMargin(56.0f, 40.0f, 56.0f + CheeseStrikeMenu::FriendsTabWidth, 40.0f))
			[
				SNew(SCheeseStrikeFitBox)
				[
					SAssignNew(LobbyContent, SBox).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
				]
			]
		]
		// The game's name over the front end, at the top in the middle.
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.0f, 36.0f, 0.0f, 0.0f))
		[
			SAssignNew(Masthead, SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("CHEESESTRIKE"))).Font(Font(56, FName("Bold"))).ColorAndOpacity(Cheese)
				.ShadowOffset(FVector2D(2.0f, 2.0f)).ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("GOUDA OVERLOAD"))).Font(Font(20, FName("Bold"))).ColorAndOpacity(Ink)
				.ShadowOffset(FVector2D(1.0f, 1.0f)).ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
			]
		]
		// The pages' panel on the left.
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill)
		[
			SAssignNew(Panel, SBorder)
			.BorderImage(&PagesPanelBrush)
			.BorderBackgroundColor(PanelColour)
			.Padding(FMargin(48.0f, 64.0f))
			[
				SAssignNew(Content, SBox).WidthOverride(400.0f).VAlign(VAlign_Center)
			]
		]
		// The friends, on the right: a tab, opened by hovering it in the lobby, and open on the find page.
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Fill)
		[
			SAssignNew(FriendsBox, SBox).WidthOverride(CheeseStrikeMenu::FriendsTabWidth).Visibility(EVisibility::Collapsed)
			[
				SAssignNew(FriendsContent, SBox)
			]
		]
		// The question before closing the host's lobby or quitting: over everything, and it takes every press.
		// Modal: a layer over the whole screen that blurs and darkens all that is behind it, other UI too, and is hit
		// itself, so that nothing under it can be clicked.
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SAssignNew(Dialog, SBackgroundBlur)
			.BlurStrength(CheeseStrikeMenu::PauseBlur)
			.Padding(0.0f)
			.Visibility(EVisibility::Collapsed)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f)).Padding(0.0f)
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SAssignNew(DialogContent, SBox).WidthOverride(CheeseStrikeMenu::DialogWidth)
				]
			]
		]
	];
}

void SCheeseStrikeMenu::Show(EMenuPage InPage)
{
	// Each page comes back with the focus where it was left, or on its first item.
	if (InPage != Page)
	{
		PageFocus.Add(static_cast<uint8>(Page), FocusName);
		FocusName = PageFocus.FindRef(static_cast<uint8>(InPage));
		FocusIndex = 0;
	}
	bFocusPending = true;
	Page = InPage;
	bConfirmingBackToLobby = false;
	// Whatever was open of the last page is shut, and the page drawn afresh.
	for (const TWeakPtr<SMenuAnchor>& Each : Dropdowns)
	{
		if (const TSharedPtr<SMenuAnchor> Anchor = Each.Pin())
		{
			Anchor->SetIsOpen(false);
		}
	}
	ShownLobbyKey.Reset();
	ShownLobbyStatus = INDEX_NONE;
	ShownPageKey.Reset();
	Refresh();
}

void SCheeseStrikeMenu::Refresh()
{
	MenuFocusRingsShown = CheeseStrikeInput::DeviceOf(Controller.Get()) != CheeseStrikeInput::EDevice::Mouse;
	if (FriendsBox.IsValid())
	{
		UCheeseStrikeSessionSubsystem* Sessions = SessionsOf(Controller.Get());
		const bool bPanel = Page == EMenuPage::Lobby || Page == EMenuPage::Find;
		// Open on the find page, and in the lobby while hovered.
		const bool bOpen = bPanel && (Page == EMenuPage::Find || FriendsBox->IsHovered());
		FriendsBox->SetVisibility(bPanel ? EVisibility::Visible : EVisibility::Collapsed);
		// Eased open and shut, rather than snapping: a fifth of a second to the width it wants.
		const double Now = FPlatformTime::Seconds();
		FriendsWidth = FMath::FInterpConstantTo(FriendsWidth, bOpen ? 380.0f : CheeseStrikeMenu::FriendsTabWidth, static_cast<float>(FMath::Min(0.1, Now - FriendsWidthAt)), 1500.0f);
		FriendsWidthAt = Now;
		FriendsBox->SetWidthOverride(FriendsWidth);
		// Asked for when it opens, and again every 20 s while it is open, so that a friend who joins a game shows.
		if (bOpen && Sessions != nullptr && Now - FriendsReadAt > 20.0)
		{
			FriendsReadAt = Now;
			Sessions->RefreshFriends();
		}
		// Drawn again only when a friend, a player or the status changes (rebuilding it would restart a spinner).
		FString Key = FString::Printf(TEXT("%d|%s|"), bOpen ? 1 : 0, Sessions != nullptr ? *Sessions->GetStatus() : TEXT(""));
		if (Sessions != nullptr)
		{
			for (const FCheeseStrikeFriend& Each : Sessions->GetFriends())
			{
				Key += FString::Printf(TEXT("%s/%d/%d/%d/%d/%d;"), *Each.Name, static_cast<int32>(Each.State), Each.Humans, static_cast<int32>(Each.Status), static_cast<int32>(Each.Permission), Each.bInMyParty ? 1 : 0);
			}
		}
		if (Sessions != nullptr)
		{
			// A picture arriving, and the host's choice, show too.
			for (int32 Index = 0; Index < Sessions->GetFriends().Num(); ++Index)
			{
				Key += Sessions->AvatarOf(Index) != nullptr ? TEXT("a") : TEXT("-");
			}
			Key += Sessions->FriendsMayJoinHere() ? TEXT("J") : TEXT("I");
			for (int32 Section = 0; Section < CheeseStrikeFriends::NumSections; ++Section)
			{
				Key += ClosedFriendSections.Contains(static_cast<uint8>(Section)) ? TEXT("c") : TEXT("o");
			}
		}
		if (const ACheeseStrikeGameState* State = Controller.IsValid() ? Controller->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr)
		{
			for (const APlayerState* Each : State->PlayerArray)
			{
				Key += FString::Printf(TEXT("%s%s;"), *Each->GetPlayerName(), Each->IsABot() ? TEXT("*") : TEXT(""));
			}
		}
		// (Not while a dropdown is open, which drawing the panel again would close.)
		if (Key != ShownFriendsKey && FriendsContent.IsValid() && !IsDropdownOpen())
		{
			ShownFriendsKey = Key;
			FriendsContent->SetContent(MakeFriendsPanel(bOpen));
		}
	}
	if (Dialog.IsValid())
	{
		Dialog->SetVisibility(Asking != EMenuAsk::None ? EVisibility::Visible : EVisibility::Collapsed);
		if (Asking != ShownAsk && DialogContent.IsValid())
		{
			const bool bWasAsking = ShownAsk != EMenuAsk::None;
			ShownAsk = Asking;
			DialogFocusables.Reset();
			bBuildingDialog = true;
			DialogContent->SetContent(Asking != EMenuAsk::None ? MakeDialog() : SNullWidget::NullWidget);
			bBuildingDialog = false;
			if (Asking == EMenuAsk::None && bWasAsking)
			{
				// The question answered or put away: the focus goes back to what had it.
				FocusName = FocusBeforeAsk;
				bFocusPending = true;
			}
		}
		// While the question is up nothing behind it can take a press, a click or the focus: the pad and the keys stay in the dialog.
		const bool bBehind = Asking == EMenuAsk::None;
		for (const TSharedPtr<SWidget>& Each : TArray<TSharedPtr<SWidget>>{ Panel, FriendsBox, Lobby })
		{
			if (Each.IsValid() && Each->IsEnabled() != bBehind)
			{
				Each->SetEnabled(bBehind);
			}
		}
	}
	const bool bLobby = Page == EMenuPage::Lobby;
	const bool bFrontEnd = Page == EMenuPage::Main || Page == EMenuPage::Find;
	// The credits and the options are wide: a wider panel, and the game's name out of their way.
	const bool bWide = Page == EMenuPage::Credits || Page == EMenuPage::Options;
	if (Content.IsValid())
	{
		Content->SetWidthOverride(Page == EMenuPage::Options ? 900.0f : (bWide ? 640.0f : 400.0f));
		Content->SetVAlign(bWide ? VAlign_Fill : VAlign_Center);
	}
	if (Lobby.IsValid())
	{
		Lobby->SetVisibility(bLobby ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (Blur.IsValid())
	{
		// The pause menu, and the settings opened from it while still paused; not the front end's settings.
		const bool bPausedHere = Page == EMenuPage::Pause || (Page == EMenuPage::Options && Controller.IsValid() && Controller->IsPaused());
		Blur->SetVisibility(bPausedHere ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
	if (Masthead.IsValid())
	{
		Masthead->SetVisibility(bFrontEnd ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
	if (Panel.IsValid())
	{
		Panel->SetVisibility(bLobby || Page == EMenuPage::None ? EVisibility::Collapsed : EVisibility::Visible);
	}
	if (bLobby && LobbyContent.IsValid())
	{
		// Drawn again only when something in it has changed: rebuilding it restarts the spinner.
		// Nor while the host has a dropdown open, which drawing it again would close.
		const FString Key = LobbyKey();
		const int32 Status = static_cast<int32>(LobbyStatusNow());
		if (CheeseStrikeMenu::ShouldRedrawLobby(Key != ShownLobbyKey, Status != ShownLobbyStatus, IsDropdownOpen()))
		{
			ShownLobbyKey = Key;
			ShownLobbyStatus = Status;
			Dropdowns.Reset();
			DropdownButtons.Reset();
			Focusables.Reset();
			bFocusPending = true;
			++LobbyBuilds;
			LobbyContent->SetContent(MakeLobby());
		}
	}
	else if (Content.IsValid())
	{
		// Likewise: a page built afresh has no size for a frame, and the panel, centred on it, would jump.
		ShownLobbyKey.Reset();
		const FString Key = PageKey();
		if (Key != ShownPageKey)
		{
			ShownPageKey = Key;
			Focusables.Reset();
			bFocusPending = true;
			Content->SetContent(MakePage());
		}
	}
	KeepFocus();
}

int32 SCheeseStrikeMenu::UserIndex() const
{
	const ULocalPlayer* Local = Controller.IsValid() ? Controller->GetLocalPlayer() : nullptr;
	const TSharedPtr<const FSlateUser> User = Local != nullptr ? Local->GetSlateUser() : nullptr;
	return User.IsValid() ? User->GetUserIndex() : 0;
}

TSharedRef<SWidget> SCheeseStrikeMenu::Focusable(const FString& Name, TSharedRef<SWidget> Widget)
{
	FString Unique = Name;
	TArray<FFocusable>& Items = bBuildingDialog ? DialogFocusables : Focusables;
	for (int32 Number = 2; Items.ContainsByPredicate([&Unique](const FFocusable& Each) { return Each.Name == Unique; }); ++Number)
	{
		Unique = FString::Printf(TEXT("%s #%d"), *Name, Number);
	}
	Items.Add({Unique, Widget});
	return Widget;
}

TArray<FString> SCheeseStrikeMenu::GetFocusableNames() const
{
	TArray<FString> Names;
	for (const FFocusable& Each : Active())
	{
		Names.Add(Each.Name);
	}
	return Names;
}

void SCheeseStrikeMenu::FocusItem(const FString& Name)
{
	const int32 Index = GetFocusableNames().IndexOfByKey(Name);
	if (Index != INDEX_NONE)
	{
		FocusName = Name;
		FocusIndex = Index;
		bFocusPending = !GiveFocus(Active()[Index].Widget.Pin());
	}
}

bool SCheeseStrikeMenu::IsReallyEnabled(const TSharedPtr<SWidget>& Widget)
{
	for (TSharedPtr<SWidget> At = Widget; At.IsValid(); At = At->GetParentWidget())
	{
		if (!At->IsEnabled())
		{
			return false;
		}
	}
	return Widget.IsValid();
}

bool SCheeseStrikeMenu::GiveFocus(const TSharedPtr<SWidget>& Widget)
{
	if (!Widget.IsValid() || !FSlateApplication::IsInitialized())
	{
		return false;
	}
	// As a move of the focus would (so that it is ringed), whatever the player last pressed.
	const int32 User = UserIndex();
	FSlateApplication::Get().SetUserFocus(User, Widget, EFocusCause::Navigation);
	return Widget->HasUserFocus(User).IsSet();
}

void SCheeseStrikeMenu::KeepFocus()
{
	if (Page == EMenuPage::None)
	{
		return;
	}
	// A dropdown's list just opened: its chosen item takes the focus.
	if (const TSharedPtr<SWidget> Drop = DropFocus.Pin())
	{
		if (GiveFocus(Drop) || !IsDropdownOpen())
		{
			DropFocus.Reset();
		}
		return;
	}
	if (IsDropdownOpen())
	{
		return;
	}
	// Which item has it, if one does.
	const int32 User = UserIndex();
	for (int32 Index = 0; Index < Active().Num(); ++Index)
	{
		const TSharedPtr<SWidget> Widget = Active()[Index].Widget.Pin();
		if (Widget.IsValid() && Widget->HasUserFocus(User).IsSet())
		{
			FocusName = Active()[Index].Name;
			FocusIndex = Index;
			bFocusPending = false;
			return;
		}
	}
	// None has: the page was drawn again or just shown, or the focus was lost or left on the menu itself (a click on
	// nothing). It goes to the same item, or the one in its place, or the first that can take it; but it is not taken from
	// something else that has it, such as the console.
	const TSharedPtr<SWidget> Focused = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetUserFocusedWidget(User) : nullptr;
	if (!bFocusPending && Focused.IsValid() && Focused != AsShared())
	{
		return;
	}
	const int32 Chosen = CheeseStrikeMenu::FocusAfterRedraw(GetFocusableNames(), FocusName, FocusIndex);
	for (int32 Step = 0; Chosen != INDEX_NONE && Step < Active().Num(); ++Step)
	{
		const int32 Index = (Chosen + Step) % Active().Num();
		const TSharedPtr<SWidget> Widget = Active()[Index].Widget.Pin();
		if (IsReallyEnabled(Widget))
		{
			FocusName = Active()[Index].Name;
			FocusIndex = Index;
			bFocusPending = !GiveFocus(Widget);
			return;
		}
	}
	bFocusPending = true;
}

bool SCheeseStrikeMenu::CloseDropdowns()
{
	bool bClosed = false;
	for (int32 Index = 0; Index < Dropdowns.Num(); ++Index)
	{
		const TSharedPtr<SMenuAnchor> Anchor = Dropdowns[Index].Pin();
		if (Anchor.IsValid() && Anchor->IsOpen())
		{
			Anchor->SetIsOpen(false);
			bClosed = true;
			if (DropdownButtons.IsValidIndex(Index))
			{
				GiveFocus(DropdownButtons[Index].Pin());
			}
		}
	}
	DropFocus.Reset();
	return bClosed;
}

FNavigationReply SCheeseStrikeMenu::OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
	const EUINavigation Way = InNavigationEvent.GetNavigationType();
	// The credits and the settings' controls have nothing to focus but Back (and the tabs): up and down scroll them.
	if ((Page == EMenuPage::Credits || (Page == EMenuPage::Options && OptionsTab == EOptionsTab::Controls)) && (Way == EUINavigation::Up || Way == EUINavigation::Down))
	{
		if (const TSharedPtr<SScrollBox> Scroll = CreditsScroll.Pin())
		{
			Scroll->SetScrollOffset(FMath::Clamp(Scroll->GetScrollOffset() + (Way == EUINavigation::Down ? 90.0f : -90.0f), 0.0f, Scroll->GetScrollOffsetOfEnd()));
		}
		return FNavigationReply::Stop();
	}
	// With the focus on the menu itself (a click on nothing), a move puts it back on the page's item.
	if (HasUserFocus(InNavigationEvent.GetUserIndex()).IsSet())
	{
		KeepFocus();
		return FNavigationReply::Stop();
	}
	// From an item, to the nearest that way on the screen, the page's own way rather than Slate's (which doesn't look into
	// the options' scrolling list), or along them in order for Tab.
	int32 From = INDEX_NONE;
	TArray<TSharedPtr<SWidget>> Widgets;
	TArray<FSlateRect> Rects;
	for (const FFocusable& Each : Active())
	{
		const TSharedPtr<SWidget> Widget = Each.Widget.Pin();
		const bool bFrom = Widget.IsValid() && Widget->HasUserFocus(InNavigationEvent.GetUserIndex()).IsSet();
		if (bFrom || (IsReallyEnabled(Widget) && Widget->GetVisibility().IsVisible()))
		{
			From = bFrom ? Widgets.Num() : From;
			Widgets.Add(Widget);
			Rects.Add(Widget->GetCachedGeometry().GetLayoutBoundingRect());
		}
	}
	if (From == INDEX_NONE)
	{
		return SCompoundWidget::OnNavigation(MyGeometry, InNavigationEvent);
	}
	int32 To = INDEX_NONE;
	if (Way == EUINavigation::Next || Way == EUINavigation::Previous)
	{
		To = (From + (Way == EUINavigation::Next ? 1 : Widgets.Num() - 1)) % Widgets.Num();
	}
	else
	{
		To = CheeseStrikeMenu::NextInDirection(Rects[From], Rects, Way);
	}
	return Widgets.IsValidIndex(To) && To != From ? FNavigationReply::Explicit(Widgets[To]) : FNavigationReply::Stop();
}

void SCheeseStrikeMenu::OpenDropdown(int32 Index)
{
	for (int32 Each = 0; Each < Dropdowns.Num(); ++Each)
	{
		if (const TSharedPtr<SMenuAnchor> Anchor = Dropdowns[Each].Pin())
		{
			Anchor->SetIsOpen(Each == Index);
		}
	}
}

bool SCheeseStrikeMenu::IsBlurringTheGame() const
{
	return Blur.IsValid() && Blur->GetVisibility() != EVisibility::Collapsed;
}

FReply SCheeseStrikeMenu::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Slate takes the space bar as Enter on whatever has the focus, so that a jump pressed as the menu comes up, or in it,
	// would press a button. Only Enter, a click or a gamepad's A press one here (the match's own Space is the game's).
	if (InKeyEvent.GetKey() == EKeys::SpaceBar)
	{
		return FReply::Handled();
	}
	return SCompoundWidget::OnPreviewKeyDown(MyGeometry, InKeyEvent);
}

FReply SCheeseStrikeMenu::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return InKeyEvent.GetKey() == EKeys::SpaceBar ? FReply::Handled() : SCompoundWidget::OnKeyUp(MyGeometry, InKeyEvent);
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeCloseLobbyButton()
{
	return MakeButton(TEXT("CLOSE LOBBY"), [this]() { AskBefore(EMenuAsk::CloseLobby); }, EButtonKind::Danger);
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeQuitButton()
{
	return MakeButton(TEXT("QUIT"), [this]() { AskBefore(EMenuAsk::Quit); }, EButtonKind::Danger);
}

void SCheeseStrikeMenu::AskBefore(EMenuAsk Question)
{
	FocusBeforeAsk = FocusName;
	Asking = Question;
	// The focus starts on keeping things as they are, so that a press too many doesn't close the lobby or quit.
	FocusName = Question == EMenuAsk::CloseLobby ? TEXT("KEEP IT OPEN") : TEXT("KEEP PLAYING");
	FocusIndex = 0;
	bFocusPending = true;
	Refresh();
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeDialog()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	const bool bClose = Asking == EMenuAsk::CloseLobby;
	const FString Keep = bClose ? TEXT("KEEP IT OPEN") : TEXT("KEEP PLAYING");
	const UCheeseStrikeSessionSubsystem* Sessions = SessionsOf(Controller.Get());
	const bool bHostingNow = !bClose && Player.IsValid() && Player->HasAuthority() && Sessions != nullptr && Sessions->IsInAMatch();
	// The card is rounded, and its text wraps at the width the card gives it (its width less the padding), so that
	// the card is its full height from the first frame it is drawn in.
	return SNew(SBorder).BorderImage(&Looks().DialogCard).Padding(FMargin(CheeseStrikeMenu::DialogPadding, 28.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(bClose ? TEXT("CLOSE THE LOBBY?") : TEXT("QUIT THE GAME?")) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeLine(bClose ? TEXT("Everyone in it is sent home, and nobody can join it again.") : TEXT("Quit to desktop"), true, true, CheeseStrikeMenu::DialogWidth - 2.0f * CheeseStrikeMenu::DialogPadding)
		]
		// Quitting as the host closes the lobby or ends the match for everyone in it.
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, bHostingNow ? 10.0f : 0.0f, 0.0f, 0.0f))
		[
			bHostingNow ? MakeLine(TEXT("You are hosting: everyone in your lobby will be sent back to the menu."), true, true, CheeseStrikeMenu::DialogWidth - 2.0f * CheeseStrikeMenu::DialogPadding) : SNullWidget::NullWidget
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 20.0f, 0.0f, 0.0f))
		[
			MakeButton(bClose ? TEXT("CLOSE LOBBY") : TEXT("QUIT"), [this, Player, bClose]()
			{
				Asking = EMenuAsk::None;
				Refresh();
				if (Player.IsValid())
				{
					if (bClose) { Player->LeaveFromMenu(); } else { Player->QuitFromMenu(); }
				}
			}, EButtonKind::Danger)
		]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(Keep, [this]() { Asking = EMenuAsk::None; Refresh(); }) ]
	];
}

void SCheeseStrikeMenu::ConfirmBackToLobby(bool bAsk)
{
	bConfirmingBackToLobby = bAsk;
	// Asked, the focus is on keeping on playing, so that a press too many doesn't end the match.
	FocusName = bAsk ? TEXT("KEEP PLAYING") : TEXT("BACK TO LOBBY");
	FocusIndex = 0;
	bFocusPending = true;
	Refresh();
}

FReply SCheeseStrikeMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape and B go back (closing an open dropdown first, or the host's question about going back to the lobby), and so
	// does the gamepad's Menu button, as Escape in a match.
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		if (Key != EKeys::Gamepad_Special_Right && CloseDropdowns())
		{
			return FReply::Handled();
		}
		if (Asking != EMenuAsk::None && Key != EKeys::Gamepad_Special_Right)
		{
			Asking = EMenuAsk::None;
			Refresh();
			return FReply::Handled();
		}
		if (Page == EMenuPage::Pause && bConfirmingBackToLobby)
		{
			ConfirmBackToLobby(false);
			return FReply::Handled();
		}
		if (Controller.IsValid())
		{
			Controller->MenuPressed();
		}
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

const FButtonStyle& SCheeseStrikeMenu::ButtonStyle(EButtonKind Kind) const
{
	switch (Kind)
	{
	case EButtonKind::Primary: return Looks().Primary;
	case EButtonKind::Danger: return Looks().Danger;
	case EButtonKind::Quiet: return Looks().Quiet;
	default: return Looks().Plain;
	}
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakePage()
{
	switch (Page)
	{
	case EMenuPage::Main: return MakeMain();
	case EMenuPage::Find: return MakeFind();
	case EMenuPage::Pause: return MakePause();
	case EMenuPage::Credits: return MakeCredits();
	case EMenuPage::Options: return MakeOptions();
	default: return SNullWidget::NullWidget;
	}
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeSpinner(float Radius) const
{
	return SNew(SCheeseStrikeLoader).Radius(Radius + 2.0f);
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeTitle(const FString& Text)
{
	return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(34, FName("Bold"))).ColorAndOpacity(Cheese).Margin(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
}

float SCheeseStrikeMenu::WrapWidth() const
{
	// The same widths as the content box is given in Refresh.
	return (Page == EMenuPage::Options ? 900.0f : (Page == EMenuPage::Credits ? 640.0f : 400.0f)) - 24.0f;
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeLine(const FString& Text, bool bDim, bool bWrap, float WrapAt)
{
	return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(16)).ColorAndOpacity(bDim ? Muted : Ink).Margin(FMargin(0.0f, 4.0f)).WrapTextAt(bWrap ? (WrapAt > 0.0f ? WrapAt : WrapWidth()) : 0.0f);
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeButton(const FString& Text, TFunction<void()> Do, EButtonKind Kind, TSharedPtr<SWidget> Inside)
{
	TSharedRef<SHorizontalBox> Label = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(18, FName("Bold")))
		];
	if (Inside.IsValid())
	{
		Label->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f)) [ Inside.ToSharedRef() ];
	}
	return SNew(SBox).Padding(FMargin(0.0f, 4.0f))
	[
		Focusable(Text, SNew(SCheeseStrikeButton)
		.ButtonStyle(&ButtonStyle(Kind))
		.ContentPadding(FMargin(18.0f, 10.0f))
		.HAlign(HAlign_Left)
		.OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
		[
			Label
		])
	];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeChoiceButtons(const TArray<FString>& Choices, int32 Chosen, bool bCanPick, TFunction<void(int32)> Pick)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		Row->AddSlot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
		[
			Focusable(Choices[Index], SNew(SCheeseStrikeButton)
			.ButtonStyle(&ButtonStyle(Index == Chosen ? EButtonKind::Primary : EButtonKind::Plain))
			.IsEnabled(bCanPick)
			.ContentPadding(FMargin(12.0f, 6.0f))
			.OnClicked_Lambda([Pick, Index]() { Pick(Index); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(FText::FromString(Choices[Index])).Font(Font(15, FName("Bold")))
			])
		];
	}
	return Row;
}

bool SCheeseStrikeMenu::IsDropdownOpen() const
{
	for (const TWeakPtr<SMenuAnchor>& Each : Dropdowns)
	{
		const TSharedPtr<SMenuAnchor> Anchor = Each.Pin();
		if (Anchor.IsValid() && Anchor->IsOpen())
		{
			return true;
		}
	}
	return false;
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeReadOnly(TAttribute<FText> Value)
{
	return SNew(SBorder)
		.BorderImage(&Looks().ReadOnlyValue)
		.Padding(FMargin(16.0f, 10.0f))
		[
			SNew(STextBlock).Text(Value).Font(Font(18, FName("Bold"))).ColorAndOpacity(Ink)
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeDropdown(const TArray<FString>& Choices, TFunction<int32()> GetChosen, bool bCanPick, TFunction<void(int32)> Pick, TArray<const FSlateBrush*> Pictures)
{
	// The choice shows as it is now, so that a rule changed (here or by the host) needs no new lobby.
	const TAttribute<FText> Shown = TAttribute<FText>::CreateLambda([Choices, GetChosen]()
	{
		const int32 Chosen = GetChosen();
		return FText::FromString(Choices.IsValidIndex(Chosen) ? Choices[Chosen] : FString(TEXT("-")));
	});
	if (!bCanPick)
	{
		// What the host chose, as plain as the host's own dropdown shut, with nothing to press.
		return MakeReadOnly(Shown);
	}
	// The anchor is made after its list's buttons, which close it: they reach it through this.
	const TSharedRef<TWeakPtr<SMenuAnchor>> Holder = MakeShared<TWeakPtr<SMenuAnchor>>();
	// The list's chosen item, which takes the focus as it opens, and the dropdown's own button, which has it back as it shuts.
	const TSharedRef<TWeakPtr<SWidget>> ChosenItem = MakeShared<TWeakPtr<SWidget>>();
	const TSharedRef<TWeakPtr<SWidget>> Opener = MakeShared<TWeakPtr<SWidget>>();
	const auto List = [this, Holder, ChosenItem, Opener, Choices, GetChosen, Pick, Pictures]() -> TSharedRef<SWidget>
	{
		// Made as it opens, with what is chosen then.
		const int32 Chosen = GetChosen();
		const TSharedPtr<SMenuAnchor> Anchor = Holder->Pin();
		const float Width = Anchor.IsValid() ? static_cast<float>(Anchor->GetCachedGeometry().GetLocalSize().X) : 0.0f;
		TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
		TArray<TSharedPtr<SWidget>> Buttons;
		for (int32 Index = 0; Index < Choices.Num(); ++Index)
		{
			TSharedRef<SHorizontalBox> Inside = SNew(SHorizontalBox);
			if (Pictures.IsValidIndex(Index) && Pictures[Index] != nullptr)
			{
				Inside->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
				[
					SNew(SBox).WidthOverride(64.0f).HeightOverride(36.0f) [ SNew(SImage).Image(Pictures[Index]) ]
				];
			}
			Inside->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Choices[Index])).Font(Font(18, FName("Bold")))
			];
			TSharedRef<SButton> Item = SNew(SCheeseStrikeButton)
				.ButtonStyle(&ButtonStyle(Index == Chosen ? EButtonKind::Primary : EButtonKind::Quiet))
				.ContentPadding(FMargin(12.0f, 7.0f))
				.OnClicked_Lambda([Holder, Pick, Index]()
				{
					// Shut first, so that the lobby may be drawn again with the choice.
					if (const TSharedPtr<SMenuAnchor> Open = Holder->Pin())
					{
						Open->SetIsOpen(false);
					}
					Pick(Index);
					return FReply::Handled();
				})
				[
					Inside
				];
			Buttons.Add(Item);
			if (Index == Chosen || (Index == 0 && !Choices.IsValidIndex(Chosen)))
			{
				*ChosenItem = Item;
			}
			Items->AddSlot().AutoHeight().Padding(FMargin(0.0f, 1.0f)) [ Item ];
		}
		return SNew(SCheeseStrikeDropList)
			.Items(Buttons)
			.OnClose_Lambda([this, Holder, Opener]()
			{
				if (const TSharedPtr<SMenuAnchor> Open = Holder->Pin())
				{
					Open->SetIsOpen(false);
				}
				DropFocus.Reset();
				GiveFocus(Opener->Pin());
			})
			[
				SNew(SBox).MinDesiredWidth(Width)
				[
					SNew(SBorder).BorderImage(&Looks().DropdownList).Padding(FMargin(4.0f))
					[
						Items
					]
				]
			];
	};
	TSharedRef<SMenuAnchor> Anchor = SNew(SMenuAnchor)
		.Placement(MenuPlacement_ComboBox)
		.Method(EPopupMethod::UseCurrentWindow)
		.ShowMenuBackground(false)
		.OnGetMenuContent_Lambda(List);
	*Holder = Anchor;
	Dropdowns.Add(Anchor);
	TWeakPtr<SMenuAnchor> Weak = Anchor;
	TSharedRef<SButton> Button = SNew(SCheeseStrikeButton)
		.ButtonStyle(&ButtonStyle(EButtonKind::Plain))
		.ContentPadding(FMargin(16.0f, 10.0f))
		.OnClicked_Lambda([this, Weak, ChosenItem]()
		{
			if (const TSharedPtr<SMenuAnchor> Open = Weak.Pin())
			{
				Open->SetIsOpen(!Open->IsOpen());
				// Opened, its chosen item has the focus, for a gamepad to pick with.
				DropFocus = Open->IsOpen() ? *ChosenItem : TWeakPtr<SWidget>();
				KeepFocus();
			}
			return FReply::Handled();
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Shown).Font(Font(18, FName("Bold")))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SCheeseStrikeDropArrow).Open_Lambda([Weak]() { const TSharedPtr<SMenuAnchor> Open = Weak.Pin(); return Open.IsValid() && Open->IsOpen(); })
			]
		];
	*Opener = Button;
	DropdownButtons.Add(Button);
	Anchor->SetContent(Focusable(FString::Printf(TEXT("Dropdown %d"), Dropdowns.Num()), Button));
	return Anchor;
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeRuleRow(const FString& Label, TSharedRef<SWidget> Setter)
{
	return SNew(SBox).Padding(FMargin(0.0f, 6.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(CheeseStrikeMenu::RuleLabelWidth)
				[
					SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(18)).ColorAndOpacity(Muted)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center) [ Setter ]
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeMain()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	const UCheeseStrikeSessionSubsystem* Sessions = SessionsOf(Player.Get());
	const bool bHosting = Sessions != nullptr && Sessions->GetPending() == ESessionPending::Hosting;
	// Hosting takes a moment (offering the match, then reloading the level into its lobby): the button says so, turning,
	// and nothing else can be pressed meanwhile.
	TSharedRef<SWidget> Host = MakeButton(TEXT("HOST A MATCH"), [Player]() { if (Player.IsValid()) Player->HostFromMenu(); });
	if (bHosting)
	{
		Host = SNew(SBox).Padding(FMargin(0.0f, 4.0f))
			[
				SNew(SBorder).BorderImage(&Looks().PrimaryFace).Padding(FMargin(18.0f, 10.0f)).ForegroundColor(FLinearColor::White)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ SNew(STextBlock).Text(FText::FromString(TEXT("HOSTING A MATCH"))).Font(Font(18, FName("Bold"))).ColorAndOpacity(FLinearColor::White) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.0f, 0.0f, 0.0f, 0.0f)) [ MakeSpinner(9.0f) ]
				]
			];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeFarewell() ]
		+ SVerticalBox::Slot().AutoHeight() [ Host ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SVerticalBox).IsEnabled(!bHosting)
			+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("FIND A MATCH"), [Player]() { if (Player.IsValid()) Player->ChooseMenu(EMenuPage::Find); }) ]
			+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("SETTINGS"), [Player]() { if (Player.IsValid()) Player->ChooseMenu(EMenuPage::Options); }) ]
			+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("CREDITS"), [Player]() { if (Player.IsValid()) Player->ChooseMenu(EMenuPage::Credits); }) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeQuitButton() ]
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeFarewell()
{
	UCheeseStrikeSessionSubsystem* Sessions = SessionsOf(Controller.Get());
	const CheeseStrikeSessions::EFarewell Farewell = Sessions != nullptr ? Sessions->GetFarewell() : CheeseStrikeSessions::EFarewell::None;
	if (Farewell == CheeseStrikeSessions::EFarewell::None)
	{
		return SNullWidget::NullWidget;
	}
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	// A red-edged note over the buttons, until it is dismissed (or the player hosts or joins again).
	return SNew(SBox).Padding(FMargin(0.0f, 0.0f, 0.0f, 20.0f))
		[
			SNew(SBorder)
			.BorderImage(Looks().Chip(FLinearColor(0.2f, 0.05f, 0.045f, 0.95f), FLinearColor(0.85f, 0.24f, 0.2f)))
			.Padding(FMargin(18.0f, 14.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(CheeseStrikeSessions::FarewellTitle(Farewell))).Font(Font(20, FName("Bold"))).ColorAndOpacity(FLinearColor(1.0f, 0.62f, 0.55f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 8.0f)) [ MakeLine(CheeseStrikeSessions::FarewellText(Farewell), false, true, WrapWidth() - 36.0f) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
				[
					MakeButton(TEXT("OK"), [Player]()
					{
						if (UCheeseStrikeSessionSubsystem* Them = SessionsOf(Player.Get()))
						{
							Them->DismissFarewell();
						}
					})
				]
			]
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeCredits()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (int32 Index = 0; Index < CheeseStrikeCredits::NumCredits(); ++Index)
	{
		const CheeseStrikeCredits::FCredit& Credit = CheeseStrikeCredits::Credits[Index];
		List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 2.0f)) [ SNew(STextBlock).Text(FText::FromString(FString(Credit.Heading).ToUpper())).Font(Font(15, FName("Bold"))).ColorAndOpacity(Cheese).WrapTextAt(WrapWidth()) ];
		List->AddSlot().AutoHeight() [ MakeLine(Credit.Text) ];
	}
	// Up and down scroll the list (OnNavigation).
	TSharedPtr<SScrollBox> Scroll;
	TSharedRef<SWidget> CreditsPage = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("CREDITS")) ]
		+ SVerticalBox::Slot().FillHeight(1.0f) [ SAssignNew(Scroll, SScrollBox) + SScrollBox::Slot() [ List ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeButton(TEXT("BACK"), [Player]() { if (Player.IsValid()) Player->BackFromMenu(); }) ];
	CreditsScroll = Scroll;
	return CreditsPage;
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeFind()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	UCheeseStrikeSessionSubsystem* Sessions = SessionsOf(Player.Get());
	const bool bJoining = Sessions != nullptr && Sessions->GetPending() == ESessionPending::Joining;
	const bool bSearching = Sessions != nullptr && Sessions->IsBusy() && !bJoining;
	// What is going on, with a spinner while it goes on: joining a friend's game until its lobby comes.
	const FString Status = bJoining ? FString(TEXT("Joining the lobby")) : (Sessions != nullptr ? Sessions->GetStatus() : FString(TEXT("No online service")));
	TSharedRef<SHorizontalBox> StatusRow = SNew(SHorizontalBox);
	if (bJoining || bSearching)
	{
		StatusRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f)) [ MakeSpinner(9.0f) ];
	}
	StatusRow->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center) [ MakeLine(Status, !bJoining, true, WrapWidth() - 28.0f) ];
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("FIND A MATCH")) ]
		+ SVerticalBox::Slot().AutoHeight() [ StatusRow ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f))
		[
			// Nothing to press while joining: the game picked is on its way.
			SNew(SVerticalBox).IsEnabled(!bJoining)
			+ SVerticalBox::Slot().AutoHeight() [ MakeLine(TEXT("Matches are the games of your friends, and the ones you're invited to. The friends on the right are their games; there's no public list."), true, true) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeButton(TEXT("BACK"), [Player]() { if (Player.IsValid()) Player->BackFromMenu(); }) ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeFriendsPanel(bool bOpen)
{
	// Rounded, like every card: white, tinted by the border's colour.
	static const FSlateRoundedBoxBrush FriendsCardBrush(FLinearColor::White, 12.0f);
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	UCheeseStrikeSessionSubsystem* Sessions = SessionsOf(Player.Get());
	if (!bOpen)
	{
		return SNew(SBorder).BorderImage(&FriendsCardBrush).BorderBackgroundColor(PanelColour).Padding(FMargin(12.0f, 24.0f))
		[
			MakeLine(TEXT("FRIENDS"), false, false)
		];
	}
	FriendBrushes.Reset();
	TSharedRef<SVerticalBox> Friends = SNew(SVerticalBox);
	if (Sessions != nullptr)
	{
		const TArray<FCheeseStrikeFriend>& List = Sessions->GetFriends();
		if (List.IsEmpty())
		{
			Friends->AddSlot().AutoHeight() [ MakeLine(TEXT("No friends to show."), true, true, 330.0f) ];
		}
		// In sections, each with a header that folds it. Who is in our party is by Steam's lobby id, or by name among the players here.
		TArray<FString> HereNames;
		if (const ACheeseStrikeGameState* State = Player.IsValid() ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr)
		{
			for (const APlayerState* Each : State->PlayerArray)
			{
				if (Each != nullptr && !Each->IsABot())
				{
					HereNames.Add(Each->GetPlayerName());
				}
			}
		}
		TArray<int32> Listed[CheeseStrikeFriends::NumSections];
		for (int32 Index = 0; Index < List.Num(); ++Index)
		{
			const FCheeseStrikeFriend& Each = List[Index];
			const bool bParty = Each.bInMyParty || (Sessions->IsInAMatch() && HereNames.Contains(Each.Name));
			const CheeseStrikeFriends::ESection Section = CheeseStrikeFriends::SectionFor(bParty, Each.Status, Each.Permission, CheeseStrikeFriends::HasSpace(Each.Humans), CheeseStrikeFriends::PlayedBefore(Each.bRunningThisGame, Each.bPublishesStatus));
			Listed[static_cast<int32>(Section)].Add(Index);
		}
		for (int32 Section = 0; Section < CheeseStrikeFriends::NumSections; ++Section)
		{
			// Every section shows, with a count of 0 when nobody is in it.
			Friends->AddSlot().AutoHeight().Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f)) [ MakeSectionHeader(static_cast<CheeseStrikeFriends::ESection>(Section), Listed[Section].Num()) ];
			if (!ClosedFriendSections.Contains(static_cast<uint8>(Section)))
			{
				for (const int32 Index : Listed[Section])
				{
					Friends->AddSlot().AutoHeight().Padding(FMargin(0.0f, 4.0f)) [ MakeFriendRow(Index) ];
				}
			}
		}
	}
	// In the game (a lobby or a match): who is in it, and the host's Kick.
	TSharedRef<SVerticalBox> Players = SNew(SVerticalBox);
	const bool bHost = Player.IsValid() && Player->IsLocalController() && Player->HasAuthority();
	const bool bInGame = Sessions != nullptr && Sessions->IsInAMatch();
	if (bInGame)
	{
		if (const ACheeseStrikeGameState* State = Player.IsValid() ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr)
		{
			for (const APlayerState* Each : State->PlayerArray)
			{
				if (Each == nullptr || Each->IsABot())
				{
					continue;
				}
				const bool bSelf = Each == Player->PlayerState;
				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center) [ MakeLine(Each->GetPlayerName() + (bSelf ? TEXT("  (you)") : TEXT("")), false, false) ];
				if (CheeseStrikeFriends::CanKick(bHost, bSelf))
				{
					const ACheeseStrikePlayerState* Who = Cast<ACheeseStrikePlayerState>(Each);
					Row->AddSlot().AutoWidth().VAlign(VAlign_Center) [ MakeButton(TEXT("KICK"), [Player, Who]() { if (Player.IsValid() && Who != nullptr) Player->KickFromMenu(const_cast<ACheeseStrikePlayerState*>(Who)); }, EButtonKind::Danger) ];
				}
				Players->AddSlot().AutoHeight().Padding(FMargin(0.0f, 2.0f)) [ Row ];
			}
		}
	}
	return SNew(SBorder).BorderImage(&FriendsCardBrush).BorderBackgroundColor(PanelColour).Padding(FMargin(24.0f, 28.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("FRIENDS")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeLine(Sessions != nullptr ? Sessions->GetStatus() : FString(TEXT("No online service")), true, true, 330.0f) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f)) [ bInGame ? MakePartyPermission(bHost, Sessions) : SNullWidget::NullWidget ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f)) [ MakeButton(TEXT("REFRESH"), [Player]() { if (Player.IsValid()) Player->FindFromMenu(); }) ]
		+ SVerticalBox::Slot().FillHeight(1.0f) [ SNew(SScrollBox) + SScrollBox::Slot() [ Friends ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 14.0f, 0.0f, 0.0f)) [ bInGame ? MakeLine(TEXT("In this game"), true, false) : SNullWidget::NullWidget ]
		+ SVerticalBox::Slot().AutoHeight() [ Players ]
	];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakePartyPermission(bool bHost, UCheeseStrikeSessionSubsystem* Sessions)
{
	// The host's choice, a property of every player in the party (it rides on the session): the host picks, the others read it.
	using namespace CheeseStrikeFriends;
	const TArray<FString> Choices = { PermissionLabel(EPermission::InviteOnly), PermissionLabel(EPermission::InvitesAndFriends) };
	const TWeakObjectPtr<UCheeseStrikeSessionSubsystem> Weak = Sessions;
	// A dropdown like the lobby's rules, the host's to change; the others see the choice in a plain box.
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeLine(TEXT("Who can join the party"), true, false) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
		[
			MakeDropdown(Choices, [Weak]() { return Weak.IsValid() && Weak->FriendsMayJoinHere() ? 1 : 0; }, bHost,
				[Weak](int32 Picked) { if (Weak.IsValid()) Weak->SetFriendsMayJoin(Picked == 1); })
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeSectionHeader(CheeseStrikeFriends::ESection Section, int32 Count)
{
	const uint8 Id = static_cast<uint8>(Section);
	const bool bClosed = ClosedFriendSections.Contains(Id);
	const FString Text = FString::Printf(TEXT("%s %s (%d)"), bClosed ? TEXT(">") : TEXT("v"), *CheeseStrikeFriends::SectionTitle(Section).ToUpper(), Count);
	return SNew(SCheeseStrikeButton)
		.ButtonStyle(&ButtonStyle(EButtonKind::Quiet))
		.ContentPadding(FMargin(8.0f, 4.0f))
		.HAlign(HAlign_Left)
		.OnClicked_Lambda([this, Id]()
		{
			if (ClosedFriendSections.Contains(Id))
			{
				ClosedFriendSections.Remove(Id);
			}
			else
			{
				ClosedFriendSections.Add(Id);
			}
			// Drawn again at the next refresh.
			ShownFriendsKey.Reset();
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(15, FName("Bold"))).ColorAndOpacity(Muted)
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeFriendRow(int32 Index)
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	UCheeseStrikeSessionSubsystem* Sessions = SessionsOf(Player.Get());
	const FCheeseStrikeFriend& Friend = Sessions->GetFriends()[Index];
	const bool bInAGame = Sessions != nullptr && Sessions->IsInAMatch();
	TSharedRef<SWidget> Action = SNew(SSpacer);
	if (Friend.State == CheeseStrikeFriends::EState::Join)
	{
		Action = MakeButton(TEXT("JOIN"), [Sessions, Index]() { Sessions->JoinFriend(Index); }, EButtonKind::Primary);
	}
	else if (bInAGame && !Friend.bInMyParty && CheeseStrikeFriends::PlayedBefore(Friend.bRunningThisGame, Friend.bPublishesStatus) && CheeseStrikeFriends::CanInvite(true, false) && Friend.State != CheeseStrikeFriends::EState::Full)
	{
		Action = MakeButton(TEXT("INVITE"), [Sessions, Index]() { Sessions->InviteFriend(Index); });
	}
	// The friend's picture, when Steam has it (else a space of the same size).
	TSharedRef<SWidget> Picture = SNew(SSpacer).Size(FVector2D(40.0f, 40.0f));
	if (UTexture2D* Texture = Sessions != nullptr ? Sessions->AvatarOf(Index) : nullptr)
	{
		TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
		Brush->SetResourceObject(Texture);
		Brush->ImageSize = FVector2D(40.0f, 40.0f);
		FriendBrushes.Add(Brush);
		Picture = SNew(SImage).Image(Brush.Get());
	}
	// Hovering the row says what the panel knows of them. Whether they have played before is what their presence shows (see PlayedBefore).
	const bool bOwns = CheeseStrikeFriends::PlayedBefore(Friend.bRunningThisGame, Friend.bPublishesStatus);
	const FString Hover = CheeseStrikeFriends::Tooltip(bOwns, Friend.Status, Friend.Permission, CheeseStrikeFriends::HasSpace(Friend.Humans), Friend.Humans);
	const bool bInAParty = Friend.Status == CheeseStrikeFriends::EStatus::InLobby || Friend.Status == CheeseStrikeFriends::EStatus::InMatch;
	return SNew(SHorizontalBox)
		.ToolTipText(FText::FromString(Hover))
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f)) [ Picture ]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight() [ MakeLine(Friend.Name, false, false) ]
			+ SVerticalBox::Slot().AutoHeight() [ MakeLine(bInAParty && (Friend.State == CheeseStrikeFriends::EState::Join || Friend.State == CheeseStrikeFriends::EState::Full) ? CheeseStrikeFriends::StateLine(Friend.State, Friend.Humans) : CheeseStrikeFriends::StatusLabel(Friend.Status), true, false) ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Action ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeNameChip(const CheeseStrikeRules::FRosterEntry& Seat, bool bHost, const FLinearColor& Colour, bool bMine, TFunction<void()> Click)
{
	// A player's chip is the side's colour, deep, edged in it; a bot's the same, dimmer (its icon says it is one); an empty
	// seat only its edge, faint. The player's own is edged in cheese, so that they find themself. A pale side (the Mozza
	// Radicals' white) is darkened further, so that the white name stays clear on it.
	const float Deep = FMath::Min(1.0f, 0.42f / FMath::Max(0.05f, Colour.GetLuminance()));
	const FLinearColor Fill = Seat.bEmpty ? FLinearColor(Colour.R, Colour.G, Colour.B, 0.16f) : (Seat.bBot ? Shade(Colour, 0.2f * Deep) : Shade(Colour, 0.4f * Deep));
	const FLinearColor Edge = Seat.bEmpty ? Shade(Colour, 0.55f, 0.55f) : (Seat.bBot ? FLinearColor::Transparent : (bMine ? Cheese : Colour));
	TSharedRef<SWidget> Inside = SNullWidget::NullWidget;
	if (Seat.bEmpty)
	{
		Inside = SNew(STextBlock).Text(FText::FromString(TEXT("Invite a friend"))).Font(Font(18)).ColorAndOpacity(Shade(Colour, 0.8f, 0.6f));
	}
	else
	{
		// A long name (Steam allows 32 letters) is written smaller, and cut short with an ellipsis if it still doesn't fit.
		const FString Name = Seat.Name.Left(CheeseStrikeMenu::LongestName);
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
			[
				SNew(SBox).WidthOverride(28.0f).HeightOverride(28.0f)
				[
					SNew(SImage).Image(Seat.bBot ? Looks().BotIcon.Get() : Looks().PlayerIcon.Get()).ColorAndOpacity(Seat.bBot ? Shade(Colour, 0.9f) : FLinearColor::White)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Name)).Font(Font(CheeseStrikeMenu::SeatNameSize(Name), Seat.bBot ? FName("Regular") : FName("Bold")))
				.ColorAndOpacity(Seat.bBot ? FLinearColor(0.78f, 0.77f, 0.74f) : FLinearColor::White)
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				.ToolTipText(Name.Len() > 16 ? FText::FromString(Name) : FText())
			];
		if (bHost)
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SBox).WidthOverride(28.0f).HeightOverride(28.0f) [ SNew(SImage).Image(Looks().CrownIcon.Get()) ]
			];
		}
		Inside = Row;
	}
	if (!Click)
	{
		return SNew(SBorder)
			.BorderImage(Looks().Chip(Fill, Edge))
			.Padding(FMargin(16.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				Inside
			];
	}
	// A seat that does something when clicked: brighter under the mouse, and ringed when a gamepad has it.
	return Focusable(FString::Printf(TEXT("Seat %s"), Seat.bEmpty ? TEXT("open") : *Seat.Name), SNew(SCheeseStrikeButton)
		.ButtonStyle(Looks().SeatButton(Fill, Edge))
		.ContentPadding(FMargin(16.0f, 0.0f))
		.VAlign(VAlign_Center)
		.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); })
		[
			Inside
		]);
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeSeatColumn(const FString& Title, const FLinearColor& Colour, const TArray<CheeseStrikeRules::FRosterEntry>& Seats, int32 Team)
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	const ACheeseStrikeGameState* Match = Player.IsValid() && Player->GetWorld() != nullptr ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const FString HostName = Match != nullptr ? Match->GetHostName() : FString();
	const ACheeseStrikePlayerState* Mine = Player.IsValid() ? Player->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	const FString MyName = Mine != nullptr ? Mine->GetPlayerName() : FString();
	// The seats change hands only in a mode of sides, before the countdown: a seat on the other side takes the player over
	// to it, and the host moves a bot by clicking it.
	const bool bSidesOpen = Team != INDEX_NONE && Match != nullptr && Match->HasTeams() && !Match->IsCountingDown() && HaveMatch() && LobbyStatusNow() != CheeseStrikeMenu::ELobbyStatus::Travelling;
	const bool bHostHere = Player.IsValid() && Player->HasAuthority();
	const bool bOtherSide = Mine != nullptr && Mine->GetTeam() != Team;
	// The side's name sits in a band as tall as the page's title ("LOBBY", with its margin) on the other side of the lobby, so that
	// the first seat starts level with the top of the map's picture there (each seat has 4 of padding above it).
	const TSharedRef<SWidget> TitleOfThePage = MakeTitle(TEXT("LOBBY"));
	TitleOfThePage->SlatePrepass(1.0f);
	const float Band = TitleOfThePage->GetDesiredSize().Y - 4.0f;
	TSharedRef<SVerticalBox> Column = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(Band).Padding(FMargin(4.0f, 0.0f, 0.0f, 12.0f)).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Title.ToUpper())).Font(Font(26, FName("Bold"))).ColorAndOpacity(Colour)
			]
		];
	// The seats share the column's height: the column is the side, full or not.
	for (const CheeseStrikeRules::FRosterEntry& Seat : Seats)
	{
		const bool bHost = !Seat.bBot && !Seat.bEmpty && !HostName.IsEmpty() && Seat.Name == HostName;
		const bool bMe = !Seat.bBot && !Seat.bEmpty && !MyName.IsEmpty() && Seat.Name == MyName;
		TFunction<void()> Click;
		if (bSidesOpen && bHostHere && Seat.bBot && Seat.Bot != INDEX_NONE)
		{
			const int32 Bot = Seat.Bot;
			Click = [Player, Bot]() { if (Player.IsValid()) Player->MoveBotFromMenu(Bot); };
		}
		else if (bSidesOpen && bOtherSide)
		{
			Click = [Player]() { if (Player.IsValid()) Player->SwitchSideFromMenu(); };
		}
		Column->AddSlot().FillHeight(1.0f).MaxHeight(84.0f).Padding(FMargin(0.0f, 4.0f)) [ MakeNameChip(Seat, bHost, Colour, bMe, Click) ];
	}
	return Column;
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeSeatColumns(bool bTeams)
{
	const ACheeseStrikeGameState* Match = Controller.IsValid() && Controller->GetWorld() != nullptr ? Controller->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	TSharedRef<SHorizontalBox> Columns = SNew(SHorizontalBox);
	if (bTeams)
	{
		// A side each: its players in the order they joined, then the bots that will stand in in the order they were added,
		// then its empty seats, eight in all.
		for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
		{
			const TArray<CheeseStrikeRules::FRosterEntry> Seats = CheeseStrikeRules::WithEmptySeats(Match != nullptr ? Match->TeamColumn(Team) : TArray<CheeseStrikeRules::FRosterEntry>(), CheeseStrikeRules::TeamSize, Team);
			Columns->AddSlot().FillWidth(1.0f).Padding(FMargin(Team > 0 ? 20.0f : 0.0f, 0.0f, 0.0f, 0.0f)) [ MakeSeatColumn(CheeseStrikeRules::TeamNames[Team], CheeseStrikeModes::TeamColour(Team), Seats, Team) ];
		}
		return Columns;
	}
	// Everyone for themself: every seat in one colour, with no side's name, in the order they were taken (the players as they
	// joined, then the bots as they were added); laid out in columns of a side's height, so that the seats are the size they
	// are in a team match, the first column filled first.
	const TArray<CheeseStrikeRules::FRosterEntry> Seats = CheeseStrikeRules::WithEmptySeats(Match != nullptr ? Match->Seats() : TArray<CheeseStrikeRules::FRosterEntry>(), CheeseStrikeRules::SeatCount, INDEX_NONE);
	const int32 PerColumn = FMath::DivideAndRoundUp(Seats.Num(), CheeseStrikeMenu::FreeForAllColumns);
	for (int32 Column = 0; Column < CheeseStrikeMenu::FreeForAllColumns; ++Column)
	{
		TArray<CheeseStrikeRules::FRosterEntry> Part;
		for (int32 Index = Column * PerColumn; Index < FMath::Min(Seats.Num(), (Column + 1) * PerColumn); ++Index)
		{
			Part.Add(Seats[Index]);
		}
		Columns->AddSlot().FillWidth(1.0f).Padding(FMargin(Column > 0 ? 20.0f : 0.0f, 0.0f, 0.0f, 0.0f)) [ MakeSeatColumn(Column == 0 ? FString(TEXT("Everyone for themself")) : FString(), CheeseStrikeModes::TeamColour(INDEX_NONE), Part) ];
	}
	return Columns;
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeStepper(const FString& Label, const FString& Value, bool bCanPick, TFunction<void()> Less, TFunction<void()> More)
{
	const auto Button = [this, bCanPick](const FString& Text, TFunction<void()> Do)
	{
		return Focusable(Text, SNew(SCheeseStrikeButton)
			.ButtonStyle(&ButtonStyle(EButtonKind::Plain))
			.IsEnabled(bCanPick)
			.ContentPadding(FMargin(14.0f, 6.0f))
			.OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(15, FName("Bold")))
			]);
	};
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth() [ Button(TEXT("-"), Less) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(16.0f, 0.0f))
		[
			SNew(SBox).MinDesiredWidth(150.0f)
			[
				SNew(STextBlock).Text(FText::FromString(Value)).Font(Font(18, FName("Bold"))).ColorAndOpacity(Ink).Justification(ETextJustify::Center)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth() [ Button(TEXT("+"), More) ];
	if (Label.IsEmpty())
	{
		return Row;
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 4.0f)) [ MakeLine(Label, true) ]
		+ SVerticalBox::Slot().AutoHeight() [ Row ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeMapPicture(TFunction<int32()> GetMap)
{
	// The map as it is now: the host picking another changes the picture and its name in place.
	const auto Known = [GetMap]() { const int32 MapIndex = GetMap(); return MapIndex >= 0 && MapIndex < CheeseStrikeRules::NumMaps() ? MapIndex : INDEX_NONE; };
	return SNew(SBox).WidthOverride(CheeseStrikeMenu::MapPictureWidth).HeightOverride(CheeseStrikeMenu::MapPictureWidth * 9.0f / 16.0f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot() [ SNew(SBorder).BorderImage(&Looks().MapBack) ]
			+ SOverlay::Slot()
			[
				SNew(SImage).Image_Lambda([Known]() -> const FSlateBrush*
				{
					const int32 MapIndex = Known();
					const FSlateBrush* Picture = MapIndex != INDEX_NONE ? Looks().MapPicture(CheeseStrikeRules::Maps[MapIndex].Picture) : nullptr;
					return Picture != nullptr ? Picture : FCoreStyle::Get().GetBrush("NoBrush");
				})
			]
			+ SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Fill)
			[
				SNew(SBorder).BorderImage(&Looks().MapNameBar).Padding(FMargin(14.0f, 8.0f))
				[
					SNew(STextBlock).Font(Font(22, FName("Bold"))).ColorAndOpacity(Cheese).Text_Lambda([Known]()
					{
						const int32 MapIndex = Known();
						return FText::FromString(MapIndex != INDEX_NONE ? Upper(CheeseStrikeRules::Maps[MapIndex].Name) : FString(TEXT("THIS LEVEL")));
					})
				]
			]
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeLobbyRules()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	const ACheeseStrikeGameState* Match = Player.IsValid() && Player->GetWorld() != nullptr ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bHost = Player.IsValid() && Player->HasAuthority();
	// The rules' values are read as they are drawn (Now), and the host's picks with the others as they are then, so that a
	// rule changed redraws nothing but its own value (LobbyKey leaves them out): drawing the lobby afresh flickers. What
	// changes which rows there are (the mode) or what they offer (the bots' counts, by the seats) does draw it again.
	const auto Now = [Player]() -> const ACheeseStrikeGameState*
	{
		return Player.IsValid() && Player->GetWorld() != nullptr ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	};
	const auto KillLimitNow = [Now]() { const ACheeseStrikeGameState* State = Now(); return State != nullptr ? State->GetKillLimit() : CheeseStrikeRules::KillLimit; };
	const auto RespawnDelayNow = [Now]() { const ACheeseStrikeGameState* State = Now(); return State != nullptr ? State->GetRespawnDelay() : CheeseStrikeRules::RespawnDelay; };
	const CheeseStrikeModes::EMode ModeNow = Match != nullptr ? Match->GetMode() : CheeseStrikeModes::EMode::FreeForAll;
	const CheeseStrikeModes::FModeInfo& ModeRules = CheeseStrikeModes::Info(ModeNow);
	// The modes in the lobby's order, free-for-all last.
	TArray<FString> ModeNames;
	int32 ModeChosen = INDEX_NONE;
	for (int32 Index = 0; Index < CheeseStrikeModes::NumModes; ++Index)
	{
		ModeNames.Add(Upper(CheeseStrikeModes::Info(CheeseStrikeModes::LobbyOrder[Index]).Name));
		ModeChosen = CheeseStrikeModes::LobbyOrder[Index] == ModeNow ? Index : ModeChosen;
	}
	TArray<FString> RoundNames;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeModes::RoundLimitChoices); ++Index)
	{
		RoundNames.Add(FString::FromInt(CheeseStrikeModes::RoundLimitChoices[Index]));
	}
	const auto RoundsChosen = [Now]()
	{
		const ACheeseStrikeGameState* State = Now();
		const int32 Rounds = State != nullptr ? State->GetRoundLimit() : CheeseStrikeModes::RoundLimit;
		int32 Chosen = INDEX_NONE;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeModes::RoundLimitChoices); ++Index)
		{
			Chosen = CheeseStrikeModes::RoundLimitChoices[Index] == Rounds ? Index : Chosen;
		}
		return Chosen;
	};
	TArray<FString> TimeNames;
	for (const float Seconds : CheeseStrikeModes::RoundTimeChoices)
	{
		TimeNames.Add(FString::Printf(TEXT("%d min"), FMath::RoundToInt(Seconds / 60.0f)));
	}
	const auto TimeChosen = [Now]()
	{
		const ACheeseStrikeGameState* State = Now();
		const float Seconds = State != nullptr ? State->GetRoundTime() : CheeseStrikeModes::RoundTime;
		int32 Chosen = INDEX_NONE;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeModes::RoundTimeChoices); ++Index)
		{
			Chosen = FMath::IsNearlyEqual(CheeseStrikeModes::RoundTimeChoices[Index], Seconds) ? Index : Chosen;
		}
		return Chosen;
	};
	TArray<FString> Limits;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeRules::KillLimitChoices); ++Index)
	{
		Limits.Add(FString::FromInt(CheeseStrikeRules::KillLimitChoices[Index]));
	}
	const auto LimitChosen = [KillLimitNow]()
	{
		int32 Chosen = 0;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeRules::KillLimitChoices); ++Index)
		{
			Chosen = CheeseStrikeRules::KillLimitChoices[Index] == KillLimitNow() ? Index : Chosen;
		}
		return Chosen;
	};
	TArray<FString> Delays;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeRules::RespawnDelayChoices); ++Index)
	{
		Delays.Add(FString::Printf(TEXT("%.0f s"), CheeseStrikeRules::RespawnDelayChoices[Index]));
	}
	const auto DelayChosen = [RespawnDelayNow]()
	{
		int32 Chosen = 0;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeRules::RespawnDelayChoices); ++Index)
		{
			Chosen = FMath::IsNearlyEqual(CheeseStrikeRules::RespawnDelayChoices[Index], RespawnDelayNow()) ? Index : Chosen;
		}
		return Chosen;
	};
	TArray<FString> MapNames;
	TArray<const FSlateBrush*> MapPictures;
	for (int32 Index = 0; Index < CheeseStrikeRules::NumMaps(); ++Index)
	{
		MapNames.Add(Upper(CheeseStrikeRules::Maps[Index].Name));
		MapPictures.Add(Looks().MapPicture(CheeseStrikeRules::Maps[Index].Picture));
	}
	const auto MapChosen = [Now]() { const ACheeseStrikeGameState* State = Now(); return State != nullptr ? State->GetMapIndex() : INDEX_NONE; };
	TArray<FString> Skills;
	for (const CheeseStrikeBots::FSkill& Skill : CheeseStrikeBots::Skills)
	{
		Skills.Add(Upper(Skill.Name));
	}
	const auto SkillChosen = [Now]() { const ACheeseStrikeGameState* State = Now(); return State != nullptr ? State->GetBotSkill() : CheeseStrikeBots::DefaultSkill; };
	// How many bots: from none to every seat the players leave.
	int32 Players = 0;
	for (const CheeseStrikeRules::FRosterEntry& Entry : Match != nullptr ? Match->Roster() : TArray<CheeseStrikeRules::FRosterEntry>())
	{
		Players += Entry.bBot ? 0 : 1;
	}
	const int32 Most = CheeseStrikeRules::MostBots(Players);
	const auto BotsChosen = [Now, Players]() { const ACheeseStrikeGameState* State = Now(); return CheeseStrikeRules::BotsIn(Players, State != nullptr ? State->GetBotsWanted() : CheeseStrikeRules::DefaultBots); };
	const int32 Bots = BotsChosen();
	// Once the countdown has begun, the rules are what the match is played by: the host sees them as a joiner does.
	const bool bCanPick = bHost && Match != nullptr && !Match->IsCountingDown();

	// A row a rule, each showing its choice and opening onto the rest, so that the whole lobby fits on the screen.
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	const auto Row = [this, &Rows](const FString& Label, TSharedRef<SWidget> Setter) { Rows->AddSlot().AutoHeight() [ MakeRuleRow(Label, Setter) ]; };
	Row(TEXT("Map"), MakeDropdown(MapNames, MapChosen, bCanPick, [Player, KillLimitNow, RespawnDelayNow](int32 Index) { if (Player.IsValid()) Player->SetRulesFromMenu(KillLimitNow(), RespawnDelayNow(), Index); }, MapPictures));
	Row(TEXT("Mode"), MakeDropdown(ModeNames, [ModeChosen]() { return ModeChosen; }, bCanPick, [Player](int32 Index)
	{
		if (Player.IsValid() && Index >= 0 && Index < CheeseStrikeModes::NumModes)
		{
			Player->SetModeFromMenu(static_cast<int32>(CheeseStrikeModes::LobbyOrder[Index]));
		}
	}));
	// Wrapped at the width it has, not at the width it had last drawn (AutoWrapText), which a lobby drawn afresh doesn't
	// know for a frame: the summary would stand a word a line, and the whole lobby shrink to fit it for that frame.
	Rows->AddSlot().AutoHeight().Padding(FMargin(CheeseStrikeMenu::RuleLabelWidth, 0.0f, 0.0f, 4.0f))
	[
		SNew(STextBlock).Text(FText::FromString(ModeRules.Summary)).Font(Font(14)).ColorAndOpacity(Muted).WrapTextAt(CheeseStrikeMenu::MapPictureWidth - CheeseStrikeMenu::RuleLabelWidth)
	];
	// What the mode plays to: kills for one or for a side, rounds, or (Gun Game) the ladder alone; the wait to come back where there is one.
	if (ModeRules.WinBy == CheeseStrikeModes::EWinBy::PlayerKills || ModeRules.WinBy == CheeseStrikeModes::EWinBy::TeamKills)
	{
		Row(ModeRules.WinBy == CheeseStrikeModes::EWinBy::TeamKills ? TEXT("Team kills to win") : TEXT("Kills to win"),
			MakeDropdown(Limits, LimitChosen, bCanPick, [Player, RespawnDelayNow](int32 Index) { if (Player.IsValid()) Player->SetRulesFromMenu(CheeseStrikeRules::KillLimitChoices[Index], RespawnDelayNow()); }));
	}
	else if (ModeRules.WinBy == CheeseStrikeModes::EWinBy::Rounds)
	{
		Row(TEXT("Rounds to win"), MakeDropdown(RoundNames, RoundsChosen, bCanPick, [Player](int32 Index) { if (Player.IsValid()) Player->SetRoundsFromMenu(CheeseStrikeModes::RoundLimitChoices[Index]); }));
		Row(TEXT("Round time"), MakeDropdown(TimeNames, TimeChosen, bCanPick, [Player](int32 Index) { if (Player.IsValid()) Player->SetRoundTimeFromMenu(CheeseStrikeModes::RoundTimeChoices[Index]); }));
	}
	else if (ModeRules.WinBy == CheeseStrikeModes::EWinBy::Hill)
	{
		// King of the Hill plays to points, which the kill limit carries.
		TArray<FString> Points;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeHills::TargetChoices); ++Index)
		{
			Points.Add(FString::FromInt(CheeseStrikeHills::TargetChoices[Index]));
		}
		const auto PointsChosen = [KillLimitNow]()
		{
			int32 Chosen = INDEX_NONE;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheeseStrikeHills::TargetChoices); ++Index)
			{
				Chosen = CheeseStrikeHills::TargetChoices[Index] == KillLimitNow() ? Index : Chosen;
			}
			return Chosen;
		};
		Row(ModeRules.bTeams ? TEXT("Team points to win") : TEXT("Points to win"),
			MakeDropdown(Points, PointsChosen, bCanPick, [Player, RespawnDelayNow](int32 Index) { if (Player.IsValid()) Player->SetRulesFromMenu(CheeseStrikeHills::TargetChoices[Index], RespawnDelayNow()); }));
	}
	if (ModeRules.bRespawns)
	{
		Row(TEXT("Back after a death"), MakeDropdown(Delays, DelayChosen, bCanPick, [Player, KillLimitNow](int32 Index) { if (Player.IsValid()) Player->SetRulesFromMenu(KillLimitNow(), CheeseStrikeRules::RespawnDelayChoices[Index]); }));
	}
	TArray<FString> BotCounts;
	for (int32 Count = 0; Count <= FMath::Max(Most, Bots); ++Count)
	{
		BotCounts.Add(Count > 0 && Count >= Most ? FString::Printf(TEXT("%d (every seat)"), Count) : FString::FromInt(Count));
	}
	Row(TEXT("Bots"), MakeDropdown(BotCounts, BotsChosen, bCanPick, [Player](int32 Count) { if (Player.IsValid()) Player->SetBotsFromMenu(Count); }));
	Row(TEXT("Bot skill"), MakeDropdown(Skills, SkillChosen, bCanPick, [Player](int32 Index) { if (Player.IsValid()) Player->SetBotSkillFromMenu(Index); }));
	if (!bHost)
	{
		Rows->AddSlot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f)) [ MakeLine(TEXT("The host sets the rules."), true, false) ];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("LOBBY")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f)) [ MakeMapPicture(MapChosen) ]
		+ SVerticalBox::Slot().AutoHeight() [ Rows ];
}

FString SCheeseStrikeMenu::LobbyKey() const
{
	const ACheeseStrikeGameState* Match = Controller.IsValid() && Controller->GetWorld() != nullptr ? Controller->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const CheeseStrikeMenu::ELobbyStatus Status = LobbyStatusNow();
	if (Match == nullptr || Status == CheeseStrikeMenu::ELobbyStatus::Joining)
	{
		return FString::Printf(TEXT("status %d"), static_cast<int32>(Status));
	}
	const ACheeseStrikePlayerState* Mine = Controller->GetPlayerState<ACheeseStrikePlayerState>();
	// Not the rules' values, which the lobby's rows show as they are (MakeLobbyRules): only what changes its rows (the mode),
	// who can pick (the countdown, in the status) and the seats.
	FString Key = FString::Printf(TEXT("%d %d %s %d %d %s|"), static_cast<int32>(Status), Controller->HasAuthority() ? 1 : 0, *Match->GetHostName(), static_cast<int32>(Match->GetMode()),
		Mine != nullptr ? Mine->GetTeam() : -1, *(Mine != nullptr ? Mine->GetPlayerName() : FString()));
	// Every seat, in its order and on its side (a player going over, a bot moved, the order they came in).
	for (const CheeseStrikeRules::FRosterEntry& Entry : Match->Seats())
	{
		Key += FString::Printf(TEXT("%d%s%d,"), Entry.Team, *Entry.Name, Entry.bBot ? 1 : 0);
	}
	return Key;
}

FString SCheeseStrikeMenu::PageKey() const
{
	FString Key = FString::FromInt(static_cast<int32>(Page));
	if (Page == EMenuPage::Pause)
	{
		Key += bConfirmingBackToLobby ? TEXT("|confirming") : TEXT("");
	}
	const UCheeseStrikeSessionSubsystem* Sessions = SessionsOf(Controller.Get());
	if (Sessions != nullptr && (Page == EMenuPage::Main || Page == EMenuPage::Find))
	{
		Key += FString::Printf(TEXT("|pending %d"), static_cast<int32>(Sessions->GetPending()));
	}
	if (Page == EMenuPage::Find && Sessions != nullptr)
	{
		Key += FString::Printf(TEXT("|busy %d|"), Sessions->IsBusy() ? 1 : 0) + Sessions->GetStatus();
		for (const FString& Found : Sessions->GetFound())
		{
			Key += TEXT("|") + Found;
		}
	}
	if (Page == EMenuPage::Options)
	{
		// What is pressed rather than slid: the sliders show their values by themselves, and drawing the page again would drop one being dragged.
		Key += FString::Printf(TEXT("|tab %d"), static_cast<int32>(OptionsTab));
		if (const UCheeseStrikeSettings* Settings = UCheeseStrikeSettings::Get())
		{
			const FIntPoint Resolution = Settings->GetScreenResolution();
			const FIntPoint Drawn = Settings->GetResolutionShown();
			Key += FString::Printf(TEXT("|%d %d %dx%d %dx%d %d %.0f %d"), Settings->GetOverallScalabilityLevel(), static_cast<int32>(Settings->GetFullscreenMode()), Resolution.X, Resolution.Y, Drawn.X, Drawn.Y,
				Settings->IsVSyncEnabled() ? 1 : 0, Settings->GetFrameRateLimit(), Settings->IsLookInverted() ? 1 : 0) + (Settings->IsAimAssistOn() ? TEXT(" assist") : TEXT(""));
		}
	}
	return Key;
}

bool SCheeseStrikeMenu::HaveMatch() const
{
	const ACheeseStrikePlayerController* Player = Controller.Get();
	const ACheeseStrikeGameState* Match = Player != nullptr && Player->GetWorld() != nullptr ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Match == nullptr)
	{
		return false;
	}
	if (Player->HasAuthority())
	{
		return true;
	}
	// A joiner has the host's match once it has come with the host in it and the joiner too: a lobby drawn before then
	// would be a guess (every seat a bot, nobody crowned).
	return Player->PlayerState != nullptr && Match->PlayerArray.Contains(Player->PlayerState) && Match->PlayerArray.Num() >= 2 && !Match->GetHostName().IsEmpty() && Match->GetBotNames().Num() > 0;
}

FVector2D SCheeseStrikeMenu::LobbyWants() const
{
	// The lobby's margins (Construct) round what it lays out.
	return LobbyContent.IsValid() ? LobbyContent->GetDesiredSize() + FVector2D(2.0 * 56.0, 2.0 * 40.0) : FVector2D::ZeroVector;
}

bool SCheeseStrikeMenu::IsLobbyReady() const
{
	const CheeseStrikeMenu::ELobbyStatus Status = LobbyStatusNow();
	return Page == EMenuPage::Lobby && Status != CheeseStrikeMenu::ELobbyStatus::Joining && Status != CheeseStrikeMenu::ELobbyStatus::Travelling;
}

CheeseStrikeMenu::ELobbyStatus SCheeseStrikeMenu::LobbyStatusNow() const
{
	const ACheeseStrikePlayerController* Player = Controller.Get();
	const ACheeseStrikeGameState* Match = Player != nullptr && Player->GetWorld() != nullptr ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bTravelling = (Match != nullptr && Match->GetMatchState() == MatchState::LeavingMap) || (Player != nullptr && Player->IsLeaving());
	return CheeseStrikeMenu::LobbyStatus(HaveMatch(), bTravelling, Match != nullptr && Match->IsCountingDown(), Match != nullptr ? Match->SecondsToStart() : 0.0f, Player != nullptr && Player->HasAuthority());
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeLobby()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	const ACheeseStrikeGameState* Match = Player.IsValid() && Player->GetWorld() != nullptr ? Player->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bHost = Player.IsValid() && Player->HasAuthority();
	const CheeseStrikeMenu::ELobbyStatus Status = LobbyStatusNow();
	// The seconds count down by themselves, so that the lobby is drawn again only when something in it changes and the spinner turns on unbroken.
	TWeakObjectPtr<const ACheeseStrikeGameState> Watched = Match;
	const auto Seconds = [Watched]() { return FText::FromString(Watched.IsValid() ? FString::Printf(TEXT("%.0f"), FMath::CeilToFloat(Watched->SecondsToStart())) : FString()); };
	// The rules first, so that their items come first in the focus's order (the page's first item is its first rule).
	const TSharedRef<SWidget> Rules = MakeLobbyRules();

	// The host's lobby is the match: leaving it closes it for everyone.
	TSharedRef<SHorizontalBox> Bottom = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ bHost ? MakeCloseLobbyButton() : MakeButton(TEXT("LEAVE"), [Player]() { if (Player.IsValid()) Player->LeaveFromMenu(); }) ];
	// In a mode of sides, until the countdown: over to the other side (clicking a seat there does the same).
	const ACheeseStrikePlayerState* Mine = Player.IsValid() ? Player->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	if (Match != nullptr && Match->HasTeams() && Mine != nullptr && (Status == CheeseStrikeMenu::ELobbyStatus::CanStart || Status == CheeseStrikeMenu::ELobbyStatus::Waiting))
	{
		const int32 Other = 1 - FMath::Clamp(Mine->GetTeam(), 0, 1);
		const bool bRoom = CheeseStrikeRules::HasRoomForAPlayer(Match->Roster(), Other);
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(SBox).IsEnabled(bRoom)
			[
				MakeButton(FString::Printf(TEXT("JOIN %s"), *FString(CheeseStrikeRules::TeamNames[Other]).ToUpper()), [Player]() { if (Player.IsValid()) Player->SwitchSideFromMenu(); })
			]
		];
	}
	Bottom->AddSlot().FillWidth(1.0f) [ SNew(SSpacer) ];
	const auto Saying = [this](const FString& Text)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeSpinner() ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f)) [ MakeLine(Text, false, false) ];
	};
	if (Status == CheeseStrikeMenu::ELobbyStatus::Joining)
	{
		// Nothing of the match yet: no seats or rules to guess at.
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center) [ Saying(TEXT("Joining the lobby")) ];
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.0f) [ MakeTitle(TEXT("LOBBY")) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 24.0f, 0.0f, 0.0f)) [ Bottom ];
	}
	if (Status == CheeseStrikeMenu::ELobbyStatus::Travelling)
	{
		// Where to: the match's map, or back to the lobby after the results. A joiner whose connection to the level being
		// left is already closed goes by where the host told them to go.
		const int32 MapIndex = Match != nullptr ? Match->GetMapIndex() : (Player.IsValid() ? Player->GetLeavingFor() : INDEX_NONE);
		// From a match (one the lobby sent everyone to, or one over), it is back to the lobby.
		const bool bFromAMatch = Match != nullptr && (Match->GetPlayersExpected() > 0 || !Match->GetWinnerName().IsEmpty());
		const bool bToTheMatch = !bFromAMatch && MapIndex >= 0 && MapIndex < CheeseStrikeRules::NumMaps();
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center) [ Saying(bToTheMatch ? FString::Printf(TEXT("Going to %s"), CheeseStrikeRules::Maps[MapIndex].Name) : FString(TEXT("Back to the lobby"))) ];
		if (Match == nullptr)
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.0f) [ MakeTitle(TEXT("LOBBY")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 24.0f, 0.0f, 0.0f)) [ Bottom ];
		}
	}
	else if (Status == CheeseStrikeMenu::ELobbyStatus::Starting)
	{
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center) [ Saying(TEXT("Starting")) ];
	}
	else if (bHost && Status == CheeseStrikeMenu::ELobbyStatus::Counting)
	{
		TSharedRef<SHorizontalBox> Inside = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeSpinner() ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f)) [ SNew(STextBlock).Text_Lambda(Seconds).Font(Font(18, FName("Bold"))) ];
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center) [ MakeButton(TEXT("CANCEL"), [Player]() { if (Player.IsValid()) Player->CancelStartFromMenu(); }, EButtonKind::Danger, Inside) ];
	}
	else if (Status == CheeseStrikeMenu::ELobbyStatus::CanStart)
	{
		// Not alone, nor with a side empty: the button says what is missing, and can't be pressed until it is there.
		const FString WhyNot = Match != nullptr ? CheeseStrikeRules::WhyNotStart(Match->Seats(), Match->HasTeams()) : FString();
		if (WhyNot.IsEmpty())
		{
			Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center) [ MakeButton(TEXT("START THE MATCH"), [Player]() { if (Player.IsValid()) Player->StartMatchFromMenu(); }, EButtonKind::Primary) ];
		}
		else
		{
			const TSharedRef<SWidget> Why = SNew(STextBlock).Text(FText::FromString(WhyNot.ToUpper())).Font(Font(13, FName("Bold")));
			Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).IsEnabled(false) [ MakeButton(TEXT("START THE MATCH"), []() {}, EButtonKind::Primary, Why) ]
			];
		}
	}
	else if (Status == CheeseStrikeMenu::ELobbyStatus::Counting)
	{
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeSpinner() ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock).Text_Lambda([Seconds]() { return FText::FromString(TEXT("Starting in ") + Seconds().ToString()); }).Font(Font(16)).ColorAndOpacity(Ink)
			]
		];
	}
	else
	{
		Bottom->AddSlot().AutoWidth().VAlign(VAlign_Center) [ SNew(STextBlock).Text(FText::FromString(TEXT("Waiting for the host to start."))).Font(Font(16)).ColorAndOpacity(Muted) ];
	}

	// Sides of a team match; everyone in one colour, with no sides, in a free-for-all.
	const ACheeseStrikeGameState* Shown = Controller.IsValid() && Controller->GetWorld() != nullptr ? Controller->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bTeams = Shown == nullptr || Shown->HasTeams();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill).Padding(FMargin(0.0f, 0.0f, CheeseStrikeMenu::LobbyGap, 0.0f))
			[
				SNew(SBox).WidthOverride(CheeseStrikeMenu::MapPictureWidth)
				[
					Rules
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f) [ MakeSeatColumns(bTeams) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 24.0f, 0.0f, 0.0f)) [ Bottom ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakePause()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	if (bConfirmingBackToLobby)
	{
		// The host's Back to Lobby ends the match for everyone, so it asks first.
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("BACK TO LOBBY?")) ]
			+ SVerticalBox::Slot().AutoHeight() [ MakeLine(TEXT("The match ends for everyone, and everyone goes back to the lobby together, bots and all."), true) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeButton(TEXT("END THE MATCH"), [Player]() { if (Player.IsValid()) Player->BackToLobbyFromMenu(); }, EButtonKind::Danger) ]
			+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("KEEP PLAYING"), [this]() { ConfirmBackToLobby(false); }) ];
	}
	// The host takes everyone back to the lobby, keeping the match's group together; a joiner leaves it alone.
	const bool bHost = Player.IsValid() && Player->CanGoBackToLobby();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("PAUSED")) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeLine(TEXT("The match goes on without you."), true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f)) [ MakeButton(TEXT("RESUME"), [Player]() { if (Player.IsValid()) Player->TogglePause(); }, EButtonKind::Primary) ]
		+ SVerticalBox::Slot().AutoHeight() [ MakeButton(TEXT("SETTINGS"), [Player]() { if (Player.IsValid()) Player->ChooseMenu(EMenuPage::Options); }) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			bHost ? MakeButton(TEXT("BACK TO LOBBY"), [this]() { ConfirmBackToLobby(true); }) : MakeButton(TEXT("LEAVE THE MATCH"), [Player]() { if (Player.IsValid()) Player->LeaveFromMenu(); })
		]
		+ SVerticalBox::Slot().AutoHeight() [ MakeQuitButton() ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeOptionRow(const FString& Label, TSharedRef<SWidget> Setter)
{
	return SNew(SBox).Padding(FMargin(0.0f, 7.0f)).MinDesiredHeight(40.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(OptionLabelWidth) [ SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(17)).ColorAndOpacity(Ink) ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center) [ Setter ]
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeSlider(float Min, float Max, float Step, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FString(float)> Show)
{
	// Kept when let go, rather than written out at every step of a drag.
	const auto Keep = []()
	{
		if (UCheeseStrikeSettings* Settings = UCheeseStrikeSettings::Get())
		{
			Settings->SaveSettings();
		}
	};
	// Left and right move it a step at once, a gamepad's and the keyboard's, each kept as it is made; up and down go on to
	// the next row. A drag with the mouse is kept when let go.
	const TSharedRef<TWeakPtr<SSlider>> Made = MakeShared<TWeakPtr<SSlider>>();
	TSharedRef<SSlider> Slider = SNew(SCheeseStrikeSlider)
		.Style(&Looks().Slider)
		.MinValue(Min)
		.MaxValue(Max)
		.StepSize(Step)
		.MouseUsesStep(true)
		.RequiresControllerLock(false)
		.Value_Lambda([Get]() { return Get(); })
		.OnValueChanged_Lambda([Set, Made, Keep](float Value)
		{
			Set(Value);
			const TSharedPtr<SSlider> Moved = Made->Pin();
			if (Moved.IsValid() && !Moved->HasMouseCapture())
			{
				Keep();
			}
		})
		.OnMouseCaptureEnd_Lambda(Keep)
		.OnControllerCaptureEnd_Lambda(Keep);
	*Made = Slider;
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			Focusable(TEXT("Slider"), Slider)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(16.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(SBox).WidthOverride(150.0f)
			[
				SNew(STextBlock).Text_Lambda([Get, Show]() { return FText::FromString(Show(Get())); }).Font(Font(17, FName("Bold"))).ColorAndOpacity(Ink)
			]
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeOptions()
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	TArray<FString> Tabs = {TEXT("CONTROLS"), TEXT("AIMING"), TEXT("SOUND"), TEXT("GRAPHICS")};
	const bool bInMatch = Page == EMenuPage::Options && Player.IsValid() && Player->IsPaused();
	const bool bControls = OptionsTab == EOptionsTab::Controls;
	// The controls are a table that scrolls under its heading; the other tabs' rows scroll as one.
	TSharedPtr<SScrollBox> Scroll;
	TSharedRef<SWidget> SettingsPage = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("SETTINGS")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 18.0f))
		[
			MakeChoiceButtons(Tabs, static_cast<int32>(OptionsTab), true, [this](int32 Index)
			{
				OptionsTab = static_cast<EOptionsTab>(Index);
				Refresh();
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 20.0f, 6.0f)) [ bControls ? ControlsRow(TEXT("ACTION"), TEXT("KEYBOARD AND MOUSE"), TEXT("XBOX"), TEXT("NINTENDO"), true) : SNullWidget::NullWidget ]
		// Clear of the scroll bar on the right.
		+ SVerticalBox::Slot().FillHeight(1.0f) [ SAssignNew(Scroll, SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll) + SScrollBox::Slot().Padding(FMargin(0.0f, 0.0f, bControls ? 20.0f : 0.0f, 0.0f)) [ bControls ? MakeControls() : MakeOptionsTab() ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
		[
			bControls ? SNullWidget::NullWidget : MakeLine(bInMatch ? TEXT("The match goes on; each change is made at once and kept.") : TEXT("Each change is made at once and kept."), true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f)).HAlign(HAlign_Left)
		[
			SNew(SBox).WidthOverride(304.0f) [ MakeButton(TEXT("BACK"), [Player]() { if (Player.IsValid()) Player->BackFromMenu(); }) ]
		];
	// Up and down scroll the controls (OnNavigation), as the credits' do.
	CreditsScroll = Scroll;
	return SettingsPage;
}

TSharedRef<SWidget> SCheeseStrikeMenu::ControlsRow(const FString& Action, const FString& Keys, const FString& Xbox, const FString& Nintendo, bool bHeading) const
{
	// The action, then its keys, then each pad's buttons, the column of what the player uses now lit.
	const bool bPadLit = CheeseStrikeInput::IsUsingGamepad(Controller.Get());
	// Each column is a fixed width (880 across: the page less the clear of the scroll bar), its text wrapped inside it. Fill
	// widths with the wrapping set from outside re-flowed the text a frame after the table was laid out, which was the
	// flicker side to side that remained after the up and down one (waves 8 and 9).
	const auto Cell = [](const FString& Text, const FSlateFontInfo& Look, const FLinearColor& Colour, float Across, float Clear)
	{
		return SNew(SBox).WidthOverride(Across)
			.Padding(FMargin(0.0f, 0.0f, Clear, 0.0f))
			[
				SNew(STextBlock).Text(FText::FromString(Text)).Font(Look).ColorAndOpacity(Colour).AutoWrapText(true)
			];
	};
	const FSlateFontInfo Look = bHeading ? Font(14, FName("Bold")) : Font(17);
	const FSlateFontInfo KeyLook = bHeading ? Font(14, FName("Bold")) : Font(17, FName("Bold"));
	const FLinearColor Name = bHeading ? Muted : Ink;
	const FLinearColor KeysColour = bHeading ? Muted : (bPadLit ? Muted : Cheese);
	const FLinearColor PadColour = bHeading ? Muted : (bPadLit ? Cheese : Muted);
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Cell(Action, Look, Name, 317.0f, 12.0f) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Cell(Keys, KeyLook, KeysColour, 229.0f, 12.0f) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Cell(Xbox, KeyLook, PadColour, 167.0f, 12.0f) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Cell(Nintendo, KeyLook, PadColour, 167.0f, 4.0f) ];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeControls()
{
	const UCheeseStrikeInputSet* Set = UCheeseStrikeInputSet::Get(Controller.Get());
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	if (Set == nullptr)
	{
		List->AddSlot().AutoHeight() [ MakeLine(TEXT("The controls aren't there to show."), true) ];
		return List;
	}
	for (const CheeseStrikeControls::FRow& Each : CheeseStrikeControls::Rows(*Set))
	{
		const auto Or = [](const FString& Text) { return Text.IsEmpty() ? FString(TEXT("-")) : Text; };
		List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 5.0f)) [ ControlsRow(Each.Action, Or(Each.Keyboard), Or(Each.Xbox), Or(Each.Nintendo), false) ];
	}
	return List;
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeOptionsTab()
{
	UCheeseStrikeSettings* Settings = UCheeseStrikeSettings::Get();
	if (Settings == nullptr)
	{
		return MakeLine(TEXT("The settings aren't there to change."), true);
	}
	TWeakObjectPtr<UCheeseStrikeSettings> Kept = Settings;
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	const auto Add = [&Rows](TSharedRef<SWidget> Row) { Rows->AddSlot().AutoHeight() [ Row ]; };
	const auto Times = [](float Value) { return FString::Printf(TEXT("%.2f x"), Value); };
	const auto Percent = [](float Value) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.0f)); };
	switch (OptionsTab)
	{
	case EOptionsTab::Controls:
		// The controls' table is MakeControls.
		break;
	case EOptionsTab::Aiming:
		Add(MakeOptionRow(TEXT("Look sensitivity"), MakeSlider(CheeseStrikeSettings::MinSensitivity, CheeseStrikeSettings::MaxSensitivity, 0.05f,
			[Kept]() { return Kept.IsValid() ? Kept->GetMouseSensitivity() : CheeseStrikeSettings::DefaultSensitivity; },
			[Kept](float Value) { if (Kept.IsValid()) Kept->SetMouseSensitivity(Value); }, Times)));
		Add(MakeOptionRow(TEXT("Aiming sensitivity"), MakeSlider(CheeseStrikeSettings::MinAimedSensitivity, CheeseStrikeSettings::MaxAimedSensitivity, 0.05f,
			[Kept]() { return Kept.IsValid() ? Kept->GetAimedSensitivity() : CheeseStrikeSettings::DefaultAimedSensitivity; },
			[Kept](float Value) { if (Kept.IsValid()) Kept->SetAimedSensitivity(Value); }, Times)));
		Rows->AddSlot().AutoHeight().Padding(FMargin(OptionLabelWidth, 0.0f, 0.0f, 4.0f)) [ MakeLine(TEXT("While aiming down the sights, as a share of the look's (the mouse's and a gamepad's stick)."), true, true, WrapWidth() - OptionLabelWidth) ];
		Add(MakeOptionRow(TEXT("Invert look"), MakeChoiceButtons({TEXT("OFF"), TEXT("ON")}, Settings->IsLookInverted() ? 1 : 0, true, [this, Kept](int32 Index)
		{
			if (Kept.IsValid())
			{
				Kept->SetLookInverted(Index == 1);
				Kept->SaveSettings();
			}
			Refresh();
		})));
		Add(MakeOptionRow(TEXT("Aim assist"), MakeChoiceButtons({TEXT("OFF"), TEXT("ON")}, Settings->IsAimAssistOn() ? 1 : 0, true, [this, Kept](int32 Index)
		{
			if (Kept.IsValid())
			{
				Kept->SetAimAssistOn(Index == 1);
				Kept->SaveSettings();
			}
			Refresh();
		})));
		Rows->AddSlot().AutoHeight().Padding(FMargin(OptionLabelWidth, 0.0f, 0.0f, 4.0f)) [ MakeLine(TEXT("A gamepad's: the stick turns slower over an enemy, and a little toward them. Never the mouse."), true, true, WrapWidth() - OptionLabelWidth) ];
		break;
	case EOptionsTab::Sound:
		Add(MakeOptionRow(TEXT("Master volume"), MakeSlider(0.0f, 1.0f, 0.01f,
			[Kept]() { return Kept.IsValid() ? Kept->GetMasterVolume() : CheeseStrikeSettings::DefaultVolume; },
			[Kept](float Value) { if (Kept.IsValid()) Kept->SetMasterVolume(Value); }, Percent)));
		Add(MakeOptionRow(TEXT("Effects volume"), MakeSlider(0.0f, 1.0f, 0.01f,
			[Kept]() { return Kept.IsValid() ? Kept->GetEffectsVolume() : CheeseStrikeSettings::DefaultVolume; },
			[Kept](float Value) { if (Kept.IsValid()) Kept->SetEffectsVolume(Value); }, Percent)));
		Add(MakeOptionRow(TEXT("Music volume"), MakeSlider(0.0f, 1.0f, 0.01f,
			[Kept]() { return Kept.IsValid() ? Kept->GetMusicVolume() : CheeseStrikeSettings::DefaultMusicVolume; },
			[Kept](float Value) { if (Kept.IsValid()) Kept->SetMusicVolume(Value); }, Percent)));
		Rows->AddSlot().AutoHeight().Padding(FMargin(OptionLabelWidth, 0.0f, 0.0f, 4.0f)) [ MakeLine(TEXT("The effects are the game's sounds, the music the front end's and the lobby's; the master volume is everything, the menu's too."), true, true, WrapWidth() - OptionLabelWidth) ];
		break;
	case EOptionsTab::Graphics:
	{
		Add(MakeOptionRow(TEXT("Field of view"), MakeSlider(CheeseStrikeSettings::MinFieldOfView, CheeseStrikeSettings::MaxFieldOfView, 1.0f,
			[Kept]() { return Kept.IsValid() ? Kept->GetFieldOfView() : CheeseStrikeSettings::DefaultFieldOfView; },
			[Kept](float Value) { if (Kept.IsValid()) Kept->SetFieldOfView(Value); },
			[](float Value) { return FString::Printf(TEXT("%.0f°"), Value); })));
		{
			// What that is across this screen, which most games give instead.
			TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
			Rows->AddSlot().AutoHeight().Padding(FMargin(OptionLabelWidth, 0.0f, 0.0f, 4.0f))
			[
				SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Muted).WrapTextAt(WrapWidth() - OptionLabelWidth).Text_Lambda([Kept, Player]()
				{
					FVector2D Size(16.0, 9.0);
					const ULocalPlayer* Local = Player.IsValid() ? Player->GetLocalPlayer() : nullptr;
					if (Local != nullptr && Local->ViewportClient != nullptr)
					{
						Local->ViewportClient->GetViewportSize(Size);
					}
					const float Aspect = Size.Y > 0.0 ? static_cast<float>(Size.X / Size.Y) : 16.0f / 9.0f;
					const float Across = CheeseStrikeSettings::HorizontalFieldOfView(Kept.IsValid() ? Kept->GetFieldOfView() : CheeseStrikeSettings::DefaultFieldOfView, Aspect);
					return FText::FromString(FString::Printf(TEXT("From the top of the view to the bottom; %.0f° across this screen."), Across));
				})
			];
		}
		TArray<FString> Qualities;
		for (const TCHAR* Name : QualityNames)
		{
			Qualities.Add(Name);
		}
		Add(MakeOptionRow(TEXT("Quality"), MakeChoiceButtons(Qualities, Settings->GetOverallScalabilityLevel(), true, [this, Kept](int32 Index)
		{
			if (Kept.IsValid())
			{
				Kept->SetOverallScalabilityLevel(Index);
				ApplyGraphics(Kept.Get());
			}
			Refresh();
		})));
		TArray<FString> Modes;
		int32 ModeChosen = INDEX_NONE;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(WindowModes); ++Index)
		{
			Modes.Add(WindowModeNames[Index]);
			ModeChosen = WindowModes[Index] == Settings->GetFullscreenMode() ? Index : ModeChosen;
		}
		Add(MakeOptionRow(TEXT("Window"), MakeChoiceButtons(Modes, ModeChosen, true, [this, Kept](int32 Index)
		{
			if (Kept.IsValid())
			{
				Kept->SetFullscreenMode(WindowModes[Index]);
				// Full screen, the window is the screen's own size, and the resolution picked is what the game is drawn at.
				if (WindowModes[Index] != EWindowMode::Windowed)
				{
					Kept->SetScreenResolution(Kept->GetDesktopResolution());
				}
				ApplyWindow(Kept.Get());
				ApplyGraphics(Kept.Get());
			}
			Refresh();
		})));
		const TArray<FIntPoint> Resolutions = ResolutionsHere();
		const int32 ResolutionChosen = CheeseStrikeSettings::ResolutionIndex(Resolutions, Settings->GetResolutionShown());
		const FIntPoint Shown = Resolutions.IsValidIndex(ResolutionChosen) ? Resolutions[ResolutionChosen] : Settings->GetResolutionShown();
		const bool bFullScreen = Settings->GetFullscreenMode() != EWindowMode::Windowed;
		const auto PickResolution = [this, Kept, Resolutions, bFullScreen](int32 Index)
		{
			if (Kept.IsValid() && Resolutions.IsValidIndex(Index))
			{
				// Full screen, what the game is drawn at; in a window, the window's size.
				if (bFullScreen)
				{
					Kept->SetFullscreenResolution(Resolutions[Index]);
					ApplyGraphics(Kept.Get());
				}
				else
				{
					Kept->SetScreenResolution(Resolutions[Index]);
					ApplyWindow(Kept.Get());
				}
			}
			Refresh();
		};
		Add(MakeOptionRow(TEXT("Resolution"), MakeStepper(FString(), FString::Printf(TEXT("%d x %d"), Shown.X, Shown.Y), Resolutions.Num() > 1,
			[PickResolution, ResolutionChosen]() { PickResolution(FMath::Max(0, ResolutionChosen - 1)); },
			[PickResolution, ResolutionChosen, Resolutions]() { PickResolution(FMath::Min(Resolutions.Num() - 1, ResolutionChosen + 1)); })));
		if (bFullScreen)
		{
			Rows->AddSlot().AutoHeight().Padding(FMargin(OptionLabelWidth, 0.0f, 0.0f, 4.0f)) [ MakeLine(TEXT("Full screen, the game is drawn at this resolution and the menus at the screen's own."), true, true, WrapWidth() - OptionLabelWidth) ];
		}
		Add(MakeOptionRow(TEXT("VSync"), MakeChoiceButtons({TEXT("OFF"), TEXT("ON")}, Settings->IsVSyncEnabled() ? 1 : 0, true, [this, Kept](int32 Index)
		{
			if (Kept.IsValid())
			{
				Kept->SetVSyncEnabled(Index == 1);
				ApplyGraphics(Kept.Get());
			}
			Refresh();
		})));
		TArray<FString> Limits;
		for (int32 Index = 0; Index < CheeseStrikeSettings::NumFrameRateLimits(); ++Index)
		{
			Limits.Add(CheeseStrikeSettings::FrameRateLimits[Index] > 0 ? FString::FromInt(CheeseStrikeSettings::FrameRateLimits[Index]) : FString(TEXT("NONE")));
		}
		Add(MakeOptionRow(TEXT("Frame rate limit"), MakeChoiceButtons(Limits, CheeseStrikeSettings::FrameRateLimitIndex(Settings->GetFrameRateLimit()), true, [this, Kept](int32 Index)
		{
			if (Kept.IsValid())
			{
				Kept->SetFrameRateLimit(static_cast<float>(CheeseStrikeSettings::FrameRateLimits[Index]));
				ApplyGraphics(Kept.Get());
			}
			Refresh();
		})));
		break;
	}
	}
	return Rows;
}
