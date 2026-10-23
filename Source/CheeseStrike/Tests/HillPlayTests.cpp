// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play King of the Hill in the editor (see PlayTesting.h): a side
// scoring while it alone holds the hill and not while it is contested, to
// the target; the hill warning, moving to its next spot and players coming
// back away from it, in the solo mode; bots going to the hill on The Creamery
// and holding it; and every map's hill spots, fair and on floors that last.

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "Map/CheeseStrikeCheese.h"
#include "Map/CheeseStrikeHill.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeHills.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace HillPlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** On the shooter level: the host's spot, 4 m from the joiner's, and two places well out of the way. */
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);
	const FVector AsideSpot(1150.0f, -1450.0f, 100.0f);
	const FVector FarSpot(1450.0f, -1450.0f, 100.0f);
	constexpr float HillRadius = 250.0f;

	ACheeseStrikeHill* Hill(int32 Player)
	{
		return ACheeseStrikeHill::Find(PlayTesting::World(Player));
	}

	/** Player `Of`'s state as world `Seen` has it. */
	ACheeseStrikePlayerState* StateOf(int32 Of, int32 Seen)
	{
		const ACheeseStrikePlayerController* Own = PlayTesting::Controller(Of);
		const ACheeseStrikeGameState* Match = PlayTesting::GameState(Seen);
		if (Own == nullptr || Own->PlayerState == nullptr || Match == nullptr)
		{
			return nullptr;
		}
		for (APlayerState* Each : Match->PlayerArray)
		{
			if (Each != nullptr && Each->GetPlayerId() == Own->PlayerState->GetPlayerId())
			{
				return Cast<ACheeseStrikePlayerState>(Each);
			}
		}
		return nullptr;
	}

	/** The floor under `At` in the host's world. */
	FVector Floor(const FVector& At)
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(HillPlayTests), false);
		for (TActorIterator<ACheeseStrikeCharacter> It(PlayTesting::World(0)); It; ++It)
		{
			Query.AddIgnoredActor(*It);
		}
		return PlayTesting::World(0)->LineTraceSingleByChannel(Hit, At, At - FVector(0.0f, 0.0f, 500.0f), ECC_WorldStatic, Query) ? Hit.ImpactPoint : At - FVector(0.0f, 0.0f, 96.0f);
	}

	/** Puts the match's hill round the host's spot, then the out-of-the-way one, moving `MovesIn` seconds from now. */
	void PlaceHill(float MovesIn)
	{
		ACheeseStrikeHill* Ours = Hill(0);
		if (Ours != nullptr)
		{
			Ours->Begin({Floor(HostSpot), Floor(AsideSpot)}, {HillRadius, HillRadius}, PlayTesting::Now(0) + MovesIn);
		}
	}

	bool AnnouncedWith(int32 Player, const TCHAR* Words)
	{
		const ACheeseStrikeGameState* Match = PlayTesting::GameState(Player);
		return Match != nullptr && Match->GetAnnouncements().ContainsByPredicate([Words](const FAnnouncement& Each) { return Each.Text.Contains(Words); });
	}

	/** How many of the starts are at least the hill's clearance from `Centre`. */
	int32 StartsClearOf(const FVector& Centre)
	{
		int32 Clear = 0;
		for (TActorIterator<APlayerStart> It(PlayTesting::World(0)); It; ++It)
		{
			Clear += FVector::Dist2D(It->GetActorLocation(), Centre) >= CheeseStrikeHills::SpawnClear ? 1 : 0;
		}
		return Clear;
	}

	int32 ScoreBefore = 0;
	int32 PointsBefore = 0;
	/** The sides' scores and the players' time on the hill as the test's hill went up: the level's own hill may have scored a little while everyone came in. */
	int32 Base[2] = {};
	int32 BaseSeconds[3] = {};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASideScoresWhileItAloneHoldsTheHill, "CheeseStrike.Play.ASideScoresWhileItAloneHoldsTheHill", HillPlayTests::Flags)

bool FASideScoresWhileItAloneHoldsTheHill::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace HillPlayTests;
	using CheeseStrikeHills::EHold;
	StartPlaying({3, TEXT("?Mode=KingOfTheHill?KillLimit=6?RespawnDelay=1")});
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestTrue(TEXT("King of the Hill is on, in teams"), Match != nullptr && Match->GetMode() == CheeseStrikeModes::EMode::KingOfTheHill && Match->HasTeams());
		TestEqual(TEXT("to the host's points"), Match->GetKillLimit(), 6);
		if (TestNotNull(TEXT("the match has a hill"), Hill(0)))
		{
			TestTrue(TEXT("standing somewhere (this level has no spots: by the start nearest the middle)"), Hill(0)->HasBegun());
		}
		TestTrue(TEXT("the host and the second joiner are on a side, the first joiner on the other"), Match->AreTeammates(StateOf(0, 0), StateOf(2, 0)) && Match->AreEnemies(StateOf(0, 0), StateOf(1, 0)));
		PlaceHill(1000.0f);
		Base[0] = Match->GetTeamScore(0);
		Base[1] = Match->GetTeamScore(1);
		for (int32 Player = 0; Player < 3; ++Player)
		{
			BaseSeconds[Player] = StateOf(Player, 0)->GetHillSeconds();
		}
	});
	// The host alone in it: their side holds it, and scores a point a second.
	PutAt(0, HostSpot, -90.0f);
	PutAt(1, AsideSpot + FVector(0.0f, 600.0f, 0.0f), 0.0f);
	PutAt(2, JoinerSpot, 90.0f);
	Wait(2.6f);
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikeHill* Ours = Hill(0);
		TestTrue(TEXT("the host stands in it"), Ours->ContainsActor(Character(0)));
		TestFalse(TEXT("their teammate 4 m off doesn't"), Ours->ContainsActor(Character(2)));
		TestTrue(TEXT("the host's side holds it"), Ours->GetHold().State == EHold::Held && Ours->GetHold().Holder == 0);
		TestEqual(TEXT("by name"), Ours->GetHolderName(), FString(CheeseStrikeRules::TeamNames[0]));
		TestTrue(FString::Printf(TEXT("and has scored for it (%d)"), Match->GetTeamScore(0) - Base[0]), Match->GetTeamScore(0) - Base[0] >= 2);
		TestEqual(TEXT("the other side nothing"), Match->GetTeamScore(1), Base[1]);
		TestTrue(FString::Printf(TEXT("the host's time on the hill counts (%d s)"), StateOf(0, 0)->GetHillSeconds() - BaseSeconds[0]), StateOf(0, 0)->GetHillSeconds() - BaseSeconds[0] >= 2);
		TestEqual(TEXT("the teammate's doesn't"), StateOf(2, 0)->GetHillSeconds(), BaseSeconds[2]);
		TestTrue(TEXT("everyone is told it was captured"), AnnouncedWith(0, TEXT("captured the hill")));
	});
	WaitUntil(TEXT("a joiner seeing the host's side hold it"), 5.0f, []() { const ACheeseStrikeHill* Seen = Hill(1); return Seen != nullptr && Seen->GetHold().State == EHold::Held && Seen->GetHold().Holder == 0 && GameState(1)->GetTeamScore(0) >= Base[0] + 2; });
	// An enemy steps in: contested, and nobody scores.
	PutAt(1, HostSpot + FVector(120.0f, 0.0f, 0.0f), 180.0f);
	Wait(0.5f);
	Then([]() { ScoreBefore = GameState(0)->GetTeamScore(0) + GameState(0)->GetTeamScore(1); });
	Wait(2.0f);
	Then([this]()
	{
		TestTrue(TEXT("with an enemy in it too, it is contested"), Hill(0)->GetHold().State == EHold::Contested);
		TestEqual(TEXT("and nobody scores"), GameState(0)->GetTeamScore(0) + GameState(0)->GetTeamScore(1), ScoreBefore);
	});
	WaitUntil(TEXT("a joiner seeing it contested"), 5.0f, []() { const ACheeseStrikeHill* Seen = Hill(2); return Seen != nullptr && Seen->GetHold().State == EHold::Contested; });
	// The enemy leaves: the host's side holds it again, to the target.
	PutAt(1, AsideSpot + FVector(0.0f, 600.0f, 0.0f), 0.0f);
	WaitUntil(TEXT("the match ending at the target"), 12.0f, []() { return GameState(0)->HasMatchEnded(); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestEqual(TEXT("the host's side reached it"), Match->GetTeamScore(0), 6);
		TestTrue(TEXT("the other side short of it"), Match->GetTeamScore(1) < 6);
		TestEqual(TEXT("and wins"), Match->GetWinningTeam(), 0);
		TestEqual(TEXT("by name"), Match->GetWinnerName(), FString(CheeseStrikeRules::TeamNames[0]));
	});
	WaitUntil(TEXT("a joiner hearing who won"), 5.0f, []() { return GameState(1)->HasMatchEnded() && GameState(1)->GetWinningTeam() == 0; });
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHillMovesAndPlayersComeBackAwayFromIt, "CheeseStrike.Play.TheHillMovesAndPlayersComeBackAwayFromIt", HillPlayTests::Flags)

bool FTheHillMovesAndPlayersComeBackAwayFromIt::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace HillPlayTests;
	using CheeseStrikeHills::EHold;
	StartPlaying({2, TEXT("?Mode=SoloKingOfTheHill?RespawnDelay=1")});
	Then([this]()
	{
		TestTrue(TEXT("solo King of the Hill is on"), GameState(0)->GetMode() == CheeseStrikeModes::EMode::SoloKingOfTheHill && !GameState(0)->HasTeams());
		TestEqual(TEXT("to the usual 100 points"), GameState(0)->GetKillLimit(), CheeseStrikeHills::SoloTarget);
		PlaceHill(CheeseStrikeHills::MoveWarning + 6.0f);
	});
	PutAt(0, HostSpot, -90.0f);
	PutAt(1, FarSpot, 0.0f);
	Wait(1.5f);
	Then([this]()
	{
		const ACheeseStrikeHill* Ours = Hill(0);
		TestTrue(TEXT("the host alone holds it, as themself"), Ours->GetHold().State == EHold::Held && Ours->GetHold().Holder == StateOf(0, 0)->GetPlayerId());
		TestTrue(FString::Printf(TEXT("and scores their own points (%d)"), StateOf(0, 0)->GetHillPoints()), StateOf(0, 0)->GetHillPoints() >= 1);
		TestFalse(TEXT("not yet warning"), Ours->IsWarning());
	});
	WaitUntil(TEXT("the hill warning that it is moving"), 9.0f, []() { return Hill(0)->IsWarning(); });
	Then([this]() { TestEqual(TEXT("to the next spot"), Hill(0)->GetNextIndex(), 1); });
	WaitUntil(TEXT("the warning announced"), 2.0f, []() { return AnnouncedWith(0, TEXT("Hill moving in")); });
	WaitUntil(TEXT("the joiner seeing the warning"), 3.0f, []() { return Hill(1) != nullptr && Hill(1)->IsWarning() && AnnouncedWith(1, TEXT("Hill moving in")); });
	WaitUntil(TEXT("the hill moving"), 8.0f, []() { return Hill(0)->GetMoves() == 1; });
	Then([this]()
	{
		const ACheeseStrikeHill* Ours = Hill(0);
		TestEqual(TEXT("it is at its next spot"), Ours->GetSpotIndex(), 1);
		TestTrue(TEXT("out of the way"), FVector::Dist2D(Ours->GetCentre(), AsideSpot) < 10.0f);
		TestEqual(TEXT("and will go back to the first"), Ours->GetNextIndex(), 0);
		TestTrue(TEXT("its minute starts again"), Ours->SecondsToMove() > CheeseStrikeHills::HillTime - 2.0f);
		TestFalse(TEXT("the host is no longer in it"), Ours->ContainsActor(Character(0)));
		PointsBefore = StateOf(0, 0)->GetHillPoints();
	});
	WaitUntil(TEXT("the move announced"), 2.0f, []() { return AnnouncedWith(0, TEXT("The hill has moved")); });
	WaitUntil(TEXT("the joiner seeing it move"), 3.0f, []() { const ACheeseStrikeHill* Seen = Hill(1); return Seen != nullptr && Seen->GetSpotIndex() == 1 && FVector::Dist2D(Seen->GetCentre(), AsideSpot) < 10.0f; });
	Wait(1.5f);
	Then([this]() { TestEqual(TEXT("the host, where it was, scores no more"), StateOf(0, 0)->GetHillPoints(), PointsBefore); });
	// The joiner steps in, then is killed: they come back away from it, if any start is.
	PutAt(1, AsideSpot, 0.0f);
	Wait(1.5f);
	Then([this]() { TestTrue(TEXT("the joiner holds it now"), Hill(0)->IsHeldBy(StateOf(1, 0))); });
	Play(1, TEXT("cmd CheeseStrikeDie\nwait 0.3"));
	WaitUntil(TEXT("the joiner coming back"), 6.0f, []() { const ACheeseStrikeCharacter* Back = Character(1); return Back != nullptr && Back->IsAlive() && Back->GetController() != nullptr; });
	Then([this]()
	{
		const FVector Centre = Hill(0)->GetCentre();
		const float Off = FVector::Dist2D(Character(1)->GetActorLocation(), Centre);
		if (StartsClearOf(Centre) > 0)
		{
			TestTrue(FString::Printf(TEXT("the joiner came back %.0f m from the hill"), Off / 100.0f), Off >= CheeseStrikeHills::SpawnClear);
		}
		else
		{
			AddInfo(TEXT("every start is near the hill on this level: anywhere will do"));
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsGoToTheHillAndHoldIt, "CheeseStrike.Play.BotsGoToTheHillAndHoldIt", HillPlayTests::Flags)

bool FBotsGoToTheHillAndHoldIt::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace HillPlayTests;
	FSession Session;
	Session.Level = TEXT("/Game/CheeseStrike/Maps/Lvl_Creamery");
	Session.Options = TEXT("?Mode=KingOfTheHill?Bots=7");
	StartPlaying(Session);
	WaitUntil(TEXT("the navigation mesh"), 30.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		return Navigation != nullptr && Navigation->GetDefaultNavDataInstance() != nullptr && !Navigation->IsNavigationBeingBuilt(World(0));
	});
	Then([this]()
	{
		const ACheeseStrikeHill* Ours = Hill(0);
		if (TestNotNull(TEXT("the Creamery's hill"), Ours))
		{
			TestEqual(TEXT("goes round the map's five spots"), Ours->NumSpots(), 5);
			TestTrue(TEXT("starting at the press"), FVector::Dist2D(Ours->GetCentre(), FVector::ZeroVector) < 10.0f);
		}
		// The host stands aside, on their dock, out of it.
		int32 Bots = 0;
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			++Bots;
		}
		TestEqual(TEXT("seven bots"), Bots, 7);
	});
	WaitUntil(TEXT("bots in the hill"), 40.0f, []()
	{
		int32 In = 0;
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			const ACheeseStrikeCharacter* Body = Cast<ACheeseStrikeCharacter>(It->GetPawn());
			In += Body != nullptr && Body->IsAlive() && Hill(0)->ContainsActor(Body) ? 1 : 0;
		}
		return In >= 2;
	});
	Then([this]()
	{
		TMap<FString, int32> Doing;
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			Doing.FindOrAdd(It->Doing())++;
		}
		FString Said;
		int32 ForTheHill = 0;
		for (const TPair<FString, int32>& Each : Doing)
		{
			Said += FString::Printf(TEXT("%s %d; "), *Each.Key, Each.Value);
			ForTheHill += Each.Key.Contains(TEXT("hill")) ? Each.Value : 0;
		}
		AddInfo(FString::Printf(TEXT("the bots: %s"), *Said));
		TestTrue(TEXT("most bots are about the hill"), ForTheHill >= 4);
	});
	WaitUntil(TEXT("a side scoring on the hill"), 40.0f, []() { return GameState(0)->GetTeamScore(0) + GameState(0)->GetTeamScore(1) >= 3; });
	Then([this]()
	{
		int32 Kills = 0;
		for (const APlayerState* Each : GameState(0)->PlayerArray)
		{
			const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Each);
			Kills += Ours != nullptr ? Ours->GetKills() : 0;
		}
		AddInfo(FString::Printf(TEXT("%s; %d kills"), *GameState(0)->ScoreSummary(), Kills));
	});
	StopPlaying();
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FEveryMapHasFairHills, "CheeseStrike.Play.EveryMapHasFairHills", HillPlayTests::Flags)

void FEveryMapHasFairHills::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	// Every map a host can pick that a script builds (not the template's level, whose hill stands by its middle start).
	for (int32 Index = 0; Index < CheeseStrikeRules::NumMaps(); ++Index)
	{
		if (FString(CheeseStrikeRules::Maps[Index].Level).StartsWith(TEXT("/Game/CheeseStrike/Maps/")))
		{
			OutBeautifiedNames.Add(FString(CheeseStrikeRules::Maps[Index].Name).Replace(TEXT(" "), TEXT("")));
			OutTestCommands.Add(CheeseStrikeRules::Maps[Index].Level);
		}
	}
}

bool FEveryMapHasFairHills::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	FSession Session;
	Session.Level = Parameters;
	StartPlaying(Session);
	WaitUntil(TEXT("the navigation mesh"), 30.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		return Navigation != nullptr && Navigation->GetDefaultNavDataInstance() != nullptr && !Navigation->IsNavigationBeingBuilt(World(0));
	});
	Then([this]()
	{
		UWorld* Here = World(0);
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Here);
		TArray<const ACheeseStrikeHillSpot*> Spots;
		for (TActorIterator<ACheeseStrikeHillSpot> It(Here); It; ++It)
		{
			Spots.Add(*It);
		}
		Spots.Sort([](const ACheeseStrikeHillSpot& A, const ACheeseStrikeHillSpot& B) { return A.GetOrder() < B.GetOrder(); });
		TestTrue(FString::Printf(TEXT("three to five hill spots (%d)"), Spots.Num()), Spots.Num() >= 3 && Spots.Num() <= 5);
		// Each side's starts, by the tags the map gives them.
		TArray<FVector> SideStarts[CheeseStrikeRules::Teams];
		for (TActorIterator<APlayerStart> It(Here); It; ++It)
		{
			for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
			{
				if (It->PlayerStartTag == FName(CheeseStrikeModes::TeamStartTags[Team]))
				{
					SideStarts[Team].Add(It->GetActorLocation());
				}
			}
		}
		if (!TestTrue(TEXT("both sides have starts"), SideStarts[0].Num() > 0 && SideStarts[1].Num() > 0) || !TestNotNull(TEXT("a navigation system"), Navigation))
		{
			return;
		}
		const auto Nearest = [](const TArray<FVector>& Starts, const FVector& At)
		{
			float Best = TNumericLimits<float>::Max();
			for (const FVector& Each : Starts)
			{
				Best = FMath::Min(Best, static_cast<float>(FVector::Dist2D(Each, At)));
			}
			return Best;
		};
		FCollisionQueryParams Query(SCENE_QUERY_STAT(EveryMapHasFairHills), false);
		for (TActorIterator<ACheeseStrikeCharacter> It(Here); It; ++It)
		{
			Query.AddIgnoredActor(*It);
		}
		float Lean = 0.0f;
		float Reach = 0.0f;
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			const ACheeseStrikeHillSpot* Spot = Spots[Index];
			const FVector At = Spot->GetActorLocation();
			const FString Name = Spot->GetActorLabel();
			TestEqual(FString::Printf(TEXT("%s is %d in the round"), *Name, Index), Spot->GetOrder(), Index);
			// On a floor that lasts, clear and level round its middle (which may hold the cover it is named for: the press,
			// the Mother Wheel, a pump's housing): most of a ring of points at seven tenths of its radius are on a floor at its
			// height that the mice never eat, on the navigation mesh and reached from both sides' starts.
			constexpr int32 Samples = 8;
			int32 Level = 0;
			int32 Lasting = 0;
			TArray<FVector> OnMesh;
			for (int32 Sample = 0; Sample < Samples; ++Sample)
			{
				const float Angle = 2.0f * UE_PI * Sample / Samples;
				const FVector Round = At + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Spot->GetRadius() * 0.7f;
				FHitResult Under;
				if (!Here->LineTraceSingleByChannel(Under, Round + FVector(0.0f, 0.0f, 250.0f), Round - FVector(0.0f, 0.0f, 200.0f), ECC_WorldStatic, Query) || FMath::Abs(Under.ImpactPoint.Z - At.Z) >= 60.0f)
				{
					continue;
				}
				++Level;
				const ACheeseStrikeCheese* Cheese = Cast<ACheeseStrikeCheese>(Under.GetActor());
				if (Cheese != nullptr && Cheese->IsEdible())
				{
					continue;
				}
				++Lasting;
				FNavLocation Found;
				if (Navigation->ProjectPointToNavigation(Under.ImpactPoint, Found, FVector(60.0f, 60.0f, 120.0f)))
				{
					OnMesh.Add(Found.Location);
				}
			}
			TestTrue(FString::Printf(TEXT("%s is clear and level round its middle (%d of %d)"), *Name, Level, Samples), Level >= Samples - 3);
			TestTrue(FString::Printf(TEXT("%s is on a floor the mice never eat (%d of %d)"), *Name, Lasting, Samples), Lasting >= Samples - 3);
			if (TestTrue(FString::Printf(TEXT("%s is on the navigation mesh (%d of %d)"), *Name, OnMesh.Num(), Samples), OnMesh.Num() >= Samples / 2))
			{
				for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
				{
					FNavLocation From;
					if (Navigation->ProjectPointToNavigation(SideStarts[Team][0], From, FVector(100.0f, 100.0f, 250.0f)))
					{
						UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(Here, From.Location, OnMesh[0]);
						TestTrue(FString::Printf(TEXT("%s is reached from %s's starts"), *Name, CheeseStrikeRules::TeamNames[Team]), Path != nullptr && Path->IsValid() && !Path->IsPartial());
					}
				}
			}
			const float ToGouda = Nearest(SideStarts[0], At);
			const float ToMozza = Nearest(SideStarts[1], At);
			TestTrue(FString::Printf(TEXT("%s is well away from both sides' starts (%.0f m, %.0f m)"), *Name, ToGouda / 100.0f, ToMozza / 100.0f), FMath::Min(ToGouda, ToMozza) >= 1000.0f);
			Lean += ToGouda - ToMozza;
			Reach += (ToGouda + ToMozza) / 2.0f;
			if (Index == 0)
			{
				TestTrue(FString::Printf(TEXT("the first, %s, is as far from both sides (%.0f m, %.0f m)"), *Name, ToGouda / 100.0f, ToMozza / 100.0f), FMath::Abs(ToGouda - ToMozza) <= 0.1f * (ToGouda + ToMozza) / 2.0f);
			}
		}
		if (Spots.Num() > 0)
		{
			TestTrue(FString::Printf(TEXT("the round favours neither side (%.1f m on average, of %.0f)"), Lean / Spots.Num() / 100.0f, Reach / Spots.Num() / 100.0f), FMath::Abs(Lean) / Spots.Num() <= 0.05f * Reach / Spots.Num());
		}
	});
	StopPlaying();
	return true;
}

#endif
