// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Rind Market played in the editor (see PlayTesting.h), walked over and looked
// across as its notes say (docs/unreal/maps/market.md): what stands in it, the
// lines its stone and timber close for good, whatever the mice eat, every way
// up and across, and the ways the bots and mice go.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "CheeseStrikeCharacter.h"
#include "Map/CheeseStrikeCheese.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"

namespace MarketTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const TCHAR* const Market = TEXT("/Game/CheeseStrike/Maps/Lvl_RindMarket");

	/** What a straight line first hits in the host's world, or null; past the cheese too, if `bPastCheese` (as it will be once the mice have eaten it). */
	AActor* MarketHit(const FVector& From, const FVector& To, bool bPastCheese = false)
	{
		UWorld* World = PlayTesting::World(0);
		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("MarketTests")), true);
		for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		if (bPastCheese)
		{
			for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
			{
				Params.AddIgnoredActor(*It);
			}
		}
		return World != nullptr && World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params) ? Hit.GetActor() : nullptr;
	}

	FName MarketStructureOf(const AActor* Actor)
	{
		const ACheeseStrikeCheese* Cheese = Cast<ACheeseStrikeCheese>(Actor);
		return Cheese != nullptr ? Cheese->GetStructure() : NAME_None;
	}

	/** Puts the player at `At` facing `Yaw`, walks forward for `Seconds`, then checks where they are with `Arrived`. */
	void MarketWalk(FAutomationTestBase* Test, const FString& What, const FVector& At, float Yaw, float Seconds, TFunction<bool(const FVector&)> Arrived)
	{
		PlayTesting::PutAt(0, At, Yaw);
		PlayTesting::Play(0, FString::Printf(TEXT("hold Move 0 1\nwait %.1f\nrelease Move\nwait 0.4"), Seconds));
		PlayTesting::Then([Test, What, Arrived]()
		{
			const ACheeseStrikeCharacter* Character = PlayTesting::Character(0);
			const FVector Now = Character != nullptr ? Character->GetActorLocation() : FVector::ZeroVector;
			Test->TestTrue(FString::Printf(TEXT("%s (at %.0f, %.0f, %.0f)"), *What, Now.X, Now.Y, Now.Z), Character != nullptr && Arrived(Now));
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRindMarketHoldsTogether, "CheeseStrike.Play.RindMarketHoldsTogether", MarketTests::Flags)

bool FRindMarketHoldsTogether::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace MarketTests;
	FSession Session;
	Session.Level = Market;
	StartPlaying(Session);
	Then([this]()
	{
		UWorld* World = PlayTesting::World(0);
		// Starts for sixteen, eight a side, and more through the town for free-for-all.
		TMap<FName, int32> Starts;
		TArray<APlayerStart*> TeamStarts;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Starts.FindOrAdd(It->PlayerStartTag)++;
			if (It->PlayerStartTag != FName(TEXT("Free")))
			{
				TeamStarts.Add(*It);
			}
		}
		TestEqual(TEXT("eight Gouda starts"), Starts.FindRef(FName(TEXT("Gouda"))), 8);
		TestEqual(TEXT("eight Mozza starts"), Starts.FindRef(FName(TEXT("Mozza"))), 8);
		TestTrue(FString::Printf(TEXT("free-for-all starts through the town (%d)"), Starts.FindRef(FName(TEXT("Free")))), Starts.FindRef(FName(TEXT("Free"))) >= 10);
		// Cheese in every structure the notes name, none of it in the clock tower.
		TMap<FName, int32> Blocks;
		int32 Total = 0;
		bool bAllWhole = true;
		bool bTowerBare = true;
		for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
		{
			Blocks.FindOrAdd(It->GetStructure())++;
			++Total;
			bAllWhole &= FMath::IsNearlyEqual(It->GetHealth(), CheeseStrikeRules::CheeseBlockHealth);
			const FVector At = It->GetActorLocation();
			bTowerBare &= FMath::Abs(At.X) > 600.0f || FMath::Abs(At.Y) > 600.0f;
		}
		TestTrue(FString::Printf(TEXT("a thousand blocks of cheese, under twelve hundred (%d)"), Total), Total >= 1000 && Total <= 1200);
		TestTrue(TEXT("every block uneaten"), bAllWhole);
		TestTrue(TEXT("no cheese in the clock tower"), bTowerBare);
		for (const TCHAR* Team : {TEXT("Gouda"), TEXT("Mozza")})
		{
			for (const TCHAR* Structure : {TEXT("ArcadeHouse"), TEXT("BalconyHouse"), TEXT("Inn"), TEXT("Bakery"), TEXT("Gallery"), TEXT("MainBridge"), TEXT("Store"), TEXT("Tunnel"),
				TEXT("DryingRack"), TEXT("YardCheese"), TEXT("Stall")})
			{
				const FName Name(FString(Structure) + Team + (FString(Structure) == TEXT("Stall") ? TEXT("0") : TEXT("")));
				TestTrue(FString::Printf(TEXT("the %s is there (%d)"), *Name.ToString(), Blocks.FindRef(Name)), Blocks.FindRef(Name) > 0);
			}
		}
		TestTrue(TEXT("the Clock Passage is there"), Blocks.FindRef(FName(TEXT("ClockPassage"))) > 0);
		TestTrue(TEXT("the Snicket's bridge is there"), Blocks.FindRef(FName(TEXT("SnicketBridge"))) > 0);
		FBox Extent(ForceInit);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetRootComponent() != nullptr && It->GetFolderPath().ToString().StartsWith(TEXT("Ground")))
			{
				Extent += It->GetComponentsBoundingBox();
			}
		}
		const FVector Size = Extent.GetSize();
		TestTrue(FString::Printf(TEXT("about 104 by 74 m (%.0f by %.0f)"), Size.X / 100.0f, Size.Y / 100.0f), Size.X >= 10400.0f && Size.Y >= 7400.0f && Size.X < 11000.0f);

		// Lines that stone and timber close for good, whatever the mice eat.
		bool bHighStreet = true;
		for (float Y = 2950.0f; Y <= 3650.0f; Y += 50.0f)
		{
			bHighStreet &= MarketHit(FVector(-4000, Y, 150), FVector(4000, Y, 150), true) != nullptr;
		}
		TestTrue(TEXT("no line runs along High Street from gate to gate"), bHighStreet);
		bool bAlleys = true;
		for (float Y = -3650.0f; Y <= -2750.0f; Y += 50.0f)
		{
			bAlleys &= MarketHit(FVector(-4000, Y, 150), FVector(4000, Y, 150), true) != nullptr;
		}
		TestTrue(TEXT("nor along the alleys"), bAlleys);
		const AActor* MainLine = MarketHit(FVector(-3400, 0, 150), FVector(3400, 0, 150), true);
		TestTrue(TEXT("the clock tower stops the line down Main Street from street to street"), MainLine != nullptr && MainLine->GetActorLabel().StartsWith(TEXT("Tower")));
		const AActor* ThroughTower = MarketHit(FVector(0, 2500, 150), FVector(0, -2500, 150), true);
		TestTrue(TEXT("the pier between the tower's doors stops the line from the Clock Passage to the Snicket"), ThroughTower != nullptr && ThroughTower->GetActorLabel() == TEXT("Tower_Pier"));
		// Nobody sees into a gate yard: not from the lanes in front of the gates, nor from the belfry.
		int32 SeenInto = 0;
		for (const APlayerStart* Start : TeamStarts)
		{
			const FVector Head = Start->GetActorLocation() + FVector(0, 0, 60);
			const float Side = FMath::Sign(Head.X);
			for (const FVector& From : {FVector(Side * 3700, 3300, 160), FVector(Side * 3400, 0, 160), FVector(Side * 3700, -3300, 160), FVector(Side * 3700, 1800, 160),
				FVector(500, 500, 860), FVector(-500, 500, 860), FVector(500, -500, 860), FVector(-500, -500, 860)})
			{
				if (MarketHit(From, Head, true) == nullptr)
				{
					AddError(FString::Printf(TEXT("%s is seen from (%.0f, %.0f, %.0f)"), *Start->GetActorLabel(), From.X, From.Y, From.Z));
					++SeenInto;
				}
			}
		}
		TestEqual(TEXT("no start is seen from the lanes or the belfry"), SeenInto, 0);
		// The houses, before the mice: walls that hide, windows that look out, the gallery's parapet.
		TestEqual(TEXT("the balcony house's wall hides its ground floor from the square"), MarketStructureOf(MarketHit(FVector(-1000, -1200, 150), FVector(-1000, -2000, 150))), FName(TEXT("BalconyHouseGouda")));
		TestNull(TEXT("the Inn's upstairs window looks over the square"), MarketHit(FVector(-2000, 762.5, 520), FVector(-1000, 762.5, 520)));
		TestEqual(TEXT("the gallery's parapet hides who crouches behind it from the square"), MarketStructureOf(MarketHit(FVector(-1000, 1000, 150), FVector(-1000, 1700, 450))), FName(TEXT("GalleryGouda")));
		TestEqual(TEXT("a stall's counter hides who crouches behind it from the tower"), MarketStructureOf(MarketHit(FVector(-600, 0, 90), FVector(-1300, 0, 90))), FName(TEXT("StallGouda0")));
	});
	// The bots and the mice can go everywhere from a gate, by the navigation mesh.
	WaitUntil(TEXT("the navigation mesh"), 30.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		FNavLocation Spot;
		return Navigation != nullptr && !Navigation->IsNavigationBeingBuilt(World(0)) && Navigation->ProjectPointToNavigation(FVector(-4850, -700, 100), Spot, FVector(100, 100, 300));
	});
	Then([this]()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		const FVector Gate(-4850, -700, 100);
		struct FPlace { const TCHAR* Name; FVector At; };
		for (const FPlace& Place : {FPlace{TEXT("the other gate"), FVector(4850, 700, 100)}, FPlace{TEXT("the tower's landing"), FVector(0, 300, 400)},
			FPlace{TEXT("the belfry"), FVector(-300, 0, 750)}, FPlace{TEXT("the gallery"), FVector(-1000, 1650, 400)}, FPlace{TEXT("the arcade house's upper floor"), FVector(-1200, 2400, 400)},
			FPlace{TEXT("the Snicket's bridge"), FVector(0, -1962, 400)}, FPlace{TEXT("the bridge over Main Street"), FVector(-2600, 0, 400)}, FPlace{TEXT("the Inn's upper floor"), FVector(-2200, 900, 400)},
			FPlace{TEXT("High Street by the well house"), FVector(0, 3600, 100)}, FPlace{TEXT("the alleys by the wash house"), FVector(0, -2900, 100)},
			FPlace{TEXT("the store"), FVector(-2700, -2100, 100)}, FPlace{TEXT("the covered way"), FVector(-2700, -3500, 100)}, FPlace{TEXT("the balcony"), FVector(-1200, -1420, 400)}})
		{
			UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(World(0), Gate, Place.At);
			const bool bWhole = Path != nullptr && Path->IsValid() && !Path->IsPartial();
			const FVector End = Path != nullptr && Path->PathPoints.Num() > 0 ? Path->PathPoints.Last() : FVector::ZeroVector;
			TestTrue(FString::Printf(TEXT("%s can be walked to from a gate (%s, to %.0f, %.0f, %.0f)"), Place.Name, Path == nullptr ? TEXT("no path") : Path->IsPartial() ? TEXT("partly") : TEXT("whole"), End.X, End.Y, End.Z), bWhole);
		}
	});
	// Every way up and across can be walked.
	MarketWalk(this, TEXT("up the tower's first flight to its landing"), FVector(-315, -340, 100), 90.0f, 3.0f, [](const FVector& At) { return At.Z > 400.0f && At.Y > 200.0f; });
	MarketWalk(this, TEXT("up the tower's second flight to the belfry"), FVector(315, 300, 450), -90.0f, 3.0f, [](const FVector& At) { return At.Z > 750.0f; });
	MarketWalk(this, TEXT("up the arcade house's stairs"), FVector(-380, 2790, 100), -90.0f, 3.0f, [](const FVector& At) { return At.Z > 400.0f; });
	MarketWalk(this, TEXT("up the stairs from High Street"), FVector(-1450, 3005, 100), 0.0f, 3.0f, [](const FVector& At) { return At.Z > 400.0f; });
	MarketWalk(this, TEXT("out of the arcade house onto the gallery"), FVector(-662.5, 2100, 450), -90.0f, 1.5f, [](const FVector& At) { return At.Y < 1750.0f && At.Z > 400.0f; });
	MarketWalk(this, TEXT("over the Snicket's bridge from house to house"), FVector(-500, -1962.5, 450), 0.0f, 2.0f, [](const FVector& At) { return At.X > 350.0f && At.Z > 400.0f; });
	MarketWalk(this, TEXT("over the bridge from the Inn to the Bakery"), FVector(-2600, 600, 450), -90.0f, 2.8f, [](const FVector& At) { return At.Y < -400.0f && At.Z > 400.0f; });
	MarketWalk(this, TEXT("through the Clock Passage into the square"), FVector(0, 3000, 100), -90.0f, 3.0f, [](const FVector& At) { return At.Y < 1700.0f && At.Z < 200.0f; });
	StopPlaying();
	return true;
}

#endif
