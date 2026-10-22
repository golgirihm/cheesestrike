// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The count before the match and each of its rounds: big in the middle of the
// screen, each second coming in large and settling (3, 2, 1), then FIGHT, as
// CheeseStrikeStart::Shown has it, under a smaller GET READY while it counts.

#include "CheeseStrikeHUD.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikeHUDDraw.h"
#include "Rules/CheeseStrikeStart.h"

void ACheeseStrikeHUD::DrawCountdown(const ACheeseStrikeGameState* Match)
{
	if (Match == nullptr || Canvas == nullptr || !Match->HasMatchStarted())
	{
		return;
	}
	const CheeseStrikeStart::FShown Shown = CheeseStrikeStart::Shown(static_cast<float>(Match->GetServerWorldTimeSeconds()), Match->GetGoAt());
	if (Shown.Text.IsEmpty() || Shown.Alpha <= 0.0f)
	{
		return;
	}
	const float U = Unit();
	const FVector2D Middle(Canvas->SizeX / 2.0f, Canvas->SizeY * 0.4f);
	// One size of the font, stretched for the settling, so that its glyphs are made once.
	const FSlateFontInfo Big = CheeseStrikeHUDDraw::Font(Shown.bGo ? 96.0f * U : 132.0f * U);
	const FVector2D Size = CheeseStrikeHUDDraw::Measure(Shown.Text, Big) * Shown.Scale;
	const FLinearColor Colour = CheeseStrikeHUDDraw::Faded(CheeseStrikeHUDDraw::Accent, Shown.Alpha);
	CheeseStrikeHUDDraw::Text(Canvas, Shown.Text, FVector2D(Middle.X, Middle.Y - Size.Y * 0.5f), Big, Colour, 0.5f, true, FVector2D(Shown.Scale, Shown.Scale));
	if (!Shown.bGo)
	{
		const FSlateFontInfo Small = CheeseStrikeHUDDraw::Font(22.0f * U);
		const FVector2D LabelSize = CheeseStrikeHUDDraw::Measure(TEXT("GET READY"), Small);
		const FVector2D BigSize = CheeseStrikeHUDDraw::Measure(Shown.Text, Big) * 1.45f;
		CheeseStrikeHUDDraw::Text(Canvas, TEXT("GET READY"), FVector2D(Middle.X, Middle.Y - BigSize.Y * 0.5f - LabelSize.Y), Small, CheeseStrikeHUDDraw::Ink, 0.5f);
	}
}
