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
 * is over and its results have been up for their time, everyone travels
 * back to the lobby's level, and a joiner then waits for the host's lobby to
 * come. Until the lobby is ready to show (ACheeseStrikePlayerController::
 * IsLobbyReady), the screen the results were last seen on stays over
 * everything, the loading cover included, with a spinner and "Back to the
 * lobby" in its corner; never a lobby still being made.
 *
 * What is held is the results as they were last drawn (HoldWhatIsShown:
 * the view's own pixels, the HUD's scoreboard over the match), or a screen
 * of its own (Hold), which a post-game screen of its own, such as a podium,
 * would give instead. Without pixels to read (a server with no renderer),
 * the cover is the lobby's tile.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikePostGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The longest it is held, in seconds: a lobby that never comes doesn't keep the player under it. */
	static constexpr float HoldFor = 45.0f;

	virtual void Deinitialize() override;

	/** Holds the screen as it is now, until the lobby is ready; nothing if it is held already. */
	void HoldWhatIsShown();
	/** Holds `Screen` until the lobby is ready, in place of anything held. */
	void Hold(TSharedRef<SWidget> Screen);
	/** Lets go: the lobby is ready, or the player is somewhere else. */
	void Release();
	bool IsHeld() const { return Cover.IsValid(); }
	/** Whether what is held is the screen's own pixels (rather than the tile, without them). */
	bool HasPicture() const { return Picture.IsValid(); }

private:
	/** Puts `Screen` over the view, with the corner's spinner and words. */
	void Show(TSharedRef<SWidget> Screen);
	bool Watch(float DeltaSeconds);

	TSharedPtr<SWidget> Cover;
	TSharedPtr<FSlateDynamicImageBrush> Picture;
	FTSTicker::FDelegateHandle Ticker;
	double HeldSince = 0.0;
	int32 Grabs = 0;
};
