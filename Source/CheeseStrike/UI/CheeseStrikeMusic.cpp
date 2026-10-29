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
#include "CheeseStrikeGameState.h"
#include "HAL/PlatformTime.h"

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

	float OffsetFor(double ServerNow, double StartedAt, float Length)
	{
		return FMath::Clamp(static_cast<float>(ServerNow - StartedAt), 0.0f, FMath::Max(Length, 0.0f));
	}

	double HandOver(double ServerNow, float Elapsed)
	{
		return ServerNow - FMath::Max(Elapsed, 0.0f);
	}

	bool ShouldFollow(int32 Playing, float PlayingElapsed, int32 HostSong, float HostElapsed, float HostLength)
	{
		if (HostSong < 0 || HostElapsed >= HostLength - EndingSeconds)
		{
			return false;
		}
		return Playing != HostSong || FMath::Abs(PlayingElapsed - HostElapsed) > InStep;
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

float UCheeseStrikeMusicSubsystem::GetElapsed() const
{
	return Song >= 0 ? static_cast<float>(FPlatformTime::Seconds() - SongClock) : 0.0f;
}

bool UCheeseStrikeMusicSubsystem::FollowsTheHost(const UWorld* World)
{
	return World != nullptr && World->GetNetMode() == NM_Client;
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
		bOn = false;
		if (Player != nullptr && Player->IsPlaying())
		{
			Player->FadeOut(FadeSeconds, 0.0f);
		}
		return;
	}
	// Back in with another song; a joiner's is the host's, which Follow puts on.
	if (!FollowsTheHost(LastWorld.Get()))
	{
		PlayNextSong(CheeseStrikeMusic::FadeInSeconds);
	}
}

void UCheeseStrikeMusicSubsystem::Follow(UWorld* World)
{
	ACheeseStrikeGameState* Match = World != nullptr ? World->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Match == nullptr || !bWanted)
	{
		return;
	}
	const double ServerNow = Match->GetServerWorldTimeSeconds();
	const ENetMode Mode = World->GetNetMode();
	if (Mode == NM_ListenServer || Mode == NM_DedicatedServer)
	{
		// The host's song on its game state, each new level's too (whose clock starts again), and again if the clocks drift apart.
		if (!bOn || Song < 0)
		{
			return;
		}
		const double StartedAt = CheeseStrikeMusic::HandOver(ServerNow, GetElapsed());
		if (Match->GetMusicSong() != Song || FMath::Abs(Match->GetMusicStartedAt() - StartedAt) > 0.05)
		{
			Match->SetMusic(Song, static_cast<float>(StartedAt));
		}
		return;
	}
	if (Mode != NM_Client || !LoadSongs())
	{
		return;
	}
	const int32 HostSong = Match->GetMusicSong();
	if (!Loaded.IsValidIndex(HostSong))
	{
		return;
	}
	const float Length = Loaded[HostSong]->Duration;
	const float HostElapsed = CheeseStrikeMusic::OffsetFor(ServerNow, Match->GetMusicStartedAt(), Length);
	if (CheeseStrikeMusic::ShouldFollow(bOn ? Song : -1, GetElapsed(), HostSong, HostElapsed, Length))
	{
		// In at once where the host is; a song coming on from nothing fades in, as alone.
		PlaySong(HostSong, HostElapsed, bOn ? 0.3f : CheeseStrikeMusic::FadeInSeconds);
	}
}

void UCheeseStrikeMusicSubsystem::SkipSong()
{
	if (bWanted && !FollowsTheHost(LastWorld.Get()))
	{
		PlayNextSong(0.0f);
	}
}

bool UCheeseStrikeMusicSubsystem::LoadSongs()
{
	if (Loaded.Num() == CheeseStrikeMusic::NumSongs())
	{
		return true;
	}
	Loaded.Reset();
	for (int32 Index = 0; Index < CheeseStrikeMusic::NumSongs(); ++Index)
	{
		USoundWave* Wave = LoadObject<USoundWave>(nullptr, CheeseStrikeMusic::Songs[Index]);
		if (Wave == nullptr)
		{
			UE_LOG(LogCheeseStrike, Warning, TEXT("Music: %s isn't there"), CheeseStrikeMusic::Songs[Index]);
			Loaded.Reset();
			return false;
		}
		// One song plays to its end, then the next comes on.
		Wave->bLooping = false;
		Wave->SoundClassObject = UCheeseStrikeSettings::MusicSoundClass();
		Loaded.Add(Wave);
	}
	return true;
}

void UCheeseStrikeMusicSubsystem::PlayNextSong(float FadeSeconds)
{
	if (LastWorld.Get() == nullptr || !LoadSongs())
	{
		return;
	}
	const int32 Count = Loaded.Num();
	PlaySong(CheeseStrikeMusic::NextSong(Count, Song, FMath::RandRange(0, Song >= 0 && Count > 1 ? Count - 2 : Count - 1)), 0.0f, FadeSeconds);
}

void UCheeseStrikeMusicSubsystem::PlaySong(int32 Index, float Offset, float FadeSeconds)
{
	UWorld* World = LastWorld.Get();
	if (World == nullptr || !Loaded.IsValidIndex(Index))
	{
		return;
	}
	Song = Index;
	SongClock = FPlatformTime::Seconds() - Offset;
	bOn = true;
	UE_LOG(LogCheeseStrike, Display, TEXT("Music: %s from %.1f s"), *Loaded[Song]->GetName(), Offset);
	// Made once, kept from one level to the next; without an audio device (-nosound), none, though the song is still timed.
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
	if (FadeSeconds > 0.0f)
	{
		Player->FadeIn(FadeSeconds, 1.0f, Offset);
	}
	else
	{
		Player->Play(Offset);
	}
}

void UCheeseStrikeMusicSubsystem::SongEnded(UAudioComponent* Ended)
{
	// A song that played to its end (not one faded out, nor one stopped to change songs) is followed by another, next tick;
	// a joiner's by the host's next, which Follow puts on.
	if (!bWanted || Ended != Player || GetGameInstance() == nullptr)
	{
		return;
	}
	GetGameInstance()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (bWanted && Player != nullptr && !Player->IsPlaying())
		{
			bOn = false;
			if (!FollowsTheHost(LastWorld.Get()))
			{
				PlayNextSong(0.0f);
			}
		}
	}));
}
