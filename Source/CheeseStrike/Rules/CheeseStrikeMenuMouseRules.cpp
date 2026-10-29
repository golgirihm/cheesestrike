// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMenuMouseRules.h"

namespace CheeseStrikeMenuMouse
{
	float NextVisitIn(bool bFirst, float Random)
	{
		const float Share = FMath::Clamp(Random, 0.0f, 1.0f);
		return bFirst ? FMath::Max(NotBefore, FMath::Lerp(FirstFrom, FirstTo, Share)) : FMath::Lerp(EveryFrom, EveryTo, Share);
	}

	float VisitSeconds(float WalkIn, float WalkOff)
	{
		return FMath::Max(0.0f, WalkIn) + SniffSeconds + TurnSeconds + LookSeconds + FMath::Max(0.0f, WalkOff);
	}

	FMoment At(float Since, float WalkIn, float WalkOff)
	{
		FMoment Moment;
		if (Since < 0.0f || Since >= VisitSeconds(WalkIn, WalkOff))
		{
			return Moment;
		}
		float Left = Since;
		const auto Within = [&Left](float Length) { if (Left < Length) { return true; } Left -= Length; return false; };
		if (Within(FMath::Max(0.0f, WalkIn)))
		{
			Moment.Phase = EPhase::WalkIn;
			Moment.Into = Left;
			Moment.Along = WalkIn > 0.0f ? Left / WalkIn : 1.0f;
			return Moment;
		}
		Moment.Along = 1.0f;
		if (Within(SniffSeconds))
		{
			Moment.Phase = EPhase::Sniff;
			Moment.Into = Left;
			return Moment;
		}
		if (Within(TurnSeconds))
		{
			Moment.Phase = EPhase::Turn;
			Moment.Into = Left;
			Moment.HeadShare = FMath::SmoothStep(0.0f, 1.0f, Left / TurnSeconds);
			return Moment;
		}
		if (Within(LookSeconds))
		{
			Moment.Phase = EPhase::Look;
			Moment.Into = Left;
			Moment.HeadShare = 1.0f;
			return Moment;
		}
		Moment.Phase = EPhase::WalkOff;
		Moment.Into = Left;
		Moment.Along = WalkOff > 0.0f ? FMath::Clamp(Left / WalkOff, 0.0f, 1.0f) : 1.0f;
		Moment.HeadShare = 1.0f - FMath::SmoothStep(0.0f, 1.0f, Left / TurnBackSeconds);
		return Moment;
	}

	float HeadTurnToward(float FacingYaw, float ToCheeseYaw)
	{
		return FMath::Clamp(FMath::FindDeltaAngleDegrees(FacingYaw, ToCheeseYaw), -MostHeadTurn, MostHeadTurn);
	}
}
