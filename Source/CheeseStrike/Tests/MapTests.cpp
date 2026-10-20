// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the maps in the editor (see PlayTesting.h): The Creamery
// walked over and looked across as its notes say (docs/unreal/maps/creamery.md),
// and a host picking a map in the lobby and everyone going to it.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "Map/CheeseStrikeCheese.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"

namespace MapTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const TCHAR* const Creamery = TEXT("/Game/CheeseStrike/Maps/Lvl_Creamery");

	/** What a straight line between two points first hits in the host's world, or null. */
	AActor* FirstHit(const FVector& From, const FVector& To)
	{
		UWorld* World = PlayTesting::World(0);
		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("MapTests")), true);
		for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		return World != nullptr && World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params) ? Hit.GetActor() : nullptr;
	}

	FName StructureOf(const AActor* Actor)
	{
		const ACheeseStrikeCheese* Cheese = Cast<ACheeseStrikeCheese>(Actor);
		return Cheese != nullptr ? Cheese->GetStructure() : NAME_None;
	}

	/** Puts the player at `At` facing `Yaw`, plays `Script` (or walks forward for so many seconds, if it is a number), then checks where they are with `Arrived`. */
	void Walk(FAutomationTestBase* Test, const FString& What, const FVector& At, float Yaw, const FString& Script, TFunction<bool(const FVector&)> Arrived)
	{
		PlayTesting::PutAt(0, At, Yaw);
		PlayTesting::Play(0, Script.IsNumeric() ? FString::Printf(TEXT("hold Move 0 1\nwait %s\nrelease Move\nwait 0.4"), *Script) : Script);
		PlayTesting::Then([Test, What, Arrived]()
		{
			const ACheeseStrikeCharacter* Character = PlayTesting::Character(0);
			const FVector Now = Character != nullptr ? Character->GetActorLocation() : FVector::ZeroVector;
			Test->TestTrue(FString::Printf(TEXT("%s (at %.0f, %.0f, %.0f)"), *What, Now.X, Now.Y, Now.Z), Character != nullptr && Arrived(Now));
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCreameryHoldsTogether, "CheeseStrike.Play.TheCreameryHoldsTogether", MapTests::Flags)

bool FTheCreameryHoldsTogether::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace MapTests;
	FSession Session;
	Session.Level = Creamery;
	StartPlaying(Session);
	Then([this]()
	{
		UWorld* World = PlayTesting::World(0);
		// Starts for sixteen, and cheese in every structure its notes name, each block with the eating the design gives it.
		int32 Starts = 0;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			++Starts;
		}
		TestTrue(FString::Printf(TEXT("starts for sixteen (%d)"), Starts), Starts >= 16);
		TMap<FName, int32> Blocks;
		bool bAllWhole = true;
		FBox Extent(ForceInit);
		for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
		{
			Blocks.FindOrAdd(It->GetStructure())++;
			bAllWhole &= FMath::IsNearlyEqual(It->GetHealth(), CheeseStrikeRules::CheeseBlockHealth);
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetRootComponent() != nullptr && It->GetFolderPath().ToString().StartsWith(TEXT("Ground")))
			{
				Extent += It->GetComponentsBoundingBox();
			}
		}
		int32 Total = 0;
		for (const TPair<FName, int32>& Each : Blocks)
		{
			Total += Each.Value;
		}
		TestTrue(FString::Printf(TEXT("hundreds of blocks of cheese (%d)"), Total), Total >= 500);
		for (const TCHAR* Structure : {TEXT("WheelStack"), TEXT("HallSeal"), TEXT("Racks"), TEXT("CheeseBridge"), TEXT("DockWallGouda"), TEXT("DockWallMozza"), TEXT("PalletsGouda"), TEXT("PalletsMozza")})
		{
			TestTrue(FString::Printf(TEXT("the %s is there (%d)"), Structure, Blocks.FindRef(FName(Structure))), Blocks.FindRef(FName(Structure)) > 0);
		}
		TestTrue(TEXT("every block uneaten"), bAllWhole);
		const FVector Size = Extent.GetSize();
		TestTrue(FString::Printf(TEXT("about 130 by 100 m (%.0f by %.0f)"), Size.X / 100.0f, Size.Y / 100.0f), Size.X >= 12000.0f && Size.Y >= 9000.0f);

		// What the cheese does to the lines across the map, before the mice.
		TestEqual(TEXT("the Wheel Stack closes the yard's line from dock to dock"), StructureOf(FirstHit(FVector(-3300, 3550, 150), FVector(3300, 3550, 150))), FName(TEXT("WheelStack")));
		TestNotNull(TEXT("no line runs along the hall's north wall through the Squeeze"), FirstHit(FVector(-2300, 2200, 150), FVector(2300, 2200, 150)));
		TestEqual(TEXT("the Seals close the hall's north doors"), StructureOf(FirstHit(FVector(-1400, 2600, 150), FVector(-1400, 1000, 150))), FName(TEXT("HallSeal")));
		const AActor* HallLine = FirstHit(FVector(-3000, 0, 150), FVector(3000, 0, 150));
		TestTrue(TEXT("the press stops the line through the hall's main doors"), HallLine != nullptr && HallLine->GetActorLabel().StartsWith(TEXT("Hall_Press")));
		TestEqual(TEXT("a rack's shelves are full of cheese"), StructureOf(FirstHit(FVector(-2100, 850, 150), FVector(-1500, 850, 150))), FName(TEXT("Racks")));
		TestNull(TEXT("while the aisles between the racks are clear"), FirstHit(FVector(-1500, 1700, 150), FVector(-1500, 300, 150)));
	});
	// Every way up and across can be walked.
	Walk(this, TEXT("up the dock's ramp onto the platform"), FVector(-5200, -1600, 100), 180.0f, TEXT("3.0"), [](const FVector& At) { return At.Z > 200.0f && At.X < -5900.0f; });
	Walk(this, TEXT("up to the hall's mezzanine"), FVector(-2150, 0, 100), -90.0f, TEXT("3.5"), [](const FVector& At) { return At.Z > 450.0f; });
	Walk(this, TEXT("up to the annex's roof, turning onto it at the top"), FVector(-4800, -2070, 100), 0.0f, TEXT("hold Move 0 1\nwait 2.5\nrelease Move\nlook 90 0\nhold Move 0 1\nwait 1\nrelease Move\nwait 0.4"), [](const FVector& At) { return At.Z > 480.0f; });
	Walk(this, TEXT("out of the canal up its ramp"), FVector(-3000, -3200, -200), 180.0f, TEXT("6.5"), [](const FVector& At) { return At.Z > 50.0f && At.X < -5300.0f; });
	Walk(this, TEXT("out of the canal up its steps"), FVector(-1100, -2950, -200), 180.0f, TEXT("3.5"), [](const FVector& At) { return At.Z > 50.0f; });
	Walk(this, TEXT("over the Cheese Bridge"), FVector(0, -2500, 100), -90.0f, TEXT("3.0"), [](const FVector& At) { return At.Z > 50.0f && At.Y < -3700.0f; });
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHostPicksTheMap, "CheeseStrike.Play.TheHostPicksTheMap", MapTests::Flags)

bool FTheHostPicksTheMap::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace MapTests;
	static TArray<FString> LobbyBots;
	// A host and a joiner in the lobby on the shooter level, as a match is hosted.
	StartPlaying({2, TEXT("?KillLimit=5"), false, true});
	Then([this]()
	{
		ACheeseStrikeGameMode* Mode = World(0)->GetAuthGameMode<ACheeseStrikeGameMode>();
		if (!TestNotNull(TEXT("a game mode"), Mode))
		{
			return;
		}
		TestEqual(TEXT("the lobby is on the shooter level, and has it picked"), Mode->GetMapIndex(), Mode->CurrentMapIndex());
		Controller(0)->SetRulesFromMenu(5, 3.0f, 0);
		TestEqual(TEXT("the host picks the Creamery"), Mode->GetMapIndex(), 0);
		LobbyBots = GameState(0)->GetBotNames();
	});
	WaitUntil(TEXT("the joiner seeing the pick"), 3.0f, []() { return GameState(1) != nullptr && GameState(1)->GetMapIndex() == 0; });
	Then([]() { World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->AskToStart(true); });
	WaitUntil(TEXT("everyone on the Creamery with the match on"), 90.0f, []()
	{
		static int32 Calls = 0;
		if (++Calls % 60 == 0)
		{
			for (int32 Player = 0; Player < 2; ++Player)
			{
				UWorld* Here = World(Player);
				UE_LOG(LogTemp, Display, TEXT("Map test: player %d in %s, match %s, character %s"), Player, Here != nullptr ? *Here->GetOutermost()->GetName() : TEXT("no world"),
					GameState(Player) != nullptr ? (GameState(Player)->HasMatchStarted() ? TEXT("on") : TEXT("waiting")) : TEXT("no state"), Character(Player) != nullptr ? TEXT("yes") : TEXT("no"));
			}
		}
		for (int32 Player = 0; Player < 2; ++Player)
		{
			UWorld* Here = World(Player);
			if (Here == nullptr || CheeseStrikeRules::MapIndexFor(Here->GetOutermost()->GetName()) != 0 || GameState(Player) == nullptr || !GameState(Player)->HasMatchStarted() || Character(Player) == nullptr)
			{
				return false;
			}
		}
		return true;
	});
	Then([this]()
	{
		int32 Characters = 0;
		for (TActorIterator<ACheeseStrikeCharacter> It(World(0)); It; ++It)
		{
			++Characters;
		}
		TestEqual(TEXT("both players and the bots that fill in, sixteen in all"), Characters, CheeseStrikeRules::Teams * CheeseStrikeRules::TeamSize);
		TestTrue(TEXT("the bots are the ones the lobby showed"), GameState(0)->GetBotNames() == LobbyBots);
		TestEqual(TEXT("with the rules the lobby had"), GameState(0)->GetKillLimit(), 5);
		TestEqual(TEXT("and the map is still the Creamery for the next match"), World(0)->GetAuthGameMode<ACheeseStrikeGameMode>()->GetMapIndex(), 0);
	});
	StopPlaying();
	return true;
}

#endif
