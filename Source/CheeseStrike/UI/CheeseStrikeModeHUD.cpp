// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeModeHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Input/CheeseStrikeInput.h"
#include "Modes/CheeseStrikeLastStand.h"
#include "Modes/CheeseStrikeWatchCamera.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "UI/CheeseStrikeHUD.h"
#include "UI/CheeseStrikeHUDDraw.h"
#include "UI/CheeseStrikeHillHUD.h"

/** The HUD's drawing, named through an alias rather than used as a whole: the packaged game's unity build would carry a using-directive into the files after this one. */
namespace HudDraw = CheeseStrikeHUDDraw;

namespace
{
	/** Seconds the word that a streak is ready stays big. */
	constexpr float ModeHudEarnedLife = 3.5f;

	/** Where `At` is on the screen; false behind the view. */
	bool ModeHudOnScreen(const AHUD* Hud, const FVector& At, FVector2D& Out)
	{
		const FVector Projected = Hud->Project(At, true);
		Out = FVector2D(Projected.X, Projected.Y);
		return Projected.Z > 0.0f;
	}

	/**
	 * A notice in the middle of the screen at `Y`: a title, and a line under
	 * it if any. Being out of a round says so this way; so can any state a
	 * mode puts a player in (a last stand).
	 */
	void ModeHudNotice(UCanvas* Canvas, float Y, const FString& Title, const FLinearColor& Colour, const FString& Under)
	{
		const float U = HudDraw::Unit(Canvas);
		const FSlateFontInfo Big = HudDraw::Font(30.0f * U);
		HudDraw::Text(Canvas, Title, FVector2D(Canvas->SizeX / 2.0f, Y), Big, Colour, 0.5f);
		if (!Under.IsEmpty())
		{
			HudDraw::Text(Canvas, Under.ToUpper(), FVector2D(Canvas->SizeX / 2.0f, Y + HudDraw::Measure(Title, Big).Y + 4.0f * U), HudDraw::Font(18.0f * U), HudDraw::Soft, 0.5f);
		}
	}

	void ModeHudDrawAnnouncements(UCanvas* Canvas, const ACheeseStrikeGameState* Match, const ACheeseStrikePlayerState* Mine)
	{
		const float U = HudDraw::Unit(Canvas);
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
			const FLinearColor Colour = bAgainstMe ? HudDraw::Danger : (Word.Team != CheeseStrikeModes::NoTeam && Match->HasTeams() ? HudDraw::SideColour(Word.Team) : HudDraw::Ink);
			// Fading out over its last second.
			const float Fade = FMath::Clamp(ACheeseStrikeGameState::AnnouncementLife - Age, 0.0f, 1.0f);
			const FSlateFontInfo Line = HudDraw::Font((bOverload ? 26.0f : 21.0f) * U);
			// The name in it (as long as Steam's, 32 letters) cut with dots where it would spread too far.
			const FString Text = CheeseStrikeHUD::FitNameIn(Word.Text.ToUpper(), Word.Who.ToUpper(), 360.0f * U, [&Line](const FString& Part) { return static_cast<float>(HudDraw::Measure(Part, Line).X); });
			HudDraw::Text(Canvas, Text, FVector2D(Canvas->SizeX / 2.0f, Y), Line, HudDraw::Faded(Colour, Fade), 0.5f);
			Y += HudDraw::Measure(Word.Text, Line).Y + 6.0f * U;
			if (bAgainstMe && Age < CheeseStrikeStreaks::OverloadWarning + CheeseStrikeStreaks::OverloadRain)
			{
				// Blinking while the warning lasts.
				if (Age > CheeseStrikeStreaks::OverloadWarning || FMath::Fmod(Age, 0.6f) < 0.4f)
				{
					HudDraw::Text(Canvas, TEXT("GOUDA OVERLOAD INCOMING  ·  TAKE COVER"), FVector2D(Canvas->SizeX / 2.0f, Y), HudDraw::Font(34.0f * U), HudDraw::Danger, 0.5f);
				}
				Y += 52.0f * U;
			}
		}
	}

	/** Getting someone up, by a teammate or by themself: green, whatever the side's colour. */
	const FLinearColor ModeHudReviveGreen(0.22f, 0.86f, 0.32f, 1.0f);
	constexpr float ModeHudReviveBarHeight = 14.0f;

	/** A thick rounded bar `Width` across, its middle at X, filled `Done` (0 to 1) in green over a dark track: reviving, or getting up. */
	void ModeHudReviveBar(UCanvas* Canvas, float X, float Y, float Width, float Done)
	{
		const float U = HudDraw::Unit(Canvas);
		const float Height = ModeHudReviveBarHeight * U;
		const float Radius = Height / 2.0f;
		const float Left = X - Width / 2.0f;
		HudDraw::FShapes Shapes;
		Shapes.RoundedRect(Left - 2.0f * U, Y - 2.0f * U, Width + 4.0f * U, Height + 4.0f * U, Radius + 2.0f * U, HudDraw::Panel);
		Shapes.RoundedRect(Left, Y, Width, Height, Radius, HudDraw::Faded(ModeHudReviveGreen, 0.22f));
		const float Filled = Width * FMath::Clamp(Done, 0.0f, 1.0f);
		if (Filled > 1.0f)
		{
			Shapes.RoundedRect(Left, Y, Filled, Height, FMath::Min(Radius, Filled / 2.0f), ModeHudReviveGreen);
		}
		Shapes.Draw(Canvas);
	}

	/** Where the modes' lines over the HUD's score summary begin, at the bottom in the middle. */
	float ModeHudOverScore(const UCanvas* Canvas)
	{
		return Canvas->SizeY - (ACheeseStrikeHUD::Margin + ACheeseStrikeHUD::ScoreHeight + 12.0f) * HudDraw::Unit(Canvas);
	}

	/** The streaks, over `Bottom`: the one to call in, and word of one just earned. */
	void ModeHudDrawStreaks(AHUD* Hud, UCanvas* Canvas, const ACheeseStrikeGameState* Match, const ACheeseStrikePlayerState* Mine, float Bottom)
	{
		const float U = HudDraw::Unit(Canvas);
		const float Middle = Canvas->SizeX / 2.0f;
		const TArray<CheeseStrikeStreaks::EStreak> Ready = Mine->GetReadyStreaks();
		// The keys as the player plays: the keyboard's, or the gamepad's if they last used one.
		const FString StreakKey = UCheeseStrikeInputSet::Prompt(Hud->PlayerOwner, &UCheeseStrikeInputSet::Streak);
		const float SinceEarned = Match->GetServerWorldTimeSeconds() - Mine->GetLastEarnedAt();
		if (!Mine->IsMarkingStrike() && Mine->GetLastEarned() != CheeseStrikeStreaks::EStreak::None && Mine->GetLastEarnedAt() > 0.0f && SinceEarned < ModeHudEarnedLife && Ready.Contains(Mine->GetLastEarned()))
		{
			HudDraw::Text(Canvas, FString::Printf(TEXT("%s READY  ·  PRESS %s"), CheeseStrikeStreaks::Info(Mine->GetLastEarned()).Name, *StreakKey).ToUpper(), FVector2D(Middle, Canvas->SizeY * 0.62f), HudDraw::Font(30.0f * U), HudDraw::Accent, 0.5f);
		}
		float Y = Bottom;
		if (Ready.Num() > 0)
		{
			// The streak to call in, with its key in a box: "[4] UNMANNED AERIAL CHEESE +1".
			const FSlateFontInfo Line = HudDraw::Font(17.0f * U);
			FString Name = FString(CheeseStrikeStreaks::Info(Ready[0]).Name).ToUpper();
			if (Ready.Num() > 1)
			{
				Name += FString::Printf(TEXT("  +%d"), Ready.Num() - 1);
			}
			const FVector2D NameSize = HudDraw::Measure(Name, Line);
			// The key's box fits its label: "Q", or the d-pad's down.
			const float Key = FMath::Max(NameSize.Y * 0.95f, static_cast<float>(HudDraw::Measure(StreakKey, Line).X) + 10.0f * U);
			const float KeyHigh = NameSize.Y * 0.95f;
			const float Width = Key + 10.0f * U + NameSize.X;
			Y -= KeyHigh;
			const float Left = Middle - Width / 2.0f;
			HudDraw::FShapes Shapes;
			Shapes.RoundedRect(Left - 6.0f * U, Y - 3.0f * U, Width + 12.0f * U, KeyHigh + 6.0f * U, 6.0f * U, HudDraw::Panel);
			Shapes.RoundedFrame(Left, Y, Key, KeyHigh, 4.0f * U, FMath::Max(1.0f, 1.5f * U), HudDraw::Accent);
			Shapes.Draw(Canvas);
			HudDraw::Text(Canvas, StreakKey, FVector2D(Left + Key / 2.0f, Y + (KeyHigh - HudDraw::Measure(StreakKey, Line).Y) / 2.0f), Line, HudDraw::Accent, 0.5f);
			HudDraw::Text(Canvas, Name, FVector2D(Left + Key + 10.0f * U, Y + (KeyHigh - NameSize.Y) / 2.0f), Line, HudDraw::Accent);
			Y -= 10.0f * U;
		}
		// The run of kills and the streak it is on its way to are the HUD's streak panel (ACheeseStrikeHUD::DrawStreak); choosing
		// where a Cheese Strike goes, its strike map (ACheeseStrikeHUD::DrawStrikeMap).
	}

	void ModeHudDrawReveal(AHUD* Hud, UCanvas* Canvas, const ACheeseStrikeGameState* Match, const ACheeseStrikePlayerState* Mine)
	{
		// The sweeps' marks are the HUD's (ACheeseStrikeHUD::UpdateSweep), which also puts them on the minimap.
		// A second drone adds its time to the first's: the sweeps go on from the first, and the seconds and the bar show it all.
		const float Until = Match->RevealEndsFor(Mine);
		const float Now = Match->GetServerWorldTimeSeconds();
		const float Lasts = Until - Match->RevealStartedFor(Mine);
		const CheeseStrikeHUD::FSweep Sweep = CheeseStrikeHUD::SweepAt(Now, Until, Lasts, CheeseStrikeStreaks::SweepPeriod);
		if (Sweep.Index == INDEX_NONE)
		{
			return;
		}
		const float U = HudDraw::Unit(Canvas);
		HudDraw::FShapes Shapes;
		for (const FVector& Mark : Mine->SweepMarks)
		{
			FVector2D At;
			if (ModeHudOnScreen(Hud, Mark + FVector(0.0f, 0.0f, 40.0f), At))
			{
				// A small red diamond, with a dark rim, where each enemy was.
				const float Size = 8.0f * U;
				const float Rim = Size + 2.0f * U;
				Shapes.Quad(At + FVector2D(0, -Rim), At + FVector2D(Rim, 0), At + FVector2D(0, Rim), At + FVector2D(-Rim, 0), FLinearColor(0.0f, 0.0f, 0.0f, 0.55f * Sweep.Fade));
				Shapes.Quad(At + FVector2D(0, -Size), At + FVector2D(Size, 0), At + FVector2D(0, Size), At + FVector2D(-Size, 0), HudDraw::Faded(HudDraw::Danger, Sweep.Fade));
			}
		}
		// Under the minimap and the place's name: its name, its seconds, and a bar running down.
		const float Left = ACheeseStrikeHUD::Margin * U;
		const float Top = (ACheeseStrikeHUD::Margin + ACheeseStrikeHUD::MapSide + 40.0f) * U;
		const float Width = ACheeseStrikeHUD::MapSide * U;
		const float Remaining = FMath::Clamp((Until - Now) / FMath::Max(Lasts, CheeseStrikeStreaks::RevealTime), 0.0f, 1.0f);
		const FSlateFontInfo Label = HudDraw::Font(14.0f * U);
		const float LabelHeight = HudDraw::Measure(TEXT("U"), Label).Y;
		Shapes.Rect(Left, Top + LabelHeight + 4.0f * U, Width, 3.0f * U, HudDraw::Dim);
		Shapes.Rect(Left, Top + LabelHeight + 4.0f * U, Width * Remaining, 3.0f * U, HudDraw::Accent);
		Shapes.Draw(Canvas);
		HudDraw::Text(Canvas, TEXT("UNMANNED AERIAL CHEESE"), FVector2D(Left, Top), Label, HudDraw::Accent);
		HudDraw::Text(Canvas, FString::Printf(TEXT("%.0f"), FMath::CeilToFloat(Until - Now)), FVector2D(Left + Width, Top), Label, HudDraw::Accent, 1.0f);
	}

	/** Execution: down, the time until getting up and who comes; standing, a mark over teammates down, the seconds over anyone down near, the prompt over someone down in reach, and the revive under way. */
	void ModeHudDrawLastStand(AHUD* Hud, UCanvas* Canvas, const ACheeseStrikeGameState* Match, const ACheeseStrikePlayerState* Mine, const ACheeseStrikeCharacter* Own)
	{
		const UCheeseStrikeLastStand* Stand = Own->GetLastStand();
		if (Stand == nullptr || !Match->GetModeInfo().bLastStand)
		{
			return;
		}
		const float U = HudDraw::Unit(Canvas);
		const float Middle = Canvas->SizeX / 2.0f;
		const float Y = Canvas->SizeY * 0.66f;
		const FLinearColor Side = HudDraw::SideColour(Mine->GetTeam());
		const FVector Eye = Hud->PlayerOwner != nullptr && Hud->PlayerOwner->PlayerCameraManager != nullptr ? Hud->PlayerOwner->PlayerCameraManager->GetCameraLocation() : Own->GetActorLocation();
		// Teammates down, wherever they are: a cross over them, for finding them. Anyone down near and in sight: the seconds
		// until they get up by themselves.
		HudDraw::FShapes Marks;
		struct FDownMark
		{
			FVector2D At;
			bool bMate;
			int32 Seconds;
			bool bNear;
		};
		TArray<FDownMark> Down;
		for (TActorIterator<ACheeseStrikeCharacter> It(Hud->GetWorld()); It; ++It)
		{
			const UCheeseStrikeLastStand* Theirs = It->GetLastStand();
			if (*It == Own || !It->IsAlive() || !It->IsDowned() || Theirs == nullptr || Theirs->InExecution())
			{
				continue;
			}
			const bool bMate = Match->AreTeammates(Mine, It->GetPlayerState());
			const FVector Over = It->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
			bool bNear = FVector::Dist(Eye, Over) <= CheeseStrikeModes::SelfReviveShownWithin;
			if (bNear)
			{
				FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeHudDown), false);
				for (TActorIterator<ACheeseStrikeCharacter> Each(Hud->GetWorld()); Each; ++Each)
				{
					Params.AddIgnoredActor(*Each);
				}
				bNear = !Hud->GetWorld()->LineTraceTestByChannel(Eye, Over, ECC_Visibility, Params);
			}
			FVector2D At;
			if ((bMate || bNear) && ModeHudOnScreen(Hud, Over + FVector(0.0f, 0.0f, 30.0f), At))
			{
				if (bMate)
				{
					Marks.Line(At - FVector2D(9.0f * U, 0.0f), At + FVector2D(9.0f * U, 0.0f), 4.0f * U, Side);
					Marks.Line(At - FVector2D(0.0f, 9.0f * U), At + FVector2D(0.0f, 9.0f * U), 4.0f * U, Side);
				}
				Down.Add({At, bMate, CheeseStrikeModes::SelfReviveSeconds(Theirs->SelfReviveLeft()), bNear});
			}
		}
		Marks.Draw(Canvas);
		for (const FDownMark& Mark : Down)
		{
			const FLinearColor Colour = Mark.bMate ? Side : HudDraw::Danger;
			if (Mark.bMate)
			{
				HudDraw::Text(Canvas, TEXT("DOWN"), FVector2D(Mark.At.X, Mark.At.Y - 32.0f * U), HudDraw::Font(14.0f * U), Side, 0.5f);
			}
			if (Mark.bNear)
			{
				HudDraw::Text(Canvas, FString::Printf(TEXT("UP IN %d"), Mark.Seconds), FVector2D(Mark.At.X, Mark.At.Y + (Mark.bMate ? 14.0f : -10.0f) * U), HudDraw::Font(14.0f * U), Colour, 0.5f);
			}
		}
		const FSlateFontInfo Line = HudDraw::Font(18.0f * U);
		if (Stand->IsDowned())
		{
			const ACheeseStrikeCharacter* By = Stand->WorkedOnBy();
			const UCheeseStrikeLastStand* Theirs = By != nullptr ? By->GetLastStand() : nullptr;
			const ACheeseStrikeCharacter* Executor = Stand->IsBeingExecuted() ? Stand->GetExecution().Other.Get() : nullptr;
			const int32 Seconds = CheeseStrikeModes::SelfReviveSeconds(Stand->SelfReviveLeft());
			const FString Under = Executor != nullptr ? FString::Printf(TEXT("%s is executing you"), *Executor->DisplayName())
				: Theirs != nullptr ? FString::Printf(TEXT("%s is reviving you"), *By->DisplayName())
				: FString::Printf(TEXT("getting up in %d  ·  a teammate can revive you sooner  ·  an enemy up close can execute you"), Seconds);
			// Being executed, the words go low, out of the way of the execution being watched.
			ModeHudNotice(Canvas, Executor != nullptr ? Canvas->SizeY * 0.8f : Y, TEXT("DOWN"), HudDraw::Danger, Under);
			if (Executor == nullptr)
			{
				// Reviving fills the green bar; otherwise it fills toward getting up alone.
				const float Done = Theirs != nullptr ? Theirs->WorkDone() : 1.0f - Stand->SelfReviveLeft() / CheeseStrikeModes::SelfReviveTime;
				ModeHudReviveBar(Canvas, Middle, Y + 84.0f * U, 300.0f * U, Done);
			}
			return;
		}
		if (const ACheeseStrikeCharacter* Working = Stand->GetWorkingOn())
		{
			HudDraw::Text(Canvas, FString::Printf(TEXT("REVIVING %s"), *Working->DisplayName().ToUpper()), FVector2D(Middle, Y), Line, ModeHudReviveGreen, 0.5f);
			ModeHudReviveBar(Canvas, Middle, Y + 30.0f * U, 300.0f * U, Stand->WorkDone());
			return;
		}
		CheeseStrikeModes::EInteract Kind = CheeseStrikeModes::EInteract::None;
		if (const ACheeseStrikeCharacter* Near = Stand->InteractTarget(Kind))
		{
			const bool bRevive = Kind == CheeseStrikeModes::EInteract::Revive;
			HudDraw::Text(Canvas, CheeseStrikeModes::InteractPrompt(Kind, UCheeseStrikeInputSet::Prompt(Hud->PlayerOwner, &UCheeseStrikeInputSet::Interact), Near->DisplayName()), FVector2D(Middle, Y), Line, bRevive ? Side : HudDraw::Danger, 0.5f);
		}
	}

	/** Gun Game: the step the player is on and what comes next, over `Bottom`; returns where the lines over it may begin. */
	float ModeHudDrawGunGame(UCanvas* Canvas, const ACheeseStrikeGameState* Match, const ACheeseStrikePlayerState* Mine, float Bottom)
	{
		if (Match->GetModeInfo().WinBy != CheeseStrikeModes::EWinBy::Ladder)
		{
			return Bottom;
		}
		const float U = HudDraw::Unit(Canvas);
		const int32 Steps = CheeseStrikeArsenal::GunGameLadder().Num();
		const int32 Level = Mine->GetGunLevel();
		const FString Text = Level + 1 < Steps
			? FString::Printf(TEXT("%d OF %d  ·  %s  ·  NEXT: %s"), Level + 1, Steps, *ACheeseStrikeGameState::GunGameWeaponName(Level).ToUpper(), *ACheeseStrikeGameState::GunGameWeaponName(Level + 1).ToUpper())
			: FString::Printf(TEXT("%d OF %d  ·  %s  ·  ONE KILL TO WIN"), Level + 1, Steps, *ACheeseStrikeGameState::GunGameWeaponName(Level).ToUpper());
		const FSlateFontInfo Line = HudDraw::Font(17.0f * U);
		const float Top = Bottom - HudDraw::Measure(Text, Line).Y;
		HudDraw::Text(Canvas, Text, FVector2D(Canvas->SizeX / 2.0f, Top), Line, HudDraw::Accent, 0.5f);
		return Top - 8.0f * U;
	}

	void ModeHudDrawOut(UCanvas* Canvas, const AHUD* Hud, const ACheeseStrikeGameState* Match)
	{
		const APlayerController* Owner = Hud->GetOwningPlayerController();
		const ACheeseStrikeWatchCamera* Watch = Owner != nullptr ? Cast<ACheeseStrikeWatchCamera>(Owner->GetViewTarget()) : nullptr;
		const ACheeseStrikeCharacter* Watched = Watch != nullptr ? Watch->GetWatched() : nullptr;
		FString Under = Watched != nullptr ? FString::Printf(TEXT("watching %s  ·  fire for the next"), *Watched->DisplayName()) : FString();
		if (Match->IsRoundBreak())
		{
			Under = FString::Printf(TEXT("next round in %.0f"), FMath::CeilToFloat(Match->SecondsToNextRound()));
		}
		ModeHudNotice(Canvas, Canvas->SizeY * 0.72f, TEXT("OUT UNTIL THE NEXT ROUND"), HudDraw::Danger, Under);
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
		const ACheeseStrikePlayerState* Mine = Owner != nullptr ? Owner->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
		const ACheeseStrikeGameState* Match = Hud != nullptr && Hud->GetWorld() != nullptr ? Hud->GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
		if (Canvas == nullptr || Mine == nullptr || Match == nullptr || !Match->HasMatchStarted() || Match->GetMatchState() == MatchState::LeavingMap)
		{
			return;
		}
		ModeHudDrawAnnouncements(Canvas, Match, Mine);
		if (Match->HasMatchEnded())
		{
			return;
		}
		const ACheeseStrikeCharacter* Own = Cast<ACheeseStrikeCharacter>(Owner->GetPawn());
		// King of the Hill: its marker, and the strip over the score saying who holds it.
		const float OverScore = CheeseStrikeHillHUD::Draw(Hud, Canvas, ModeHudOverScore(Canvas));
		if (Mine->IsOut())
		{
			ModeHudDrawOut(Canvas, Hud, Match);
		}
		else if (Own != nullptr && Own->IsAlive())
		{
			ModeHudDrawReveal(Hud, Canvas, Match, Mine);
			const float Bottom = ModeHudDrawGunGame(Canvas, Match, Mine, OverScore);
			ModeHudDrawStreaks(Hud, Canvas, Match, Mine, Bottom);
			ModeHudDrawLastStand(Hud, Canvas, Match, Mine, Own);
		}
	}
}
