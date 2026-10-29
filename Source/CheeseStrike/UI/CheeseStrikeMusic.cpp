// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMusic.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "CheeseStrike.h"
#include "Settings/CheeseStrikeSettings.h"

namespace CheeseStrikeMusic
{
	const TCHAR* const Songs[] = {
		TEXT("/Game/Free_Cyber_Metal_Music_Pack/WAVs/Reborn_WAV.Reborn_WAV"),
		TEXT("/Game/Free_Cyber_Metal_Music_Pack/WAVs/The_Devil_s_Confidant_WAV.The_Devil_s_Confidant_WAV"),
		TEXT("/Game/Free_Cyber_Metal_Music_Pack/WAVs/Focus_WAV.Focus_WAV"),
		TEXT("/Game/Free_Cyber_Metal_Music_Pack/WAVs/Focus_2_WAV.Focus_2_WAV"),
		TEXT("/Game/Free_Cyber_Metal_Music_Pack/WAVs/Cauldronized_WAV.Cauldronized_WAV"),
		TEXT("/Game/Free_Cyber_Metal_Music_Pack/WAVs/Opener_WAV.Opener_WAV"),
	};

	int32 NumSongs()
	{
		return UE_ARRAY_COUNT(Songs);
	}

	int32 NextSong(int32 Count, int32 Last, int32 Roll)
	{
		if (Count <= 1)
		{
			return 0;
		}
		const bool bSkip = Last >= 0 && Last < Count;
		const int32 Pick = FMath::Clamp(Roll, 0, Count - (bSkip ? 2 : 1));
		return bSkip && Pick >= Last ? Pick + 1 : Pick;
	}

	bool IsWanted(EMenuPage Page, bool bPausedInAMatch, bool bCountingDown, bool bLeaving, bool bHeld)
	{
		if (bHeld || bLeaving || bPausedInAMatch)
		{
			return false;
		}
		switch (Page)
		{
		case EMenuPage::Main:
		case EMenuPage::Find:
		case EMenuPage::Credits:
			return true;
		// The settings, from the front end or over the lobby (whose countdown fades it out as the lobby's does).
		case EMenuPage::Options:
		case EMenuPage::Lobby:
			return !bCountingDown;
		default:
			return false;
		}
	}
}

void UCheeseStrikeMusicSubsystem::Deinitialize()
{
	if (Player != nullptr)
	{
		Player->Stop();
		Player = nullptr;
	}
	Super::Deinitialize();
}

bool UCheeseStrikeMusicSubsystem::IsPlaying() const
{
	return Player != nullptr && Player->IsPlaying();
}

void UCheeseStrikeMusicSubsystem::Want(UWorld* World, bool bPlay, float FadeSeconds)
{
	if (World != nullptr)
	{
		LastWorld = World;
	}
	if (bPlay == bWanted)
	{
		return;
	}
	bWanted = bPlay;
	if (!bPlay)
	{
		if (Player != nullptr && Player->IsPlaying())
		{
			Player->FadeOut(FadeSeconds, 0.0f);
		}
		return;
	}
	// Back in with another song.
	PlayNextSong(CheeseStrikeMusic::FadeInSeconds);
}

void UCheeseStrikeMusicSubsystem::PlayNextSong(float FadeSeconds)
{
	UWorld* World = LastWorld.Get();
	if (World == nullptr)
	{
		return;
	}
	if (Loaded.Num() == 0)
	{
		for (int32 Index = 0; Index < CheeseStrikeMusic::NumSongs(); ++Index)
		{
			USoundWave* Wave = LoadObject<USoundWave>(nullptr, CheeseStrikeMusic::Songs[Index]);
			if (Wave == nullptr)
			{
				UE_LOG(LogCheeseStrike, Warning, TEXT("Music: %s isn't there"), CheeseStrikeMusic::Songs[Index]);
				Loaded.Reset();
				return;
			}
			// One song plays to its end, then the next comes on.
			Wave->bLooping = false;
			Wave->SoundClassObject = UCheeseStrikeSettings::MusicSoundClass();
			Loaded.Add(Wave);
		}
	}
	const int32 Count = Loaded.Num();
	Song = CheeseStrikeMusic::NextSong(Count, Song, FMath::RandRange(0, Song >= 0 && Count > 1 ? Count - 2 : Count - 1));
	// Made once, kept from one level to the next; without an audio device (-nosound), none.
	if (Player == nullptr)
	{
		Player = UGameplayStatics::CreateSound2D(World, Loaded[Song], 1.0f, 1.0f, 0.0f, nullptr, true, false);
		if (Player == nullptr)
		{
			return;
		}
		Player->bIsUISound = true;
		Player->OnAudioFinishedNative.AddUObject(this, &UCheeseStrikeMusicSubsystem::SongEnded);
		UCheeseStrikeSettings::ApplySound();
	}
	else
	{
		Player->SetSound(Loaded[Song]);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Music: %s"), *Loaded[Song]->GetName());
	if (FadeSeconds > 0.0f)
	{
		Player->FadeIn(FadeSeconds, 1.0f);
	}
	else
	{
		Player->Play();
	}
}

void UCheeseStrikeMusicSubsystem::SongEnded(UAudioComponent* Ended)
{
	// A song that played to its end (not one faded out, nor one stopped to change songs) is followed by another, next tick.
	if (!bWanted || Ended != Player || GetGameInstance() == nullptr)
	{
		return;
	}
	GetGameInstance()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (bWanted && Player != nullptr && !Player->IsPlaying())
		{
			PlayNextSong(0.0f);
		}
	}));
}
