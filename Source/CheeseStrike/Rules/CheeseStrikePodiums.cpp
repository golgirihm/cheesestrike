// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePodiums.h"

namespace CheeseStrikePodiums
{
	namespace
	{
		/** A step of Gun Game, or a point of the hill, is worth more than any number of kills. */
		constexpr int32 PodiumRankWeight = 100000;
		/** The wheels the pedestals are stacked from, in centimetres. */
		constexpr float PodiumWheelHeight = 34.0f;

		/**
		 * Each hero's own emotes (Content/Paragon*), for the first, second and
		 * third places: a victory pose, a boast, a taunt. In the order of
		 * CheeseStrikeBodies::All: Lt. Belica, Murdock, Wraith, Twinblast.
		 */
		const TCHAR* const PodiumEmotes[][Places] = {
			{
				TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Emote_WhoopWhoop.Emote_WhoopWhoop"),
				TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Emote_Salute.Emote_Salute"),
				TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Emote_Browsing_M1.Emote_Browsing_M1"),
			},
			{
				TEXT("/Game/ParagonMurdock/Characters/Heroes/Murdock/Animations/Emote_Master_SevenShotSalute_T3.Emote_Master_SevenShotSalute_T3"),
				TEXT("/Game/ParagonMurdock/Characters/Heroes/Murdock/Animations/Emote_Taunt_LeanOnGun_T2.Emote_Taunt_LeanOnGun_T2"),
				TEXT("/Game/ParagonMurdock/Characters/Heroes/Murdock/Animations/Emote_Taunt_WatchingYou_T1.Emote_Taunt_WatchingYou_T1"),
			},
			{
				TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/Emote_DoIt.Emote_DoIt"),
				TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/Level_Start.Level_Start"),
				TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/Emote_ThumbsDown.Emote_ThumbsDown"),
			},
			{
				TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Emote_ShowOff_M2.Emote_ShowOff_M2"),
				TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Emote_GunToss_M1.Emote_GunToss_M1"),
				TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Emote_Taunt_Quickdraw_T2.Emote_Taunt_Quickdraw_T2"),
			},
		};
	}

	CheeseStrikeRules::FStanding RankedStanding(const FContender& Contender, CheeseStrikeModes::EWinBy WinBy, bool bTeams)
	{
		CheeseStrikeRules::FStanding Ranked = Contender.Standing;
		if (WinBy == CheeseStrikeModes::EWinBy::Ladder)
		{
			Ranked.Kills += (Contender.GunLevel + 1) * PodiumRankWeight;
		}
		if (WinBy == CheeseStrikeModes::EWinBy::Hill)
		{
			// The hill ranks by the time captured, kills breaking a tie; the solo hill is played to points, so those come first.
			if (bTeams)
			{
				Ranked.Kills = Contender.HillSeconds * PodiumRankWeight + Contender.Standing.Kills;
			}
			else
			{
				Ranked.Kills = Contender.HillPoints * PodiumRankWeight + Contender.HillSeconds;
			}
		}
		return Ranked;
	}

	TArray<int32> Choose(TConstArrayView<FContender> Contenders, CheeseStrikeModes::EWinBy WinBy, bool bTeams, int32 WinningTeam)
	{
		const bool bWinnersOnly = bTeams && WinningTeam != CheeseStrikeModes::NoTeam && Contenders.ContainsByPredicate([WinningTeam](const FContender& Each) { return Each.Team == WinningTeam; });
		// Each one's ranked standing, its order made unique by where it is in the list, to find it again once sorted.
		TArray<CheeseStrikeRules::FStanding> Standings;
		TArray<int32> Orders;
		for (int32 Index = 0; Index < Contenders.Num(); ++Index)
		{
			if (bWinnersOnly && Contenders[Index].Team != WinningTeam)
			{
				continue;
			}
			CheeseStrikeRules::FStanding Ranked = RankedStanding(Contenders[Index], WinBy, bTeams);
			Ranked.Order = Ranked.Order * Contenders.Num() + Index;
			Standings.Add(Ranked);
			Orders.Add(Index);
		}
		TArray<int32> Chosen;
		for (const CheeseStrikeRules::FStanding& Standing : CheeseStrikeRules::Sorted(Standings))
		{
			if (Chosen.Num() == Places)
			{
				break;
			}
			Chosen.Add(Standing.Order % FMath::Max(1, Contenders.Num()));
		}
		return Chosen;
	}

	int32 ScoreOf(const FContender& Contender, CheeseStrikeModes::EWinBy WinBy, bool bTeams)
	{
		if (WinBy == CheeseStrikeModes::EWinBy::Ladder)
		{
			return Contender.GunLevel + 1;
		}
		if (WinBy == CheeseStrikeModes::EWinBy::Hill && !bTeams)
		{
			return Contender.HillPoints;
		}
		return Contender.Standing.Kills;
	}

	FString ScoreWords(int32 Score, CheeseStrikeModes::EWinBy WinBy, bool bTeams, int32 Steps)
	{
		if (WinBy == CheeseStrikeModes::EWinBy::Ladder)
		{
			return Steps > 0 ? FString::Printf(TEXT("STEP %d OF %d"), Score, Steps) : FString::Printf(TEXT("STEP %d"), Score);
		}
		if (WinBy == CheeseStrikeModes::EWinBy::Hill && !bTeams)
		{
			return FString::Printf(TEXT("%d %s"), Score, Score == 1 ? TEXT("POINT") : TEXT("POINTS"));
		}
		return FString::Printf(TEXT("%d %s"), Score, Score == 1 ? TEXT("KILL") : TEXT("KILLS"));
	}

	bool IsShowing(bool bEnded, float LobbyAt, float Now)
	{
		return bEnded && LobbyAt > 0.0f && Now >= LobbyAt - Seconds;
	}

	int32 SecondsLeft(float LobbyAt, float Now)
	{
		return FMath::Max(0, FMath::CeilToInt(LobbyAt - Now - 0.25f));
	}

	int32 SlotOf(int32 Place)
	{
		static const int32 PodiumSlots[Places] = {1, 0, 2};
		return Place >= 0 && Place < Places ? PodiumSlots[Place] : INDEX_NONE;
	}

	int32 Wheels(int32 Place)
	{
		return FMath::Clamp(Places - Place, 1, Places);
	}

	float PedestalHeight(int32 Place)
	{
		return Wheels(Place) * PodiumWheelHeight;
	}

	const TCHAR* Emote(int32 Body, int32 Place)
	{
		if (Body < 0 || Body >= UE_ARRAY_COUNT(PodiumEmotes) || Place < 0 || Place >= Places)
		{
			return nullptr;
		}
		return PodiumEmotes[Body][Place];
	}
}
