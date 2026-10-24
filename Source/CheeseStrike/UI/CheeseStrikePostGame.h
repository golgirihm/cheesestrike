// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CheeseStrikePostGame.generated.h"

struct FSlateDynamicImageBrush;
class SWidget;

/**
 * The post-game screen, held up on the way back to the lobby: when a match
 * is over and its results and podium have been up for their time, everyone
 * travels back to the lobby's level, and a joiner then waits for the host's
 * lobby to come. Until the lobby is ready to show (ACheeseStrikePlayerController::
 * IsLobbyReady), the last frame the view drew stays over everything, the
 * loading cover included, and then fades away over the lobby; never a lobby
 * still being made.
 *
 * What is held is the next frame the view draws, as it is drawn (the scene
 * at the view's own resolution and quality, the HUD's words over it, but no
 * Slate), read as the engine's screenshots are, after the frame is drawn and
 * before it is shown, which works in a window, full screen and offscreen
 * alike. Until it comes the screen is left as it is; without one (a server
 * with no renderer) the cover is the lobby's tile. A wait past a moment
 * shows a spinner in the corner, and no words.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikePostGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The longest it is held, in seconds: a lobby that never comes doesn't keep the player under it. */
	static constexpr float HoldFor = 45.0f;
	/** How long it fades away over the lobby, in seconds. */
	static constexpr float FadeFor = 0.4f;
	/** How long it is held before the spinner shows, in seconds: the usual way back has none. */
	static constexpr float SpinnerAfter = 1.5f;
	/** How long the next frame is waited for before the tile is held instead, in seconds. */
	static constexpr float FrameWait = 0.5f;

	virtual void Deinitialize() override;

	/** Holds the next frame the view draws, until the lobby is ready; nothing if one is held (or coming) already. */
	void HoldWhatIsShown();
	/** Lets go at once: the player is somewhere else. */
	void Release();
	/** Whether the screen is held (or its frame on the way); not while it fades away over the ready lobby. */
	bool IsHeld() const { return bWaiting || (Cover.IsValid() && FadingSince < 0.0); }
	/** Whether what is held is the view's own frame (rather than the tile, without one). */
	bool HasPicture() const { return Picture.IsValid(); }
	/** The size of that frame; zero without one. */
	FIntPoint GetPictureSize() const { return PictureSize; }

private:
	/** Puts `Screen` over the view, with the corner's spinner. */
	void Show(TSharedRef<SWidget> Screen);
	bool Watch(float DeltaSeconds);
	/** The engine's screenshot: ours, or a play script's that came while ours was asked for (saved as it asked). */
	void OnFrame(int32 Width, int32 Height, const TArray<FColor>& Colours);
	void StopListening();

	TSharedPtr<SWidget> Cover;
	TSharedPtr<FSlateDynamicImageBrush> Picture;
	FIntPoint PictureSize = FIntPoint::ZeroValue;
	FTSTicker::FDelegateHandle Ticker;
	FDelegateHandle Listening;
	double HeldSince = 0.0;
	double FadingSince = -1.0;
	/** How long it has been held, by the frames drawn since. */
	float Waited = 0.0f;
	bool bWaiting = false;
	int32 Grabs = 0;
};
