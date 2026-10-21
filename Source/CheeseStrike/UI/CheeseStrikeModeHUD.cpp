// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeModeHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Modes/CheeseStrikeWatchCamera.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "Streaks/CheeseStrikeBombs.h"

namespace
{
	const FLinearColor ModeHudWhite(1.0f, 1.0f, 1.0f, 0.92f);
	const FLinearColor ModeHudFaint(1.0f, 1.0f, 1.0f, 0.7f);
	const FLinearColor ModeHudYellow(0.94f, 0.89f, 0.26f);
	const FLinearColor ModeHudRed(1.0f, 0.22f, 0.15f);
	constexpr float ModeHudMargin = 24.0f;
	/** Seconds the word that a streak is ready stays big. */
	constexpr float ModeHudEarnedLife = 3.5f;

	void ModeHudCentred(AHUD* Hud, const FString& Text, float X, float Y, UFont* Font, float Scale, const FLinearColor& Colour)
	{
		float Width = 0.0f;
		float Height = 0.0f;
		Hud->GetTextSize(Text, Width, Height, Font, Scale);
		// A shadow, so that it reads over a bright floor.
		Hud->DrawText(Text, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f * Colour.A), X - Width / 2.0f + 1.5f, Y + 1.5f, Font, Scale);
		Hud->DrawText(Text, Colour, X - Width / 2.0f, Y, Font, Scale);
	}

	/** Where `At` is on the screen; false behind the view. */
	bool ModeHudOnScreen(const AHUD* Hud, const FVector& At, FVector2D& Out)
	{
		const FVector Projected = Hud->Project(At, true);
		Out = FVector2D(Projected.X, Projected.Y);
		return Projected.Z > 0.0f;
	}

	/** A ring on the ground, `Radius` round `Centre`, as the view sees it. */
	void ModeHudRing(AHUD* Hud, const FVector& Centre, float Radius, const FLinearColor& Colour, float Thickness)
	{
		constexpr int32 Segments = 24;
		FVector2D Last = FVector2D::ZeroVector;
		bool bLast = false;
		for (int32 Step = 0; Step <= Segments; ++Step)
		{
			const float Angle = 2.0f * PI * Step / Segments;
			FVector2D Here;
			const bool bHere = ModeHudOnScreen(Hud, Centre + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 5.0f), Here);
			if (bHere && bLast)
			{
				Hud->DrawLine(Last.X, Last.Y, Here.X, Here.Y, Colour, Thickness);
			}
			Last = Here;
			bLast = bHere;
		}
	}

	/** A small diamond with its middle at `At` on the screen. */
	void ModeHudDiamond(AHUD* Hud, const FVector2D& At, float Size, const FLinearColor& Colour)
	{
		const FVector2D Points[] = {At + FVector2D(0, -Size), At + FVector2D(Size, 0), At + FVector2D(0, Size), At + FVector2D(-Size, 0)};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector2D& From = Points[Index];
			const FVector2D& To = Points[(Index + 1) % 4];
			Hud->DrawLine(From.X, From.Y, To.X, To.Y, Colour, 2.0f);
		}
	}

	FString ModeHudClock(float Seconds)
	{
		const int32 Whole = FMath::CeilToInt(FMath::Max(0.0f, Seconds));
		return FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60);
	}

	void ModeHudDrawAnnouncements(AHUD* Hud, UCanvas* Canvas, const ACheeseStrikeGameState* Match, const ACheeseStrikePlayerState* Mine)
	{
		UFont* Font = GEngine->GetMediumFont();
		const float Now = Match->GetServerWorldTimeSeconds();
		float Y = Canvas->SizeY * 0.2f;
		for (const FAnnouncement& Word : Match->GetAnnouncements())
		{
			const float Age = Now - Word.At;
			if (Age > ACheeseStrikeGameState::AnnouncementLife || Age < -1.0f)
			{
				continue;
			}
			const bool bOverload = Word.Streak == static_cast<uint8>(CheeseStrikeStreaks::EStreak::GoudaOverload);
			// The Overload's warning, to those it falls on, for as long as it lasts: take cover.
			const bool bAgainstMe = bOverload && Mine != nullptr && Mine->GetPlayerName() != Word.Who && (Word.Team == CheeseStrikeModes::NoTeam || Word.Team != Mine->GetTeam());
			const FLinearColor Colour = bAgainstMe ? ModeHudRed : (Word.Team != CheeseStrikeModes::NoTeam && Match->HasTeams() ? CheeseStrikeModes::TeamColour(Word.Team) : ModeHudWhite);
			ModeHudCentred(Hud, Word.Text, Canvas->SizeX / 2.0f, Y, Font, bOverload ? 1.6f : 1.3f, Colour);
			Y += bOverload ? 34.0f : 28.0f;
			if (bAgainstMe && Age < CheeseStrikeStreaks::OverloadWarning + CheeseStrikeStreaks::OverloadRain)
			{
				// Blinking while the warning lasts.
				if (Age > CheeseStrikeStreaks::OverloadWarning || FMath::Fmod(Age, 0.6f) < 0.4f)
				{
					ModeHudCentred(Hud, TEXT("GOUDA OVERLOAD INCOMING  ·  TAKE COVER"), Canvas->SizeX / 2.0f, Y, GEngine->GetLargeFont(), 1.5f, ModeHudRed);
				}
				Y += 40.0f;
			}
		}
	}

	void ModeHudDrawStreaks(AHUD* Hud, UCanvas* Canvas, const ACheeseStrikeGameState* Match, const ACheeseStrikePlayerState* Mine)
	{
		UFont* Font = GEngine->GetMediumFont();
		const float Middle = Canvas->SizeX / 2.0f;
		// Over the HUD's score summary at the bottom.
		const float Bottom = Canvas->SizeY - ModeHudMargin - 72.0f * Canvas->SizeY / 1080.0f;
		const TArray<CheeseStrikeStreaks::EStreak> Ready = Mine->GetReadyStreaks();
		const float SinceEarned = Match->GetServerWorldTimeSeconds() - Mine->GetLastEarnedAt();
		if (!Mine->IsMarkingStrike() && Mine->GetLastEarned() != CheeseStrikeStreaks::EStreak::None && Mine->GetLastEarnedAt() > 0.0f && SinceEarned < ModeHudEarnedLife && Ready.Contains(Mine->GetLastEarned()))
		{
			ModeHudCentred(Hud, FString::Printf(TEXT("%s READY  ·  PRESS 4"), CheeseStrikeStreaks::Info(Mine->GetLastEarned()).Name).ToUpper(), Middle, Canvas->SizeY * 0.62f, Font, 1.6f, ModeHudYellow);
		}
		if (Ready.Num() > 0)
		{
			FString Line = FString::Printf(TEXT("[4]  %s"), CheeseStrikeStreaks::Info(Ready[0]).Name).ToUpper();
			if (Ready.Num() > 1)
			{
				Line += FString::Printf(TEXT("   +%d"), Ready.Num() - 1);
			}
			ModeHudCentred(Hud, Line, Middle, Bottom - 48.0f, Font, 1.2f, ModeHudYellow);
		}
		if (Mine->GetStreak() > 0)
		{
			const CheeseStrikeStreaks::EStreak Next = CheeseStrikeStreaks::NextAfter(Mine->GetStreak());
			const FString Run = Next != CheeseStrikeStreaks::EStreak::None
				? FString::Printf(TEXT("%d in a row  ·  %s at %d"), Mine->GetStreak(), CheeseStrikeStreaks::Info(Next).Name, CheeseStrikeStreaks::Info(Next).Kills)
				: FString::Printf(TEXT("%d in a row"), Mine->GetStreak());
			ModeHudCentred(Hud, Run, Middle, Bottom - 24.0f, Font, 1.0f, ModeHudFaint);
		}

		// Marking a Cheese Strike: where its wheels will land.
		if (Mine->IsMarkingStrike())
		{
			FVector Spot;
			float Yaw = 0.0f;
			if (Mine->StrikeAim(Spot, Yaw))
			{
				// Each wheel where it will land: the first thing it meets coming down.
				for (const FVector& Wheel : CheeseStrikeStreaks::StrikeLine(Spot, Yaw))
				{
					FVector Impact;
					if (ACheeseStrikeBombs::Landing(Hud->GetWorld(), Wheel, Impact))
					{
						ModeHudRing(Hud, Impact, 90.0f, ModeHudRed, 2.0f);
					}
				}
				ModeHudRing(Hud, Spot, CheeseStrikeStreaks::StrikeBlast.Radius, FLinearColor(1.0f, 0.4f, 0.2f, 0.6f), 1.5f);
			}
			ModeHudCentred(Hud, TEXT("FIRE TO MARK THE CHEESE STRIKE  ·  4 OR ESC TO CALL IT OFF"), Middle, Canvas->SizeY * 0.68f, Font, 1.2f, ModeHudYellow);
		}
	}

	void ModeHudDrawReveal(AHUD* Hud, UCanvas* Canvas, const ACheeseStrikeGameState* Match, ACheeseStrikePlayerState* Mine)
	{
		const float Until = Match->RevealEndsFor(Mine);
		if (Until <= 0.0f)
		{
			Mine->SweepIndex = -1;
			Mine->SweepMarks.Reset();
			return;
		}
		const float Now = Match->GetServerWorldTimeSeconds();
		const float Since = Now - (Until - CheeseStrikeStreaks::RevealTime);
		const int32 Sweep = FMath::FloorToInt(Since / CheeseStrikeStreaks::SweepPeriod);
		// Each sweep sees where every enemy is, and that is what shows until the next.
		if (Sweep != Mine->SweepIndex)
		{
			Mine->SweepIndex = Sweep;
			Mine->SweepMarks.Reset();
			for (TActorIterator<ACheeseStrikeCharacter> It(Hud->GetWorld()); It; ++It)
			{
				if (It->IsAlive() && Match->AreEnemies(Mine, It->GetPlayerState()))
				{
					Mine->SweepMarks.Add(It->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f));
				}
			}
		}
		const float Fade = 1.0f - 0.6f * FMath::Fmod(Since, CheeseStrikeStreaks::SweepPeriod) / CheeseStrikeStreaks::SweepPeriod;
		for (const FVector& Mark : Mine->SweepMarks)
		{
			FVector2D At;
			if (ModeHudOnScreen(Hud, Mark, At))
			{
				ModeHudDiamond(Hud, At, 9.0f, FLinearColor(ModeHudRed.R, ModeHudRed.G, ModeHudRed.B, Fade));
			}
		}
		Hud->DrawText(FString::Printf(TEXT("UNMANNED AERIAL CHEESE  %.0f"), FMath::CeilToFloat(Until - Now)), ModeHudYellow, ModeHudMargin, Canvas->SizeY * 0.3f, GEngine->GetMediumFont(), 1.1f);
	}

	void ModeHudDrawOut(AHUD* Hud, UCanvas* Canvas, const ACheeseStrikeGameState* Match, const ACheeseStrikePlayerState* Mine)
	{
		UFont* Font = GEngine->GetMediumFont();
		const float Middle = Canvas->SizeX / 2.0f;
		const float Y = Canvas->SizeY * 0.72f;
		ModeHudCentred(Hud, TEXT("OUT UNTIL THE NEXT ROUND"), Middle, Y, Font, 1.5f, ModeHudRed);
		const APlayerController* Owner = Hud->GetOwningPlayerController();
		const ACheeseStrikeWatchCamera* Watch = Owner != nullptr ? Cast<ACheeseStrikeWatchCamera>(Owner->GetViewTarget()) : nullptr;
		const ACheeseStrikeCharacter* Watched = Watch != nullptr ? Watch->GetWatched() : nullptr;
		FString Under = Watched != nullptr ? FString::Printf(TEXT("watching %s  ·  fire for the next"), *Watched->DisplayName()) : FString();
		if (Match->IsRoundBreak())
		{
			Under = FString::Printf(TEXT("next round in %.0f"), FMath::CeilToFloat(Match->SecondsToNextRound()));
		}
		if (!Under.IsEmpty())
		{
			ModeHudCentred(Hud, Under, Middle, Y + 34.0f, Font, 1.1f, ModeHudWhite);
		}
	}
}

namespace CheeseStrikeModeHUD
{
	bool IsOut(const AHUD* Hud)
	{
		const APlayerController* Owner = Hud != nullptr ? Hud->GetOwningPlayerController() : nullptr;
		const ACheeseStrikePlayerState* Mine = Owner != nullptr ? Owner->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
		return Mine != nullptr && Mine->IsOut();
	}

	void Draw(AHUD* Hud, UCanvas* Canvas)
	{
		const APlayerController* Owner = Hud != nullptr ? Hud->GetOwningPlayerController() : nullptr;
		ACheeseStrikePlayerState* Mine = Owner != nullptr ? Owner->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
		const ACheeseStrikeGameState* Match = Hud != nullptr && Hud->GetWorld() != nullptr ? Hud->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
		if (Canvas == nullptr || Mine == nullptr || Match == nullptr || !Match->HasMatchStarted() || Match->GetMatchState() == MatchState::LeavingMap)
		{
			return;
		}
		ModeHudDrawAnnouncements(Hud, Canvas, Match, Mine);
		if (Match->HasMatchEnded())
		{
			return;
		}
		const ACheeseStrikeCharacter* Own = Cast<ACheeseStrikeCharacter>(Owner->GetPawn());
		if (Mine->IsOut())
		{
			ModeHudDrawOut(Hud, Canvas, Match, Mine);
		}
		else if (Own != nullptr && Own->IsAlive())
		{
			ModeHudDrawReveal(Hud, Canvas, Match, Mine);
			ModeHudDrawStreaks(Hud, Canvas, Match, Mine);
		}
	}
}
