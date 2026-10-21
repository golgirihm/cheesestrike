// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGunfire.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "UObject/Package.h"

namespace CheeseStrikeGunfire
{
	FSoundAttenuationSettings Attenuation()
	{
		FSoundAttenuationSettings Settings;
		Settings.bAttenuate = true;
		Settings.bSpatialize = true;
		Settings.AttenuationShape = EAttenuationShape::Sphere;
		Settings.AttenuationShapeExtents = FVector(FullWithin, 0.0f, 0.0f);
		Settings.FalloffDistance = FalloffDistance;
		Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		Settings.dBAttenuationAtMax = AttenuationAtEnd;
		Settings.FalloffMode = ENaturalSoundFalloffMode::Silent;
		// Right beside the listener a shot isn't thrown to one ear.
		Settings.NonSpatializedRadiusStart = 150.0f;
		Settings.NonSpatializedRadiusEnd = 50.0f;
		Settings.bAttenuateWithLPF = true;
		Settings.bEnableLogFrequencyScaling = true;
		Settings.AbsorptionMethod = EAirAbsorptionMethod::Linear;
		Settings.LPFRadiusMin = DullFrom;
		Settings.LPFRadiusMax = DullestAt;
		Settings.LPFFrequencyAtMin = BrightHertz;
		Settings.LPFFrequencyAtMax = DullestHertz;
		Settings.HPFFrequencyAtMin = 0.0f;
		Settings.HPFFrequencyAtMax = 0.0f;
		return Settings;
	}

	void SetConcurrency(FSoundConcurrencySettings& Settings)
	{
		Settings.SetMaxCount(MostAtOnce);
		Settings.bLimitToOwner = false;
		Settings.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
	}

	float Volume(float Distance)
	{
		return Attenuation().Evaluate(FTransform::Identity, FVector(Distance, 0.0f, 0.0f));
	}

	float LowPass(float Distance)
	{
		if (Distance <= DullFrom)
		{
			return BrightHertz;
		}
		if (Distance >= DullestAt)
		{
			return DullestHertz;
		}
		// As the engine's log frequency scaling has it: even steps in pitch, not in hertz.
		const float Through = (Distance - DullFrom) / (DullestAt - DullFrom);
		return FMath::Exp(FMath::Lerp(FMath::Loge(BrightHertz), FMath::Loge(DullestHertz), Through));
	}

	USoundAttenuation* AttenuationAsset()
	{
		// Kept from the garbage collector for good: one for every machine's shots.
		static USoundAttenuation* Asset = nullptr;
		if (Asset == nullptr)
		{
			Asset = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("CheeseStrikeGunfireAttenuation"));
			Asset->Attenuation = Attenuation();
			Asset->AddToRoot();
		}
		return Asset;
	}

	USoundConcurrency* ConcurrencyAsset()
	{
		// One for all shots, so that the limit is on them all together.
		static USoundConcurrency* Asset = nullptr;
		if (Asset == nullptr)
		{
			Asset = NewObject<USoundConcurrency>(GetTransientPackage(), TEXT("CheeseStrikeGunfireConcurrency"));
			SetConcurrency(Asset->Concurrency);
			Asset->AddToRoot();
		}
		return Asset;
	}
}
