// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UAudioComponent;
class UWorld;

/**
 * Whether the weapons' sounds are heard: each shot, reload, dry fire and
 * draw a weapon starts is noted while a test (or `CheeseStrikeSoundsPlaying`)
 * asks, and the audio thread is asked every frame whether the sound has a
 * voice, playing audibly (`FActiveSound::IsPlayingAudio`: among the voices
 * the mixer chose to play, not stopped for want of one or never started).
 * Wave 16 found gunfire and reloads going silent for good in long matches
 * while the hit's thud still played: every sound made in code
 * (CheeseStrikeSynth::MakeSound) held a voice playing silence for ever, and
 * once all 32 were held the weapons' Lyra sounds, at 0.6 of the volume, lost
 * every sort for a voice. `CheeseStrike.Play.GunfireIsHeardThroughALongMatch`
 * plays a long match and checks every one.
 */
namespace CheeseStrikeHeard
{
	enum class EKind : uint8
	{
		/** A shot of the listener's own. */
		OwnShot,
		/** A sound of the listener's own weapon's handling: a reload's clip and bolt, a dry fire, a draw. */
		OwnHandling,
		/** Someone else's shot, from where it was fired. */
		OthersShot,
		/** Someone else's weapon's handling. */
		OthersHandling,
	};

	struct FHeard
	{
		FString What;
		EKind Kind = EKind::OwnShot;
		/** When it was started (seconds, real clock). */
		double At = 0.0;
		uint64 ComponentId = 0;
		/** Whether the audio thread has seen it playing audibly. Written there, read here. */
		TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Heard = MakeShared<std::atomic<bool>, ESPMode::ThreadSafe>(false);
	};

	/** Whether sounds are being noted. */
	extern bool bRecording;
	/** Notes a sound a weapon has just started (or queued on its beat), if recording. */
	void Note(const UAudioComponent* Component, EKind Kind, const FString& What);
	/** Asks the audio thread about every sound noted and not yet heard; call once a frame while recording. */
	void Poll(UWorld* World);
	/** Forgets every sound noted. */
	void Reset();
	/** Every sound noted, in order. */
	const TArray<FHeard>& All();
	/** The voices playing now in `World`'s audio device, and the most it has (-1, -1 without one). */
	TPair<int32, int32> Voices(UWorld* World);
}
