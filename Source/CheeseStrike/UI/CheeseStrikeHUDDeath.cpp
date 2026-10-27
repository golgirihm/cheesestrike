// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The death screen: the local player is killed and the screen melts. A sheet
// of cheese slides down from the top, drips run from it down the screen,
// "YOU GOT CHEESED" comes up and sags, dripping from its letters, and who did
// it and how long until the player is back show under it. The drips and the
// sag are CheeseStrikeHUD's (Drip, SheetDepth, LetterSag), the same at every
// frame for a moment after the death.

#include "CheeseStrikeHUD.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikeHUDDraw.h"

/** The HUD's drawing, named through an alias rather than used as a whole: the packaged game's unity build would carry a using-directive into the files after this one. */
namespace HudDraw = CheeseStrikeHUDDraw;

/** For judging the death screen from screenshots (development): holds it at so many seconds after the death; -1 lets it run. */
static TAutoConsoleVariable<float> CVarMeltAt(TEXT("CheeseStrike.HUD.MeltAt"), -1.0f, TEXT("Hold the death screen at this many seconds after the death (-1: as it happens)."));

namespace
{
	/** Melted cheese: its light middle, its body, its deeper edges, and a wet shine. Opaque, so that the sheet drawn over the drips' tops hides them whole. */
	const FLinearColor MeltLight(1.0f, 0.86f, 0.36f, 1.0f);
	const FLinearColor MeltBody(0.99f, 0.73f, 0.19f, 1.0f);
	const FLinearColor MeltDeep(0.84f, 0.5f, 0.06f, 1.0f);
	const FLinearColor MeltShine(1.0f, 0.97f, 0.84f, 0.55f);
	const FLinearColor MeltOutline(0.12f, 0.06f, 0.0f, 0.85f);

	/** How low the sheet's wavy lower edge hangs at `X` (pixels across a screen `Width` wide), for a sheet `Depth` deep. */
	float SheetEdge(float X, float Width, float Depth)
	{
		const float Across = X / FMath::Max(1.0f, Width);
		return Depth * (1.0f + 0.22f * FMath::Sin(Across * 2.0f * UE_PI * 3.3f + 1.3f) + 0.12f * FMath::Sin(Across * 2.0f * UE_PI * 7.7f + 0.4f));
	}

	/**
	 * The soft rim round a drip drawn hard of quads (FShapes::SoftRim): down its left side (`Lefts`, top to bottom), round its drop's
	 * end (a half ellipse `Radius` across and `Depth` down from `End`, as FShapes::Bulb draws it, in `EndColour`) and up its right,
	 * open across its top, where it runs on into what it hangs from.
	 */
	void DripRim(HudDraw::FShapes& Shapes, const TArray<FVector2D>& Lefts, const TArray<FVector2D>& Rights, const TArray<FLinearColor>& LeftColours,
		const TArray<FLinearColor>& RightColours, const FVector2D& End, float Radius, float Depth, const FLinearColor& EndColour)
	{
		TArray<FVector2D> Rim(Lefts);
		TArray<FLinearColor> RimColours(LeftColours);
		constexpr int32 BulbSides = 14;
		for (int32 Side = BulbSides - 1; Side >= 1; --Side)
		{
			const float Angle = UE_PI * Side / BulbSides;
			Rim.Add(End + FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Depth));
			RimColours.Add(EndColour);
		}
		for (int32 Index = Rights.Num() - 1; Index >= 0; --Index)
		{
			Rim.Add(Rights[Index]);
			RimColours.Add(RightColours[Index]);
		}
		Shapes.SoftRim(Rim, RimColours, true);
	}

	/**
	 * A drip running from the sheet's edge at `X`, `Width` wide, down to `End`, drawn before the sheet so that the sheet covers its
	 * top: the drip and the sheet are one shape, with no seam. Each side of its neck is a fillet, a quarter of an ellipse that leaves
	 * the sheet's edge level (where the edge is on that side) and turns down into the drip, so the outline runs on from the sheet's
	 * without a corner; it is the sheet's edge colour where it meets the sheet and shades to its own over the neck. Above the edge
	 * it reaches only a little way in (`Hidden`), so that nothing of it shows through the sheet as both fade.
	 */
	void SheetDrip(HudDraw::FShapes& Shapes, float X, float Width, float ScreenWidth, float Depth, float End, float Unit)
	{
		const float Top = SheetEdge(X, ScreenWidth, Depth);
		const float Length = End - Top;
		if (Length <= 1.0f)
		{
			return;
		}
		const float Hidden = 6.0f * Unit;
		const float FilletAcross = Width * 0.55f;
		const float FilletDown = FMath::Min(18.0f * Unit, Length * 0.6f);
		// The drip's own half width, Below its top: thinning as it runs, gathering into a drop at the end.
		const auto Body = [&](float Below)
		{
			const float Along = FMath::Clamp(Below / Length, 0.0f, 1.0f);
			return Width / 2.0f * (1.0f - 0.28f * FMath::SmoothStep(0.1f, 0.6f, Along) + 0.38f * FMath::SmoothStep(0.72f, 1.0f, Along));
		};
		// What a side's fillet adds at height Y, for a side whose fillet starts level with the sheet's edge at EdgeY.
		const auto Fillet = [&](float Y, float EdgeY)
		{
			const float T = FMath::Clamp((Y - EdgeY) / FilletDown, 0.0f, 1.0f);
			return FilletAcross * (1.0f - FMath::Sqrt(FMath::Max(0.0f, 1.0f - FMath::Square(1.0f - T))));
		};
		const float LeftEdge = SheetEdge(X - Width / 2.0f - FilletAcross, ScreenWidth, Depth);
		const float RightEdge = SheetEdge(X + Width / 2.0f + FilletAcross, ScreenWidth, Depth);
		const float Start = FMath::Min3(LeftEdge, RightEdge, Top) - Hidden;
		const auto Row = [&](float Y, FVector2D (&Points)[4], FLinearColor (&Across)[4])
		{
			const float Left = Body(Y - Top) + Fillet(Y, LeftEdge);
			const float Right = Body(Y - Top) + Fillet(Y, RightEdge);
			const float Xs[4] = { X - Left, X - Left / 3.0f, X + Right / 3.0f, X + Right };
			for (int32 Column = 0; Column < 4; ++Column)
			{
				const float Edge = SheetEdge(Xs[Column], ScreenWidth, Depth);
				Points[Column] = FVector2D(Xs[Column], FMath::Max(Y, Edge - Hidden));
				// The sheet's light edge colour at and above the edge, then the drip's own, shaded round, over the neck.
				const float Own = FMath::SmoothStep(0.0f, FilletDown, static_cast<float>(Points[Column].Y) - Edge);
				Across[Column] = FMath::Lerp(MeltLight, Column == 1 ? MeltLight : Column == 2 ? MeltBody : MeltDeep, Own);
			}
		};
		const int32 Steps = FMath::Clamp(FMath::CeilToInt((End - Start) / (4.0f * Unit)), 8, 96);
		FVector2D Upper[4];
		FLinearColor UpperAcross[4];
		Row(Start, Upper, UpperAcross);
		// Its sides, for the soft rim round it.
		TArray<FVector2D> Lefts = {Upper[0]};
		TArray<FVector2D> Rights = {Upper[3]};
		TArray<FLinearColor> LeftColours = {UpperAcross[0]};
		TArray<FLinearColor> RightColours = {UpperAcross[3]};
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			// Rows closer together near the top, where the neck curves.
			const float Share = static_cast<float>(Step) / Steps;
			FVector2D Lower[4];
			FLinearColor LowerAcross[4];
			Row(FMath::Lerp(Start, End, FMath::Lerp(Share, Share * Share, 0.5f)), Lower, LowerAcross);
			Lefts.Add(Lower[0]);
			Rights.Add(Lower[3]);
			LeftColours.Add(LowerAcross[0]);
			RightColours.Add(LowerAcross[3]);
			for (int32 Column = 0; Column < 3; ++Column)
			{
				Shapes.Quad(Upper[Column], Upper[Column + 1], Lower[Column + 1], Lower[Column], UpperAcross[Column], UpperAcross[Column + 1], LowerAcross[Column + 1], LowerAcross[Column]);
			}
			FMemory::Memcpy(Upper, Lower, sizeof(Upper));
			FMemory::Memcpy(UpperAcross, LowerAcross, sizeof(UpperAcross));
		}
		const float Drop = Body(Length);
		Shapes.Bulb(FVector2D(X, End), Drop, Drop * 1.05f, MeltLight, MeltDeep);
		DripRim(Shapes, Lefts, Rights, LeftColours, RightColours, FVector2D(X, End), Drop, Drop * 1.05f, MeltDeep);
		if (Length > FilletDown + Width)
		{
			Shapes.Rect(X - Width * 0.3f, Top + FilletDown, FMath::Max(1.5f * Unit, Width * 0.12f), Length - FilletDown - Width * 0.2f, MeltShine);
		}
	}

	/**
	 * A drip hanging from `Top` at `X`, `Width` wide, its end `Length` further
	 * down (from a letter): a neck curving out into what it runs from, a body shaded round like
	 * a rope of cheese and swelling a little towards its end, a round end, and a
	 * shine down its left. As an outline, the same a little larger, dark.
	 */
	void MeltDrip(HudDraw::FShapes& Shapes, float X, float Top, float Width, float Length, float Unit, bool bOutline)
	{
		if (Length <= 1.0f)
		{
			return;
		}
		const float Grow = bOutline ? 2.5f * Unit : 0.0f;
		const float Neck = FMath::Min(18.0f * Unit, Length * 0.6f);
		const auto HalfAt = [&](float Down)
		{
			// Out into the sheet at the top, thinning as it runs, gathering into a drop at the end.
			const float Fillet = FMath::Square(FMath::Max(0.0f, 1.0f - Down / Neck));
			const float Along = Down / Length;
			return Width / 2.0f * (1.0f + 1.1f * Fillet - 0.28f * FMath::SmoothStep(0.1f, 0.6f, Along) + 0.38f * FMath::SmoothStep(0.72f, 1.0f, Along)) + Grow;
		};
		const auto Row = [&](float Down, FVector2D (&Points)[4])
		{
			const float Half = HalfAt(Down);
			const float Y = Top + Down;
			Points[0] = FVector2D(X - Half, Y);
			Points[1] = FVector2D(X - Half / 3.0f, Y);
			Points[2] = FVector2D(X + Half / 3.0f, Y);
			Points[3] = FVector2D(X + Half, Y);
		};
		// A row's colours across, shaded round.
		const auto Colours = [&](float Down, FLinearColor (&Out)[4])
		{
			Out[0] = bOutline ? MeltOutline : MeltDeep;
			Out[1] = bOutline ? MeltOutline : MeltLight;
			Out[2] = bOutline ? MeltOutline : MeltBody;
			Out[3] = bOutline ? MeltOutline : MeltDeep;
		};
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(Length / (6.0f * Unit)), 3, 40);
		FVector2D Upper[4];
		Row(0.0f, Upper);
		FLinearColor UpperAcross[4];
		Colours(0.0f, UpperAcross);
		// Its sides, for the soft rim round it.
		TArray<FVector2D> Lefts = {Upper[0]};
		TArray<FVector2D> Rights = {Upper[3]};
		TArray<FLinearColor> LeftColours = {UpperAcross[0]};
		TArray<FLinearColor> RightColours = {UpperAcross[3]};
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			// Rows closer together in the neck, where it curves.
			const float Along = FMath::Square(static_cast<float>(Step) / Steps);
			FVector2D Lower[4];
			const float LowerDown = FMath::Lerp(0.0f, Length, FMath::Lerp(static_cast<float>(Step) / Steps, Along, 0.5f));
			Row(LowerDown, Lower);
			FLinearColor LowerAcross[4];
			Colours(LowerDown, LowerAcross);
			Lefts.Add(Lower[0]);
			Rights.Add(Lower[3]);
			LeftColours.Add(LowerAcross[0]);
			RightColours.Add(LowerAcross[3]);
			for (int32 Column = 0; Column < 3; ++Column)
			{
				Shapes.Quad(Upper[Column], Upper[Column + 1], Lower[Column + 1], Lower[Column], UpperAcross[Column], UpperAcross[Column + 1], LowerAcross[Column + 1], LowerAcross[Column]);
			}
			FMemory::Memcpy(Upper, Lower, sizeof(Upper));
			FMemory::Memcpy(UpperAcross, LowerAcross, sizeof(UpperAcross));
		}
		const float End = HalfAt(Length);
		Shapes.Bulb(FVector2D(X, Top + Length), End, End * 1.05f, bOutline ? MeltOutline : MeltLight, bOutline ? MeltOutline : MeltDeep);
		DripRim(Shapes, Lefts, Rights, LeftColours, RightColours, FVector2D(X, Top + Length), End, End * 1.05f, bOutline ? MeltOutline : MeltDeep);
		if (!bOutline && Length > Neck + Width)
		{
			Shapes.Rect(X - Width * 0.3f, Top + Neck, FMath::Max(1.5f * Unit, Width * 0.12f), Length - Neck - Width * 0.2f, MeltShine);
		}
	}
}

void ACheeseStrikeHUD::DrawDeath(const ACheeseStrikeCharacter* Character, const ACheeseStrikeGameState* Match, bool bOut)
{
	const float Width = Canvas->SizeX;
	const float Height = Canvas->SizeY;
	const float U = Unit();
	const float Now = Match != nullptr ? static_cast<float>(Match->GetServerWorldTimeSeconds()) : GetWorld()->GetTimeSeconds();
	const float Seconds = CVarMeltAt.GetValueOnGameThread() >= 0.0f ? CVarMeltAt.GetValueOnGameThread()
		: Character->GetDiedAt() >= 0.0f ? FMath::Max(0.0f, Now - Character->GetDiedAt()) : CheeseStrikeHUD::MeltTime;
	// Out of the round, only a while: then the mode's notice of being out.
	if (!CheeseStrikeHUD::ShowsDeathScreen(bOut, Seconds))
	{
		return;
	}
	const float Alpha = CheeseStrikeHUD::DeathScreenAlpha(bOut, Seconds);

	// The view darkens a little, and the sheet comes down with its drips: the drips first, their tops under the sheet, so that the
	// sheet's edge runs on into each drip's neck as one shape.
	HudDraw::FShapes Shapes;
	Shapes.Rect(0.0f, 0.0f, Width, Height, FLinearColor(0.03f, 0.015f, 0.0f, 0.42f * FMath::Clamp(Seconds / 0.5f, 0.0f, 1.0f)));
	const float Depth = CheeseStrikeHUD::SheetDepth(Seconds) * Height;
	if (Depth > 0.5f)
	{
		// Each drip's neck, where the sheet runs on into it: its middle and how far either side its fillets reach.
		TArray<FVector2D, TInlineAllocator<32>> Necks;
		for (int32 Index = 0; Index < CheeseStrikeHUD::Drips; ++Index)
		{
			const CheeseStrikeHUD::FDrip Drip = CheeseStrikeHUD::Drip(Index, Seconds);
			const float X = Drip.X * Width;
			SheetDrip(Shapes, X, Drip.Width * Height, Width, Depth, SheetEdge(X, Width, Depth) - 2.0f * U + Drip.Length * Height, U);
			Necks.Add(FVector2D(X, Drip.Width * Height * (0.5f + 0.55f)));
		}
		// The wet shine along the edge stops short of each neck, so that no line crosses where the sheet and a drip are one.
		const auto Shine = [&](float X)
		{
			float Away = 1.0f;
			for (const FVector2D& Neck : Necks)
			{
				Away = FMath::Min(Away, FMath::SmoothStep(Neck.Y, Neck.Y + 14.0f * U, FMath::Abs(X - Neck.X)));
			}
			return FLinearColor(MeltShine.R, MeltShine.G, MeltShine.B, MeltShine.A * Away);
		};
		constexpr int32 Steps = 192;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float X0 = Width * Step / Steps;
			const float X1 = Width * (Step + 1) / Steps;
			const float Y0 = SheetEdge(X0, Width, Depth);
			const float Y1 = SheetEdge(X1, Width, Depth);
			Shapes.Quad(FVector2D(X0, 0.0f), FVector2D(X1, 0.0f), FVector2D(X1, Y1), FVector2D(X0, Y0), MeltDeep, MeltDeep, MeltLight, MeltLight);
			// Its wavy edge soft, as the drips' sides are.
			Shapes.Fringe(FVector2D(X0, Y0), FVector2D(X1, Y1), FVector2D(0.0f, CheeseStrikeHUD::SoftEdge), MeltLight);
			// A wet shine just above the edge, away from the drips.
			const FLinearColor Shine0 = Shine(X0);
			const FLinearColor Shine1 = Shine(X1);
			if (Shine0.A > 0.01f || Shine1.A > 0.01f)
			{
				Shapes.Quad(FVector2D(X0, Y0 - 9.0f * U), FVector2D(X1, Y1 - 9.0f * U), FVector2D(X1, Y1 - 6.0f * U), FVector2D(X0, Y0 - 6.0f * U), Shine0, Shine1, Shine1, Shine0);
			}
		}
	}
	Shapes.Fade(Alpha);
	Shapes.Draw(Canvas);

	// The words, letter by letter: they come up a little large and settle, then sag, stretching down, and drip.
	const FString Words = TEXT("YOU GOT CHEESED");
	const float Size = 110.0f * U;
	const float Drop = Size / 76.0f;
	const FSlateFontInfo Big = HudDraw::Font(Size);
	const float Appear = FMath::Clamp((Seconds - 0.12f) / 0.25f, 0.0f, 1.0f);
	if (Appear <= 0.0f)
	{
		return;
	}
	const float Grow = 1.0f + 0.25f * FMath::Square(1.0f - Appear);
	TArray<float> Lefts;
	float Across = 0.0f;
	for (int32 Index = 0; Index < Words.Len(); ++Index)
	{
		Lefts.Add(Across);
		Across += HudDraw::Measure(Words.Mid(Index, 1), Big).X * Grow;
	}
	const float LineHeight = HudDraw::Measure(Words, Big).Y;
	const float Left = (Width - Across) / 2.0f;
	const float Top = Height * 0.33f - LineHeight * (Grow - 1.0f) / 2.0f;

	// A dark band behind them, soft at its edges, so that cheese-coloured words read over cheese.
	HudDraw::FShapes Band;
	const float BandTop = Height * 0.33f - 24.0f * U;
	const float BandBottom = Height * 0.33f + LineHeight * 1.3f + 110.0f * U;
	const FLinearColor Clear(0.04f, 0.02f, 0.0f, 0.0f);
	const FLinearColor Shade(0.04f, 0.02f, 0.0f, 0.5f * Appear);
	const float Soft1 = FMath::Lerp(BandTop, BandBottom, 0.25f);
	const float Soft2 = FMath::Lerp(BandTop, BandBottom, 0.75f);
	Band.Quad(FVector2D(0.0f, BandTop), FVector2D(Width, BandTop), FVector2D(Width, Soft1), FVector2D(0.0f, Soft1), Clear, Clear, Shade, Shade);
	Band.Rect(0.0f, Soft1, Width, Soft2 - Soft1, Shade);
	Band.Quad(FVector2D(0.0f, Soft2), FVector2D(Width, Soft2), FVector2D(Width, BandBottom), FVector2D(0.0f, BandBottom), Shade, Shade, Clear, Clear);
	Band.Fade(Alpha);
	Band.Draw(Canvas);

	struct FLetter
	{
		FString Glyph;
		FVector2D At;
		FVector2D Stretch;
		float Sag;
	};
	TArray<FLetter> Letters;
	for (int32 Index = 0; Index < Words.Len(); ++Index)
	{
		if (Words[Index] == TEXT(' '))
		{
			continue;
		}
		const float Sag = CheeseStrikeHUD::LetterSag(Index, Seconds);
		Letters.Add({Words.Mid(Index, 1), FVector2D(Left + Lefts[Index], Top + 10.0f * U * Sag), FVector2D(Grow, Grow * (1.0f + 0.4f * Sag)), Sag});
	}
	// Drips from under some of the letters, longer as they sag; outlined like the letters.
	HudDraw::FShapes Outline;
	HudDraw::FShapes Fill;
	for (int32 Index = 0; Index < Letters.Num(); ++Index)
	{
		const FLetter& Letter = Letters[Index];
		if (Index % 3 == 1 || Index == Letters.Num() - 1)
		{
			const float Glyph = HudDraw::Measure(Letter.Glyph, Big).X * Letter.Stretch.X;
			const float Bottom = Letter.At.Y + LineHeight * Letter.Stretch.Y * 0.8f - 8.0f * U * Drop;
			const float Length = (20.0f + 90.0f * (Index % 2 == 0 ? 1.0f : 0.55f)) * U * Drop * Letter.Sag;
			const float DripWidth = (Index % 2 == 0 ? 11.0f : 8.0f) * U * Drop;
			const float X = Letter.At.X + Glyph * (Index % 2 == 0 ? 0.42f : 0.6f);
			MeltDrip(Outline, X, Bottom, DripWidth, Length, U, true);
			MeltDrip(Fill, X, Bottom, DripWidth, Length, U, false);
		}
	}
	Outline.Fade(Alpha);
	Outline.Draw(Canvas);
	const FLinearColor Fade(1.0f, 1.0f, 1.0f, Appear * Alpha);
	const float Edge = 2.6f * U * Drop;
	for (const FLetter& Letter : Letters)
	{
		for (const FVector2D& Way : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1.6f), FVector2D(0, 1.8f), FVector2D(1.4f, 0), FVector2D(-1.4f, 0), FVector2D(0, -1.4f)})
		{
			HudDraw::Text(Canvas, Letter.Glyph, Letter.At + Way * Edge, Big, MeltOutline * Fade, 0.0f, false, Letter.Stretch);
		}
	}
	Fill.Fade(Alpha);
	Fill.Draw(Canvas);
	// Each letter a slab of cheese: its deeper underside, then its face.
	for (const FLetter& Letter : Letters)
	{
		HudDraw::Text(Canvas, Letter.Glyph, Letter.At + FVector2D(0.0f, 1.4f * Edge), Big, MeltDeep * Fade, 0.0f, false, Letter.Stretch);
	}
	for (const FLetter& Letter : Letters)
	{
		HudDraw::Text(Canvas, Letter.Glyph, Letter.At, Big, MeltLight * Fade, 0.0f, false, Letter.Stretch);
	}

	// Who did it, and the seconds until the player is back.
	const float Under = FMath::Clamp((Seconds - 0.5f) / 0.3f, 0.0f, 1.0f) * Alpha;
	if (Under > 0.0f)
	{
		// Clear of the letters, however far they sag.
		const float Below = Height * 0.33f + LineHeight * 1.3f + 16.0f * U;
		const FString By = !Character->GetKilledBy().IsEmpty() ? FString::Printf(TEXT("BY %s"), *Character->GetKilledBy().ToUpper())
			: (Character->GetDiedHow().IsEmpty() ? FString(TEXT("KILLED")) : Character->GetDiedHow().ToUpper());
		const FSlateFontInfo ByFont = HudDraw::Font(30.0f * U);
		const FString Fitted = CheeseStrikeHUD::FitNameIn(By, Character->GetKilledBy().ToUpper(), 480.0f * U, [&ByFont](const FString& Text) { return static_cast<float>(HudDraw::Measure(Text, ByFont).X); });
		HudDraw::Text(Canvas, Fitted, FVector2D(Width / 2.0f, Below), ByFont, HudDraw::Faded(HudDraw::Ink, Under), 0.5f);
		// Out of the round there is no coming back until the next: the mode's notice says so.
		if (Match != nullptr && !bOut)
		{
			const int32 Back = CheeseStrikeHUD::SecondsToComeBack(Match->GetRespawnDelay(), Now, Character->GetDiedAt());
			HudDraw::Text(Canvas, FString::Printf(TEXT("BACK IN %d"), Back), FVector2D(Width / 2.0f, Below + 44.0f * U), HudDraw::Font(20.0f * U), HudDraw::Faded(HudDraw::Soft, Under), 0.5f);
		}
	}
}
