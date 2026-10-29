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

	/**
	 * In a session everyone hears the host's song at the host's point in it:
	 * the game state carries the song and the server's time it began
	 * (ACheeseStrikeGameState::GetMusicSong, GetMusicStartedAt). How far into
	 * a song of `Length` seconds it is at the server's `ServerNow`, the song
	 * having begun at `StartedAt`: from 0 to its length.
	 */
	float OffsetFor(double ServerNow, double StartedAt, float Length);
	/** The handover to a new level's game state (whose clock starts afresh): the server's time there the song `Elapsed` seconds in began at. */
	double HandOver(double ServerNow, float Elapsed);
	/** How far a joiner's song may be from the host's before it is put back in step, in seconds. */
	constexpr float InStep = 0.25f;
	/** A song this near its end is left to finish: the host is about to put on the next. */
	constexpr float EndingSeconds = 0.5f;
	/**
	 * Whether a joiner playing `Playing` (-1 for nothing), `PlayingElapsed`
	 * seconds into it, should put on the host's `HostSong` (-1 for none yet)
	 * at `HostElapsed`, of `HostLength` seconds: when it is another song, or
	 * the same out of step, but not for a song the host is about to end.
	 */
	bool ShouldFollow(int32 Playing, float PlayingElapsed, int32 HostSong, float HostElapsed, float HostLength);
}

/**
 * The menu's music: metal songs under the front end and the lobby, one
 * after another chosen at random (never the same twice running), at the
 * options' music volume (UCheeseStrikeSettings::MusicSoundClass), going on
 * unbroken from one level to the next (hosting reloads the level into its
 * lobby), fading out into the match as the lobby's countdown runs, and back
 * in with another song when the lobby is back. The player controller says
 * each frame whether it is wanted (CheeseStrikeMusic::IsWanted); this plays
 * it. In a session the host's machine picks the songs and puts each on the
 * game state, and every joiner plays that song from where the host is in it
 * (`Follow`); the front end alone picks its own. Nothing is heard without an
 * audio device (-nosound), though the songs are still chosen and timed.
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
	/** Seconds into that song now (by the clock, heard or not); 0 for none. */
	float GetElapsed() const;
	/**
	 * Each frame, after Want: in a session, the host's machine puts its song
	 * and when it began on `World`'s game state (again on each new level's),
	 * and a joiner's plays the song there from the host's point in it,
	 * switching when the host does. Alone, nothing.
	 */
	void Follow(UWorld* World);
	/** The host's machine (or alone) puts on another song at once, as at a song's end. */
	void SkipSong();

private:
	/** Loads the songs once; false if any is missing. */
	bool LoadSongs();
	/** Puts on the next song, coming in over `FadeSeconds` (0: at once). */
	void PlayNextSong(float FadeSeconds);
	/** Puts on song `Index` from `Offset` seconds into it, coming in over `FadeSeconds` (0: at once). */
	void PlaySong(int32 Index, float Offset, float FadeSeconds);
	void SongEnded(UAudioComponent* Ended);
	/** Whether `World` is a joiner's in a session, whose songs are the host's. */
	static bool FollowsTheHost(const UWorld* World);

	UPROPERTY()
	TObjectPtr<UAudioComponent> Player;
	UPROPERTY()
	TArray<TObjectPtr<USoundWave>> Loaded;
	TWeakObjectPtr<UWorld> LastWorld;
	int32 Song = -1;
	/** The clock's time (FPlatformTime::Seconds) the song playing would have begun at, heard from its start. */
	double SongClock = 0.0;
	bool bWanted = false;
	/** Whether a song is on: put on and neither faded out nor ended. */
	bool bOn = false;
};
