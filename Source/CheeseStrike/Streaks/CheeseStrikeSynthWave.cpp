// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSynthWave.h"
#include "Sound/SoundGenerator.h"

namespace
{
	/** Plays a wave's samples once, at the mixer's rate (read between them), on every channel it is asked for; then it is finished. */
	class FCheeseStrikeSynthGenerator : public ISoundGenerator
	{
	public:
		FCheeseStrikeSynthGenerator(TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> InSamples, int32 InSourceRate, float InSampleRate, int32 InChannels)
			: Samples(MoveTemp(InSamples)), Step(static_cast<double>(FMath::Max(1, InSourceRate)) / FMath::Max(1.0f, InSampleRate)), Channels(FMath::Max(1, InChannels))
		{
		}

		virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override
		{
			const int32 Count = Samples.IsValid() ? Samples->Num() : 0;
			for (int32 Index = 0; Index + Channels <= NumSamples; Index += Channels)
			{
				float Value = 0.0f;
				if (At < Count - 1)
				{
					const int32 Low = static_cast<int32>(At);
					const float Between = static_cast<float>(At - Low);
					Value = FMath::Lerp(static_cast<float>((*Samples)[Low]), static_cast<float>((*Samples)[Low + 1]), Between) / 32768.0f;
				}
				for (int32 Channel = 0; Channel < Channels; ++Channel)
				{
					OutAudio[Index + Channel] = Value;
				}
				At += Step;
			}
			return NumSamples;
		}

		virtual bool IsFinished() const override
		{
			return !Samples.IsValid() || At >= Samples->Num() - 1;
		}

	private:
		TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> Samples;
		double Step = 1.0;
		int32 Channels = 1;
		double At = 0.0;
	};
}

UCheeseStrikeSynthWave::UCheeseStrikeSynthWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bLooping = false;
	SoundGroup = SOUNDGROUP_Effects;
}

void UCheeseStrikeSynthWave::SetSamples(const TArray<int16>& InSamples, int32 InSampleRate)
{
	Samples = MakeShared<const TArray<int16>, ESPMode::ThreadSafe>(InSamples);
	SamplesRate = FMath::Max(1, InSampleRate);
	SetSampleRate(SamplesRate);
	Duration = static_cast<float>(InSamples.Num()) / SamplesRate;
}

ISoundGeneratorPtr UCheeseStrikeSynthWave::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	return MakeShared<FCheeseStrikeSynthGenerator, ESPMode::ThreadSafe>(Samples, SamplesRate, InParams.SampleRate, InParams.NumChannels > 0 ? InParams.NumChannels : NumChannels);
}
