// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Choosing where a Cheese Strike goes: while its player marks it
// (ACheeseStrikePlayerState::IsMarkingStrike), the minimap comes big into
// the middle of the screen, turned as the view was when they began, and the
// look (the mouse or the right stick) moves a mark over it instead of the
// view (ACheeseStrikeCharacter::MoveStrikeMark). Round the mark, the area
// the wheels fall on (CheeseStrikeStreaks::StrikeRadius) and where each will
// land; the map follows the mark when it nears an edge
// (CheeseStrikeHUD::FollowMark). Fire calls the strike in there; the Streak
// key or Escape calls it off.

#include "CheeseStrikeHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeHUDDraw.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "Input/CheeseStrikeInput.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "Streaks/CheeseStrikeBombs.h"
#include "Streaks/CheeseStrikeOverhead.h"

/** The HUD's drawing, named through an alias rather than used as a whole: the packaged game's unity build would carry a using-directive into the files after this one. */
namespace HudDraw = CheeseStrikeHUDDraw;

bool ACheeseStrikeHUD::DrawStrikeMap(const ACheeseStrikePlayerController* Controller, const ACheeseStrikeCharacter* Character, float FieldOfView)
{
	ACheeseStrikePlayerState* Mine = Controller != nullptr ? Controller->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	if (Mine == nullptr || !Mine->IsMarkingStrike() || Controller->PlayerCameraManager == nullptr || Character == nullptr)
	{
		bStrikeMapShown = false;
		return false;
	}
	UWorld* World = GetWorld();
	Minimap.Gather(World);
	const float U = Unit();
	const float Yaw = Mine->GetStrikeYaw();
	const FVector At = Character->GetActorLocation();

	// The mark kept over the level, and the map's middle following it (from the player, as it opens).
	const FBox2D& Level = Minimap.GetBounds();
	if (Level.bIsValid)
	{
		const FVector2D Mark = Mine->GetStrikeTarget();
		const FVector2D Held(FMath::Clamp(Mark.X, Level.Min.X, Level.Max.X), FMath::Clamp(Mark.Y, Level.Min.Y, Level.Max.Y));
		if (!Held.Equals(Mark, 0.1f))
		{
			Mine->SetStrikeTarget(Held);
		}
	}
	const FVector2D Mark = Mine->GetStrikeTarget();
	if (!bStrikeMapShown)
	{
		StrikeMapMiddle = FVector2D(At);
		bStrikeMapShown = true;
	}
	StrikeMapMiddle = CheeseStrikeHUD::FollowMark(StrikeMapMiddle, Mark, Yaw, CheeseStrikeHUD::StrikeMapRange * CheeseStrikeHUD::StrikeMapFollow);

	// Big in the middle, as much of the screen's height as leaves room for its words over and under it.
	const FSlateFontInfo Title = HudDraw::Font(22.0f * U);
	const FSlateFontInfo Line = HudDraw::Font(15.0f * U);
	const float Side = FMath::Min(620.0f * U, FMath::Min(Canvas->SizeX - 2.0f * Margin * U, Canvas->SizeY - 200.0f * U));
	const FVector2D Centre(Canvas->SizeX / 2.0f, Canvas->SizeY / 2.0f - 6.0f * U);
	FMapFrame Frame;
	Frame.Min = Centre - FVector2D(Side, Side) / 2.0f;
	Frame.Max = Centre + FVector2D(Side, Side) / 2.0f;
	Frame.Middle = StrikeMapMiddle;
	Frame.ViewYaw = Yaw;
	Frame.Scale = Side / 2.0f / CheeseStrikeHUD::StrikeMapRange;
	Frame.Corner = 24.0f * U;
	Frame.bStrike = true;
	const auto OnMap = [&Frame, &Centre](const FVector2D& Where) { return Centre + CheeseStrikeHUD::MapPoint(Where - Frame.Middle, Frame.ViewYaw, Frame.Scale); };

	// The game behind dimmed, so that the map stands out from it.
	HudDraw::FShapes Under;
	Under.Rect(0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.0f, 0.0f, 0.0f, 0.38f));
	Under.Draw(Canvas);
	DrawMap(Controller, Character, FieldOfView, Frame);

	// The area: filled faintly, ringed, a pulse running out to its edge, and where each wheel will land.
	const float Now = World->GetTimeSeconds();
	const FVector2D MarkAt = OnMap(Mark);
	const float Radius = CheeseStrikeStreaks::StrikeRadius * Frame.Scale;
	HudDraw::FShapes Shapes;
	// A faint dashed line from the player to the mark.
	const FVector2D PlayerAt = OnMap(FVector2D(At));
	const float Length = FVector2D::Distance(PlayerAt, MarkAt);
	if (Length > Radius)
	{
		const FVector2D Way = (MarkAt - PlayerAt) / Length;
		for (float Along = 22.0f * U; Along + 8.0f * U < Length - Radius; Along += 16.0f * U)
		{
			Shapes.Line(PlayerAt + Way * Along, PlayerAt + Way * (Along + 8.0f * U), 2.0f * U, FLinearColor(1.0f, 1.0f, 1.0f, 0.35f));
		}
	}
	// Over nothing (past the level's edge), grey, and a shot does nothing.
	FVector Spot;
	const bool bGround = Mine->StrikeSpot(Spot);
	const FLinearColor Area = bGround ? HudDraw::Danger : FLinearColor(0.6f, 0.6f, 0.62f, 1.0f);
	Shapes.Disc(MarkAt, Radius, HudDraw::Faded(Area, 0.2f), 48);
	const float Pulse = FMath::Fmod(Now, 1.2f) / 1.2f;
	if (bGround)
	{
		Shapes.Arc(MarkAt, Radius * Pulse, 2.0f * U, 0.0f, 360.0f, HudDraw::Faded(Area, 0.5f * (1.0f - Pulse)), false);
	}
	Shapes.Arc(MarkAt, Radius, 2.5f * U, 0.0f, 360.0f, HudDraw::Faded(Area, 0.95f), false);
	if (bGround)
	{
		const FCheeseStrikeOverhead& Overhead = FCheeseStrikeOverhead::Of(World);
		for (const FVector& Wheel : CheeseStrikeStreaks::StrikeArea(Spot, Yaw))
		{
			FVector Impact;
			float Roof = 0.0f;
			if (ACheeseStrikeBombs::StrikeLanding(World, Overhead, Wheel, Impact, Roof))
			{
				const FVector2D WheelAt = OnMap(FVector2D(Impact));
				Shapes.Disc(WheelAt, 5.0f * U, FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), 16);
				Shapes.Disc(WheelAt, 3.5f * U, HudDraw::Faded(HudDraw::Danger, 0.95f), 16);
			}
		}
	}
	// The mark itself: a ring with ticks, white over the red.
	const float Tick = 9.0f * U;
	const float Gap = 5.0f * U;
	Shapes.Arc(MarkAt, Gap + Tick + 2.0f * U, 1.5f * U, 0.0f, 360.0f, HudDraw::Ink, false);
	for (const FVector2D& Way : {FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1)})
	{
		Shapes.Line(MarkAt + Way * (Gap - 1.0f * U), MarkAt + Way * (Gap + Tick + 1.0f * U), 4.0f * U, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		Shapes.Line(MarkAt + Way * Gap, MarkAt + Way * (Gap + Tick), 2.0f * U, HudDraw::Ink);
	}
	Shapes.Draw(Canvas);

	// What it is over the map, and the keys under it, as the player plays.
	const bool bPad = CheeseStrikeInput::IsUsingGamepad(Controller);
	const FString Fire = UCheeseStrikeInputSet::Prompt(Controller, &UCheeseStrikeInputSet::Fire);
	const FString Streak = UCheeseStrikeInputSet::Prompt(Controller, &UCheeseStrikeInputSet::Streak);
	const FString Menu = UCheeseStrikeInputSet::Prompt(Controller, &UCheeseStrikeInputSet::Menu);
	const float TitleHigh = HudDraw::Measure(TEXT("C"), Title).Y;
	HudDraw::Text(Canvas, TEXT("CHOOSE WHERE THE CHEESE STRIKE FALLS"), FVector2D(Centre.X, Frame.Min.Y - TitleHigh - 10.0f * U), Title, HudDraw::Accent, 0.5f);
	const FString Keys = FString::Printf(TEXT("%s TO MOVE  ·  %s TO CALL IT IN  ·  %s OR %s TO CALL IT OFF"), bPad ? TEXT("RIGHT STICK") : TEXT("MOUSE"), *Fire, *Streak, *Menu);
	HudDraw::Text(Canvas, Keys, FVector2D(Centre.X, Frame.Max.Y + 12.0f * U), Line, HudDraw::Ink, 0.5f);
	if (!bGround)
	{
		HudDraw::Text(Canvas, TEXT("NOTHING TO STRIKE THERE"), FVector2D(MarkAt.X, MarkAt.Y + Radius + 6.0f * U), Line, HudDraw::Danger, 0.5f);
	}
	return true;
}
