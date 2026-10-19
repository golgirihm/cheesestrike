// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CheeseStrikeHUD.generated.h"

/**
 * What a player sees over the view in a fight, drawn in code: a crosshair
 * whose gap opens with the weapon's spread, a mark when a shot hits (longer
 * ticks for the head), health, and the magazine and reserve.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** The crosshair's lines: how long, how thick, and the least gap from the middle. */
	static constexpr float CrosshairLength = 9.0f;
	static constexpr float CrosshairThickness = 2.0f;
	static constexpr float CrosshairGap = 5.0f;
	/** Seconds a hit mark shows. */
	static constexpr float HitMarkLife = 0.25f;

	/** Pixels from the middle of a view `Width` pixels wide a line `Degrees` off the centre lands, at a field of view of `FieldOfView` degrees. */
	static float PixelsForDegrees(float Degrees, float FieldOfView, float Width);

	virtual void DrawHUD() override;
};
