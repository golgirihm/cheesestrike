// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "CheeseStrikeSynthWave.generated.h"

/**
 * A sound made in code (CheeseStrikeSynth's 16-bit mono at its SampleRate), played by a generator of its own for each
 * playing, which ends with the samples: a playing of it is done, its voice let go, when they run out. (A procedural wave
 * fed through its queue, as these were until wave 16, never ends by itself: the mixer goes on asking it for samples and
 * gets silence, so every hover, click, thud and ka-ching left a voice playing nothing, until the voices ran out and the
 * oldest, the menu's music, was stopped for the next.) The same wave plays any number of times, at once or one after
 * another.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeSynthWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UCheeseStrikeSynthWave(const FObjectInitializer& ObjectInitializer);

	/** What it plays: `InSamples` at `InSampleRate` (its length follows). */
	void SetSamples(const TArray<int16>& InSamples, int32 InSampleRate);
	int32 NumSamples() const { return Samples.IsValid() ? Samples->Num() : 0; }

	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	/** Shared with the generators of its playings, which run on the audio thread. */
	TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> Samples;
	int32 SamplesRate = 22050;
};
