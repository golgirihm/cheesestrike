// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play Crumb Kitchen in the editor (see PlayTesting.h): its
// structures and the lines they close, every way up and through walked, no
// line from the room into either side's starts, and bots fighting through it
// (docs/unreal/maps/kitchen.md).

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerState.h"
#include "Map/CheeseStrikeCheese.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"

namespace KitchenTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const TCHAR* const Kitchen = TEXT("/Game/CheeseStrike/Maps/Lvl_Kitchen");

	/** What a straight line first hits in the host's world (not the characters; not the cheese either, if `bOpen`, as after the mice), or null. */
	AActor* KitchenFirstHit(const FVector& From, const FVector& To, bool bOpen = false)
	{
		UWorld* World = PlayTesting::World(0);
		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("KitchenTests")), true);
		for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		if (bOpen)
		{
			for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
			{
				Params.AddIgnoredActor(*It);
			}
		}
		return World != nullptr && World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params) ? Hit.GetActor() : nullptr;
	}

	FName KitchenStructureOf(const AActor* Actor)
	{
		const ACheeseStrikeCheese* Cheese = Cast<ACheeseStrikeCheese>(Actor);
		return Cheese != nullptr ? Cheese->GetStructure() : NAME_None;
	}

	/** Checks that the line from `From` to `To` first meets cheese of `Structure`, naming what it met. */
	void KitchenLineMeets(FAutomationTestBase* Test, const FString& What, const FVector& From, const FVector& To, const TCHAR* Structure)
	{
		const AActor* Hit = KitchenFirstHit(From, To);
		Test->TestTrue(FString::Printf(TEXT("%s (it meets %s)"), *What, Hit != nullptr ? *Hit->GetActorLabel() : TEXT("nothing")), KitchenStructureOf(Hit) == FName(Structure));
	}

	/** Puts the player at `At` facing `Yaw`, plays `Script` (or walks forward for so many seconds, if it is a number), then checks where they are with `Arrived`. */
	void KitchenWalk(FAutomationTestBase* Test, const FString& What, const FVector& At, float Yaw, const FString& Script, TFunction<bool(const FVector&)> Arrived)
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

	/** Where a player could stand and look from: on the floor, the worktop and the table, in a grid, where nothing is. */
	TArray<FVector> KitchenLookouts(UWorld* World)
	{
		TArray<FVector> Spots;
		auto Free = [World](const FVector& At)
		{
			return !World->OverlapAnyTestByChannel(At, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(40.0f));
		};
		for (float X = -2500.0f; X <= 2500.0f; X += 250.0f)
		{
			for (float Y = -1950.0f; Y <= 1850.0f; Y += 250.0f)
			{
				Spots.Add(FVector(X, Y, 160.0f));
			}
			for (float Y : {1980.0f, 2200.0f, 2420.0f})
			{
				Spots.Add(FVector(X, Y, 610.0f));
			}
			if (FMath::Abs(X) <= 1250.0f)
			{
				for (float Y : {-1400.0f, -1100.0f, -800.0f})
				{
					Spots.Add(FVector(X, Y, 535.0f));
				}
			}
		}
		return Spots.FilterByPredicate(Free);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrumbKitchenHoldsTogether, "CheeseStrike.Play.CrumbKitchenHoldsTogether", KitchenTests::Flags)

bool FCrumbKitchenHoldsTogether::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace KitchenTests;
	FSession Session;
	Session.Level = Kitchen;
	StartPlaying(Session);
	Then([this]()
	{
		UWorld* World = PlayTesting::World(0);
		// Starts for sixteen, each side's tagged, and cheese in every structure its notes name.
		TMap<FString, int32> Starts;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Starts.FindOrAdd(It->PlayerStartTag.ToString())++;
		}
		TestTrue(FString::Printf(TEXT("eight starts for the Gouda (%d)"), Starts.FindRef(TEXT("Gouda"))), Starts.FindRef(TEXT("Gouda")) >= 8);
		TestTrue(FString::Printf(TEXT("eight starts for the Mozza (%d)"), Starts.FindRef(TEXT("Mozza"))), Starts.FindRef(TEXT("Mozza")) >= 8);
		TMap<FName, int32> Blocks;
		int32 Edible = 0;
		FBox Extent(ForceInit);
		for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
		{
			Blocks.FindOrAdd(It->GetStructure())++;
			Edible += It->IsEdible() ? 1 : 0;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetRootComponent() != nullptr && It->GetFolderPath().ToString().StartsWith(TEXT("Ground")))
			{
				Extent += It->GetComponentsBoundingBox();
			}
		}
		TestTrue(FString::Printf(TEXT("three hundred blocks the mice may eat (%d)"), Edible), Edible >= 300);
		for (const TCHAR* Structure : {TEXT("Wedge"), TEXT("WedgeRind"), TEXT("HollowPillar"), TEXT("CupboardSeal"), TEXT("BoardGouda"), TEXT("BoardMozza"), TEXT("CupboardGouda"),
			TEXT("CupboardMozza"), TEXT("CheddarGouda"), TEXT("CheddarMozza"), TEXT("CrumbsGouda"), TEXT("CrumbsMozza"), TEXT("UnderTableGouda"), TEXT("UnderTableMozza"), TEXT("TableCheese")})
		{
			TestTrue(FString::Printf(TEXT("the %s is there (%d)"), Structure, Blocks.FindRef(FName(Structure))), Blocks.FindRef(FName(Structure)) > 0);
		}
		const FVector Size = Extent.GetSize();
		TestTrue(FString::Printf(TEXT("about 70 by 46 m (%.0f by %.0f)"), Size.X / 100.0f, Size.Y / 100.0f), Size.X >= 7000.0f && Size.Y >= 4500.0f);

		// What the cheese does to the lines, before the mice.
		KitchenLineMeets(this, TEXT("the pillar in the wedge's hollow stops the line along the counter"), FVector(-1900, 1700, 120), FVector(1900, 1700, 120), TEXT("HollowPillar"));
		KitchenLineMeets(this, TEXT("the wedge's slope is cheese"), FVector(0, 1000, 800), FVector(0, 1000, 0), TEXT("Wedge"));
		KitchenLineMeets(this, TEXT("its sides are its rind, in wax"), FVector(-1500, 950, 100), FVector(0, 950, 100), TEXT("WedgeRind"));
		const AActor* Slot = KitchenFirstHit(FVector(400, 1700, 800), FVector(400, 1700, -50));
		TestTrue(FString::Printf(TEXT("a slot at the slope's top opens down into the hollow (%s)"), Slot != nullptr ? *Slot->GetActorLabel() : TEXT("nothing")), Slot != nullptr && Slot->GetActorLabel().StartsWith(TEXT("Floor")));
		KitchenLineMeets(this, TEXT("the seal of cheese stops the hole from Under the Sink into the Gouda cupboard"), FVector(-300, 2200, 100), FVector(-1000, 2200, 100), TEXT("CupboardSeal"));
		const AActor* Table = KitchenFirstHit(FVector(900, -800, 700), FVector(900, -800, 50));
		TestTrue(TEXT("the table top roofs the floor under it"), Table != nullptr && Table->GetActorLabel().StartsWith(TEXT("Table_Top")));
		KitchenLineMeets(this, TEXT("the board of cheese on the table is cover along its middle"), FVector(-900, -1120, 480), FVector(900, -1120, 480), TEXT("TableCheese"));
		KitchenLineMeets(this, TEXT("the cheese on the Gouda board stops the line along the worktop"), FVector(-1580, 2120, 520), FVector(-700, 2120, 520), TEXT("BoardGouda"));

		// No line from the room into either side's starts, before the mice or after them: the dresser and what stands
		// round it are hard cover.
		TArray<FVector> Spots = KitchenLookouts(World);
		AddInfo(FString::Printf(TEXT("%d spots to look from"), Spots.Num()));
		for (bool bOpen : {false, true})
		{
			int32 Seen = 0;
			FString Example;
			for (TActorIterator<APlayerStart> It(World); It; ++It)
			{
				const FString Tag = It->PlayerStartTag.ToString();
				if (Tag != TEXT("Gouda") && Tag != TEXT("Mozza"))
				{
					continue;
				}
				const float Side = Tag == TEXT("Gouda") ? -1.0f : 1.0f;
				for (const FVector& Spot : Spots)
				{
					// The room outside that side's end: from its dresser's front on.
					if (Spot.X * Side > 2400.0f)
					{
						continue;
					}
					const FVector Target = It->GetActorLocation() + FVector(0, 0, 40);
					if (KitchenFirstHit(Spot, Target, bOpen) == nullptr && KitchenFirstHit(Spot, Target - FVector(0, 0, 80), bOpen) == nullptr)
					{
						++Seen;
						if (Seen <= 6)
						{
							Example += FString::Printf(TEXT("from %.0f, %.0f, %.0f to %s; "), Spot.X, Spot.Y, Spot.Z, *It->GetActorLabel());
						}
					}
				}
			}
			TestEqual(FString::Printf(TEXT("no line into a side's starts %s (%s)"), bOpen ? TEXT("once the cheese is eaten") : TEXT("before the mice"), *Example), Seen, 0);
		}
	});
	// Every way up and through can be walked.
	KitchenWalk(this, TEXT("up the Gouda cookbooks onto the worktop"), FVector(-2190, 1000, 100), 90.0f, TEXT("3.0"), [](const FVector& At) { return At.Z > 500.0f && At.Y > 1900.0f; });
	KitchenWalk(this, TEXT("up the Mozza cookbooks onto the worktop"), FVector(2190, 1000, 100), 90.0f, TEXT("3.0"), [](const FVector& At) { return At.Z > 500.0f && At.Y > 1900.0f; });
	KitchenWalk(this, TEXT("up the wedge's slope onto the worktop"), FVector(0, 100, 100), 90.0f, TEXT("4.3"), [](const FVector& At) { return At.Z > 500.0f && At.Y > 1900.0f; });
	KitchenWalk(this, TEXT("along the hollow and through its door under the sink"), FVector(-1000, 1580, 100), 0.0f,
		TEXT("hold Move 0 1\nwait 1.45\nrelease Move\nlook 90 0\nhold Move 0 1\nwait 1.8\nrelease Move\nwait 0.4"), [](const FVector& At) { return At.Y > 1960.0f && FMath::Abs(At.X) < 700.0f && At.Z < 300.0f; });
	KitchenWalk(this, TEXT("through the Gouda cupboard's door"), FVector(-1350, 1300, 100), 90.0f, TEXT("2.2"), [](const FVector& At) { return At.Y > 1960.0f && At.Z < 300.0f; });
	KitchenWalk(this, TEXT("up the books onto the table"), FVector(-2100, -1100, 100), 0.0f, TEXT("2.5"), [](const FVector& At) { return At.Z > 420.0f && At.X > -1300.0f; });
	KitchenWalk(this, TEXT("down into the sink and out of it"), FVector(-620, 2330, 550), 0.0f, TEXT("3.0"), [](const FVector& At) { return At.Z > 500.0f && At.X > 560.0f; });
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsPlayCrumbKitchen, "CheeseStrike.Play.BotsPlayCrumbKitchen", KitchenTests::Flags)

bool FBotsPlayCrumbKitchen::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace KitchenTests;
	// A player and fifteen bots: a full match.
	static TSet<FString> Where;
	static TSet<FString> Fought;
	static float StartedAt = 0.0f;
	Where.Reset();
	Fought.Reset();
	FSession Session;
	Session.Level = Kitchen;
	Session.Options = TEXT("?Bots=15?KillLimit=50");
	StartPlaying(Session);
	auto KitchenBots = []()
	{
		TArray<ACheeseStrikeBotController*> Found;
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			Found.Add(*It);
		}
		return Found;
	};
	WaitUntil(TEXT("fifteen bots"), 10.0f, [KitchenBots]() { return KitchenBots().Num() == 15; });
	// Every place can be walked to from a side's starts, by the navigation mesh the bots go by.
	WaitUntil(TEXT("the navigation mesh"), 20.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		FNavLocation Spot;
		return Navigation != nullptr && !Navigation->IsNavigationBeingBuilt(World(0)) && Navigation->ProjectPointToNavigation(FVector(-3020, 200, 100), Spot, FVector(100, 100, 300));
	});
	Then([this]()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		const FVector Start(-3020, 200, 100);
		struct FPlace { const TCHAR* Name; FVector At; };
		for (const FPlace& Place : {FPlace{TEXT("the worktop"), FVector(-1700, 2000, 470)}, FPlace{TEXT("the sink"), FVector(0, 2250, 400)},
			FPlace{TEXT("the other end of the worktop"), FVector(1700, 2000, 470)}, FPlace{TEXT("the wedge's slope"), FVector(0, 900, 200)},
			FPlace{TEXT("the hollow"), FVector(-400, 1700, 20)}, FPlace{TEXT("under the sink"), FVector(250, 2050, 40)}, FPlace{TEXT("the Gouda cupboard"), FVector(-1350, 2150, 40)},
			FPlace{TEXT("the table top"), FVector(0, -1400, 400)}, FPlace{TEXT("under the table"), FVector(-600, -1100, 20)}, FPlace{TEXT("the other side's starts"), FVector(3020, 200, 100)}})
		{
			UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(World(0), Start, Place.At);
			TestTrue(FString::Printf(TEXT("%s can be walked to from the Gouda starts (%s)"), Place.Name, Path == nullptr ? TEXT("no path") : Path->IsPartial() ? TEXT("partly") : TEXT("whole")),
				Path != nullptr && Path->IsValid() && !Path->IsPartial());
		}
		StartedAt = Now(0);
	});
	// Long enough to leave the ends, spread through the kitchen and fight there.
	WaitUntil(TEXT("the bots up on the worktop, on the floor and at the table, finding each other and killing"), 90.0f, [KitchenBots]()
	{
		int32 Kills = 0;
		for (ACheeseStrikeBotController* Bot : KitchenBots())
		{
			const ACheeseStrikePlayerState* State = Bot->GetPlayerState<ACheeseStrikePlayerState>();
			const ACheeseStrikeCharacter* Body = Cast<ACheeseStrikeCharacter>(Bot->GetPawn());
			if (State == nullptr)
			{
				continue;
			}
			Kills += State->GetKills();
			if (Body == nullptr || !Body->IsAlive())
			{
				continue;
			}
			if (Bot->SeesTarget())
			{
				Fought.Add(State->GetPlayerName());
			}
			const FVector At = Body->GetActorLocation();
			Where.Add(At.Z > 420.0f && At.Y > 1900.0f ? TEXT("the worktop") : At.Z > 380.0f && At.Y < -600.0f ? TEXT("the table top")
				: At.Y > 1900.0f ? TEXT("the cupboards") : At.Y < -600.0f && At.Y > -1600.0f && FMath::Abs(At.X) < 1300.0f ? TEXT("under the table") : TEXT("the floor"));
		}
		return Now(0) - StartedAt >= 30.0f && Kills >= 8 && Fought.Num() >= 10 && Where.Contains(TEXT("the worktop")) && Where.Contains(TEXT("the floor"))
			&& (Where.Contains(TEXT("the table top")) || Where.Contains(TEXT("under the table")));
	});
	Then([this]()
	{
		FString Places;
		for (const FString& Each : Where)
		{
			Places += Each + TEXT("; ");
		}
		AddInfo(FString::Printf(TEXT("bots went to: %s"), *Places));
		TestTrue(TEXT("bots went up onto the worktop"), Where.Contains(TEXT("the worktop")));
		TestTrue(TEXT("and fought across the floor"), Where.Contains(TEXT("the floor")));
		TestTrue(FString::Printf(TEXT("most bots found someone to fight (%d)"), Fought.Num()), Fought.Num() >= 10);
	});
	StopPlaying();
	return true;
}

#endif
