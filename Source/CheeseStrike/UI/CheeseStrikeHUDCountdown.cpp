// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The count before the match and each of its rounds: big in the middle of the
// screen, each second coming in large and settling (3, 2, 1), then FIGHT, as
// CheeseStrikeStart::Shown has it, under a smaller GET READY while it counts.
// The words are blocks of cheese, holed and lit, rendered in Blender
// (tools/art/countdown_glyphs.py, imported by tools/art/import_countdown.py);
// without those pictures, the HUD's lettering in the cheese's yellow.

#include "CheeseStrikeHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikeHUDDraw.h"
#include "Rules/CheeseStrikeStart.h"

UTexture2D* ACheeseStrikeHUD::CountdownPicture(const FString& Word)
{
	if (const TObjectPtr<UTexture2D>* Known = CountdownPictures.Find(Word))
	{
		return Known->Get();
	}
	const FString Name = FString::Printf(TEXT("T_Count_%s"), Word == TEXT("FIGHT") ? TEXT("Fight") : *Word);
	UTexture2D* Picture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/CheeseStrike/UI/Countdown/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	CountdownPictures.Add(Word, Picture);
	return Picture;
}

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
	// How tall the word stands when it has settled, and so where GET READY sits over it.
	float Settled = 0.0f;
	UTexture2D* Picture = CountdownPicture(Shown.Text);
	if (Picture != nullptr && Picture->GetResource() != nullptr && Picture->GetSizeY() > 0)
	{
		// The picture, its middle on the middle, scaled for the settling; a soft dark copy under it, so that it reads on cheese.
		const float Height = (Shown.bGo ? 176.0f : 210.0f) * U;
		const FVector2D Size = FVector2D(Height * Picture->GetSizeX() / Picture->GetSizeY(), Height) * Shown.Scale;
		const FVector2D At = Middle - Size / 2.0f;
		CheeseStrikeHUDDraw::Picture(Canvas, Picture, At + FVector2D(5.0f, 8.0f) * U, Size, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f * Shown.Alpha));
		CheeseStrikeHUDDraw::Picture(Canvas, Picture, At, Size, FLinearColor(1.0f, 1.0f, 1.0f, Shown.Alpha));
		Settled = Height * 0.92f;
	}
	else
	{
		// One size of the font, stretched for the settling, so that its glyphs are made once.
		const FSlateFontInfo Big = CheeseStrikeHUDDraw::Font(Shown.bGo ? 96.0f * U : 132.0f * U);
		const FVector2D Size = CheeseStrikeHUDDraw::Measure(Shown.Text, Big) * Shown.Scale;
		const FLinearColor Colour = CheeseStrikeHUDDraw::Faded(CheeseStrikeHUDDraw::Accent, Shown.Alpha);
		CheeseStrikeHUDDraw::Text(Canvas, Shown.Text, FVector2D(Middle.X, Middle.Y - Size.Y * 0.5f), Big, Colour, 0.5f, true, FVector2D(Shown.Scale, Shown.Scale));
		Settled = CheeseStrikeHUDDraw::Measure(Shown.Text, Big).Y * 1.45f;
	}
	if (!Shown.bGo)
	{
		const FSlateFontInfo Small = CheeseStrikeHUDDraw::Font(22.0f * U);
		const FVector2D LabelSize = CheeseStrikeHUDDraw::Measure(TEXT("GET READY"), Small);
		CheeseStrikeHUDDraw::Text(Canvas, TEXT("GET READY"), FVector2D(Middle.X, Middle.Y - Settled * 0.5f - LabelSize.Y), Small, CheeseStrikeHUDDraw::Ink, 0.5f);
	}
}
