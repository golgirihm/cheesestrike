// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class AHUD;
class ACheeseStrikeGameState;
class ACheeseStrikePlayerController;
class ACheeseStrikePodium;
class UTexture;
class UCanvas;

/**
 * The words over the podium (ACheeseStrikePodium), drawn into the HUD's
 * canvas in its look, so that the picture the post-game screen holds on the
 * way to the lobby has them too: who won at the top, under each pedestal its
 * place in the countdown's cheese lettering, the name in the side's colour
 * and the score, and at the foot how long until the lobby. The host's button
 * to go back sooner is Slate (SCheeseStrikePodiumSkip), since it is pressed.
 */
namespace CheeseStrikePodiumHUD
{
	/** Draws the podium's words if this player's podium is up; true if it is (the HUD then draws nothing else). */
	bool Draw(AHUD* Hud, UCanvas* Canvas);
	/** The words themselves, at `Alpha`, with `Seconds` left (the countdown at `CountAlpha`), each place found on the canvas by `Project`. */
	void DrawWords(UCanvas* Canvas, const ACheeseStrikePodium* Podium, const ACheeseStrikeGameState* Match, float Alpha, int32 Seconds, float CountAlpha, TFunctionRef<bool(const FVector&, FVector2D&)> Project);
	/** Who won, as the top says: "THE GOUDA GUYS WIN", "CHEDDAR WINS"; nothing for nobody. */
	FString Heading(const FString& WinnerName, bool bTeams);
	/** The foot's words with `Seconds` left: "BACK TO THE LOBBY IN 12", and once it is all but there, nothing. */
	FString CountdownWords(int32 Seconds);
	/**
	 * How much of the countdown shows with `SecondsToLobby` left: all of it until the last second, then fading out, gone
	 * before the last frame is held on the way to the lobby, which so holds the podium and its names but no countdown.
	 */
	float CountdownAlpha(float SecondsToLobby);
	constexpr float CountdownFadeFrom = 1.0f;
	constexpr float CountdownGoneAt = 0.4f;
}

/**
 * The host's button over the podium, "BACK TO THE LOBBY" and the key that
 * presses it (Enter, or A on a gamepad), at the foot on the right: everyone
 * goes back at once (ACheeseStrikePlayerController::SkipPodium). It takes
 * the keyboard's and the pad's focus while it shows, and the mouse clicks it.
 */
class SCheeseStrikePodiumSkip : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCheeseStrikePodiumSkip) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ACheeseStrikePlayerController>, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	FReply Skip();
	TWeakObjectPtr<ACheeseStrikePlayerController> Controller;
};
