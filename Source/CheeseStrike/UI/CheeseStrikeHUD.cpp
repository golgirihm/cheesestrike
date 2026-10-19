// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "Engine/GameInstance.h"
#include "Rules/CheeseStrikeRules.h"
#include "Sessions/CheeseStrikeSessionSubsystem.h"
#include "Weapons/CheeseStrikeWeapon.h"

namespace
{
	const FLinearColor White(1.0f, 1.0f, 1.0f, 0.9f);
	const FLinearColor Dim(1.0f, 1.0f, 1.0f, 0.7f);
	const FLinearColor Yellow(0.94f, 0.89f, 0.26f);
	const FLinearColor Red(0.9f, 0.3f, 0.25f);
	constexpr float Margin = 24.0f;
}

float ACheeseStrikeHUD::PixelsForDegrees(float Degrees, float FieldOfView, float Width)
{
	const float Focal = (Width / 2.0f) / FMath::Tan(FMath::DegreesToRadians(FieldOfView / 2.0f));
	return Focal * FMath::Tan(FMath::DegreesToRadians(Degrees));
}

void ACheeseStrikeHUD::DrawHUD()
{
	Super::DrawHUD();
	const ACheeseStrikePlayerController* Controller = Cast<ACheeseStrikePlayerController>(GetOwningPlayerController());
	if (Canvas == nullptr || Controller == nullptr)
	{
		return;
	}
	const ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(Controller->GetPawn());
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bOver = Match != nullptr && Match->HasMatchEnded();
	const float FieldOfView = Controller->PlayerCameraManager != nullptr ? Controller->PlayerCameraManager->GetFOVAngle() : 90.0f;
	if (Character != nullptr && Character->IsAlive() && !bOver)
	{
		DrawFight(Character, FieldOfView);
	}
	else if (Character != nullptr && !Character->IsAlive() && !bOver)
	{
		DrawDeath(Character, Match);
	}
	DrawKillFeed(Match);
	DrawSessions();
	if (bOver || Controller->IsScoreboardShown())
	{
		DrawScoreboard(Match, bOver);
	}
}

void ACheeseStrikeHUD::DrawSessions()
{
	const UGameInstance* Game = GetGameInstance();
	const UCheeseStrikeSessionSubsystem* Sessions = Game != nullptr ? Game->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
	if (Sessions == nullptr || Sessions->GetStatus().IsEmpty())
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	float Y = Margin;
	DrawText(Sessions->GetStatus(), Dim, Margin, Y, Font, 1.0f);
	Y += 22.0f;
	for (const FString& Line : Sessions->GetFound())
	{
		DrawText(Line, White, Margin + 12.0f, Y, Font, 1.0f);
		Y += 20.0f;
	}
}

void ACheeseStrikeHUD::DrawFight(const ACheeseStrikeCharacter* Character, float FieldOfView)
{
	const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
	const float Width = Canvas->SizeX;
	const float Height = Canvas->SizeY;
	const FVector2D Middle(Width / 2.0f, Height / 2.0f);

	// The crosshair: four lines, as far apart as the weapon's spread, and no closer than the least gap.
	const float Spread = Weapon != nullptr ? Weapon->CurrentSpread() : 0.0f;
	const float Gap = CrosshairGap + PixelsForDegrees(Spread, FieldOfView, Width);
	DrawLine(Middle.X - Gap - CrosshairLength, Middle.Y, Middle.X - Gap, Middle.Y, White, CrosshairThickness);
	DrawLine(Middle.X + Gap, Middle.Y, Middle.X + Gap + CrosshairLength, Middle.Y, White, CrosshairThickness);
	DrawLine(Middle.X, Middle.Y - Gap - CrosshairLength, Middle.X, Middle.Y - Gap, White, CrosshairThickness);
	DrawLine(Middle.X, Middle.Y + Gap, Middle.X, Middle.Y + Gap + CrosshairLength, White, CrosshairThickness);

	// A hit: four diagonal ticks, longer and yellow for the head.
	if (Weapon != nullptr && Weapon->SinceHit() < HitMarkLife)
	{
		const float Tick = Weapon->bLastHitHead ? 16.0f : 11.0f;
		const FLinearColor Colour = Weapon->bLastHitHead ? Yellow : White;
		for (const FVector2D& Way : {FVector2D(1, 1), FVector2D(-1, 1), FVector2D(1, -1), FVector2D(-1, -1)})
		{
			const FVector2D From = Middle + Way * (Gap + 2.0f);
			const FVector2D To = Middle + Way * (Gap + 2.0f + Tick);
			DrawLine(From.X, From.Y, To.X, To.Y, Colour, CrosshairThickness + 1.0f);
		}
	}

	// Health at the lower left, ammunition at the lower right.
	UFont* Font = GEngine->GetMediumFont();
	DrawText(FString::Printf(TEXT("%.0f"), Character->GetHealth()), White, Margin, Height - Margin - 20.0f, Font, 1.6f);
	if (Character->IsProtected())
	{
		DrawText(TEXT("PROTECTED"), Dim, Margin, Height - Margin - 42.0f, Font, 1.0f);
	}
	if (Weapon != nullptr)
	{
		const FString Ammo = Weapon->IsReloading() ? TEXT("RELOADING") : FString::Printf(TEXT("%d / %d"), Weapon->GetMagazine(), Weapon->GetReserve());
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		GetTextSize(Ammo, TextWidth, TextHeight, Font, 1.6f);
		DrawText(Ammo, White, Width - Margin - TextWidth, Height - Margin - 20.0f, Font, 1.6f);
		DrawText(Weapon->GetStats().DisplayName.ToString(), Dim, Width - Margin - TextWidth, Height - Margin - 42.0f, Font, 1.0f);
	}
}

void ACheeseStrikeHUD::DrawDeath(const ACheeseStrikeCharacter* Character, const ACheeseStrikeGameState* Match)
{
	const float Width = Canvas->SizeX;
	const float Height = Canvas->SizeY;
	UFont* Font = GEngine->GetMediumFont();
	const FString By = Character->GetKilledBy().IsEmpty() ? TEXT("KILLED") : FString::Printf(TEXT("KILLED BY %s"), *Character->GetKilledBy().ToUpper());
	DrawCentred(By, Width / 2.0f, Height * 0.4f, Font, 2.0f, Red);
	if (Match != nullptr)
	{
		const float Left = Match->GetRespawnDelay() - (Match->GetServerWorldTimeSeconds() - Character->GetDiedAt());
		DrawCentred(FString::Printf(TEXT("back in %.0f"), FMath::Max(0.0f, FMath::CeilToFloat(Left))), Width / 2.0f, Height * 0.4f + 44.0f, Font, 1.2f, White);
	}
}

void ACheeseStrikeHUD::DrawKillFeed(const ACheeseStrikeGameState* Match)
{
	if (Match == nullptr)
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	const float Now = Match->GetServerWorldTimeSeconds();
	float Y = Margin;
	for (const FKillFeedEntry& Kill : Match->GetKillFeed())
	{
		if (Now - Kill.At > CheeseStrikeRules::KillFeedLife)
		{
			continue;
		}
		const FString Line = FString::Printf(TEXT("%s  killed  %s%s"), Kill.Killer.IsEmpty() ? TEXT("nobody") : *Kill.Killer, *Kill.Victim, Kill.bHead ? TEXT("  (head)") : TEXT(""));
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		GetTextSize(Line, TextWidth, TextHeight, Font, 1.1f);
		DrawText(Line, Kill.bHead ? Yellow : White, Canvas->SizeX - Margin - TextWidth, Y, Font, 1.1f);
		Y += TextHeight + 4.0f;
	}
}

void ACheeseStrikeHUD::DrawScoreboard(const ACheeseStrikeGameState* Match, bool bOver)
{
	if (Match == nullptr)
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	const float Width = Canvas->SizeX;
	const TArray<CheeseStrikeRules::FStanding> Standings = Match->Standings();
	const float RowHeight = 26.0f;
	const float PanelWidth = 420.0f;
	const float PanelHeight = 110.0f + RowHeight * Standings.Num();
	const float Left = (Width - PanelWidth) / 2.0f;
	const float Top = Canvas->SizeY * 0.2f;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), Left, Top, PanelWidth, PanelHeight);
	const FString Title = bOver ? (Match->GetWinnerName().IsEmpty() ? TEXT("MATCH OVER") : FString::Printf(TEXT("%s WINS"), *Match->GetWinnerName().ToUpper())) : TEXT("FREE-FOR-ALL");
	DrawCentred(Title, Width / 2.0f, Top + 16.0f, Font, 1.8f, bOver ? Yellow : White);
	DrawCentred(FString::Printf(TEXT("first to %d kills"), Match->GetKillLimit()), Width / 2.0f, Top + 50.0f, Font, 1.0f, Dim);
	float Y = Top + 80.0f;
	DrawText(TEXT("PLAYER"), Dim, Left + 20.0f, Y, Font, 1.0f);
	DrawText(TEXT("KILLS"), Dim, Left + PanelWidth - 150.0f, Y, Font, 1.0f);
	DrawText(TEXT("DEATHS"), Dim, Left + PanelWidth - 80.0f, Y, Font, 1.0f);
	Y += RowHeight;
	for (const CheeseStrikeRules::FStanding& Line : Standings)
	{
		DrawText(Line.Name, White, Left + 20.0f, Y, Font, 1.2f);
		DrawText(FString::FromInt(Line.Kills), White, Left + PanelWidth - 150.0f, Y, Font, 1.2f);
		DrawText(FString::FromInt(Line.Deaths), White, Left + PanelWidth - 80.0f, Y, Font, 1.2f);
		Y += RowHeight;
	}
}

void ACheeseStrikeHUD::DrawCentred(const FString& Text, float X, float Y, UFont* Font, float Scale, const FLinearColor& Colour)
{
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Text, TextWidth, TextHeight, Font, Scale);
	DrawText(Text, Colour, X - TextWidth / 2.0f, Y, Font, Scale);
}
