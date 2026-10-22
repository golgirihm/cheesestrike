// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSettings.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectIterator.h"

namespace CheeseStrikeSettings
{
	float HorizontalFieldOfView(float Vertical, float Aspect)
	{
		const float HalfTop = FMath::DegreesToRadians(FMath::Clamp(Vertical, 1.0f, 179.0f) / 2.0f);
		return FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(HalfTop) * FMath::Max(0.1f, Aspect)));
	}

	FVector2D LookScale(float Sensitivity, float AimedShare, bool bInvert, bool bAimed)
	{
		const float Scale = FMath::Clamp(Sensitivity, MinSensitivity, MaxSensitivity) * (bAimed ? FMath::Clamp(AimedShare, MinAimedSensitivity, MaxAimedSensitivity) : 1.0f);
		return FVector2D(Scale, bInvert ? -Scale : Scale);
	}

	const int32 FrameRateLimits[] = {0, 30, 60, 120, 144, 240};

	int32 NumFrameRateLimits()
	{
		return UE_ARRAY_COUNT(FrameRateLimits);
	}

	int32 FrameRateLimitIndex(float Limit)
	{
		int32 Best = 0;
		for (int32 Index = 1; Index < NumFrameRateLimits(); ++Index)
		{
			if (FMath::Abs(FrameRateLimits[Index] - Limit) < FMath::Abs(FrameRateLimits[Best] - Limit))
			{
				Best = Index;
			}
		}
		return Best;
	}

	TArray<FIntPoint> Resolutions(const TArray<FIntPoint>& Supported, FIntPoint Desktop)
	{
		static const FIntPoint Common[] = {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3200, 1800}, {3840, 2160}};
		TArray<FIntPoint> Offered;
		for (const FIntPoint& Each : Common)
		{
			const bool bSupported = Supported.Num() == 0 || Supported.Contains(Each);
			if (bSupported && Each.X <= Desktop.X && Each.Y <= Desktop.Y)
			{
				Offered.AddUnique(Each);
			}
		}
		if (Desktop.X > 0 && Desktop.Y > 0)
		{
			Offered.AddUnique(Desktop);
		}
		Offered.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y || (A.X * A.Y == B.X * B.Y && A.X < B.X); });
		return Offered;
	}

	int32 ResolutionIndex(const TArray<FIntPoint>& Choices, FIntPoint Resolution)
	{
		int32 Best = 0;
		for (int32 Index = 0; Index < Choices.Num(); ++Index)
		{
			if (Choices[Index] == Resolution)
			{
				return Index;
			}
			if (Choices[Index].X <= Resolution.X && Choices[Index].Y <= Resolution.Y)
			{
				Best = Index;
			}
		}
		return Best;
	}
}

UCheeseStrikeSettings* UCheeseStrikeSettings::Get()
{
	return GEngine != nullptr ? Cast<UCheeseStrikeSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UCheeseStrikeSettings::SetMouseSensitivity(float Value)
{
	MouseSensitivity = FMath::Clamp(Value, CheeseStrikeSettings::MinSensitivity, CheeseStrikeSettings::MaxSensitivity);
}

void UCheeseStrikeSettings::SetAimedSensitivity(float Value)
{
	AimedSensitivity = FMath::Clamp(Value, CheeseStrikeSettings::MinAimedSensitivity, CheeseStrikeSettings::MaxAimedSensitivity);
}

void UCheeseStrikeSettings::SetFieldOfView(float Value)
{
	FieldOfView = FMath::Clamp(Value, CheeseStrikeSettings::MinFieldOfView, CheeseStrikeSettings::MaxFieldOfView);
}

void UCheeseStrikeSettings::SetMasterVolume(float Value)
{
	MasterVolume = FMath::Clamp(Value, 0.0f, 1.0f);
	ApplySound();
}

void UCheeseStrikeSettings::SetEffectsVolume(float Value)
{
	EffectsVolume = FMath::Clamp(Value, 0.0f, 1.0f);
	ApplySound();
}

float UCheeseStrikeSettings::FieldOfViewNow()
{
	const UCheeseStrikeSettings* Settings = Get();
	return Settings != nullptr ? FMath::Clamp(Settings->FieldOfView, CheeseStrikeSettings::MinFieldOfView, CheeseStrikeSettings::MaxFieldOfView) : CheeseStrikeSettings::DefaultFieldOfView;
}

USoundClass* UCheeseStrikeSettings::MenuSoundClass()
{
	// Made once and kept: a class of its own, with no parent, so that nothing set on the others reaches it.
	static USoundClass* Menu = nullptr;
	if (Menu == nullptr)
	{
		Menu = NewObject<USoundClass>(GetTransientPackage(), TEXT("CheeseStrikeMenuSounds"));
		Menu->AddToRoot();
		if (FAudioDeviceManager* Devices = GEngine != nullptr ? GEngine->GetAudioDeviceManager() : nullptr)
		{
			Devices->RegisterSoundClass(Menu);
		}
	}
	return Menu;
}

void UCheeseStrikeSettings::ApplySound()
{
	const UCheeseStrikeSettings* Settings = Get();
	const float Master = Settings != nullptr ? Settings->MasterVolume : CheeseStrikeSettings::DefaultVolume;
	const float Effects = Settings != nullptr ? Settings->EffectsVolume : CheeseStrikeSettings::DefaultVolume;
	// One mix, made once and kept active, which sets every sound class but the menu's to the effects volume; a class
	// loaded since the last time (a new level's) gets it now, so this runs again after every level loads.
	static USoundMix* EffectsMix = nullptr;
	if (EffectsMix == nullptr)
	{
		EffectsMix = NewObject<USoundMix>(GetTransientPackage(), TEXT("CheeseStrikeEffectsMix"));
		EffectsMix->AddToRoot();
	}
	USoundClass* Menu = MenuSoundClass();
	TArray<USoundClass*> Classes;
	for (TObjectIterator<USoundClass> It; It; ++It)
	{
		if (*It != Menu && !It->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && IsValid(*It))
		{
			Classes.Add(*It);
		}
	}
	// Every class the mix sets is kept for good: the audio device holds the mix's settings by the class's address and
	// never lets go of them, so a class unloaded with a level would leave it reading freed memory every frame (and
	// crash, as it did when a match began after the classes of the level before were gone).
	static TArray<TStrongObjectPtr<USoundClass>> Kept;
	for (USoundClass* Class : Classes)
	{
		if (!Kept.ContainsByPredicate([Class](const TStrongObjectPtr<USoundClass>& Each) { return Each.Get() == Class; }))
		{
			Kept.Emplace(Class);
		}
	}
	FAudioDeviceManager* Devices = GEngine != nullptr ? GEngine->GetAudioDeviceManager() : nullptr;
	if (Devices == nullptr)
	{
		// No sound at all (-nosound, or a server without it).
		return;
	}
	Devices->IterateOverAllDevices([Master, Effects, &Classes](Audio::FDeviceId, FAudioDevice* Device)
	{
		Device->SetTransientPrimaryVolume(Master);
		for (USoundClass* Class : Classes)
		{
			Device->SetSoundMixClassOverride(EffectsMix, Class, Effects, 1.0f, 0.0f, false);
		}
		Device->PushSoundMixModifier(EffectsMix);
	});
}

void UCheeseStrikeSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MouseSensitivity = CheeseStrikeSettings::DefaultSensitivity;
	AimedSensitivity = CheeseStrikeSettings::DefaultAimedSensitivity;
	bInvertLook = false;
	FieldOfView = CheeseStrikeSettings::DefaultFieldOfView;
	MasterVolume = CheeseStrikeSettings::DefaultVolume;
	EffectsVolume = CheeseStrikeSettings::DefaultVolume;
}

void UCheeseStrikeSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	ApplySound();
}
