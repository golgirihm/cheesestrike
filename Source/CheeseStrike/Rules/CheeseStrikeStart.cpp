// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeStart.h"
#include "CheeseStrikeRules.h"
#include "Kismet/GameplayStatics.h"

namespace CheeseStrikeStart
{
	float CountdownFrom(const FString& Options)
	{
		if (!UGameplayStatics::HasOption(Options, TEXT("Countdown")))
		{
			return Countdown;
		}
		const FString Value = UGameplayStatics::ParseOption(Options, TEXT("Countdown"));
		return Value.IsNumeric() ? FMath::Clamp(FCString::Atof(*Value), 0.0f, MostCountdown) : Countdown;
	}

	float RoundCountdownFor(float MatchCountdown)
	{
		return FMath::Clamp(MatchCountdown, 0.0f, RoundCountdown);
	}

	bool IsFrozen(float Now, float GoAt)
	{
		return GoAt > 0.0f && Now < GoAt;
	}

	bool MayAct(bool bAlive, bool bCountingIn, bool bRoundOver, bool bMatchOver)
	{
		return bAlive && !bCountingIn && !bRoundOver && !bMatchOver;
	}

	bool TakesClientWord(float SinceHeld)
	{
		return SinceHeld >= 0.0f && SinceHeld < HoldGrace;
	}

	float RoundLeft(float Now, float GoAt, float EndsAt)
	{
		return EndsAt > 0.0f ? FMath::Max(0.0f, EndsAt - FMath::Max(Now, GoAt)) : 0.0f;
	}

	float ProtectedUntil(float SpawnedAt, float GoAt)
	{
		return FMath::Max(SpawnedAt, GoAt) + CheeseStrikeRules::SpawnProtection;
	}

	float AfterAttack(float Until, float Now)
	{
		return FMath::Min(Until, Now);
	}

	bool IsProtected(float Now, float Until)
	{
		return Until > 0.0f && Now < Until;
	}

	float GhostAmount(float Now, float Until)
	{
		if (Until <= 0.0f)
		{
			return 0.0f;
		}
		return Now < Until ? 1.0f : FMath::Clamp(1.0f - (Now - Until) / GhostFade, 0.0f, 1.0f);
	}

	FShown Shown(float Now, float GoAt)
	{
		FShown Out;
		if (GoAt <= 0.0f)
		{
			return Out;
		}
		const float Left = GoAt - Now;
		if (Left > MostCountdown)
		{
			return Out;
		}
		if (Left > 0.0f)
		{
			const int32 Second = FMath::CeilToInt(Left);
			// How far into this second: it comes in large and settles in the first fifth, and fades a little at its end.
			const float Into = static_cast<float>(Second) - Left;
			Out.Text = FString::FromInt(Second);
			Out.Scale = 1.0f + 0.45f * FMath::Square(1.0f - FMath::Clamp(Into / 0.2f, 0.0f, 1.0f));
			Out.Alpha = 1.0f - 0.6f * FMath::Clamp((Into - 0.75f) / 0.25f, 0.0f, 1.0f);
			return Out;
		}
		const float Since = -Left;
		if (Since >= GoShown)
		{
			return Out;
		}
		Out.Text = TEXT("FIGHT");
		Out.bGo = true;
		Out.Scale = 1.0f + 0.3f * FMath::Square(1.0f - FMath::Clamp(Since / 0.15f, 0.0f, 1.0f)) + 0.15f * Since / GoShown;
		Out.Alpha = 1.0f - FMath::Clamp((Since - GoShown * 0.5f) / (GoShown * 0.5f), 0.0f, 1.0f);
		return Out;
	}
}
