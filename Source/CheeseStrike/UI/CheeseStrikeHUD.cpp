// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "CheeseStrikeCharacter.h"
#include "Weapons/CheeseStrikeWeapon.h"

float ACheeseStrikeHUD::PixelsForDegrees(float Degrees, float FieldOfView, float Width)
{
	const float Focal = (Width / 2.0f) / FMath::Tan(FMath::DegreesToRadians(FieldOfView / 2.0f));
	return Focal * FMath::Tan(FMath::DegreesToRadians(Degrees));
}

void ACheeseStrikeHUD::DrawHUD()
{
	Super::DrawHUD();
	const APlayerController* Controller = GetOwningPlayerController();
	const ACheeseStrikeCharacter* Character = Controller != nullptr ? Cast<ACheeseStrikeCharacter>(Controller->GetPawn()) : nullptr;
	if (Canvas == nullptr || Character == nullptr)
	{
		return;
	}
	const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
	const float Width = Canvas->SizeX;
	const float Height = Canvas->SizeY;
	const FVector2D Middle(Width / 2.0f, Height / 2.0f);
	const float FieldOfView = Controller->PlayerCameraManager != nullptr ? Controller->PlayerCameraManager->GetFOVAngle() : 90.0f;

	// The crosshair: four lines, as far apart as the weapon's spread, and no closer than the least gap.
	const float Spread = Weapon != nullptr ? Weapon->CurrentSpread() : 0.0f;
	const float Gap = CrosshairGap + PixelsForDegrees(Spread, FieldOfView, Width);
	const FLinearColor White(1.0f, 1.0f, 1.0f, 0.9f);
	DrawLine(Middle.X - Gap - CrosshairLength, Middle.Y, Middle.X - Gap, Middle.Y, White, CrosshairThickness);
	DrawLine(Middle.X + Gap, Middle.Y, Middle.X + Gap + CrosshairLength, Middle.Y, White, CrosshairThickness);
	DrawLine(Middle.X, Middle.Y - Gap - CrosshairLength, Middle.X, Middle.Y - Gap, White, CrosshairThickness);
	DrawLine(Middle.X, Middle.Y + Gap, Middle.X, Middle.Y + Gap + CrosshairLength, White, CrosshairThickness);

	// A hit: four diagonal ticks, longer and yellow for the head.
	if (Weapon != nullptr && Weapon->SinceHit() < HitMarkLife)
	{
		const float Tick = Weapon->bLastHitHead ? 16.0f : 11.0f;
		const FLinearColor Colour = Weapon->bLastHitHead ? FLinearColor(0.94f, 0.89f, 0.26f) : White;
		for (const FVector2D& Way : {FVector2D(1, 1), FVector2D(-1, 1), FVector2D(1, -1), FVector2D(-1, -1)})
		{
			const FVector2D From = Middle + Way * (Gap + 2.0f);
			const FVector2D To = Middle + Way * (Gap + 2.0f + Tick);
			DrawLine(From.X, From.Y, To.X, To.Y, Colour, CrosshairThickness + 1.0f);
		}
	}

	// Health at the lower left, ammunition at the lower right.
	UFont* Font = GEngine->GetMediumFont();
	const float Margin = 24.0f;
	DrawText(FString::Printf(TEXT("%.0f"), Character->GetHealth()), White, Margin, Height - Margin - 20.0f, Font, 1.6f);
	if (Weapon != nullptr)
	{
		const FString Ammo = Weapon->IsReloading() ? TEXT("RELOADING") : FString::Printf(TEXT("%d / %d"), Weapon->GetMagazine(), Weapon->GetReserve());
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		GetTextSize(Ammo, TextWidth, TextHeight, Font, 1.6f);
		DrawText(Ammo, White, Width - Margin - TextWidth, Height - Margin - 20.0f, Font, 1.6f);
		DrawText(Weapon->GetStats().DisplayName.ToString(), FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), Width - Margin - TextWidth, Height - Margin - 42.0f, Font, 1.0f);
	}
}
