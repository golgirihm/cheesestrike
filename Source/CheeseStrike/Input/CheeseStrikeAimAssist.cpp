// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeAimAssist.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "Settings/CheeseStrikeSettings.h"
#include "Weapons/CheeseStrikeWeapon.h"

namespace
{
	const CheeseStrikeAimAssist::FTuning AimAssistDefaults;
	TAutoConsoleVariable<int32> CVarAimAssist(TEXT("CheeseStrike.AimAssist"), 1, TEXT("The look stick's aim assist: on (1) or off (0). The options can turn it off too."));
	TAutoConsoleVariable<float> CVarAimAssistRange(TEXT("CheeseStrike.AimAssist.Range"), AimAssistDefaults.Range, TEXT("How far off an enemy the aim assist helps, in cm."));
	TAutoConsoleVariable<float> CVarAimAssistRadius(TEXT("CheeseStrike.AimAssist.Radius"), AimAssistDefaults.Radius, TEXT("How far either side of an enemy's middle the crosshair gets the whole of the aim assist, in cm."));
	TAutoConsoleVariable<float> CVarAimAssistFalloff(TEXT("CheeseStrike.AimAssist.Falloff"), AimAssistDefaults.Falloff, TEXT("Out to how many times that radius the aim assist fades to nothing."));
	TAutoConsoleVariable<float> CVarAimAssistSlowdown(TEXT("CheeseStrike.AimAssist.Slowdown"), AimAssistDefaults.Slowdown, TEXT("The look stick's turn on an enemy from the hip, as a share of its speed (1: no slowdown)."));
	TAutoConsoleVariable<float> CVarAimAssistAimedSlowdown(TEXT("CheeseStrike.AimAssist.AimedSlowdown"), AimAssistDefaults.AimedSlowdown, TEXT("The same down the sights."));
	TAutoConsoleVariable<float> CVarAimAssistPull(TEXT("CheeseStrike.AimAssist.Pull"), AimAssistDefaults.Pull, TEXT("The pull toward an enemy while turning from the hip, degrees a second (0: none)."));
	TAutoConsoleVariable<float> CVarAimAssistAimedPull(TEXT("CheeseStrike.AimAssist.AimedPull"), AimAssistDefaults.AimedPull, TEXT("The same down the sights."));

	/** The point of the segment from `Low` to `High` nearest the line from `Eye` along `Along`. */
	FVector NearestOnBody(const FVector& Eye, const FVector& Along, const FVector& Low, const FVector& High)
	{
		const FVector Up = High - Low;
		const double UpLength = Up.SizeSquared();
		if (UpLength <= UE_SMALL_NUMBER)
		{
			return Low;
		}
		// Where the two lines come nearest, on the body's, kept to its ends.
		const FVector Between = Low - Eye;
		const double A = FVector::DotProduct(Along, Along);
		const double B = FVector::DotProduct(Along, Up);
		const double D = FVector::DotProduct(Along, Between);
		const double E = FVector::DotProduct(Up, Between);
		const double Denominator = A * UpLength - B * B;
		const double OnBody = Denominator > UE_SMALL_NUMBER ? (B * D - A * E) / Denominator : 0.0;
		return Low + Up * FMath::Clamp(OnBody, 0.0, 1.0);
	}
}

namespace CheeseStrikeAimAssist
{
	FTuning Tuning()
	{
		FTuning Now;
		Now.Range = CVarAimAssistRange.GetValueOnGameThread();
		Now.Radius = CVarAimAssistRadius.GetValueOnGameThread();
		Now.Falloff = FMath::Max(1.0f, CVarAimAssistFalloff.GetValueOnGameThread());
		Now.Slowdown = FMath::Clamp(CVarAimAssistSlowdown.GetValueOnGameThread(), 0.0f, 1.0f);
		Now.AimedSlowdown = FMath::Clamp(CVarAimAssistAimedSlowdown.GetValueOnGameThread(), 0.0f, 1.0f);
		Now.Pull = FMath::Max(0.0f, CVarAimAssistPull.GetValueOnGameThread());
		Now.AimedPull = FMath::Max(0.0f, CVarAimAssistAimedPull.GetValueOnGameThread());
		return Now;
	}

	bool IsOn()
	{
		const UCheeseStrikeSettings* Settings = UCheeseStrikeSettings::Get();
		return CVarAimAssist.GetValueOnGameThread() != 0 && (Settings == nullptr || Settings->IsAimAssistOn());
	}

	float Strength(float Degrees, float RadiusDegrees, const FTuning& Tuning)
	{
		const float Outer = RadiusDegrees * Tuning.Falloff;
		if (Degrees <= RadiusDegrees)
		{
			return 1.0f;
		}
		return Outer > RadiusDegrees ? FMath::Clamp((Outer - Degrees) / (Outer - RadiusDegrees), 0.0f, 1.0f) : 0.0f;
	}

	FAssist Assist(const FVector& Eye, const FRotator& View, const TArray<FTarget>& Targets, bool bAimed, bool bTurning, float DeltaSeconds, const FTuning& Tuning)
	{
		FAssist Help;
		const FVector Along = View.Vector();
		float BestDegrees = 0.0f;
		FVector BestPoint = FVector::ZeroVector;
		for (int32 Index = 0; Index < Targets.Num(); ++Index)
		{
			const FVector Point = NearestOnBody(Eye, Along, Targets[Index].Low, Targets[Index].High);
			const FVector ToPoint = Point - Eye;
			const double Distance = ToPoint.Size();
			if (Distance <= 1.0 || Distance > Tuning.Range || FVector::DotProduct(ToPoint, Along) <= 0.0)
			{
				continue;
			}
			const float Degrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToPoint / Distance, Along), -1.0, 1.0)));
			const float RadiusDegrees = FMath::Max(Tuning.MinDegrees, FMath::RadiansToDegrees(FMath::Atan(Tuning.Radius / Distance)));
			const float Held = Strength(Degrees, RadiusDegrees, Tuning);
			if (Held > Help.Strength || (Held > 0.0f && Held == Help.Strength && Degrees < BestDegrees))
			{
				Help.Strength = Held;
				Help.Target = Index;
				BestDegrees = Degrees;
				BestPoint = Point;
			}
		}
		if (Help.Target == INDEX_NONE)
		{
			return Help;
		}
		Help.TurnScale = FMath::Lerp(1.0f, bAimed ? Tuning.AimedSlowdown : Tuning.Slowdown, Help.Strength);
		if (bTurning && DeltaSeconds > 0.0f)
		{
			// Toward the nearest point of the body, by no more than the way there.
			const FRotator Off = ((BestPoint - Eye).Rotation() - View).GetNormalized();
			const FVector2D Way(Off.Yaw, Off.Pitch);
			const double Left = Way.Size();
			const double Step = FMath::Min(Left, static_cast<double>((bAimed ? Tuning.AimedPull : Tuning.Pull) * Help.Strength * DeltaSeconds));
			if (Left > UE_KINDA_SMALL_NUMBER)
			{
				Help.PullYaw = static_cast<float>(Way.X / Left * Step);
				Help.PullPitch = static_cast<float>(Way.Y / Left * Step);
			}
		}
		return Help;
	}

	FAssist ForCharacter(const ACheeseStrikeCharacter* Character, bool bTurning, float DeltaSeconds)
	{
		UWorld* World = Character != nullptr ? Character->GetWorld() : nullptr;
		const AController* Controller = Character != nullptr ? Character->GetController() : nullptr;
		if (World == nullptr || Controller == nullptr || !IsOn())
		{
			return FAssist();
		}
		const FTuning Now = Tuning();
		FVector Eye;
		FRotator View;
		Controller->GetPlayerViewPoint(Eye, View);
		const ACheeseStrikeGameState* Match = World->GetGameState<ACheeseStrikeGameState>();
		const FVector Along = View.Vector();
		TArray<FTarget> Targets;
		for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
		{
			const ACheeseStrikeCharacter* Other = *It;
			if (Other == Character || !Other->IsAlive() || (Match != nullptr && Match->AreTeammates(Character->GetPlayerState(), Other->GetPlayerState())))
			{
				continue;
			}
			// Up the middle of the capsule, from the hips to the head.
			const FVector Middle = Other->GetActorLocation();
			const float Half = Other->GetCapsuleComponent() != nullptr ? Other->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
			FTarget Target{Middle - FVector(0.0, 0.0, Half * 0.4f), Middle + FVector(0.0, 0.0, Half * 0.8f)};
			// Only the ones near the crosshair are worth a trace, and only those seen are helped toward.
			const FVector Point = NearestOnBody(Eye, Along, Target.Low, Target.High);
			const double Distance = FVector::Dist(Point, Eye);
			if (Distance > Now.Range || FVector::DotProduct(Point - Eye, Along) <= 0.0)
			{
				continue;
			}
			const float Degrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct((Point - Eye).GetSafeNormal(), Along), -1.0, 1.0)));
			const float RadiusDegrees = FMath::Max(Now.MinDegrees, FMath::RadiansToDegrees(FMath::Atan(Now.Radius / FMath::Max(1.0, Distance))));
			if (Strength(Degrees, RadiusDegrees, Now) <= 0.0f)
			{
				continue;
			}
			FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeAimAssist), false, Character);
			Params.AddIgnoredActor(Other);
			if (World->LineTraceTestByChannel(Eye, Point, ECC_Visibility, Params))
			{
				continue;
			}
			Targets.Add(Target);
		}
		const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
		return Assist(Eye, View, Targets, Weapon != nullptr && Weapon->IsAimed(), bTurning, DeltaSeconds, Now);
	}
}
