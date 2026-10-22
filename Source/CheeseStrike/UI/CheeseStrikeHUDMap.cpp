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
	}
	for (auto Seen = LastSeen.CreateIterator(); Seen; ++Seen)
	{
		if (!Seen.Key().IsValid() || Now - Seen.Value() > SeenLife)
		{
			Seen.RemoveCurrent();
		}
	}
}

void ACheeseStrikeHUD::DrawMinimap(const ACheeseStrikePlayerController* Controller, const ACheeseStrikeCharacter* Character, float FieldOfView)
{
	if (Controller->PlayerCameraManager == nullptr)
	{
		return;
	}
	UWorld* World = GetWorld();
	Minimap.Gather(World);
	const float U = Unit();
	const FVector2D Min(Margin * U, Margin * U);
	const FVector2D Max = Min + FVector2D(MapSide * U, MapSide * U);
	const FVector2D Centre = (Min + Max) / 2.0f;
	const float Half = MapSide * U / 2.0f;
	const float Scale = Half / CheeseStrikeHUD::MinimapRange;
	const FVector Eye = Controller->PlayerCameraManager->GetCameraLocation();
	const float ViewYaw = Controller->PlayerCameraManager->GetCameraRotation().Yaw;
	const FVector2D Middle(Eye.X, Eye.Y);
	// Where the player stands; or, looking on from elsewhere (killed, from where their eyes were), as if standing there.
	const float Feet = Character != nullptr && Character->IsAlive() && Controller->GetViewTarget() == Character ? FeetOf(Character) : Eye.Z - 160.0f;
	const auto OnMap = [&](const FVector& At) { return Centre + CheeseStrikeHUD::MapPoint(FVector2D(At.X, At.Y) - Middle, ViewYaw, Scale); };

	HudDraw::FShapes Shapes;
	Shapes.Rect(Min.X, Min.Y, Max.X - Min.X, Max.Y - Min.Y, FLinearColor(0.008f, 0.008f, 0.01f, 0.72f));
	Minimap.Draw(Shapes, Min, Max, Middle, ViewYaw, Feet, Scale);

	// What the player can see: a faint cone up the map.
	const float HalfCone = FMath::Min(80.0f, FieldOfView / 2.0f);
	const int32 Steps = 12;
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const float A = FMath::DegreesToRadians(FMath::Lerp(-HalfCone, HalfCone, static_cast<float>(Step) / Steps));
		const float B = FMath::DegreesToRadians(FMath::Lerp(-HalfCone, HalfCone, static_cast<float>(Step + 1) / Steps));
		const float Reach = Half * 0.92f;
		Shapes.Triangle(Centre, Centre + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * Reach, Centre + FVector2D(FMath::Sin(B), -FMath::Cos(B)) * Reach,
			FLinearColor(1.0f, 1.0f, 1.0f, 0.13f), FLinearColor(1.0f, 1.0f, 1.0f, 0.0f), FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
	}

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
			const FVector2D From = At - Centre;
			const float Out = FMath::Max(FMath::Abs(From.X), FMath::Abs(From.Y));
			const bool bBeyond = Out > Half - 6.0f * U;
			if (bBeyond)
			{
				At = Centre + From * ((Half - 6.0f * U) / Out);
			}
			MapDot(Shapes, At, (bBeyond ? 3.5f : 4.5f) * U, HudDraw::Faded(HudDraw::SideColour(Their != nullptr ? Their->GetTeam() : 0), bBeyond ? 0.7f : 1.0f), Facing, !bBeyond);
		}
		else if (ShowsOnMinimap(Other) && At.X > Min.X && At.Y > Min.Y && At.X < Max.X && At.Y < Max.Y)
		{
			const float* Seen = LastSeen.Find(Other);
			const float Fade = AreEnemiesRevealed() || Seen == nullptr ? 1.0f : FMath::Clamp(1.0f - (World->GetTimeSeconds() - *Seen) / SeenLife, 0.0f, 1.0f) * 0.6f + 0.4f;
			MapDot(Shapes, At, 4.5f * U, HudDraw::Faded(HudDraw::Danger, Fade), Facing, false);
		}
	}

	// The side's Unmanned Aerial Cheese: every enemy where its last sweep saw them, strong at the sweep and fading towards the
	// next, and a ring going out from the middle as each sweep comes.
	if (Sweep.Index != INDEX_NONE)
	{
		const ACheeseStrikePlayerState* Mine = Controller->GetPlayerState<ACheeseStrikePlayerState>();
		if (Mine != nullptr)
		{
			// Those further than the minimap reaches are held to its frame, smaller, the way they are.
			for (const FVector& Mark : Mine->SweepMarks)
			{
				FVector2D At = OnMap(Mark);
				const FVector2D From = At - Centre;
				const float Out = FMath::Max(FMath::Abs(From.X), FMath::Abs(From.Y));
				const bool bBeyond = Out > Half - 6.0f * U;
				if (bBeyond)
				{
					At = Centre + From * ((Half - 6.0f * U) / Out);
				}
				MapDot(Shapes, At, (bBeyond ? 3.0f : 4.5f) * U, HudDraw::Faded(HudDraw::Danger, Sweep.Fade * (bBeyond ? 0.75f : 1.0f)), 0.0f, false);
			}
		}
		const float Ping = Sweep.Since / 0.7f;
		if (Ping < 1.0f && Half * 0.95f * Ping > 4.0f * U)
		{
			Shapes.Arc(Centre, Half * 0.95f * Ping, 2.0f * U, 0.0f, 360.0f, HudDraw::Faded(HudDraw::Danger, 0.55f * (1.0f - Ping)), false);
		}
	}

	// The player: an arrow in the middle, pointing up, the way they face.
	const FVector2D Tip = Centre + FVector2D(0.0f, -9.0f * U);
	const FVector2D Notch = Centre + FVector2D(0.0f, 3.5f * U);
	Shapes.Triangle(Tip, Centre + FVector2D(7.0f * U, 8.0f * U), Notch, HudDraw::Ink);
	Shapes.Triangle(Tip, Notch, Centre + FVector2D(-7.0f * U, 8.0f * U), HudDraw::Ink);

	// Where the player is, by the map's names for its places, under the minimap.
	const FString Place = ACheeseStrikePlace::NameAt(World, Eye).ToUpper();

	// The frame, and north riding it.
	Shapes.Frame(Min.X, Min.Y, Max.X - Min.X, Max.Y - Min.Y, FMath::Max(1.0f, U), HudDraw::Rule);
	const FVector2D North = Centre + CheeseStrikeHUD::NorthMark(ViewYaw, Half);
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
		/** Down: how much of the bleeding out is left. */
		float Bleed;
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
			const UCheeseStrikeLastStand* Stand = Status == CheeseStrikeHUD::EStatus::Down ? Body->GetLastStand() : nullptr;
			const float Bleed = Stand != nullptr ? CheeseStrikeHUD::BleedLeft(Stand->SecondsToBleedOut(), CheeseStrikeModes::LastStandTime) : 0.0f;
			Icons.Add({Player, FVector2D(X, Top), Status, Back, Avatars->For(Player), Bleed});
			X += Size + Gap;
		}
	}
	if (Icons.Num() == 0)
	{
		return;
	}

	// The tiles: a picture, or the initials on the side's colour; a strip of the side's colour under; dimmed while coming back, darker and struck through when out;
	// down, washed and framed in orange, slowly pulsing, the strip under it the bleeding out running down.
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
			Under.Quad(Icon.At, Icon.At + FVector2D(Size, 0.0f), Icon.At + FVector2D(Size, Size), Icon.At + FVector2D(0.0f, Size),
				Side * FLinearColor(0.55f, 0.55f, 0.55f, 0.92f), Side * FLinearColor(0.5f, 0.5f, 0.5f, 0.92f), Side * FLinearColor(0.32f, 0.32f, 0.32f, 0.92f), Side * FLinearColor(0.36f, 0.36f, 0.36f, 0.92f));
		}
		if (Icon.Status == CheeseStrikeHUD::EStatus::Down)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 5.0f);
			Over.Rect(Icon.At.X, Icon.At.Y, Size, Size, FLinearColor(0.0f, 0.0f, 0.0f, 0.3f));
			Over.Rect(Icon.At.X, Icon.At.Y, Size, Size, HudDraw::Faded(CheeseStrikeHUD::DownColour, 0.2f + 0.2f * Pulse));
			Over.Frame(Icon.At.X, Icon.At.Y, Size, Size, FMath::Max(1.0f, 2.0f * U), CheeseStrikeHUD::DownColour);
			Over.Rect(Icon.At.X, Icon.At.Y + Size, Size, 3.0f * U, HudDraw::Faded(CheeseStrikeHUD::DownColour, 0.3f));
			Over.Rect(Icon.At.X, Icon.At.Y + Size, Size * Icon.Bleed, 3.0f * U, CheeseStrikeHUD::DownColour);
		}
		else if (Icon.Status != CheeseStrikeHUD::EStatus::Alive)
		{
			Over.Rect(Icon.At.X, Icon.At.Y, Size, Size, FLinearColor(0.0f, 0.0f, 0.0f, Icon.Status == CheeseStrikeHUD::EStatus::Out ? 0.78f : 0.6f));
		}
		if (Icon.Status == CheeseStrikeHUD::EStatus::Out)
		{
			Over.Line(Icon.At + FVector2D(6.0f * U, Size - 6.0f * U), Icon.At + FVector2D(Size - 6.0f * U, 6.0f * U), 2.0f * U, HudDraw::Faded(HudDraw::Danger, 0.8f));
		}
		if (Icon.Status != CheeseStrikeHUD::EStatus::Down)
		{
			Over.Rect(Icon.At.X, Icon.At.Y + Size, Size, 3.0f * U, Icon.Status == CheeseStrikeHUD::EStatus::Alive ? Side : HudDraw::Faded(Side, 0.35f));
		}
		if (Icon.Player == Me)
		{
			Over.Frame(Icon.At.X - 2.0f * U, Icon.At.Y - 2.0f * U, Size + 4.0f * U, Size + 7.0f * U, FMath::Max(1.0f, 1.5f * U), HudDraw::Ink);
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
			// The word across the foot of the tile, white on a band of the colour, so that it reads on any picture.
			const FVector2D Word = HudDraw::Measure(TEXT("DOWN"), DownWord);
			HudDraw::FShapes Band;
			Band.Rect(Icon.At.X, Icon.At.Y + Size - Word.Y - 2.0f * U, Size, Word.Y + 2.0f * U, HudDraw::Faded(CheeseStrikeHUD::DownColour, 0.9f));
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
		HudDraw::Text(Canvas, Tag.Name, FVector2D(Tag.At.X, Tag.At.Y - HudDraw::Measure(Tag.Name, Name).Y), Name, HudDraw::Faded(HudDraw::Ink, Tag.Alpha), 0.5f);
	}
}
