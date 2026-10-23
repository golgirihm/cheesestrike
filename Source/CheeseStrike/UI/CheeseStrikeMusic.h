// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/CheeseStrikeMenu.h"
#include "CheeseStrikeMusic.generated.h"

class UAudioComponent;
class USoundWave;

namespace CheeseStrikeMusic
{
	/** The loop that plays (tools/art/menu_music.py makes it). */
	extern const TCHAR* const Track;
	/** Seconds the music takes to come in, and to go out other than into a match. */
	constexpr float FadeInSeconds = 2.0f;
	constexpr float FadeOutSeconds = 1.5f;

	/**
	 * Whether the menu's music plays: under the front end's pages and the
	 * lobby (`Page`), but not once the lobby's countdown has begun
	 * (`bCountingDown`: it fades out over the countdown, into the match) or
	 * the match is leaving for its level (`bLeaving`), nor over a match, its
	 * pause menu and options included, nor while the results are held up
	 * on the way back to the lobby (`bHeld`).
	 */
	bool IsWanted(EMenuPage Page, bool bPausedInAMatch, bool bCountingDown, bool bLeaving, bool bHeld);
}

/**
 * The menu's music: an electric guitar loop under the front end and the
 * lobby, at the options' music volume (UCheeseStrikeSettings::
 * MusicSoundClass), going on unbroken from one level to the next (hosting
 * reloads the level into its lobby), fading out into the match as the
 * lobby's countdown runs, and back in when the lobby is back. The player
 * controller says each frame whether it is wanted (CheeseStrikeMusic::
 * IsWanted); this plays it. Nothing without an audio device (-nosound).
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeMusicSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** The music is wanted or not, from `World` (any of this game's levels); a change fades it in, or out over `FadeSeconds`. */
	void Want(UWorld* World, bool bPlay, float FadeSeconds = CheeseStrikeMusic::FadeOutSeconds);
	/** Whether it is wanted now, and whether it is heard (playing, fading in or out). */
	bool IsWanted() const { return bWanted; }
	bool IsPlaying() const;

private:
	UPROPERTY()
	TObjectPtr<UAudioComponent> Player;
	UPROPERTY()
	TObjectPtr<USoundWave> Loop;
	bool bWanted = false;
};
