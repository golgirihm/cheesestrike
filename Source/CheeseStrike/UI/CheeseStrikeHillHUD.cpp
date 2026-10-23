// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHillHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Map/CheeseStrikeHill.h"
#include "UI/CheeseStrikeHUDDraw.h"

/** The HUD's drawing, named through an alias rather than used as a whole: the packaged game's unity build would carry a using-directive into the files after this one. */
namespace HudDraw = CheeseStrikeHUDDraw;

namespace
{
	/** How high over the hill's floor its marker hangs (cm), and how far in from the screen's edges a marker held to them stands (design pixels): clear of the minimap and the weapon at the sides, the players along the top and the score at the bottom. */
	constexpr float HillMarkerHeight = 470.0f;
	constexpr float HillMarkerSide = 80.0f;
	constexpr float HillMarkerTop = 120.0f;
	constexpr float HillMarkerBottom = 170.0f;

	/** A crown, `Size` across about `At`: the hill's mark. */
	void HillCrown(HudDraw::FShapes& Shapes, const FVector2D& At, float Size, const FLinearColor& Colour)
	{
		const float H = Size * 0.5f;
		const FVector2D BaseLeft = At + FVector2D(-H, H * 0.45f);
		const FVector2D BaseRight = At + FVector2D(H, H * 0.45f);
		const FVector2D TopLeft = At + FVector2D(-H, -H * 0.15f);
		const FVector2D TopRight = At + FVector2D(H, -H * 0.15f);
		// The band, then three points rising from it.
		Shapes.Quad(TopLeft, TopRight, BaseRight, BaseLeft, Colour);
		Shapes.Triangle(TopLeft, At + FVector2D(-H * 0.85f, -H * 0.85f), At + FVector2D(-H * 0.3f, -H * 0.15f), Colour);
		Shapes.Triangle(At + FVector2D(-H * 0.35f, -H * 0.15f), At + FVector2D(0.0f, -H * 1.05f), At + FVector2D(H * 0.35f, -H * 0.15f), Colour);
		Shapes.Triangle(At + FVector2D(H * 0.3f, -H * 0.15f), At + FVector2D(H * 0.85f, -H * 0.85f), TopRight, Colour);
	}

	/** Where `At` (world) is for a marker: projected on the screen, or for something behind the view, far off the way it lies. */
	FVector2D HillProjected(const AHUD* Hud, UCanvas* Canvas, const FVector& At)
	{
		const APlayerController* Owner = Hud->GetOwningPlayerController();
		const APlayerCameraManager* Camera = Owner != nullptr ? Owner->PlayerCameraManager.Get() : nullptr;
		if (Camera == nullptr)
		{
			return FVector2D::ZeroVector;
		}
		const FVector Local = Camera->GetCameraRotation().UnrotateVector(At - Camera->GetCameraLocation());
		if (Local.X > 10.0f)
		{
			const FVector Projected = Canvas->Project(At, false);
			return FVector2D(Projected.X, Projected.Y);
		}
		FVector2D Way(Local.Y, -Local.Z);
		Way = Way.IsNearlyZero() ? FVector2D(0.0f, 1.0f) : Way.GetSafeNormal();
		return FVector2D(Canvas->SizeX / 2.0f, Canvas->SizeY / 2.0f) + Way * 100000.0f;
	}

	/** A marker: the crown in `Colour` (an arrow beside it pointing off the screen when held to the edge), `Word` over it and `Under` beneath. */
	void HillMarker(const AHUD* Hud, UCanvas* Canvas, const FVector& At, float Size, const FLinearColor& Colour, const FString& Word, const FString& Under)
	{
		const float U = HudDraw::Unit(Canvas);
		bool bOnEdge = false;
		const FVector2D Screen(Canvas->SizeX, Canvas->SizeY);
		const FBox2D Area(FVector2D(HillMarkerSide * U, HillMarkerTop * U), Screen - FVector2D(HillMarkerSide * U, HillMarkerBottom * U));
		const FVector2D Where = CheeseStrikeHillHUD::MarkerAt(HillProjected(Hud, Canvas, At), Area, bOnEdge);
		HudDraw::FShapes Shapes;
		HillCrown(Shapes, Where + FVector2D(1.5f * U, 1.5f * U), Size + 3.0f * U, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		HillCrown(Shapes, Where, Size, Colour);
		if (bOnEdge)
		{
			const FVector2D Way = (Where - Area.GetCenter()).GetSafeNormal();
			const FVector2D Across(-Way.Y, Way.X);
			const FVector2D Tip = Where + Way * (Size * 1.25f);
			Shapes.Triangle(Tip, Where + Way * (Size * 0.8f) + Across * (Size * 0.35f), Where + Way * (Size * 0.8f) - Across * (Size * 0.35f), Colour);
		}
		Shapes.Draw(Canvas);
		const FSlateFontInfo Big = HudDraw::Font(Size * 0.62f);
		const FSlateFontInfo Small = HudDraw::Font(Size * 0.55f);
		HudDraw::Text(Canvas, Word, FVector2D(Where.X, Where.Y - Size * 0.7f - HudDraw::Measure(Word, Big).Y), Big, Colour, 0.5f);
		HudDraw::Text(Canvas, Under, FVector2D(Where.X, Where.Y + Size * 0.4f + 2.0f * U), Small, HudDraw::Ink, 0.5f);
	}

	/** The local player's character, if alive. */
	const ACheeseStrikeCharacter* HillOwnBody(const AHUD* Hud)
	{
		const APlayerController* Owner = Hud->GetOwningPlayerController();
		const ACheeseStrikeCharacter* Own = Owner != nullptr ? Cast<ACheeseStrikeCharacter>(Owner->GetPawn()) : nullptr;
		return Own != nullptr && Own->IsAlive() ? Own : nullptr;
	}
}

namespace CheeseStrikeHillHUD
{
	FString MarkerWord(CheeseStrikeHills::EHold State, bool bMine)
	{
		if (State == CheeseStrikeHills::EHold::Contested)
		{
			return TEXT("CONTESTED");
		}
		return State == CheeseStrikeHills::EHold::Held && bMine ? TEXT("DEFEND") : TEXT("CAPTURE");
	}

	FString HoldWords(bool bTeams, CheeseStrikeHills::EHold State, bool bMine, const FString& HolderName)
	{
		switch (State)
		{
		case CheeseStrikeHills::EHold::Contested:
			return TEXT("CONTESTED");
		case CheeseStrikeHills::EHold::Held:
			if (bMine)
			{
				return bTeams ? TEXT("YOUR SIDE HOLDS THE HILL") : TEXT("YOU HOLD THE HILL");
			}
			// A side's name is plural (The Gouda Guys hold it); a player's is one.
			return FString::Printf(TEXT("%s %s THE HILL"), *HolderName.ToUpper(), bTeams ? TEXT("HOLD") : TEXT("HOLDS"));
		default:
			return TEXT("NOBODY HOLDS THE HILL");
		}
	}

	FString MoveWords(float SecondsLeft)
	{
		if (CheeseStrikeHills::IsWarning(SecondsLeft))
		{
			return FString::Printf(TEXT("MOVING IN %d"), FMath::CeilToInt(SecondsLeft));
		}
		const int32 Whole = FMath::Max(0, FMath::CeilToInt(SecondsLeft));
		return FString::Printf(TEXT("MOVES IN %d:%02d"), Whole / 60, Whole % 60);
	}

	FString DistanceWords(float Centimetres)
	{
		return FString::Printf(TEXT("%d M"), FMath::RoundToInt(FMath::Max(0.0f, Centimetres) / 100.0f));
	}

	FVector2D MarkerAt(const FVector2D& Projected, const FBox2D& Area, bool& bOnEdge)
	{
		bOnEdge = Projected.X < Area.Min.X || Projected.Y < Area.Min.Y || Projected.X > Area.Max.X || Projected.Y > Area.Max.Y;
		if (!bOnEdge)
		{
			return Projected;
		}
		const FVector2D Middle = Area.GetCenter();
		const FVector2D Way = Projected - Middle;
		const FVector2D Half = Area.GetExtent();
		const float Scale = FMath::Min(FMath::Abs(Way.X) > KINDA_SMALL_NUMBER ? Half.X / FMath::Abs(Way.X) : TNumericLimits<float>::Max(),
			FMath::Abs(Way.Y) > KINDA_SMALL_NUMBER ? Half.Y / FMath::Abs(Way.Y) : TNumericLimits<float>::Max());
		return Middle + Way * FMath::Min(Scale, 1.0f);
	}

	float Draw(AHUD* Hud, UCanvas* Canvas, float Bottom)
	{
		const UWorld* World = Hud != nullptr ? Hud->GetWorld() : nullptr;
		const ACheeseStrikeHill* Hill = ACheeseStrikeHill::Find(World);
		const ACheeseStrikeGameState* Match = World != nullptr ? World->GetGameState<ACheeseStrikeGameState>() : nullptr;
		const APlayerController* Owner = Hud != nullptr ? Hud->GetOwningPlayerController() : nullptr;
		if (Canvas == nullptr || Hill == nullptr || !Hill->HasBegun() || Match == nullptr || Owner == nullptr || Match->HasMatchEnded())
		{
			return Bottom;
		}
		const float U = HudDraw::Unit(Canvas);
		const APlayerState* Viewer = Owner->PlayerState;
		const float Time = World->GetTimeSeconds();
		const FLinearColor Colour = Hill->ColourFor(Viewer, Time);
		const CheeseStrikeHills::EHold State = Hill->GetHold().State;
		const bool bMine = Hill->IsHeldBy(Viewer);
		const ACheeseStrikeCharacter* Own = HillOwnBody(Hud);
		const bool bInside = Own != nullptr && Hill->ContainsActor(Own);
		const FVector Eye = Owner->PlayerCameraManager != nullptr ? Owner->PlayerCameraManager->GetCameraLocation() : Hill->GetCentre();

		// Over the hill, wherever it is (not while standing in it), and over where it goes while it warns.
		if (!bInside)
		{
			HillMarker(Hud, Canvas, Hill->GetCentre() + FVector(0.0f, 0.0f, HillMarkerHeight), 30.0f * U, Colour, MarkerWord(State, bMine), DistanceWords(FVector::Dist(Eye, Hill->GetCentre())));
		}
		if (Hill->IsWarning() && Hill->GetNextIndex() != Hill->GetSpotIndex())
		{
			const float Blink = FMath::Fmod(Time, 0.8f) < 0.5f ? 1.0f : 0.45f;
			HillMarker(Hud, Canvas, Hill->GetNextCentre() + FVector(0.0f, 0.0f, HillMarkerHeight), 22.0f * U, HudDraw::Faded(ACheeseStrikeHill::Neutral, Blink),
				FString::Printf(TEXT("NEXT  %d"), FMath::CeilToInt(Hill->SecondsToMove())), DistanceWords(FVector::Dist(Eye, Hill->GetNextCentre())));
		}

		// The strip over the score: who holds it, in its colour, whether the player is in it, and when it moves, its time running down under it.
		const FString Holds = HoldWords(Match->HasTeams(), State, bMine, Hill->GetHolderName());
		const FString Moves = MoveWords(Hill->SecondsToMove());
		const FString In = bInside ? TEXT("IN THE HILL") : FString();
		const FSlateFontInfo Line = HudDraw::Font(16.0f * U);
		const FSlateFontInfo Small = HudDraw::Font(13.0f * U);
		// As wide as its words need, 460 at the least.
		const float Words = 38.0f * U + HudDraw::Measure(Holds, Line).X + (In.IsEmpty() ? 0.0f : 14.0f * U + HudDraw::Measure(In, Small).X) + 28.0f * U + HudDraw::Measure(Moves, Small).X + 10.0f * U;
		const float Width = FMath::Max(460.0f * U, Words);
		const float Height = 30.0f * U;
		const float Left = (Canvas->SizeX - Width) / 2.0f;
		const float Top = Bottom - Height - 4.0f * U;
		HudDraw::FShapes Shapes;
		Shapes.Rect(Left, Top, Width, Height, HudDraw::Panel);
		Shapes.Rect(Left, Top, 5.0f * U, Height, Colour);
		const float Share = FMath::Clamp(Hill->SecondsToMove() / CheeseStrikeHills::HillTime, 0.0f, 1.0f);
		Shapes.Rect(Left, Top + Height, Width, 3.0f * U, HudDraw::Faded(HudDraw::Dim, 0.6f));
		Shapes.Rect(Left, Top + Height, Width * Share, 3.0f * U, Hill->IsWarning() ? HudDraw::Danger : HudDraw::Faded(Colour, 0.85f));
		HillCrown(Shapes, FVector2D(Left + 22.0f * U, Top + Height / 2.0f + 1.0f * U), 16.0f * U, Colour);
		Shapes.Draw(Canvas);
		const float TextTop = Top + (Height - HudDraw::Measure(TEXT("H"), Line).Y) / 2.0f;
		HudDraw::Text(Canvas, Holds, FVector2D(Left + 38.0f * U, TextTop), Line, Colour);
		HudDraw::Text(Canvas, Moves, FVector2D(Left + Width - 10.0f * U, TextTop + 1.5f * U), Small, Hill->IsWarning() ? HudDraw::Danger : HudDraw::Soft, 1.0f);
		if (!In.IsEmpty())
		{
			const float After = Left + 38.0f * U + HudDraw::Measure(Holds, Line).X + 14.0f * U;
			HudDraw::Text(Canvas, In, FVector2D(After, TextTop + 1.5f * U), Small, HudDraw::Ink);
		}
		return Top - 8.0f * U;
	}

	void DrawOnMinimap(const UWorld* World, const APlayerState* Viewer, HudDraw::FShapes& Shapes, TFunctionRef<FVector2D(const FVector&)> OnMap, const FVector2D& Centre, float Half, float Scale, float U)
	{
		const ACheeseStrikeHill* Hill = ACheeseStrikeHill::Find(World);
		if (Hill == nullptr || !Hill->HasBegun() || World == nullptr)
		{
			return;
		}
		const float Time = World->GetTimeSeconds();
		const auto Mark = [&](const FVector& Where, float Radius, const FLinearColor& Colour, float Alpha)
		{
			FVector2D At = OnMap(Where);
			const FVector2D From = At - Centre;
			const float Out = FMath::Max(FMath::Abs(From.X), FMath::Abs(From.Y));
			const float Room = Half - 8.0f * U;
			if (Out > Room)
			{
				// Further than the minimap reaches: a small crown on its frame, the way it lies.
				At = Centre + From * (Room / Out);
				Shapes.Disc(At, 7.0f * U, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f * Alpha), 16);
				HillCrown(Shapes, At, 10.0f * U, HudDraw::Faded(Colour, Alpha));
				return;
			}
			const float Across = FMath::Max(5.0f * U, Radius * Scale);
			Shapes.Disc(At, Across, HudDraw::Faded(Colour, 0.18f * Alpha), 28);
			Shapes.Arc(At, Across, 2.0f * U, 0.0f, 360.0f, HudDraw::Faded(Colour, 0.95f * Alpha), false);
			HillCrown(Shapes, At, 9.0f * U, HudDraw::Faded(Colour, Alpha));
		};
		if (Hill->IsWarning() && Hill->GetNextIndex() != Hill->GetSpotIndex())
		{
			Mark(Hill->GetNextCentre(), Hill->GetNextRadius(), ACheeseStrikeHill::Neutral, FMath::Fmod(Time, 0.8f) < 0.5f ? 0.7f : 0.3f);
		}
		Mark(Hill->GetCentre(), Hill->GetRadius(), Hill->ColourFor(Viewer, Time), 1.0f);
	}
}
