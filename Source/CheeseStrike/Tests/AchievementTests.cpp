// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The achievements and the records they are earned by
// (Rules/CheeseStrikeAchievements.h): the table, what each event counts, and
// unlocking at the thresholds. That a kill reaches the killer's machine's
// records is checked by CheeseStrike.Play.WeaponsWorkOverTheNetwork (an
// assassination).

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rules/CheeseStrikeAchievements.h"
#include "Rules/CheeseStrikeBadges.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AchievementTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheAchievementsAreAllThere, "CheeseStrike.Achievements.TheAchievementsAreAllThere", AchievementTests::Flags)
bool FTheAchievementsAreAllThere::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeAchievements;
	const TArray<FAchievement>& Table = All();
	TSet<FString> Ids;
	for (const FAchievement& Each : Table)
	{
		TestFalse(FString::Printf(TEXT("%s is the only one with its id"), *Each.Id), Ids.Contains(Each.Id));
		Ids.Add(Each.Id);
		TestTrue(FString::Printf(TEXT("%s's id is Steam's kind (ACH_, capitals, digits and underscores)"), *Each.Id), Each.Id.StartsWith(TEXT("ACH_")) && !Each.Id.Contains(TEXT(" ")) && Each.Id == Each.Id.ToUpper());
		TestTrue(FString::Printf(TEXT("%s has a name, a description, a stat and a threshold"), *Each.Id), !Each.Name.IsEmpty() && !Each.Description.IsEmpty() && !Each.Stat.IsEmpty() && Each.Threshold >= 1);
		TestEqual(FString::Printf(TEXT("%s is found by its id"), *Each.Id), Find(Each.Id), &Each);
	}
	// Kills, headshots and assassinations at 1, 10, 100 and 1000.
	for (const TCHAR* Watched : {Stat::Kills, Stat::Headshots, Stat::Assassinations})
	{
		TArray<int32> Thresholds;
		for (const FAchievement& Each : Table)
		{
			if (Each.Stat == Watched)
			{
				Thresholds.Add(Each.Threshold);
			}
		}
		TestEqual(FString::Printf(TEXT("%s at 1, 10, 100 and 1000"), Watched), Thresholds, TArray<int32>{1, 10, 100, 1000});
	}
	// Each of the lobby's modes once, and all of them.
	for (const CheeseStrikeModes::EMode Mode : CheeseStrikeModes::LobbyOrder)
	{
		TestTrue(FString::Printf(TEXT("%s played once"), CheeseStrikeModes::Info(Mode).Name), Table.ContainsByPredicate([Mode](const FAchievement& Each) { return Each.Stat == ModeStat(Mode) && Each.Threshold == 1; }));
	}
	const FAchievement* Every = Find(TEXT("ACH_ALL_MODES"));
	TestTrue(TEXT("every mode played"), Every != nullptr && Every->Stat == Stat::ModesPlayed && Every->Threshold == CheeseStrikeModes::NumLobbyModes);
	// Each streak once.
	for (int32 Index = 1; Index <= CheeseStrikeStreaks::NumStreaks; ++Index)
	{
		const CheeseStrikeStreaks::EStreak Kind = CheeseStrikeStreaks::StreakAt(Index);
		TestTrue(FString::Printf(TEXT("%s called in once"), CheeseStrikeStreaks::Info(Kind).Name), Table.ContainsByPredicate([Kind](const FAchievement& Each) { return Each.Stat == StreakStat(Kind) && Each.Threshold == 1; }));
	}
	// The firsts and the wins.
	TestTrue(TEXT("the first execution"), Table.ContainsByPredicate([](const FAchievement& Each) { return Each.Stat == Stat::Executions && Each.Threshold == 1; }));
	TestTrue(TEXT("the first revive"), Table.ContainsByPredicate([](const FAchievement& Each) { return Each.Stat == Stat::Revives && Each.Threshold == 1; }));
	TestTrue(TEXT("a match won"), Table.ContainsByPredicate([](const FAchievement& Each) { return Each.Stat == Stat::Wins && Each.Threshold == 1; }));
	TestTrue(TEXT("ten won"), Table.ContainsByPredicate([](const FAchievement& Each) { return Each.Stat == Stat::Wins && Each.Threshold == 10; }));
	TestEqual(TEXT("nothing more"), Table.Num(), 4 * 3 + CheeseStrikeModes::NumLobbyModes + 1 + CheeseStrikeStreaks::NumStreaks + 2 + 2);
	// What the user must make on Steamworks is listed in the design, each by its id.
	FString Design;
	if (TestTrue(TEXT("the design is there"), FFileHelper::LoadFileToString(Design, *FPaths::Combine(FPaths::ProjectDir(), TEXT("docs/unreal/design.md")))))
	{
		for (const FAchievement& Each : Table)
		{
			TestTrue(FString::Printf(TEXT("the design lists %s for Steamworks, with its name"), *Each.Id), Design.Contains(FString::Printf(TEXT("| %s | %s | %s |"), *Each.Id, *Each.Name, *Each.Description)));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheRecordsUnlockAtTheirThresholds, "CheeseStrike.Achievements.TheRecordsUnlockAtTheirThresholds", AchievementTests::Flags)
bool FTheRecordsUnlockAtTheirThresholds::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeAchievements;
	using CheeseStrikeBadges::EBadge;
	// What each event counts.
	TestEqual(TEXT("a kill"), StatsFor(ERecord::Kill, 0), TArray<FString>{Stat::Kills});
	TestEqual(TEXT("a headshot"), StatsFor(ERecord::Badge, static_cast<uint8>(EBadge::Headshot)), TArray<FString>{Stat::Headshots});
	TestEqual(TEXT("an assassination"), StatsFor(ERecord::Badge, static_cast<uint8>(EBadge::Assassination)), TArray<FString>{Stat::Assassinations});
	TestEqual(TEXT("an execution"), StatsFor(ERecord::Badge, static_cast<uint8>(EBadge::Execution)), TArray<FString>{Stat::Executions});
	TestEqual(TEXT("another badge, its own"), StatsFor(ERecord::Badge, static_cast<uint8>(EBadge::Revenge)), TArray<FString>{TEXT("Badge.REVENGE")});
	TestTrue(TEXT("no badge there isn't"), StatsFor(ERecord::Badge, CheeseStrikeBadges::NumBadges).IsEmpty());
	TestEqual(TEXT("a streak"), StatsFor(ERecord::StreakCalled, static_cast<uint8>(CheeseStrikeStreaks::EStreak::CheeseStrike)), TArray<FString>{TEXT("Streak.CheeseStrike")});
	TestTrue(TEXT("no streak, nothing"), StatsFor(ERecord::StreakCalled, 0).IsEmpty());
	TestEqual(TEXT("a match, and its mode"), StatsFor(ERecord::MatchPlayed, static_cast<uint8>(CheeseStrikeModes::EMode::Execution)), TArray<FString>{Stat::Matches, TEXT("Mode.Execution")});
	TestEqual(TEXT("a win"), StatsFor(ERecord::Win, 0), TArray<FString>{Stat::Wins});
	TestEqual(TEXT("a revive"), StatsFor(ERecord::Revive, 0), TArray<FString>{Stat::Revives});

	// Unlocking: at the threshold, not before, and only once.
	TMap<FString, int32> Stats;
	TestTrue(TEXT("nothing at first"), NewlyUnlocked(TMap<FString, int32>(), Stats).IsEmpty());
	TMap<FString, int32> After = Stats;
	After.Add(Stat::Kills, 1);
	TestEqual(TEXT("the first kill unlocks First Bite"), NewlyUnlocked(Stats, After), TArray<FString>{TEXT("ACH_KILLS_1")});
	Stats = After;
	After.Add(Stat::Kills, 9);
	TestTrue(TEXT("nine kills, nothing more"), NewlyUnlocked(Stats, After).IsEmpty());
	Stats = After;
	After.Add(Stat::Kills, 10);
	TestEqual(TEXT("the tenth, the next"), NewlyUnlocked(Stats, After), TArray<FString>{TEXT("ACH_KILLS_10")});
	After.Add(Stat::Kills, 1000);
	TestEqual(TEXT("a jump past several unlocks them all, in order"), NewlyUnlocked(Stats, After), TArray<FString>{TEXT("ACH_KILLS_10"), TEXT("ACH_KILLS_100"), TEXT("ACH_KILLS_1000")});
	TestTrue(TEXT("IsUnlocked by the count"), IsUnlocked(*Find(TEXT("ACH_KILLS_100")), After) && !IsUnlocked(*Find(TEXT("ACH_KILLS_100")), Stats));
	// Every mode: the lobby's modes counted, not matches of one mode many times.
	TMap<FString, int32> Modes;
	Modes.Add(ModeStat(CheeseStrikeModes::EMode::Deathmatch), 30);
	TestEqual(TEXT("one mode many times is one mode"), Value(Modes, Stat::ModesPlayed), 1);
	TMap<FString, int32> AllModes = Modes;
	for (const CheeseStrikeModes::EMode Mode : CheeseStrikeModes::LobbyOrder)
	{
		AllModes.FindOrAdd(ModeStat(Mode)) += 1;
	}
	TestEqual(TEXT("each of them once is all of them"), Value(AllModes, Stat::ModesPlayed), CheeseStrikeModes::NumLobbyModes);
	TestTrue(TEXT("which unlocks The Whole Cheese Board"), NewlyUnlocked(Modes, AllModes).Contains(TEXT("ACH_ALL_MODES")));
	// The toast.
	TestEqual(TEXT("the toast's words"), Toast(*Find(TEXT("ACH_HEADSHOTS_1"))), FString(TEXT("ACHIEVEMENT: HEAD CHEESE")));
	return true;
}

#endif
