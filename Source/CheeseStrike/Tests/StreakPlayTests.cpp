// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The streaks on every map, played in the editor (see PlayTesting.h): the
// Unmanned Aerial Cheese flies where it can be seen (high over a map in the
// open, under the roof of one indoors), a second one adds its time to the
// first's, and a Cheese Strike marked on someone kills them indoors as out,
// its wheels coming through the roof onto the floor rather than landing on it.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "Streaks/CheeseStrikeAerialCheese.h"
#include "Streaks/CheeseStrikeBombs.h"
#include "Streaks/CheeseStrikeOverhead.h"

namespace StreakPlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** The maps under a roof, and the highest their roof is over the play area (their scripts in tools/art: a ceiling, a hall's roof, the Labyrinth's dome). */
	struct FRoofed
	{
		const TCHAR* Level;
		float Roof;
	};
	const FRoofed Roofed[] = {
		{TEXT("/Game/CheeseStrike/Maps/Lvl_Labyrinth"), 770.0f},
		{TEXT("/Game/CheeseStrike/Maps/Lvl_Grotto"), 1900.0f},
		{TEXT("/Game/CheeseStrike/Maps/Lvl_FondueFloor"), 1200.0f},
		{TEXT("/Game/CheeseStrike/Maps/Lvl_Kitchen"), 1250.0f},
	};

	/** The roof's highest over `Level`, or 0 for a map in the open. */
	float RoofOf(const FString& Level)
	{
		for (const FRoofed& Each : Roofed)
		{
			if (Level == Each.Level)
			{
				return Each.Roof;
			}
		}
		return 0.0f;
	}

	ACheeseStrikePlayerState* StateOf(int32 Player)
	{
		const ACheeseStrikePlayerController* Own = PlayTesting::Controller(Player);
		return Own != nullptr ? Own->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	}

	ACheeseStrikeGameMode* Mode()
	{
		UWorld* Host = PlayTesting::World(0);
		return Host != nullptr ? Host->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	}

	TArray<ACheeseStrikeAerialCheese*> Drones()
	{
		TArray<ACheeseStrikeAerialCheese*> Found;
		for (TActorIterator<ACheeseStrikeAerialCheese> It(PlayTesting::World(0)); It; ++It)
		{
			Found.Add(*It);
		}
		return Found;
	}

	/** The first drone's reveal, as it ran out when the second came: when it began and ended. */
	float FirstFrom = 0.0f;
	float FirstUntil = 0.0f;
	/** Where the joiner stood when the strike was marked on them. */
	FVector MarkedAt = FVector::ZeroVector;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTheStreaksWorkOnEveryMap, "CheeseStrike.Play.TheStreaksWorkOnEveryMap", StreakPlayTests::Flags)

void FTheStreaksWorkOnEveryMap::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	// Every map of the game's own (not the template's shooter level, which has no roof).
	for (int32 Index = 0; Index < CheeseStrikeRules::NumMaps(); ++Index)
	{
		if (FString(CheeseStrikeRules::Maps[Index].Level).StartsWith(TEXT("/Game/CheeseStrike/")))
		{
			OutBeautifiedNames.Add(FString(CheeseStrikeRules::Maps[Index].Name).Replace(TEXT(" "), TEXT("")));
			OutTestCommands.Add(CheeseStrikeRules::Maps[Index].Level);
		}
	}
}

bool FTheStreaksWorkOnEveryMap::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace StreakPlayTests;
	using CheeseStrikeStreaks::EStreak;
	const FString Level = Parameters;
	const float Roof = RoofOf(Level);
	FSession Session;
	Session.Players = 2;
	Session.Level = Level;
	Session.Options = TEXT("?RespawnDelay=4");
	StartPlaying(Session);
	Wait(CheeseStrikeRules::SpawnProtection + 0.5f);

	// The Unmanned Aerial Cheese: high in the open; under the roof indoors, where it can be seen, all the way round.
	Then([this, Roof]()
	{
		StateOf(0)->GiveStreak(EStreak::AerialCheese);
		const float Now = GameState(0)->GetServerWorldTimeSeconds();
		TestTrue(TEXT("the drone is called in"), Mode()->CallStreak(StateOf(0), EStreak::AerialCheese, FVector::ZeroVector, 0.0f));
		const TArray<ACheeseStrikeAerialCheese*> Flying = Drones();
		if (!TestEqual(TEXT("a drone flies"), Flying.Num(), 1))
		{
			return;
		}
		const ACheeseStrikeAerialCheese* Drone = Flying[0];
		float Highest = -TNumericLimits<float>::Max();
		for (TActorIterator<APlayerStart> It(World(0)); It; ++It)
		{
			Highest = FMath::Max(Highest, static_cast<float>(It->GetActorLocation().Z));
		}
		AddInfo(FString::Printf(TEXT("the drone flies %.0f cm up, %.1f its size, %s"), Drone->WhereAt(Now).Z, Drone->GetScale(), Drone->IsUnderRoof() ? TEXT("under the roof") : TEXT("in the open")));
		TestEqual(TEXT("under a roof on a map under one, else in the open"), Drone->IsUnderRoof(), Roof > 0.0f);
		if (Roof <= 0.0f)
		{
			TestTrue(TEXT("in the open, high over the starts"), Drone->WhereAt(Now).Z >= Highest + CheeseStrikeStreaks::DroneAltitude - 1.0f);
			return;
		}
		const FCheeseStrikeOverhead Overhead(World(0));
		int32 Blocked = 0;
		const FVector Reach = ACheeseStrikeAerialCheese::Extent(Drone->GetScale());
		for (float Time = Now; Time < Now + CheeseStrikeStreaks::DroneCircle; Time += 1.0f)
		{
			const FVector At = Drone->WhereAt(Time);
			TestTrue(FString::Printf(TEXT("under the map's roof at %s"), *At.ToCompactString()), At.Z + Reach.Z < Roof);
			TestTrue(FString::Printf(TEXT("above the highest start at %s"), *At.ToCompactString()), At.Z > Highest + 100.0f);
			// Over something lower than the roof (a deck, a tunnel's roof), or running into something: out of sight a moment.
			const TOptional<float> Over = Overhead.RoofOver(FVector(At.X, At.Y, Highest + 100.0f), CheeseStrikeStreaks::DropHeight);
			const bool bUnder = !Over.IsSet() || At.Z < Over.GetValue();
			Blocked += bUnder && Overhead.IsClear(At, Drone->WhereAt(Time + 1.0f), Reach) ? 0 : 1;
		}
		AddInfo(FString::Printf(TEXT("%d of %.0f seconds of its circle are over something lower or run into something"), Blocked, CheeseStrikeStreaks::DroneCircle));
		TestTrue(FString::Printf(TEXT("its way round mostly in the clear under the roof (%d of %.0f seconds not)"), Blocked, CheeseStrikeStreaks::DroneCircle), Blocked <= 4);
	});

	// A second drone while the first is up: its whole time added to the first's, the sweeps going on from the first.
	Wait(5.0f);
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		FirstFrom = Match->RevealStartedFor(StateOf(0));
		FirstUntil = Match->RevealEndsFor(StateOf(0));
		StateOf(0)->GiveStreak(EStreak::AerialCheese);
		TestTrue(TEXT("a second drone is called in"), Mode()->CallStreak(StateOf(0), EStreak::AerialCheese, FVector::ZeroVector, 0.0f));
		TestTrue(FString::Printf(TEXT("its time added to the first's (%.1f, then %.1f)"), FirstUntil, Match->RevealEndsFor(StateOf(0))), FMath::IsNearlyEqual(Match->RevealEndsFor(StateOf(0)), FirstUntil + CheeseStrikeStreaks::RevealTime, 0.05f));
		TestEqual(TEXT("its sweeps going on from the first's"), Match->RevealStartedFor(StateOf(0)), FirstFrom);
		TestEqual(TEXT("two drones in the sky"), Drones().Num(), 2);
		float Latest = 0.0f;
		for (const ACheeseStrikeAerialCheese* Drone : Drones())
		{
			Latest = FMath::Max(Latest, Drone->GetEndAt());
		}
		TestTrue(TEXT("the second flying until the end of it all"), FMath::IsNearlyEqual(Latest, Match->RevealEndsFor(StateOf(0)), 0.05f));
	});
	WaitUntil(TEXT("the joiner hearing of the longer reveal"), 5.0f, []()
	{
		const ACheeseStrikeGameState* Match = GameState(1);
		const ACheeseStrikePlayerController* Host = Controller(0);
		for (APlayerState* Each : Match != nullptr ? Match->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			if (Each != nullptr && Host != nullptr && Host->PlayerState != nullptr && Each->GetPlayerId() == Host->PlayerState->GetPlayerId())
			{
				return FMath::IsNearlyEqual(Match->RevealEndsFor(Each), FirstUntil + CheeseStrikeStreaks::RevealTime, 0.1f);
			}
		}
		return false;
	});

	// A Cheese Strike marked where the joiner stands: it kills them, indoors through the roof onto their floor.
	Then([this, Roof]()
	{
		const ACheeseStrikeCharacter* Joiner = Character(1);
		MarkedAt = Joiner->GetActorLocation() - FVector(0.0f, 0.0f, Joiner->GetSimpleCollisionHalfHeight());
		StateOf(0)->GiveStreak(EStreak::CheeseStrike);
		const float Yaw = (MarkedAt - Character(0)->GetActorLocation()).Rotation().Yaw;
		TestTrue(TEXT("the strike is called in"), Mode()->CallStreak(StateOf(0), EStreak::CheeseStrike, MarkedAt, Yaw));
		const ACheeseStrikeBombs* Bombs = nullptr;
		for (TActorIterator<ACheeseStrikeBombs> It(World(0)); It; ++It)
		{
			Bombs = *It;
		}
		if (!TestNotNull(TEXT("the strike is coming"), Bombs) || !TestTrue(TEXT("with its wheels"), Bombs->GetDrops().Num() > 0))
		{
			return;
		}
		// The wheel nearest the mark: on the joiner's floor, through the roof indoors.
		const FCheeseStrikeDrop* Nearest = nullptr;
		for (const FCheeseStrikeDrop& Drop : Bombs->GetDrops())
		{
			if (Nearest == nullptr || FVector::Dist2D(Drop.Impact, MarkedAt) < FVector::Dist2D(Nearest->Impact, MarkedAt))
			{
				Nearest = &Drop;
			}
		}
		AddInfo(FString::Printf(TEXT("the wheel on the mark lands at %s, %.0f cm from the mark's height, through a roof %.0f cm over it"), *FVector(Nearest->Impact).ToCompactString(), Nearest->Impact.Z - MarkedAt.Z, Nearest->Roof));
		TestTrue(TEXT("the wheel on the mark lands on the joiner's floor"), FMath::Abs(Nearest->Impact.Z - MarkedAt.Z) < 60.0f);
		if (Roof > 0.0f)
		{
			TestTrue(TEXT("coming through the roof"), Nearest->Roof > 0.0f && Nearest->Impact.Z + Nearest->Roof <= Roof + 1.0f);
			TestTrue(FString::Printf(TEXT("and seen falling (%.2f s)"), Nearest->FallTime()), Nearest->FallTime() > 0.2f);
		}
	});
	WaitUntil(TEXT("the strike killing the joiner"), CheeseStrikeStreaks::StrikeWarning + 3.0f, []()
	{
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		return Joiner != nullptr && !Joiner->IsAlive();
	});
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		if (TestTrue(TEXT("in the feed"), Match->GetKillFeed().Num() > 0))
		{
			TestEqual(TEXT("by the Cheese Strike"), Match->GetKillFeed().Last().How, FString(TEXT("Cheese Strike")));
		}
	});
	StopPlaying();
	return true;
}

#endif
