// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// A bot playing King of the Hill (ACheeseStrikeBotController::ThinkAboutTheHill):
// to the hill, fighting on the way; holding it once there.

#include "CheeseStrikeBotController.h"
#include "NavigationSystem.h"
#include "CheeseStrikeCharacter.h"
#include "Map/CheeseStrikeHill.h"

namespace
{
	/** How long a bot holding the hill stays on one spot inside it (s), and how far into it it goes (a share of the radius). */
	constexpr float HillShuffle = 3.0f;
	constexpr float HillInward = 0.55f;
	/** How often a bot on its way to the hill finds its path afresh (s). */
	constexpr float HillRepath = 4.0f;

	/** A spot on the navigation mesh within `Radius` of `Where`, or `Where` itself on the mesh. */
	bool HillSpotNear(UWorld* World, const FVector& Where, float Radius, FVector& Out)
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (Navigation == nullptr)
		{
			return false;
		}
		FNavLocation Found;
		if (Navigation->GetRandomReachablePointInRadius(Where, Radius, Found) || Navigation->ProjectPointToNavigation(Where, Found, FVector(Radius, Radius, 300.0f)))
		{
			Out = Found.Location;
			return true;
		}
		return false;
	}
}

bool ACheeseStrikeBotController::ThinkAboutTheHill()
{
	ACheeseStrikeCharacter* Me = Body();
	const ACheeseStrikeHill* Hill = ACheeseStrikeHill::Find(GetWorld());
	if (Me == nullptr || Hill == nullptr || !Hill->HasBegun() || bTending)
	{
		HillPlan = CheeseStrikeHills::EBotPlan::Fight;
		return false;
	}
	const float Time = Now();
	const bool bIn = Hill->ContainsActor(Me);
	const float EnemyDistance = bSeesTarget && Target.IsValid() ? FVector::Dist(Me->GetActorLocation(), Target->GetActorLocation()) : TNumericLimits<float>::Max();
	HillPlan = CheeseStrikeHills::BotPlan(bIn, bSeesTarget && Target.IsValid(), EnemyDistance, Hill->IsWarning());
	switch (HillPlan)
	{
	case CheeseStrikeHills::EBotPlan::HoldHill:
	{
		if (bSeesTarget && Target.IsValid())
		{
			// Fights from the hill, stepping side to side (Step), rather than chasing them out of it.
			Move = CheeseStrikeBots::EMove::Hold;
			bRetreating = false;
			Me->DoSprint(false);
			if (bGoing)
			{
				StopMovement();
				bGoing = false;
			}
			return true;
		}
		// Nobody in sight: about the hill, a spot inside it now and then, so as not to stand still to be shot.
		if (!bGoing || Time - GoalSetAt > HillShuffle || !Hill->Contains(Goal))
		{
			FVector Inside;
			if (HillSpotNear(GetWorld(), Hill->GetCentre(), Hill->GetRadius() * HillInward, Inside) && Hill->Contains(Inside))
			{
				GoTo(Inside, false);
			}
		}
		return true;
	}
	case CheeseStrikeHills::EBotPlan::GoToHill:
	case CheeseStrikeHills::EBotPlan::GoToNext:
	{
		const bool bNext = HillPlan == CheeseStrikeHills::EBotPlan::GoToNext;
		const FVector Where = bNext ? Hill->GetNextCentre() : Hill->GetCentre();
		const float Radius = bNext ? Hill->GetNextRadius() : Hill->GetRadius();
		// Running while nobody is in sight; walking, to shoot, while someone is.
		const bool bSprint = !bSeesTarget;
		const bool bWrongGoal = FVector::Dist2D(Goal, Where) > Radius;
		if (!bGoing || bWrongGoal || Time - GoalSetAt > HillRepath)
		{
			FVector Toward;
			if (HillSpotNear(GetWorld(), Where, Radius * HillInward, Toward))
			{
				GoTo(Toward, bSprint);
			}
		}
		else
		{
			Me->DoSprint(bSprint);
		}
		bSearching = false;
		bRetreating = false;
		return true;
	}
	default:
		return false;
	}
}
