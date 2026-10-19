// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CheeseStrikeHUD.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikeGameState;
class ACheeseStrikePlayerController;
class UFont;

/**
 * What a player sees over the view, drawn in code. In a fight: a crosshair
 * whose gap opens with the weapon's spread, a mark when a shot hits (longer
 * ticks for the head), health, and the magazine and reserve. Always: the
 * last few kills at the top right. Dead: who did it and how long until
 * they are back. The scoreboard while its key is held, and for good once
 * the match is over, with the winner.
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

protected:
	void DrawFight(const ACheeseStrikeCharacter* Character, float FieldOfView);
	void DrawDeath(const ACheeseStrikeCharacter* Character, const ACheeseStrikeGameState* Match);
	void DrawKillFeed(const ACheeseStrikeGameState* Match);
	void DrawScoreboard(const ACheeseStrikeGameState* Match, bool bOver);
	/** What the online service is doing, and the matches found, at the top left. */
	void DrawSessions();
	/** Draws `Text` with its middle at `X`. */
	void DrawCentred(const FString& Text, float X, float Y, UFont* Font, float Scale, const FLinearColor& Colour);
};
