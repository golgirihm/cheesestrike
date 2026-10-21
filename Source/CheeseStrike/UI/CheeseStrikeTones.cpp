// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeTones.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundGenerator.h"
#include "CheeseStrike.h"
#include "Settings/CheeseStrikeSettings.h"

namespace CheeseStrikeTones
{
	namespace
	{
		/** One struck note: a sine and a quieter octave over it, rising in 6 ms and dying away by `Fall` seconds, from `Start` on. */
		float Note(float Seconds, float Start, float Pitch, float Fall)
		{
			const float Into = Seconds - Start;
			if (Into < 0.0f)
			{
				return 0.0f;
			}
			const float Rise = 1.0f - FMath::Exp(-Into / 0.006f);
			const float Body = FMath::Exp(-Into / Fall);
			const float Phase = UE_TWO_PI * Pitch * Into;
			return Rise * Body * (FMath::Sin(Phase) + 0.18f * FMath::Exp(-Into / (Fall * 0.5f)) * FMath::Sin(2.0f * Phase)) / 1.18f;
		}
	}

	float Length(ETone Tone)
	{
		return Tone == ETone::End ? 0.75f : 0.22f;
	}

	float Sample(ETone Tone, float Seconds)
	{
		const float Total = Length(Tone);
		if (Seconds < 0.0f || Seconds >= Total)
		{
			return 0.0f;
		}
		// The last 20 ms fade to nothing, so that the end never clicks.
		const float Tail = FMath::Clamp((Total - Seconds) / 0.02f, 0.0f, 1.0f);
		if (Tone == ETone::End)
		{
			// Two notes a fourth apart, the second a moment after the first (E6, then A6): brighter, and done.
			return Peak * Tail * (0.55f * Note(Seconds, 0.0f, 1318.5f, 0.16f) + 0.6f * Note(Seconds, 0.09f, 1760.0f, 0.22f));
		}
		// A soft A5, short.
		return Peak * Tail * 0.8f * Note(Seconds, 0.0f, 880.0f, 0.06f);
	}

	namespace
	{
		int32 Played[2] = {};
	}

	int32 TimesPlayed(ETone Tone)
	{
		return Played[Tone == ETone::End ? 1 : 0];
	}

	void Play(UWorld* World, ETone Tone)
	{
		++Played[Tone == ETone::End ? 1 : 0];
		UE_LOG(LogCheeseStrike, Display, TEXT("Tone: %s"), Tone == ETone::End ? TEXT("the end") : TEXT("a second"));
		if (World == nullptr || GEngine == nullptr || !GEngine->UseSound())
		{
			return;
		}
		// One wave for each tone, made once and kept; each play gets a generator of its own.
		static TMap<uint8, UCheeseStrikeToneWave*> Waves;
		UCheeseStrikeToneWave*& Wave = Waves.FindOrAdd(static_cast<uint8>(Tone));
		if (Wave == nullptr)
		{
			Wave = NewObject<UCheeseStrikeToneWave>(GetTransientPackage());
			Wave->AddToRoot();
			Wave->SetTone(Tone);
			Wave->SoundClassObject = UCheeseStrikeSettings::MenuSoundClass();
		}
		// Kept across a level change: the end's chime sounds as the match's level loads.
		UGameplayStatics::SpawnSound2D(World, Wave, 1.0f, 1.0f, 0.0f, nullptr, true, true);
	}
}

namespace
{
	/** Makes a tone's samples as the mixer asks for them, and ends with it. */
	class FCheeseStrikeToneGenerator : public ISoundGenerator
	{
	public:
		FCheeseStrikeToneGenerator(CheeseStrikeTones::ETone InTone, float InSampleRate, int32 InChannels)
			: Tone(InTone), SampleRate(FMath::Max(1.0f, InSampleRate)), Channels(FMath::Max(1, InChannels))
		{
			Frames = FMath::CeilToInt(CheeseStrikeTones::Length(Tone) * SampleRate);
		}

		virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override
		{
			for (int32 Index = 0; Index + Channels <= NumSamples; Index += Channels)
			{
				const float Value = Frame < Frames ? CheeseStrikeTones::Sample(Tone, Frame / SampleRate) : 0.0f;
				for (int32 Channel = 0; Channel < Channels; ++Channel)
				{
					OutAudio[Index + Channel] = Value;
				}
				++Frame;
			}
			return NumSamples;
		}

		virtual bool IsFinished() const override
		{
			return Frame >= Frames;
		}

	private:
		CheeseStrikeTones::ETone Tone;
		float SampleRate;
		int32 Channels;
		int32 Frames = 0;
		int32 Frame = 0;
	};
}

UCheeseStrikeToneWave::UCheeseStrikeToneWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bLooping = false;
	SetTone(CheeseStrikeTones::ETone::Second);
}

void UCheeseStrikeToneWave::SetTone(CheeseStrikeTones::ETone InTone)
{
	Tone = InTone;
	Duration = CheeseStrikeTones::Length(Tone);
}

ISoundGeneratorPtr UCheeseStrikeToneWave::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	return MakeShared<FCheeseStrikeToneGenerator, ESPMode::ThreadSafe>(Tone, InParams.SampleRate, InParams.NumChannels > 0 ? InParams.NumChannels : NumChannels);
}
