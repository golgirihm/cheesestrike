// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The HUD's parts that are about where everyone is: the minimap, the row of
// players along the top, the name tags over teammates, and noticing which
// enemies the player can see.

#include "CheeseStrikeHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CheeseStrikeAvatars.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikeHUDDraw.h"
#include "CheeseStrikeHillHUD.h"
#include "CheeseStrikePickupHUD.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "Map/CheeseStrikePlace.h"
#include "Modes/CheeseStrikeLastStand.h"

/** The HUD's drawing, named through an alias rather than used as a whole: the packaged game's unity build would carry a using-directive into the files after this one. */
namespace HudDraw = CheeseStrikeHUDDraw;

namespace
{
	/** Where a character's feet are. */
	float FeetOf(const ACheeseStrikeCharacter* Character)
	{
		return Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}

	/** A dot on the minimap with a dark rim, and a tick pointing the way it faces (`Facing`, screen degrees clockwise from up) unless it has none. */
	void MapDot(HudDraw::FShapes& Shapes, const FVector2D& At, float Radius, const FLinearColor& Colour, float Facing, bool bFacing)
	{
		Shapes.Disc(At, Radius + 1.5f, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f * Colour.A), 16);
		Shapes.Disc(At, Radius, Colour, 16);
		if (bFacing)
		{
			const float Radians = FMath::DegreesToRadians(Facing);
			const FVector2D Way(FMath::Sin(Radians), -FMath::Cos(Radians));
			const FVector2D Across(-Way.Y, Way.X);
			Shapes.Triangle(At + Way * (Radius + 5.0f) , At + Way * (Radius - 1.0f) + Across * (Radius * 0.8f), At + Way * (Radius - 1.0f) - Across * (Radius * 0.8f), Colour);
		}
	}

	/**
	 * The player on the minimap: a wedge of cheese seen from above, its point
	 * the way they face (up on the minimap; `Turn` degrees clockwise from up on
	 * the strike map, which is north up), at `Centre`, `U` pixels a design
	 * pixel. A dark outline, a body shaded from its point, the rind along its
	 * round back, and a few holes.
	 */
	void CheeseWedge(HudDraw::FShapes& Shapes, const FVector2D& Centre, float U, float Turn = 0.0f)
	{
		// Every offset from the middle turned clockwise on the screen (its y runs down).
		const auto Rot = [Turn](const FVector2D& Offset) { return Offset.GetRotated(Turn); };
		const float HalfAngle = FMath::DegreesToRadians(26.0f);
		constexpr int32 Steps = 10;
		const auto Wedge = [&](const FVector2D& Tip, float From, float To, const FLinearColor& Inner, const FLinearColor& Outer)
		{
			if (From <= 0.0f)
			{
				// A whole wedge: one outline, its edge soft, shaded out from its point.
				TArray<FVector2D, TInlineAllocator<Steps + 2>> Outline;
				Outline.Add(Tip);
				for (int32 Step = 0; Step <= Steps; ++Step)
				{
					const float Angle = FMath::Lerp(-HalfAngle, HalfAngle, static_cast<float>(Step) / Steps);
					Outline.Add(Tip + Rot(FVector2D(FMath::Sin(Angle), FMath::Cos(Angle))) * To);
				}
				Shapes.SmoothFill(Outline, [&](const FVector2D& Point) { return FMath::Lerp(Inner, Outer, FMath::Clamp(FVector2D::Distance(Point, Tip) / To, 0.0f, 1.0f)); });
				return;
			}
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const float A = FMath::Lerp(-HalfAngle, HalfAngle, static_cast<float>(Step) / Steps);
				const float B = FMath::Lerp(-HalfAngle, HalfAngle, static_cast<float>(Step + 1) / Steps);
				const FVector2D WayA = Rot(FVector2D(FMath::Sin(A), FMath::Cos(A)));
				const FVector2D WayB = Rot(FVector2D(FMath::Sin(B), FMath::Cos(B)));
				if (From <= 0.0f)
				{
					Shapes.Triangle(Tip, Tip + WayA * To, Tip + WayB * To, Inner, Outer, Outer);
				}
				else
				{
					Shapes.Quad(Tip + WayA * From, Tip + WayB * From, Tip + WayB * To, Tip + WayA * To, Inner, Inner, Outer, Outer);
					// The rind's round back soft, over the outline round it.
					Shapes.Fringe(Tip + WayA * To, Tip + WayB * To, (WayA + WayB).GetSafeNormal() * CheeseStrikeHUD::SoftEdge, Outer);
				}
			}
		};
		const FVector2D Tip = Centre + Rot(FVector2D(0.0f, -15.0f * U));
		const float Length = 27.0f * U;
		const float Rind = 4.5f * U;
		Wedge(Tip + Rot(FVector2D(0.0f, -3.0f * U)), 0.0f, Length + 4.5f * U, FLinearColor(0.0f, 0.0f, 0.0f, 0.8f), FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		Wedge(Tip, 0.0f, Length - Rind, FLinearColor(1.0f, 0.92f, 0.5f, 1.0f), FLinearColor(0.98f, 0.74f, 0.2f, 1.0f));
		Wedge(Tip, Length - Rind, Length, FLinearColor(0.93f, 0.55f, 0.08f, 1.0f), FLinearColor(0.78f, 0.4f, 0.04f, 1.0f));
		const FLinearColor Hole(0.8f, 0.48f, 0.06f, 1.0f);
		Shapes.Disc(Centre + Rot(FVector2D(-3.0f * U, 2.4f * U)), 2.4f * U, Hole, 12);
		Shapes.Disc(Centre + Rot(FVector2D(2.6f * U, -2.6f * U)), 1.8f * U, Hole, 12);
		Shapes.Disc(Centre + Rot(FVector2D(1.8f * U, 5.2f * U)), 1.6f * U, Hole, 12);
		Shapes.Disc(Centre + Rot(FVector2D(-0.6f * U, -7.5f * U)), 1.1f * U, Hole, 10);
	}
}

void ACheeseStrikeHUD::LookForEnemies(const ACheeseStrikePlayerController* Controller)
{
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	if (Now < NextLook || Controller->PlayerCameraManager == nullptr)
	{
		return;
	}
	NextLook = Now + 0.1f;
	const FVector Eye = Controller->PlayerCameraManager->GetCameraLocation();
	const FVector Ahead = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	const float HalfView = FMath::DegreesToRadians(FMath::Min(170.0f, Controller->PlayerCameraManager->GetFOVAngle()) / 2.0f + 5.0f);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CheeseStrikeHUDSight), false, Controller->GetPawn());
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		const ACheeseStrikeCharacter* Other = *It;
		if (Other == Controller->GetPawn() || !Other->IsAlive() || AreTeammates(Controller->PlayerState, Other->GetPlayerState()))
		{
			continue;
		}
		// In front, within the view, and with nothing between: the head, or failing that the middle.
		for (const FVector& Part : {Other->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f), Other->GetActorLocation()})
		{
			const FVector Way = (Part - Eye).GetSafeNormal();
			if (FVector::DotProduct(Way, Ahead) < FMath::Cos(HalfView))
			{
				continue;
			}
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, Eye, Part, ECC_Visibility, Query) || Hit.GetActor() == Other)
			{
				LastSeen.Add(Other, Now);
				break;
			}
		}
		// Seen, or revealed here: their mark moves to where they are, at most once a second.
		const float* Seen = LastSeen.Find(Other);
		if ((Seen != nullptr && *Seen == Now) || AreEnemiesRevealed())
		{
			NoteEnemy(Other, AreEnemiesRevealed());
		}
	}
	for (auto Seen = LastSeen.CreateIterator(); Seen; ++Seen)
	{
		if (!Seen.Key().IsValid() || Now - Seen.Value() > SeenLife)
		{
			Seen.RemoveCurrent();
		}
	}
	EnemyMarks.Prune(Now);
}

void ACheeseStrikeHUD::DrawMinimap(const ACheeseStrikePlayerController* Controller, const ACheeseStrikeCharacter* Character, float FieldOfView)
{
	if (Controller->PlayerCameraManager == nullptr)
	{
		return;
	}
	// At the top left, round the player, turned with the view.
	const float U = Unit();
	FMapFrame Frame;
	Frame.Min = FVector2D(Margin * U, Margin * U);
	Frame.Max = Frame.Min + FVector2D(MapSide * U, MapSide * U);
	const FVector Eye = Controller->PlayerCameraManager->GetCameraLocation();
	Frame.Middle = FVector2D(Eye.X, Eye.Y);
	Frame.ViewYaw = Controller->PlayerCameraManager->GetCameraRotation().Yaw;
	Frame.Scale = MapSide * U / 2.0f / CheeseStrikeHUD::MinimapRange;
	Frame.Corner = CheeseStrikeHUD::MinimapCorner * U;
	DrawMap(Controller, Character, FieldOfView, Frame);
}

void ACheeseStrikeHUD::DrawMap(const ACheeseStrikePlayerController* Controller, const ACheeseStrikeCharacter* Character, float FieldOfView, const FMapFrame& Frame)
{
	UWorld* World = GetWorld();
	Minimap.Gather(World);
	const float U = Unit();
	const FVector2D Min = Frame.Min;
	const FVector2D Max = Frame.Max;
	const FVector2D Centre = (Min + Max) / 2.0f;
	const float Half = (Max.X - Min.X) / 2.0f;
	const float Scale = Frame.Scale;
	const FVector Eye = Controller->PlayerCameraManager->GetCameraLocation();
	const float ViewYaw = Frame.ViewYaw;
	const FVector2D Middle = Frame.Middle;
	// Where the player stands; or, looking on from elsewhere (killed, from where their eyes were), as if standing there.
	const float Feet = Character != nullptr && Character->IsAlive() && Controller->GetViewTarget() == Character ? FeetOf(Character) : Eye.Z - 160.0f;
	const auto OnMap = [&](const FVector& At) { return Centre + CheeseStrikeHUD::MapPoint(FVector2D(At.X, At.Y) - Middle, ViewYaw, Scale); };

	// Rounded corners: the level is cut to them, and the marks held inside them.
	const float Corner = Frame.Corner;
	const FVector2D InsetMin = Min + FVector2D(6.0f * U, 6.0f * U);
	const FVector2D InsetMax = Max - FVector2D(6.0f * U, 6.0f * U);
	const float InsetCorner = FMath::Max(0.0f, Corner - 6.0f * U);

	HudDraw::FShapes Shapes;
	Shapes.RoundedRect(Min.X, Min.Y, Max.X - Min.X, Max.Y - Min.Y, Corner, FLinearColor(0.008f, 0.008f, 0.01f, Frame.bStrike ? 0.92f : 0.72f));
	Minimap.Draw(Shapes, Min, Max, Middle, ViewYaw, Feet, Scale, Corner);

	// Where the player is on it: the middle of the minimap, anywhere on the strike map.
	const FVector2D PlayerAt = OnMap(Eye);
	const bool bPlayerOn = CheeseStrikeHUD::InsideRoundedRect(PlayerAt, Min, Max, Corner) > 0.0f;

	// What the player can see: a faint cone up the map.
	const float HalfCone = FMath::Min(80.0f, FieldOfView / 2.0f);
	const int32 Steps = Frame.bStrike ? 0 : 12;
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const float A = FMath::DegreesToRadians(FMath::Lerp(-HalfCone, HalfCone, static_cast<float>(Step) / Steps));
		const float B = FMath::DegreesToRadians(FMath::Lerp(-HalfCone, HalfCone, static_cast<float>(Step + 1) / Steps));
		const float Reach = Half * 0.92f;
		Shapes.Triangle(Centre, Centre + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * Reach, Centre + FVector2D(FMath::Sin(B), -FMath::Cos(B)) * Reach,
			FLinearColor(1.0f, 1.0f, 1.0f, 0.13f), FLinearColor(1.0f, 1.0f, 1.0f, 0.0f), FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
	}

	// The weapons' stands, under the players.
	CheeseStrikePickupHUD::DrawOnMap(Shapes, World, OnMap, Min, Max, U);

	// Teammates always, held to the edge when further than it reaches; enemies only when seen, or revealed.
	const APlayerState* Me = Controller->PlayerState;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		const ACheeseStrikeCharacter* Other = *It;
		if (Other == Controller->GetPawn() || !Other->IsAlive() || Other->GetPlayerState() == nullptr)
		{
			continue;
		}
		const ACheeseStrikePlayerState* Their = Other->GetPlayerState<ACheeseStrikePlayerState>();
		FVector2D At = OnMap(Other->GetActorLocation());
		const float Facing = Other->GetActorRotation().Yaw - ViewYaw;
		if (AreTeammates(Me, Their))
		{
			const bool bBeyond = CheeseStrikeHUD::InsideRoundedRect(At, InsetMin, InsetMax, InsetCorner) < 0.0f;
			if (bBeyond)
			{
				At = CheeseStrikeHUD::HoldInsideRoundedRect(At, InsetMin, InsetMax, InsetCorner);
			}
			MapDot(Shapes, At, (bBeyond ? 3.5f : 4.5f) * U, HudDraw::Faded(HudDraw::SideColour(Their != nullptr ? Their->GetTeam() : 0), bBeyond ? 0.7f : 1.0f), Facing, !bBeyond);
		}
		else if (const CheeseStrikeHUD::FEnemyMark* Mark = EnemyMark(Other))
		{
			// Never where they are now: where they were last known to be (seen, firing, swept), moved at most once a second
			// and fading until the next. A swept one further than the minimap reaches is held to its frame, smaller.
			FVector2D Known = OnMap(Mark->Where);
			const bool bBeyond = CheeseStrikeHUD::InsideRoundedRect(Known, InsetMin, InsetMax, InsetCorner) < 0.0f;
			if (bBeyond && !Mark->bSwept)
			{
				continue;
			}
			if (bBeyond)
			{
				Known = CheeseStrikeHUD::HoldInsideRoundedRect(Known, InsetMin, InsetMax, InsetCorner);
			}
			const float Fade = CheeseStrikeHUD::EnemyMarkFade(World->GetTimeSeconds() - Mark->At);
			MapDot(Shapes, Known, (bBeyond ? 3.0f : 4.5f) * U, HudDraw::Faded(HudDraw::Danger, Fade * (bBeyond ? 0.75f : 1.0f)), 0.0f, false);
		}
	}

	// The side's Unmanned Aerial Cheese: a ring going out from the middle as each sweep comes (what it saw is in the marks).
	if (Sweep.Index != INDEX_NONE)
	{
		const float Ping = Sweep.Since / 0.7f;
		if (Ping < 1.0f && Half * 0.95f * Ping > 4.0f * U)
		{
			Shapes.Arc(PlayerAt, Half * 0.95f * Ping, 2.0f * U, 0.0f, 360.0f, HudDraw::Faded(HudDraw::Danger, 0.55f * (1.0f - Ping)), false);
		}
	}

	// King of the Hill: the hill, and where it goes next while it warns.
	CheeseStrikeHillHUD::DrawOnMinimap(World, Me, Shapes, OnMap, Centre, Half, Scale, U);

	// The player: a wedge of cheese, its point the way they face: up in the minimap's middle, turned on the strike map.
	if (bPlayerOn)
	{
		CheeseWedge(Shapes, PlayerAt, U, FRotator::NormalizeAxis(Controller->PlayerCameraManager->GetCameraRotation().Yaw - ViewYaw));
	}

	// Where the player is, by the map's names for its places, under the minimap.
	const FString Place = Frame.bStrike ? FString() : ACheeseStrikePlace::NameAt(World, Eye).ToUpper();

	// The frame, and north riding it.
	Shapes.RoundedFrame(Min.X, Min.Y, Max.X - Min.X, Max.Y - Min.Y, Corner, FMath::Max(1.0f, U), HudDraw::Rule);
	const FVector2D North = CheeseStrikeHUD::HoldInsideRoundedRect(Centre + CheeseStrikeHUD::NorthMark(ViewYaw, Half), Min, Max, Corner);
	Shapes.Disc(North, 10.0f * U, FLinearColor(0.02f, 0.02f, 0.025f, 0.9f), 20);
	Shapes.Arc(North, 10.0f * U, 1.5f * U, 0.0f, 360.0f, HudDraw::Faded(HudDraw::Accent, 0.9f), false);
	Shapes.Draw(Canvas);
	const FSlateFontInfo Letter = HudDraw::Font(12.0f * U);
	HudDraw::Text(Canvas, TEXT("N"), FVector2D(North.X, North.Y - HudDraw::Measure(TEXT("N"), Letter).Y / 2.0f), Letter, HudDraw::Accent, 0.5f, false);
	if (!Place.IsEmpty())
	{
		HudDraw::Text(Canvas, Place, FVector2D(Min.X, Max.Y + 8.0f * U), HudDraw::Font(16.0f * U), HudDraw::Soft);
	}
}

void ACheeseStrikeHUD::DrawPlayers(const ACheeseStrikeGameState* Match)
{
	if (Match == nullptr)
	{
		return;
	}
	if (Avatars == nullptr)
	{
		Avatars = NewObject<UCheeseStrikeAvatars>(this);
	}
	const APlayerController* Viewer = GetOwningPlayerController();
	const APlayerState* Me = Viewer != nullptr ? Viewer->PlayerState : nullptr;
	// Each side in the order they came, the first side to the left of the middle and the second to the right; in a
	// free-for-all, everyone in one row in the middle, in the free-for-all's colour.
	const bool bTeams = Match->HasTeams();
	TArray<const ACheeseStrikePlayerState*> Sides[CheeseStrikeRules::Teams];
	for (const APlayerState* Player : Match->PlayerArray)
	{
		const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Player);
		if (Ours != nullptr && !Ours->IsInactive() && !Ours->IsOnlyASpectator())
		{
			Sides[bTeams ? FMath::Clamp(Ours->GetTeam(), 0, CheeseStrikeRules::Teams - 1) : 0].Add(Ours);
		}
	}
	for (TArray<const ACheeseStrikePlayerState*>& Side : Sides)
	{
		Side.Sort([](const ACheeseStrikePlayerState& A, const ACheeseStrikePlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	}
	const float U = Unit();
	const float Size = 40.0f * U;
	const float Gap = 6.0f * U;
	const float Between = 40.0f * U;
	const float Top = Margin * U;
	const float Middle = Canvas->SizeX / 2.0f;
	const float Now = Match->GetServerWorldTimeSeconds();
	const FSlateFontInfo Initials = HudDraw::Font(16.0f * U);
	const FSlateFontInfo Seconds = HudDraw::Font(20.0f * U);
	const FSlateFontInfo DownWord = HudDraw::Font(12.0f * U);

	struct FIcon
	{
		const ACheeseStrikePlayerState* Player;
		FVector2D At;
		CheeseStrikeHUD::EStatus Status;
		int32 Back;
		UTexture2D* Avatar;
	};
	TArray<FIcon> Icons;
	for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
	{
		const int32 Count = Sides[Team].Num();
		const float Width = Count * Size + FMath::Max(0, Count - 1) * Gap;
		float X = !bTeams ? Middle - Width / 2.0f : Team == 0 ? Middle - Between / 2.0f - Width : Middle + Between / 2.0f;
		for (const ACheeseStrikePlayerState* Player : Sides[Team])
		{
			const ACheeseStrikeCharacter* Body = BodyOf(Player);
			const CheeseStrikeHUD::EStatus Status = StatusOf(Player, Body);
			const int32 Back = Status == CheeseStrikeHUD::EStatus::Respawning ? CheeseStrikeHUD::SecondsToComeBack(Match->GetRespawnDelay(), Now, Body->GetDiedAt()) : 0;
			Icons.Add({Player, FVector2D(X, Top), Status, Back, Avatars->For(Player)});
			X += Size + Gap;
		}
	}
	if (Icons.Num() == 0)
	{
		return;
	}

	// The tiles, their corners rounded: a picture, or the initials on the side's colour, framed in the side's colour; dimmed
	// and its frame faded while coming back, darker and struck through when out; down, washed and framed thicker in orange,
	// slowly pulsing. The local player's own has a white ring round it.
	const float Round = 8.0f * U;
	const float Edge = FMath::Max(1.0f, 2.5f * U);
	HudDraw::FShapes Under;
	HudDraw::FShapes Over;
	if (bTeams)
	{
		Under.Rect(Middle - U / 2.0f, Top + 6.0f * U, U, Size - 12.0f * U, HudDraw::Rule);
	}
	for (const FIcon& Icon : Icons)
	{
		const FLinearColor Side = HudDraw::SideColour(bTeams ? Icon.Player->GetTeam() : INDEX_NONE);
		if (Icon.Avatar == nullptr)
		{
			Under.RoundedRectDown(Icon.At.X, Icon.At.Y, Size, Size, Round, Side * FLinearColor(0.55f, 0.55f, 0.55f, 0.92f), Side * FLinearColor(0.32f, 0.32f, 0.32f, 0.92f));
		}
		if (Icon.Status == CheeseStrikeHUD::EStatus::Down)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 5.0f);
			Over.RoundedRect(Icon.At.X, Icon.At.Y, Size, Size, Round, FLinearColor(0.0f, 0.0f, 0.0f, 0.3f));
			Over.RoundedRect(Icon.At.X, Icon.At.Y, Size, Size, Round, HudDraw::Faded(CheeseStrikeHUD::DownColour, 0.2f + 0.2f * Pulse));
			Over.RoundedFrame(Icon.At.X, Icon.At.Y, Size, Size, Round, FMath::Max(1.0f, 3.0f * U), CheeseStrikeHUD::DownColour);
		}
		else
		{
			if (Icon.Status != CheeseStrikeHUD::EStatus::Alive)
			{
				Over.RoundedRect(Icon.At.X, Icon.At.Y, Size, Size, Round, FLinearColor(0.0f, 0.0f, 0.0f, Icon.Status == CheeseStrikeHUD::EStatus::Out ? 0.78f : 0.6f));
			}
			const float Solid = Icon.Status == CheeseStrikeHUD::EStatus::Alive ? 1.0f : Icon.Status == CheeseStrikeHUD::EStatus::Out ? 0.25f : 0.4f;
			Over.RoundedFrame(Icon.At.X, Icon.At.Y, Size, Size, Round, Edge, HudDraw::Faded(Side, Solid));
		}
		if (Icon.Status == CheeseStrikeHUD::EStatus::Out)
		{
			Over.Line(Icon.At + FVector2D(8.0f * U, Size - 8.0f * U), Icon.At + FVector2D(Size - 8.0f * U, 8.0f * U), 2.0f * U, HudDraw::Faded(HudDraw::Danger, 0.8f));
		}
		if (Icon.Player == Me)
		{
			const float Ring = 3.0f * U;
			Over.RoundedFrame(Icon.At.X - Ring, Icon.At.Y - Ring, Size + 2.0f * Ring, Size + 2.0f * Ring, Round + Ring, FMath::Max(1.0f, 1.5f * U), HudDraw::Ink);
		}
	}
	Under.Draw(Canvas);
	for (const FIcon& Icon : Icons)
	{
		if (Icon.Avatar != nullptr)
		{
			HudDraw::Picture(Canvas, Icon.Avatar, Icon.At, FVector2D(Size, Size), FLinearColor::White);
		}
		else
		{
			const FString Letters = CheeseStrikeHUD::Initials(Icon.Player->GetPlayerName());
			HudDraw::Text(Canvas, Letters, FVector2D(Icon.At.X + Size / 2.0f, Icon.At.Y + (Size - HudDraw::Measure(Letters, Initials).Y) / 2.0f), Initials, HudDraw::Ink, 0.5f);
		}
	}
	Over.Draw(Canvas);
	for (const FIcon& Icon : Icons)
	{
		if (Icon.Status == CheeseStrikeHUD::EStatus::Respawning)
		{
			const FString Count = FString::FromInt(Icon.Back);
			HudDraw::Text(Canvas, Count, FVector2D(Icon.At.X + Size / 2.0f, Icon.At.Y + (Size - HudDraw::Measure(Count, Seconds).Y) / 2.0f), Seconds, HudDraw::Ink, 0.5f);
		}
		else if (Icon.Status == CheeseStrikeHUD::EStatus::Down)
		{
			// The word across the foot of the tile, white on a band of the colour (the tile's rounded foot), so that it reads on any picture.
			const FVector2D Word = HudDraw::Measure(TEXT("DOWN"), DownWord);
			HudDraw::FShapes Band;
			Band.Polygon(CheeseStrikeHUD::ClipToRect(CheeseStrikeHUD::RoundedRect(Icon.At, Icon.At + FVector2D(Size, Size), Round), FVector2D(Icon.At.X, Icon.At.Y + Size - Word.Y - 2.0f * U), Icon.At + FVector2D(Size, Size)),
				HudDraw::Faded(CheeseStrikeHUD::DownColour, 0.9f));
			Band.Draw(Canvas);
			HudDraw::Text(Canvas, TEXT("DOWN"), FVector2D(Icon.At.X + Size / 2.0f, Icon.At.Y + Size - Word.Y - 1.0f * U), DownWord, HudDraw::Ink, 0.5f, false);
		}
	}
}

void ACheeseStrikeHUD::DrawNameTags(const ACheeseStrikePlayerController* Controller)
{
	if (Controller->PlayerCameraManager == nullptr)
	{
		return;
	}
	const FVector Eye = Controller->PlayerCameraManager->GetCameraLocation();
	const FVector Ahead = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	const float U = Unit();
	HudDraw::FShapes Shapes;
	struct FTag
	{
		FVector2D At;
		FString Name;
		float Scale;
		float Alpha;
	};
	TArray<FTag> NameTags;
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		const ACheeseStrikeCharacter* Other = *It;
		const ACheeseStrikePlayerState* Their = Other->GetPlayerState<ACheeseStrikePlayerState>();
		if (Other == Controller->GetPawn() || !Other->IsAlive() || Their == nullptr || Their == Controller->PlayerState || !AreTeammates(Controller->PlayerState, Their))
		{
			continue;
		}
		// Over the head, seen through walls; nothing for one behind the view.
		const FVector Over = Other->GetActorLocation() + FVector(0.0f, 0.0f, Other->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 28.0f);
		if (FVector::DotProduct(Over - Eye, Ahead) <= 10.0f)
		{
			continue;
		}
		const FVector OnScreen = Canvas->Project(Over);
		const CheeseStrikeHUD::FTagLook Look = CheeseStrikeHUD::TagLook(FVector::Dist(Eye, Over));
		const FVector2D At(OnScreen.X, OnScreen.Y);
		const float Mark = 8.0f * U * Look.Scale;
		const FLinearColor Side = HudDraw::Faded(HudDraw::SideColour(Their->GetTeam()), Look.Alpha);
		Shapes.Triangle(At + FVector2D(-Mark - 1.5f * U, -Mark - 1.0f * U), At + FVector2D(Mark + 1.5f * U, -Mark - 1.0f * U), At + FVector2D(0.0f, 1.5f * U), FLinearColor(0.0f, 0.0f, 0.0f, 0.55f * Look.Alpha));
		Shapes.Triangle(At + FVector2D(-Mark, -Mark), At + FVector2D(Mark, -Mark), At, Side);
		NameTags.Add({At - FVector2D(0.0f, Mark + 4.0f * U), Their->GetPlayerName(), Look.Scale, Look.Alpha});
	}
	Shapes.Draw(Canvas);
	for (const FTag& Tag : NameTags)
	{
		const FSlateFontInfo Name = HudDraw::Font(18.0f * U * Tag.Scale);
		// A long name (Steam's run to 32 letters) cut with dots, so that a tag never spreads far across the view.
		const FString Shown = CheeseStrikeHUD::FitName(Tag.Name, NameTagWidth * U * Tag.Scale, [&Name](const FString& Text) { return static_cast<float>(HudDraw::Measure(Text, Name).X); });
		HudDraw::Text(Canvas, Shown, FVector2D(Tag.At.X, Tag.At.Y - HudDraw::Measure(Shown, Name).Y), Name, HudDraw::Faded(HudDraw::Ink, Tag.Alpha), 0.5f);
	}
}
