// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeVoice.h"
#include "Rules/CheeseStrikeModes.h"

namespace CheeseStrikeVoice
{
	FString PathOf(ELine Line)
	{
		const TCHAR* Name = nullptr;
		switch (Line)
		{
		case ELine::UacOnline: Name = TEXT("SW_UacOnline"); break;
		case ELine::UacEnemy: Name = TEXT("SW_UacEnemy"); break;
		case ELine::StrikeInbound: Name = TEXT("SW_StrikeInbound"); break;
		case ELine::StrikeEnemy: Name = TEXT("SW_StrikeEnemy"); break;
		case ELine::OverloadIncoming: Name = TEXT("SW_OverloadIncoming"); break;
		case ELine::OverloadEnemy: Name = TEXT("SW_OverloadEnemy"); break;
		case ELine::VerminInbound: Name = TEXT("SW_VerminInbound"); break;
		case ELine::Reloading: Name = TEXT("SW_Reloading"); break;
		case ELine::EnemyDown: Name = TEXT("SW_EnemyDown"); break;
		case ELine::RoundWon: Name = TEXT("SW_RoundWon"); break;
		case ELine::RoundLost: Name = TEXT("SW_RoundLost"); break;
		case ELine::Victory: Name = TEXT("SW_Victory"); break;
		case ELine::Defeat: Name = TEXT("SW_Defeat"); break;
		default: return FString();
		}
		return FString::Printf(TEXT("/Game/CheeseStrike/Voice/%s.%s"), Name, Name);
	}

	ELine ForStreak(CheeseStrikeStreaks::EStreak Kind, bool bOurs)
	{
		switch (Kind)
		{
		case CheeseStrikeStreaks::EStreak::AerialCheese: return bOurs ? ELine::UacOnline : ELine::UacEnemy;
		case CheeseStrikeStreaks::EStreak::CheeseStrike: return bOurs ? ELine::StrikeInbound : ELine::StrikeEnemy;
		case CheeseStrikeStreaks::EStreak::GoudaOverload: return bOurs ? ELine::OverloadIncoming : ELine::OverloadEnemy;
		default: return ELine::None;
		}
	}

	ELine ForRound(int32 Winner, int32 Ours)
	{
		if (Winner == CheeseStrikeModes::NoTeam || Winner < 0 || Ours < 0)
		{
			return ELine::None;
		}
		return Winner == Ours ? ELine::RoundWon : ELine::RoundLost;
	}

	ELine ForMatch(bool bDraw, bool bWeWon)
	{
		if (bDraw)
		{
			return ELine::None;
		}
		return bWeWon ? ELine::Victory : ELine::Defeat;
	}

	bool SaysVermin(float SinceCame, bool bSaid)
	{
		return !bSaid && SinceCame >= 0.0f && SinceCame < 3.0f;
	}

	bool SaysEnemyDown(bool bTeams, bool bByMe, bool bByTeammate, bool bOfEnemy, float SinceLast)
	{
		return bTeams && !bByMe && bByTeammate && bOfEnemy && SinceLast >= EnemyDownEvery;
	}

	bool CallsReload(float SinceLast)
	{
		return SinceLast >= ReloadEvery;
	}

	bool HearsReload(bool bReloaderIsMe, bool bTeammate, float Distance)
	{
		return !bReloaderIsMe && bTeammate && Distance <= ReloadEarshot;
	}
}
