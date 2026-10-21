// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeWatchCamera.generated.h"

class ACheeseStrikeCharacter;
class APlayerController;
class UCameraComponent;

/**
 * What a player out of an Elimination round sees: a moment after they
 * fall, the view moves to just behind and above a living teammate's
 * shoulder, looking where they look, and follows them; Fire goes on to
 * the next teammate, and when the one watched falls, another is taken.
 * With nobody left to watch it stays where it is. Made on the player's
 * own machine only (ACheeseStrikePlayerState, when they are out), and
 * gone when they are back in.
 */
UCLASS(NotPlaceable, Transient)
class CHEESESTRIKE_API ACheeseStrikeWatchCamera : public AActor
{
	GENERATED_BODY()

public:
	/** Seconds the fallen see their own fall before the view moves. */
	static constexpr float Delay = 2.5f;
	/** Where the view sits from the one watched: behind, and above their eyes (cm). */
	static constexpr float Behind = 220.0f;
	static constexpr float Above = 45.0f;

	ACheeseStrikeWatchCamera();

	/** Starts watching on behalf of `InPlayer`, whose view comes here after the delay. */
	void Watch(APlayerController* InPlayer);
	/** Gives the view back to the player's own character, if they have one, and goes. */
	void Stop();
	/** The teammate watched, if any. */
	ACheeseStrikeCharacter* GetWatched() const { return Watched.Get(); }

	virtual void Tick(float DeltaSeconds) override;

protected:
	/** Goes on to the next living teammate after the one watched. */
	void NextTeammate();
	/** The living teammates, in a steady order. */
	TArray<ACheeseStrikeCharacter*> Teammates() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;
	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<ACheeseStrikeCharacter> Watched;
	float ShowAt = 0.0f;
	bool bShowing = false;
};
