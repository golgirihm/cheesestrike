// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "Rules/CheeseStrikeAchievements.h"
#include "Rules/CheeseStrikeBadges.h"

namespace CheeseStrikeAchievements
{
	namespace Stat
	{
		const TCHAR* Kills = TEXT("Kills");
		const TCHAR* Headshots = TEXT("Headshots");
		const TCHAR* Assassinations = TEXT("Assassinations");
		const TCHAR* Executions = TEXT("Executions");
		const TCHAR* Revives = TEXT("Revives");
		const TCHAR* Wins = TEXT("Wins");
		const TCHAR* Matches = TEXT("Matches");
		const TCHAR* ModesPlayed = TEXT("ModesPlayed");
	}

	FString ModeStat(CheeseStrikeModes::EMode Mode)
	{
		return FString::Printf(TEXT("Mode.%s"), CheeseStrikeModes::Info(Mode).Key);
	}

	FString StreakStat(CheeseStrikeStreaks::EStreak Kind)
	{
		switch (Kind)
		{
		case CheeseStrikeStreaks::EStreak::AerialCheese: return TEXT("Streak.AerialCheese");
		case CheeseStrikeStreaks::EStreak::CheeseStrike: return TEXT("Streak.CheeseStrike");
		case CheeseStrikeStreaks::EStreak::GoudaOverload: return TEXT("Streak.GoudaOverload");
		default: return FString();
		}
	}

	TArray<FString> StatsFor(ERecord Record, uint8 Detail)
	{
		using CheeseStrikeBadges::EBadge;
		switch (Record)
		{
		case ERecord::Kill:
			return {Stat::Kills};
		case ERecord::Badge:
			if (Detail >= CheeseStrikeBadges::NumBadges)
			{
				return {};
			}
			switch (static_cast<EBadge>(Detail))
			{
			case EBadge::Headshot: return {Stat::Headshots};
			case EBadge::Assassination: return {Stat::Assassinations};
			case EBadge::Execution: return {Stat::Executions};
			default: return {FString::Printf(TEXT("Badge.%s"), CheeseStrikeBadges::Word(static_cast<EBadge>(Detail)))};
			}
		case ERecord::StreakCalled:
		{
			const FString Name = StreakStat(static_cast<CheeseStrikeStreaks::EStreak>(Detail));
			return Name.IsEmpty() ? TArray<FString>() : TArray<FString>{Name};
		}
		case ERecord::MatchPlayed:
			return Detail < CheeseStrikeModes::NumModes ? TArray<FString>{Stat::Matches, ModeStat(CheeseStrikeModes::ModeAt(Detail))} : TArray<FString>();
		case ERecord::Win:
			return {Stat::Wins};
		case ERecord::Revive:
			return {Stat::Revives};
		default:
			return {};
		}
	}

	namespace
	{
		TArray<FAchievement> MakeAchievements()
		{
			TArray<FAchievement> Out;
			const auto Add = [&Out](const TCHAR* Id, const TCHAR* Name, const FString& Description, const FString& StatName, int32 Threshold)
			{
				Out.Add({Id, Name, Description, StatName, Threshold});
			};
			Add(TEXT("ACH_KILLS_1"), TEXT("First Bite"), TEXT("Score your first kill."), Stat::Kills, 1);
			Add(TEXT("ACH_KILLS_10"), TEXT("Cheesemonger"), TEXT("Score 10 kills."), Stat::Kills, 10);
			Add(TEXT("ACH_KILLS_100"), TEXT("Big Cheese"), TEXT("Score 100 kills."), Stat::Kills, 100);
			Add(TEXT("ACH_KILLS_1000"), TEXT("Cheese Legend"), TEXT("Score 1,000 kills."), Stat::Kills, 1000);
			Add(TEXT("ACH_HEADSHOTS_1"), TEXT("Head Cheese"), TEXT("Kill with a headshot."), Stat::Headshots, 1);
			Add(TEXT("ACH_HEADSHOTS_10"), TEXT("Sharp Cheddar"), TEXT("Kill with 10 headshots."), Stat::Headshots, 10);
			Add(TEXT("ACH_HEADSHOTS_100"), TEXT("Aged to Perfection"), TEXT("Kill with 100 headshots."), Stat::Headshots, 100);
			Add(TEXT("ACH_HEADSHOTS_1000"), TEXT("Grand Cru"), TEXT("Kill with 1,000 headshots."), Stat::Headshots, 1000);
			Add(TEXT("ACH_ASSASSINATIONS_1"), TEXT("Quiet as a Mouse"), TEXT("Kill someone with the knife from behind."), Stat::Assassinations, 1);
			Add(TEXT("ACH_ASSASSINATIONS_10"), TEXT("Behind the Rind"), TEXT("Kill 10 with the knife from behind."), Stat::Assassinations, 10);
			Add(TEXT("ACH_ASSASSINATIONS_100"), TEXT("Cellar Shadow"), TEXT("Kill 100 with the knife from behind."), Stat::Assassinations, 100);
			Add(TEXT("ACH_ASSASSINATIONS_1000"), TEXT("Phantom of the Fromagerie"), TEXT("Kill 1,000 with the knife from behind."), Stat::Assassinations, 1000);
			for (const CheeseStrikeModes::EMode Mode : CheeseStrikeModes::LobbyOrder)
			{
				const CheeseStrikeModes::FModeInfo& Info = CheeseStrikeModes::Info(Mode);
				Add(*FString::Printf(TEXT("ACH_MODE_%s"), *FString(Info.Key).ToUpper()), *FString::Printf(TEXT("A Taste of %s"), Info.Name), FString::Printf(TEXT("Play a match of %s to its end."), Info.Name), ModeStat(Mode), 1);
			}
			Add(TEXT("ACH_ALL_MODES"), TEXT("The Whole Cheese Board"), TEXT("Play a match of every mode to its end."), Stat::ModesPlayed, CheeseStrikeModes::NumLobbyModes);
			Add(TEXT("ACH_STREAK_UAC"), TEXT("Eye in the Sky"), TEXT("Call in an Unmanned Aerial Cheese."), StreakStat(CheeseStrikeStreaks::EStreak::AerialCheese), 1);
			Add(TEXT("ACH_STREAK_STRIKE"), TEXT("Special Delivery"), TEXT("Call in a Cheese Strike."), StreakStat(CheeseStrikeStreaks::EStreak::CheeseStrike), 1);
			Add(TEXT("ACH_STREAK_OVERLOAD"), TEXT("Gouda Overload"), TEXT("Call in the Gouda Overload."), StreakStat(CheeseStrikeStreaks::EStreak::GoudaOverload), 1);
			Add(TEXT("ACH_EXECUTION_1"), TEXT("Final Slice"), TEXT("Execute an enemy who is down."), Stat::Executions, 1);
			Add(TEXT("ACH_REVIVE_1"), TEXT("Back on the Board"), TEXT("Revive a teammate who is down."), Stat::Revives, 1);
			Add(TEXT("ACH_WINS_1"), TEXT("Say Cheese"), TEXT("Win a match."), Stat::Wins, 1);
			Add(TEXT("ACH_WINS_10"), TEXT("Cheese Champion"), TEXT("Win 10 matches."), Stat::Wins, 10);
			return Out;
		}
	}

	const TArray<FAchievement>& All()
	{
		static const TArray<FAchievement> Achievements = MakeAchievements();
		return Achievements;
	}

	const FAchievement* Find(const FString& Id)
	{
		return All().FindByPredicate([&Id](const FAchievement& Each) { return Each.Id == Id; });
	}

	int32 Value(const TMap<FString, int32>& Stats, const FString& Name)
	{
		if (Name == Stat::ModesPlayed)
		{
			int32 Played = 0;
			for (const CheeseStrikeModes::EMode Mode : CheeseStrikeModes::LobbyOrder)
			{
				Played += Stats.FindRef(ModeStat(Mode)) > 0 ? 1 : 0;
			}
			return Played;
		}
		return Stats.FindRef(Name);
	}

	bool IsUnlocked(const FAchievement& Achievement, const TMap<FString, int32>& Stats)
	{
		return Value(Stats, Achievement.Stat) >= Achievement.Threshold;
	}

	TArray<FString> NewlyUnlocked(const TMap<FString, int32>& Before, const TMap<FString, int32>& After)
	{
		TArray<FString> Out;
		for (const FAchievement& Each : All())
		{
			if (IsUnlocked(Each, After) && !IsUnlocked(Each, Before))
			{
				Out.Add(Each.Id);
			}
		}
		return Out;
	}

	FString Toast(const FAchievement& Achievement)
	{
		return FString::Printf(TEXT("ACHIEVEMENT: %s"), *Achievement.Name.ToUpper());
	}
}
