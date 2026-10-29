// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The kill-streak panel at the bottom left, over the health: the kills in a
// row now, big in a tile of its own, and a track of a pip a kill up to the
// last streak's, each streak's kill marked with its name under it and what
// comes next over it ("Cheese Strike in 2"). Each kill pops the count and
// fills its pip; a streak's kill rings out round its pip; a death drains the
// track. The rules it draws by are CheeseStrikeHUD's (StreakPips, StreakNext,
// StreakPop, StreakFillToward), tested without a world.

#include "CheeseStrikeHUD.h"
#include "CheeseStrikeGameState.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeHUDDraw.h"
#include "CheeseStrikePlayerState.h"

/** The HUD's drawing, named through an alias rather than used as a whole: the packaged game's unity build would carry a using-directive into the files after this one. */
namespace HudDraw = CheeseStrikeHUDDraw;

/** How long a kill's badges show over the streak panel. */
static constexpr float BadgeShowSeconds = 2.5f;

static TAutoConsoleVariable<int32> CVarStreakShown(TEXT("CheeseStrike.HUD.Streak"), -1, TEXT("Show this run of kills in the kill-streak panel instead of the player's, for judging it and its pop (-1: the player's)."));

float ACheeseStrikeHUD::StreakPanelTop() const
{
	// Over the health's cross, which reaches a little over its bar.
	const float U = Unit();
	return Canvas->SizeY - (Margin + 18.0f + 4.0f + 14.0f + StreakPanelHeight) * U;
}

void ACheeseStrikeHUD::DrawStreak(const ACheeseStrikeCharacter* Character)
{
	const ACheeseStrikePlayerState* Mine = Character != nullptr ? Character->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	if (Mine == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const int32 Kills = CVarStreakShown.GetValueOnGameThread() >= 0 ? CVarStreakShown.GetValueOnGameThread() : Mine->GetStreak();
	// A kill more than was shown pops the count; fewer (a death) drains the track.
	if (Kills > StreakSeen)
	{
		StreakChangedAt = Now;
		StreakEarnedAt = CheeseStrikeStreaks::EarnedAt(Kills) != CheeseStrikeStreaks::EStreak::None ? Now : StreakEarnedAt;
	}
	StreakSeen = Kills;
	StreakFill = CheeseStrikeHUD::StreakFillToward(StreakFill, Kills, GetWorld()->GetDeltaSeconds());
	// Nothing until a kill: the panel comes in with the first.
	if (Kills <= 0)
	{
		return;
	}

	const float U = Unit();
	const float Left = Margin * U;
	const float Top = StreakPanelTop();
	const float Height = StreakPanelHeight * U;
	// As wide as the health under it: its cross, the gap and its bar.
	const float Width = (26.0f + 12.0f + 320.0f) * U;
	const float Tile = Height;
	const float Round = 9.0f * U;
	const float Since = Now - StreakChangedAt;
	const float Pop = CheeseStrikeHUD::StreakPop(Since);
	const float Flash = FMath::Clamp(1.0f - Since / CheeseStrikeHUD::StreakPopTime, 0.0f, 1.0f);
	const bool bRunning = Kills > 0;

	HudDraw::FShapes Shapes;
	// The panel; the count's tile at its left is drawn last (below), over the rest, as it grows.
	Shapes.RoundedRect(Left, Top, Width, Height, Round, HudDraw::Panel);

	// The track: a pip a kill, filled as far as the run (rising, and draining after a death), the streaks' pips taller.
	const TArray<CheeseStrikeHUD::FStreakPip> Pips = CheeseStrikeHUD::StreakPips(Kills);
	const float TrackLeft = Left + Tile + 12.0f * U;
	const float TrackRight = Left + Width - 12.0f * U;
	const float PipGap = 4.0f * U;
	const float PipWidth = (TrackRight - TrackLeft - PipGap * (Pips.Num() - 1)) / FMath::Max(1, Pips.Num());
	const float PipMiddle = Top + 31.0f * U;
	const float Small = 8.0f * U;
	const float Tall = 14.0f * U;
	for (int32 Index = 0; Index < Pips.Num(); ++Index)
	{
		const CheeseStrikeHUD::FStreakPip& Pip = Pips[Index];
		const bool bStreak = Pip.Earns != CheeseStrikeStreaks::EStreak::None;
		const float X = TrackLeft + Index * (PipWidth + PipGap);
		const float Filled = FMath::Clamp(StreakFill - Index, 0.0f, 1.0f);
		// The pip just made swells a little with the count's pop.
		const bool bNewest = Index == Kills - 1 && Since < CheeseStrikeHUD::StreakPopTime;
		const float Swell = bNewest ? (FMath::Min(Pop, 1.8f) - 1.0f) * 3.0f * U : 0.0f;
		if (bStreak)
		{
			// A streak's kill is an orange icon of the streak (lit once made), where the plain pips have a bubble.
			const float IconRadius = (Tall / 2.0f + Swell) * 0.95f;
			const FVector2D Centre(X + PipWidth / 2.0f, PipMiddle);
			const FLinearColor IconColour = HudDraw::Faded(HudDraw::Accent, Pip.bMade ? 1.0f : 0.3f);
			switch (Pip.Earns)
			{
			case CheeseStrikeStreaks::EStreak::AerialCheese:
				// The drone: a ring with a hub and four arms.
				Shapes.Arc(Centre, IconRadius * 0.45f, 1.6f * U, 0.0f, 360.0f, IconColour, false);
				Shapes.RoundedRect(Centre.X - U, Centre.Y - U, 2.0f * U, 2.0f * U, U, IconColour);
				for (const FVector2D& Way : {FVector2D(1, 1), FVector2D(-1, 1), FVector2D(1, -1), FVector2D(-1, -1)})
				{
					Shapes.Line(Centre + Way * IconRadius * 0.32f, Centre + Way * IconRadius * 0.75f, 1.6f * U, IconColour);
				}
				break;
			case CheeseStrikeStreaks::EStreak::CheeseStrike:
			{
				// A falling bomb: a rounded body with a tail.
				Shapes.RoundedRect(Centre.X - IconRadius * 0.4f, Centre.Y - IconRadius * 0.55f, IconRadius * 0.8f, IconRadius * 1.1f, IconRadius * 0.4f, IconColour);
				Shapes.SmoothPolygon({Centre + FVector2D(-IconRadius * 0.4f, -IconRadius * 0.6f), Centre + FVector2D(IconRadius * 0.4f, -IconRadius * 0.6f), Centre + FVector2D(0.0f, -IconRadius)}, IconColour);
				break;
			}
			default:
				// The Gouda Overload: a wheel of cheese with its holes.
				Shapes.RoundedRect(Centre.X - IconRadius, Centre.Y - IconRadius, 2.0f * IconRadius, 2.0f * IconRadius, IconRadius, IconColour);
				Shapes.RoundedRect(Centre.X - IconRadius * 0.45f, Centre.Y - IconRadius * 0.4f, IconRadius * 0.4f, IconRadius * 0.4f, IconRadius * 0.2f, FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
				Shapes.RoundedRect(Centre.X + IconRadius * 0.1f, Centre.Y + IconRadius * 0.1f, IconRadius * 0.35f, IconRadius * 0.35f, IconRadius * 0.17f, FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
				break;
			}
			// A streak just earned rings out round its icon.
			const float Ring = (Now - StreakEarnedAt) / 0.7f;
			if (Pip.bMade && Index == Kills - 1 && Ring >= 0.0f && Ring < 1.0f)
			{
				const float Eased = 1.0f - FMath::Square(1.0f - Ring);
				Shapes.Arc(Centre, (10.0f + 26.0f * Eased) * U, (3.0f - 2.0f * Ring) * U, 0.0f, 360.0f, HudDraw::Faded(HudDraw::Accent, 0.9f * (1.0f - Ring)), false);
			}
			continue;
		}
		const float Y = PipMiddle - Small / 2.0f;
		const float PipRound = Small / 2.0f;
		Shapes.RoundedRect(X - Swell, Y - Swell, PipWidth + 2.0f * Swell, Small + 2.0f * Swell, PipRound + Swell, HudDraw::Dim);
		if (Filled > 0.0f)
		{
			const TArray<FVector2D> Outline = CheeseStrikeHUD::RoundedRect(FVector2D(X - Swell, Y - Swell), FVector2D(X + PipWidth + Swell, Y + Small + Swell), PipRound + Swell);
			Shapes.SmoothPolygon(CheeseStrikeHUD::ClipToRect(Outline, FVector2D(X - Swell, Y - Swell), FVector2D(X - Swell + (PipWidth + 2.0f * Swell) * Filled, Y + Small + Swell)), HudDraw::Ink);
		}
	}
	Shapes.Draw(Canvas);

	// Over the track: what it is, and what comes next; under the streaks' pips, their names.
	const FSlateFontInfo Caption = HudDraw::Font(12.0f * U);
	const float CaptionTop = Top + 6.0f * U;
	HudDraw::Text(Canvas, TEXT("KILLSTREAK"), FVector2D(TrackLeft, CaptionTop), Caption, HudDraw::Soft);
	const float CaptionWidth = HudDraw::Measure(TEXT("KILLSTREAK"), Caption).X;
	// The streak's whole name if it fits beside the caption, else its word under its pip ("DRONE IN 3").
	const float NextRoom = TrackRight - TrackLeft - CaptionWidth - 12.0f * U;
	const auto CaptionMeasure = [&Caption](const FString& Text) { return static_cast<float>(HudDraw::Measure(Text, Caption).X); };
	FString Next = CheeseStrikeHUD::StreakNext(Kills);
	if (CaptionMeasure(Next) > NextRoom)
	{
		Next = CheeseStrikeHUD::FitName(Next, NextRoom, CaptionMeasure);
	}
	HudDraw::Text(Canvas, Next, FVector2D(TrackRight, CaptionTop), Caption, bRunning ? HudDraw::Accent : HudDraw::Soft, 1.0f);

	// The count's tile and the count, popping as each kill comes: grown, they stand on the panel, over it and the health (never
	// covering either), and come down into place as they shrink (CheeseStrikeHUD::StreakPopCorner).
	const FString Number = FString::FromInt(Kills);
	const FLinearColor NumberColour = bRunning ? FMath::Lerp(HudDraw::Accent, FLinearColor(1.0f, 0.95f, 0.8f, 1.0f), Flash) : HudDraw::Soft;
	const float Grown = Tile * Pop;
	const FVector2D Corner = CheeseStrikeHUD::StreakPopCorner(Left, Top, Tile, Pop, CheeseStrikeHUD::StreakPopClear * U);
	const float GrownLeft = Corner.X;
	const float GrownTop = Corner.Y;
	const FVector2D Middle(GrownLeft + Grown / 2.0f, GrownTop + Grown / 2.0f);
	const float GrownRound = Round * Pop;
	// The number is drawn in a font of the size it is shown at, not a small one scaled up, so that its edges stay smooth: the size
	// in steps of four pixels (fewer sizes for the font's cache to keep), the rest of the way a slight stretch.
	const float NumberPixels = 30.0f * U * Pop;
	const float FontPixels = Pop > 1.0f ? FMath::Max(1.0f, 4.0f * FMath::RoundToFloat(NumberPixels / 4.0f)) : FMath::RoundToFloat(NumberPixels);
	const FSlateFontInfo Count = HudDraw::Font(FontPixels);
	const float Stretch = NumberPixels / FMath::Max(1.0f, static_cast<float>(Count.Size));
	const FVector2D NumberSize = HudDraw::Measure(Number, Count) * Stretch;
	HudDraw::FShapes TileShapes;
	// Dark, edged in the cheese while a run is on, lit for a moment by a kill.
	TileShapes.RoundedRectDown(GrownLeft, GrownTop, Grown, Grown, GrownRound, FLinearColor(0.06f, 0.055f, 0.04f, 0.8f), FLinearColor(0.02f, 0.02f, 0.025f, 0.8f));
	if (Flash > 0.0f)
	{
		TileShapes.RoundedRect(GrownLeft, GrownTop, Grown, Grown, GrownRound, HudDraw::Faded(HudDraw::Accent, 0.35f * Flash * Flash));
	}
	TileShapes.RoundedFrame(GrownLeft, GrownTop, Grown, Grown, GrownRound, FMath::Max(1.0f, 1.5f * U) * FMath::Sqrt(Pop), bRunning ? HudDraw::Faded(HudDraw::Accent, 0.55f + 0.45f * Flash) : HudDraw::Rule);
	TileShapes.Draw(Canvas);
	HudDraw::Text(Canvas, Number, FVector2D(Middle.X, Middle.Y - NumberSize.Y / 2.0f), Count, NumberColour, 0.5f, true, FVector2D(Stretch, Stretch));

	// The kill's badges (HEADSHOT, REVENGE ...), over the count's tile as it pops and comes down into place, for a couple of seconds.
	if (const ACheeseStrikeGameState* Match = GetWorld()->GetGameState<ACheeseStrikeGameState>())
	{
		float BadgeSince = 0.0f;
		const uint8 Badges = LatestKillBadges(Match, Mine->GetPlayerName(), Match->GetServerWorldTimeSeconds(), BadgeShowSeconds, BadgeSince);
		if (Badges != 0)
		{
			const float BadgeAlpha = FMath::Clamp((BadgeShowSeconds - BadgeSince) / 0.4f, 0.0f, 1.0f) * FMath::Clamp(BadgeSince / 0.08f, 0.0f, 1.0f);
			HudDraw::BadgePills(Canvas, Badges, FVector2D(GrownLeft, GrownTop - 30.0f * U), 22.0f * U, 0.0f, BadgeAlpha);
		}
	}
}
