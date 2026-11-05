// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHeard.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"

namespace CheeseStrikeHeard
{
	bool bRecording = false;

	namespace
	{
		TArray<FHeard>& Noted()
		{
			static TArray<FHeard> Sounds;
			return Sounds;
		}
	}

	void Note(const UAudioComponent* Component, EKind Kind, const FString& What)
	{
		if (!bRecording || Component == nullptr)
		{
			return;
		}
		FHeard Sound;
		Sound.What = What;
		Sound.Kind = Kind;
		Sound.At = FPlatformTime::Seconds();
		Sound.ComponentId = Component->GetAudioComponentID();
		Noted().Add(MoveTemp(Sound));
	}

	void Poll(UWorld* World)
	{
		FAudioDevice* Device = World != nullptr ? World->GetAudioDeviceRaw() : nullptr;
		if (!bRecording || Device == nullptr)
		{
			return;
		}
		// Only what is still waiting, and not long gone: a sound not heard within a few seconds never will be. Each asked on
		// the audio thread, as the mixer's last update left it: among the voices it chose to play, audibly.
		const double Now = FPlatformTime::Seconds();
		for (const FHeard& Sound : Noted())
		{
			if (!Sound.Heard->load() && Now - Sound.At < 5.0)
			{
				Device->SendCommandToActiveSounds(Sound.ComponentId, [Heard = Sound.Heard](FActiveSound& Active)
				{
					if (Active.IsPlayingAudio())
					{
						Heard->store(true);
					}
				});
			}
		}
	}

	void Reset()
	{
		Noted().Reset();
	}

	const TArray<FHeard>& All()
	{
		return Noted();
	}

	TPair<int32, int32> Voices(UWorld* World)
	{
		const FAudioDevice* Device = World != nullptr ? World->GetAudioDeviceRaw() : nullptr;
		return Device != nullptr ? TPair<int32, int32>(Device->GetNumActiveSources(), Device->GetMaxChannels()) : TPair<int32, int32>(-1, -1);
	}
}
