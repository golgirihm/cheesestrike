// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/CheeseStrikeMenu.h"
#include "CheeseStrikeMusic.generated.h"

class UAudioComponent;
class USoundWave;
class UGameInstance;

namespace CheeseStrikeMusic
{
	/** The songs that play, one after another in a random order: Fab's "FREE Cyber Metal Music Pack" by Intersonic Sound. */
	extern const TCHAR* const Songs[];
	int32 NumSongs();
	/**
	 * The song to play next of `Count`, the one before having been `Last` (none: -1), by `Roll`, a number from 0 to
	 * the number of songs it may be less one: any but the one just played, each as likely.
	 */
	int32 NextSong(int32 Count, int32 Last, int32 Roll);
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
 * The menu's music: metal songs under the front end and the lobby, one
 * after another chosen at random (never the same twice running), at the
 * options' music volume (UCheeseStrikeSettings::MusicSoundClass), going on
 * unbroken from one level to the next (hosting reloads the level into its
 * lobby), fading out into the match as the lobby's countdown runs, and back
 * in with another song when the lobby is back. The player controller says
 * each frame whether it is wanted (CheeseStrikeMusic::IsWanted); this plays
 * it. Nothing without an audio device (-nosound).
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
	/** The song playing or last played (an index into CheeseStrikeMusic::Songs; -1 for none yet). */
	int32 GetSong() const { return Song; }

private:
	/** Puts on the next song, coming in over `FadeSeconds` (0: at once). */
	void PlayNextSong(float FadeSeconds);
	void SongEnded(UAudioComponent* Ended);

	UPROPERTY()
	TObjectPtr<UAudioComponent> Player;
	UPROPERTY()
	TArray<TObjectPtr<USoundWave>> Loaded;
	TWeakObjectPtr<UWorld> LastWorld;
	int32 Song = -1;
	bool bWanted = false;
};
