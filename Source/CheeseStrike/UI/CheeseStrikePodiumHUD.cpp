// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePodiumHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikeHUDDraw.h"
#include "CheeseStrikeHUDRules.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePodium.h"
#include "Input/CheeseStrikeInput.h"
#include "Rules/CheeseStrikePodiums.h"

namespace HudDraw = CheeseStrikeHUDDraw;

namespace CheeseStrikePodiumHUD
{
	FString Heading(const FString& WinnerName, bool bTeams)
	{
		if (WinnerName.IsEmpty())
		{
			return FString();
		}
		return FText::FromString(FString::Printf(TEXT("%s %s"), *WinnerName, bTeams ? TEXT("win") : TEXT("wins"))).ToUpper().ToString();
	}

	FString CountdownWords(int32 Seconds)
	{
		return Seconds > 0 ? FString::Printf(TEXT("BACK TO THE LOBBY IN %d"), Seconds) : FString();
	}

	float CountdownAlpha(float SecondsToLobby)
	{
		// Gone before the last frame is held (UCheeseStrikePostGameSubsystem, a quarter second before the lobby).
		return FMath::Clamp((SecondsToLobby - CountdownGoneAt) / (CountdownFadeFrom - CountdownGoneAt), 0.0f, 1.0f);
	}

	bool Draw(AHUD* Hud, UCanvas* Canvas)
	{
		const ACheeseStrikePlayerController* Controller = Hud != nullptr ? Cast<ACheeseStrikePlayerController>(Hud->GetOwningPlayerController()) : nullptr;
		const ACheeseStrikePodium* Podium = Controller != nullptr ? Controller->GetPodium() : nullptr;
		const ACheeseStrikeGameState* Match = Hud->GetWorld() != nullptr ? Hud->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
		if (Canvas == nullptr || Podium == nullptr || Match == nullptr)
		{
			return false;
		}
		// The words come in as the stage does.
		const float Alpha = FMath::Clamp((Hud->GetWorld()->GetRealTimeSeconds() - Controller->GetPodiumShownAt() - 0.3f) / 0.5f, 0.0f, 1.0f);
		const float ToLobby = Match->GetLobbyAt() - Match->GetServerWorldTimeSeconds();
		const int32 Seconds = CheeseStrikePodiums::SecondsLeft(Match->GetLobbyAt(), Match->GetServerWorldTimeSeconds());
		DrawWords(Canvas, Podium, Match, Alpha, Seconds, Alpha * CountdownAlpha(ToLobby), [Controller](const FVector& World, FVector2D& Out) { return Controller->ProjectWorldLocationToScreen(World, Out, true); });
		return true;
	}

	void DrawWords(UCanvas* Canvas, const ACheeseStrikePodium* Podium, const ACheeseStrikeGameState* Match, float Alpha, int32 Seconds, float CountAlpha, TFunctionRef<bool(const FVector&, FVector2D&)> Project)
	{
		const float U = HudDraw::Unit(Canvas);
		const bool bTeams = Match->HasTeams();
		const FVector2D Size(Canvas->SizeX, Canvas->SizeY);

		// At the top: who won, in their side's colour (the cheese's without sides), and the mode under it.
		const FSlateFontInfo Big = HudDraw::Font(50.0f * U);
		const FSlateFontInfo Small = HudDraw::Font(20.0f * U);
		const FString Heading = CheeseStrikePodiumHUD::Heading(Match->GetWinnerName(), bTeams);
		const FLinearColor HeadColour = bTeams ? HudDraw::SideColour(Match->GetWinningTeam()) : HudDraw::Accent;
		float Y = Size.Y * 0.03f;
		HudDraw::Text(Canvas, FText::FromString(Match->GetModeInfo().Name).ToUpper().ToString(), FVector2D(Size.X / 2.0f, Y), Small, HudDraw::Faded(HudDraw::Soft, Alpha), 0.5f);
		Y += HudDraw::Measure(TEXT("Ag"), Small).Y + 2.0f * U;
		if (!Heading.IsEmpty())
		{
			const FString Fitted = CheeseStrikeHUD::FitName(Heading, Size.X * 0.8f, [&Big](const FString& Text) { return static_cast<float>(HudDraw::Measure(Text, Big).X); });
			HudDraw::Text(Canvas, Fitted, FVector2D(Size.X / 2.0f, Y), Big, HudDraw::Faded(HeadColour, Alpha), 0.5f);
		}

		// Under each pedestal: its place in cheese, the name in the side's colour on a plate, the score under it.
		HudDraw::FShapes Shapes;
		struct FWords
		{
			FString Name;
			FString Score;
			FVector2D NameAt;
			FVector2D ScoreAt;
			FLinearColor Colour;
			UTexture2D* Glyph = nullptr;
			FVector2D GlyphAt;
			FVector2D GlyphSize;
		};
		TArray<FWords> Words;
		const FSlateFontInfo NameFont = HudDraw::Font(26.0f * U);
		const FSlateFontInfo ScoreFont = HudDraw::Font(18.0f * U);
		const float NameHeight = HudDraw::Measure(TEXT("Ag"), NameFont).Y;
		const float ScoreHeight = HudDraw::Measure(TEXT("Ag"), ScoreFont).Y;
		// Every place's foot first: the plates are as wide as the room between neighbours allows (never overlapping, whatever the
		// number of places), and the numbers and plates all stand on one line, each centred on its own pedestal.
		TArray<TPair<const FPodiumPlace*, FVector2D>> Feet;
		for (const FPodiumPlace& Placed : Podium->GetPlaces())
		{
			FVector2D Foot;
			if (Project(Podium->PedestalFront(Placed.Place), Foot))
			{
				Feet.Emplace(&Placed, Foot);
			}
		}
		float PlateWidth = 300.0f * U;
		for (int32 First = 0; First < Feet.Num(); ++First)
		{
			for (int32 Second = First + 1; Second < Feet.Num(); ++Second)
			{
				PlateWidth = FMath::Min(PlateWidth, FMath::Abs(Feet[First].Value.X - Feet[Second].Value.X) - 16.0f * U);
			}
		}
		PlateWidth = FMath::Max(PlateWidth, 60.0f * U);
		float BaseY = -FLT_MAX;
		for (const TPair<const FPodiumPlace*, FVector2D>& Each : Feet)
		{
			BaseY = FMath::Max(BaseY, Each.Value.Y);
		}
		const float GlyphHeight = 104.0f * U;
		const float GlyphTop = BaseY + 14.0f * U - GlyphHeight * 0.78f;
		for (const TPair<const FPodiumPlace*, FVector2D>& Each : Feet)
		{
			const FPodiumPlace& Placed = *Each.Key;
			const FVector2D Foot(Each.Value.X, BaseY);
			const FLinearColor Colour = Placed.Team >= 0 ? HudDraw::SideColour(Placed.Team) : HudDraw::Accent;
			const float PlateHeight = NameHeight + ScoreHeight + 28.0f * U;
			// The place's number on the pedestal's foot, and the plate a little lower under it, the name clear of the number.
			UTexture2D* Glyph = Podium->Glyph(Placed.Place + 1);
			const bool bGlyph = Glyph != nullptr && Glyph->GetResource() != nullptr && Glyph->GetSizeY() > 0;
			const float PlateTop = (bGlyph ? GlyphTop + GlyphHeight : Foot.Y + 14.0f * U) + 10.0f * U;
			Shapes.RoundedRect(Foot.X - PlateWidth / 2.0f, PlateTop, PlateWidth, PlateHeight, 10.0f * U, HudDraw::Faded(FLinearColor(0.02f, 0.02f, 0.025f, 0.72f), Alpha));
			// The side's colour along the top, out to the plate's rounded corners and clipped to them, as a score block's is.
			const FVector2D Corner(Foot.X - PlateWidth / 2.0f, PlateTop);
			const TArray<FVector2D> Outline = CheeseStrikeHUD::RoundedRect(Corner, Corner + FVector2D(PlateWidth, PlateHeight), 10.0f * U);
			Shapes.SmoothPolygon(CheeseStrikeHUD::ClipToRect(Outline, Corner, Corner + FVector2D(PlateWidth, 3.0f * U)), HudDraw::Faded(Colour, Alpha));
			if (Placed.bMine)
			{
				Shapes.RoundedFrame(Foot.X - PlateWidth / 2.0f, PlateTop, PlateWidth, PlateHeight, 10.0f * U, 2.0f * U, HudDraw::Faded(HudDraw::Accent, Alpha));
			}
			FWords& Line = Words.AddDefaulted_GetRef();
			Line.Name = CheeseStrikeHUD::FitName(Placed.Name, PlateWidth - 24.0f * U, [&NameFont](const FString& Text) { return static_cast<float>(HudDraw::Measure(Text, NameFont).X); });
			Line.Score = Placed.Score;
			Line.NameAt = FVector2D(Foot.X, PlateTop + 14.0f * U);
			Line.ScoreAt = FVector2D(Foot.X, PlateTop + 16.0f * U + NameHeight);
			Line.Colour = Colour;
			if (bGlyph)
			{
				Line.Glyph = Glyph;
				Line.GlyphSize = FVector2D(GlyphHeight * Glyph->GetSizeX() / Glyph->GetSizeY(), GlyphHeight);
				Line.GlyphAt = FVector2D(Foot.X - Line.GlyphSize.X / 2.0f, GlyphTop);
			}
		}
		Shapes.Draw(Canvas);
		// The numbers over the plates' top edges, the names under them.
		for (const FWords& Line : Words)
		{
			if (Line.Glyph != nullptr)
			{
				HudDraw::Picture(Canvas, Line.Glyph, Line.GlyphAt + FVector2D(4.0f, 6.0f) * U, Line.GlyphSize, FLinearColor(0.0f, 0.0f, 0.0f, 0.4f * Alpha));
				HudDraw::Picture(Canvas, Line.Glyph, Line.GlyphAt, Line.GlyphSize, FLinearColor(1.0f, 1.0f, 1.0f, Alpha));
			}
		}
		for (const FWords& Line : Words)
		{
			HudDraw::Text(Canvas, Line.Name, Line.NameAt, NameFont, HudDraw::Faded(Line.Colour, Alpha), 0.5f);
			HudDraw::Text(Canvas, Line.Score, Line.ScoreAt, ScoreFont, HudDraw::Faded(HudDraw::Ink, Alpha), 0.5f);
		}

		// At the foot, how long until the lobby.
		const FString Countdown = CountdownWords(Seconds);
		if (!Countdown.IsEmpty() && CountAlpha > 0.0f)
		{
			const FSlateFontInfo Foot = HudDraw::Font(22.0f * U);
			HudDraw::Text(Canvas, Countdown, FVector2D(Size.X / 2.0f, Size.Y - 48.0f * U), Foot, HudDraw::Faded(HudDraw::Soft, CountAlpha), 0.5f);
		}
	}
}

void SCheeseStrikePodiumSkip::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	static FButtonStyle Style;
	static const FSlateRoundedBoxBrush KeyChip(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 4.0f);
	static bool bStyled = false;
	if (!bStyled)
	{
		bStyled = true;
		Style.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.1f, 0.42f, 0.9f), 6.0f));
		Style.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.2f, 0.52f, 1.0f), 6.0f));
		Style.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.08f, 0.32f, 0.72f), 6.0f));
		Style.SetNormalForeground(FLinearColor(0.92f, 0.9f, 0.86f));
		Style.SetHoveredForeground(FLinearColor::White);
		Style.SetPressedForeground(FLinearColor::White);
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
	}
	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(0.0f, 0.0f, 56.0f, 40.0f))
	[
		SNew(SButton)
		.ButtonStyle(&Style)
		.IsFocusable(false)
		.ContentPadding(FMargin(18.0f, 10.0f))
		.OnClicked(this, &SCheeseStrikePodiumSkip::Skip)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Back to the lobby"))).Font(FCoreStyle::GetDefaultFontStyle(FName("Bold"), 18))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SBorder)
				.BorderImage(&KeyChip)
				.Padding(FMargin(8.0f, 2.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::FromString(Controller.IsValid() && CheeseStrikeInput::IsUsingGamepad(Controller.Get()) ? TEXT("A") : TEXT("ENTER")); }).Font(FCoreStyle::GetDefaultFontStyle(FName("Bold"), 14))
				]
			]
		]
	];
}

FReply SCheeseStrikePodiumSkip::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		return Skip();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SCheeseStrikePodiumSkip::Skip()
{
	if (ACheeseStrikePlayerController* Host = Controller.Get())
	{
		Host->SkipPodium();
	}
	return FReply::Handled();
}
