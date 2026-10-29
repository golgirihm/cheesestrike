// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The voice (Rules/CheeseStrikeVoice.h): which line each thing that happens
// says, to whom, and that every line is there.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeVoice.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoiceTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheVoiceSaysWhatConcernsTheListener, "CheeseStrike.Voice.WhatIsSaid", VoiceTests::Flags)

bool FTheVoiceSaysWhatConcernsTheListener::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeVoice;
	using CheeseStrikeStreaks::EStreak;
	// Streaks: ours, or the enemy's.
	TestTrue(TEXT("our UAC"), ForStreak(EStreak::AerialCheese, true) == ELine::UacOnline);
	TestTrue(TEXT("theirs"), ForStreak(EStreak::AerialCheese, false) == ELine::UacEnemy);
	TestTrue(TEXT("our Cheese Strike"), ForStreak(EStreak::CheeseStrike, true) == ELine::StrikeInbound);
	TestTrue(TEXT("theirs"), ForStreak(EStreak::CheeseStrike, false) == ELine::StrikeEnemy);
	TestTrue(TEXT("our Gouda Overload"), ForStreak(EStreak::GoudaOverload, true) == ELine::OverloadIncoming);
	TestTrue(TEXT("theirs"), ForStreak(EStreak::GoudaOverload, false) == ELine::OverloadEnemy);
	TestTrue(TEXT("no streak, nothing"), ForStreak(EStreak::None, true) == ELine::None);
	// Rounds and the match.
	TestTrue(TEXT("our round"), ForRound(1, 1) == ELine::RoundWon);
	TestTrue(TEXT("theirs"), ForRound(0, 1) == ELine::RoundLost);
	TestTrue(TEXT("a draw says nothing"), ForRound(CheeseStrikeModes::NoTeam, 1) == ELine::None);
	TestTrue(TEXT("won"), ForMatch(false, true) == ELine::Victory);
	TestTrue(TEXT("lost"), ForMatch(false, false) == ELine::Defeat);
	TestTrue(TEXT("a drawn match says nothing"), ForMatch(true, false) == ELine::None);
	// The vermin: once, as they come, not to someone joining long after.
	TestFalse(TEXT("not before they come"), SaysVermin(-1.0f, false));
	TestTrue(TEXT("as they come"), SaysVermin(0.1f, false));
	TestFalse(TEXT("once"), SaysVermin(0.2f, true));
	TestFalse(TEXT("not long after"), SaysVermin(10.0f, false));
	// Enemy down: a teammate's kill of an enemy in a mode of sides, now and then.
	TestTrue(TEXT("a teammate's kill"), SaysEnemyDown(true, false, true, true, 10.0f));
	TestFalse(TEXT("not one's own (the till rings)"), SaysEnemyDown(true, true, true, true, 10.0f));
	TestFalse(TEXT("not the enemy's"), SaysEnemyDown(true, false, false, true, 10.0f));
	TestFalse(TEXT("not a teammate killed (friendly fire is off, but a fall)"), SaysEnemyDown(true, false, true, false, 10.0f));
	TestFalse(TEXT("not in free-for-all"), SaysEnemyDown(false, false, true, true, 10.0f));
	TestFalse(TEXT("not twice in a few seconds"), SaysEnemyDown(true, false, true, true, EnemyDownEvery * 0.5f));
	// Reloading: called out at most once in six seconds, heard by teammates near by, never by the one reloading.
	TestTrue(TEXT("a reload is called out"), CallsReload(100.0f));
	TestFalse(TEXT("not again within six seconds"), CallsReload(5.9f));
	TestTrue(TEXT("then again"), CallsReload(ReloadEvery));
	TestTrue(TEXT("a teammate near by hears it"), HearsReload(false, true, 500.0f));
	TestFalse(TEXT("the reloader doesn't"), HearsReload(true, true, 0.0f));
	TestFalse(TEXT("an enemy doesn't"), HearsReload(false, false, 500.0f));
	TestFalse(TEXT("nor a teammate out of earshot"), HearsReload(false, true, ReloadEarshot + 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryLineIsThere, "CheeseStrike.Voice.EveryLineIsThere", VoiceTests::Flags)

bool FEveryLineIsThere::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeVoice;
	TestTrue(TEXT("nothing for no line"), PathOf(ELine::None).IsEmpty());
	TSet<FString> Paths;
	for (uint8 Line = static_cast<uint8>(ELine::UacOnline); Line <= static_cast<uint8>(ELine::Defeat); ++Line)
	{
		const FString Path = PathOf(static_cast<ELine>(Line));
		Paths.Add(Path);
		const USoundWave* Wave = LoadObject<USoundWave>(nullptr, *Path);
		if (TestNotNull(FString::Printf(TEXT("%s is there"), *Path), Wave))
		{
			TestTrue(FString::Printf(TEXT("and is a short line (%.2f s)"), Wave->Duration), Wave->Duration > 0.3f && Wave->Duration < 3.5f);
		}
	}
	TestEqual(TEXT("thirteen lines, each its own"), Paths.Num(), 13);
	return true;
}

#endif
