// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHeard.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "AudioThread.h"
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
		// Only what is still waiting, and not long gone: a sound not heard within a few seconds never will be.
		const double Now = FPlatformTime::Seconds();
		TArray<TPair<uint64, TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe>>> Waiting;
		for (const FHeard& Sound : Noted())
		{
			if (!Sound.Heard->load() && Now - Sound.At < 5.0)
			{
				Waiting.Emplace(Sound.ComponentId, Sound.Heard);
			}
		}
		if (Waiting.Num() == 0)
		{
			return;
		}
		FAudioThread::RunCommandOnAudioThread([Device, Waiting = MoveTemp(Waiting)]()
		{
			for (const TPair<uint64, TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe>>& Each : Waiting)
			{
				// As the mixer's last update left it: among the voices it chose to play, audibly.
				const FActiveSound* Active = Device->FindActiveSound(Each.Key);
				if (Active != nullptr && Active->IsPlayingAudio())
				{
					Each.Value->store(true);
				}
			}
		});
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
