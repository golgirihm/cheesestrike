// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "CheeseStrikeTones.generated.h"

class UWorld;

/**
 * The lobby's countdown tones, made by the game itself rather than recorded:
 * a soft, short tone at each second, and a brighter two-note chime at the
 * end. Each is a sine or two under a quick rise and a smooth fall, so that
 * none clicks or is shrill. The shape of each is a plain function, which
 * tests call without a world; UCheeseStrikeToneWave plays it.
 */
namespace CheeseStrikeTones
{
	enum class ETone : uint8
	{
		/** Each second of the countdown. */
		Second,
		/** The countdown's end. */
		End,
	};

	/** The loudest any tone gets, well short of full scale: they are heard over the lobby, not over everything. */
	constexpr float Peak = 0.3f;

	/** How long `Tone` sounds, in seconds. */
	float Length(ETone Tone);
	/** `Tone`'s sound `Seconds` into it, from -1 to 1; nothing before it starts or after it ends. */
	float Sample(ETone Tone, float Seconds);

	/** Plays `Tone` to the local player in `World`, at the volume the settings give the menu's sounds; nothing without sound (-nosound). It goes on if the level changes under it. */
	void Play(UWorld* World, ETone Tone);
	/** How many times `Tone` has been asked to play in this process, sound or not (for the tests). */
	int32 TimesPlayed(ETone Tone);
}

/** A tone of CheeseStrikeTones, made as it plays. */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeToneWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UCheeseStrikeToneWave(const FObjectInitializer& ObjectInitializer);

	/** Which tone it plays; sets its length to match. */
	void SetTone(CheeseStrikeTones::ETone InTone);

	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	CheeseStrikeTones::ETone Tone = CheeseStrikeTones::ETone::Second;
};
