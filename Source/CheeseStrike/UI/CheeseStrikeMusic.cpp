// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMusic.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "CheeseStrike.h"
#include "Settings/CheeseStrikeSettings.h"

namespace CheeseStrikeMusic
{
	const TCHAR* const Track = TEXT("/Game/OpenGameArt/Music/SW_MenuMusic.SW_MenuMusic");

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
		case EMenuPage::Options:
			return true;
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
	if (World == nullptr)
	{
		return;
	}
	if (Loop == nullptr)
	{
		Loop = LoadObject<USoundWave>(nullptr, CheeseStrikeMusic::Track);
		if (Loop == nullptr)
		{
			UE_LOG(LogCheeseStrike, Warning, TEXT("Music: %s isn't there"), CheeseStrikeMusic::Track);
			return;
		}
		Loop->bLooping = true;
		Loop->SoundClassObject = UCheeseStrikeSettings::MusicSoundClass();
	}
	// Made once, kept from one level to the next; without an audio device (-nosound), none.
	if (Player == nullptr)
	{
		Player = UGameplayStatics::CreateSound2D(World, Loop, 1.0f, 1.0f, 0.0f, nullptr, true, false);
		if (Player == nullptr)
		{
			return;
		}
		Player->bIsUISound = true;
		UCheeseStrikeSettings::ApplySound();
	}
	// From the top: it fades out to a stop.
	Player->FadeIn(CheeseStrikeMusic::FadeInSeconds, 1.0f);
}
