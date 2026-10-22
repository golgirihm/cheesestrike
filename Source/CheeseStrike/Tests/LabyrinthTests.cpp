// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The Labyrinth played in the editor (see PlayTesting.h), as its notes say it is
// (docs/unreal/maps/labyrinth.md): its starts, structures and the lines its
// walls close; walks up its gallery, over a bridge and through a door; bots
// finding their way through its maze and fighting; the mice eating it from the
// outside in.

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
#include "Map/CheeseStrikeMouse.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"

namespace LabyrinthTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const TCHAR* const Labyrinth = TEXT("/Game/CheeseStrike/Maps/Lvl_Labyrinth");
	// The rings, as tools/art/build_labyrinth.py lays them out (cm from the middle).
	constexpr float Apothem = 3500.0f;
	constexpr float Aisle1 = 1150.0f;
	constexpr float Aisle2 = 1810.0f;
	constexpr float Aisle3 = 2450.0f;
	constexpr float GutterFrom = 2890.0f;

	/** The point `Radius` from the middle at `Degrees` (anticlockwise from +X), `Z` up. */
	FVector Polar(float Radius, float Degrees, float Z)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		return FVector(Radius * FMath::Cos(A), Radius * FMath::Sin(A), Z);
	}

	/** The point `V` out from the middle along the corner face whose normal is at `Phi`, and `U` along it. */
	FVector OnFace(float Phi, float U, float V, float Z)
	{
		const float A = FMath::DegreesToRadians(Phi);
		const FVector2D N(FMath::Cos(A), FMath::Sin(A));
		const FVector2D T(-N.Y, N.X);
		return FVector(N.X * V + T.X * U, N.Y * V + T.Y * U, Z);
	}

	/** What a straight line first hits in the host's world (no one's character), or null. */
	AActor* LabyrinthHit(const FVector& From, const FVector& To)
	{
		UWorld* World = PlayTesting::World(0);
		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("LabyrinthTests")), true);
		for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		return World != nullptr && World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params) ? Hit.GetActor() : nullptr;
	}

	FName LabyrinthStructure(const AActor* Actor)
	{
		const ACheeseStrikeCheese* Cheese = Cast<ACheeseStrikeCheese>(Actor);
		return Cheese != nullptr ? Cheese->GetStructure() : NAME_None;
	}

	/** Puts the player at `At` facing `Yaw`, walks forward for `Seconds`, then checks where they are with `Arrived`. */
	void LabyrinthWalk(FAutomationTestBase* Test, const FString& What, const FVector& At, float Yaw, float Seconds, TFunction<bool(const FVector&)> Arrived)
	{
		PlayTesting::PutAt(0, At, Yaw);
		PlayTesting::Play(0, FString::Printf(TEXT("hold Move 0 1\nwait %.2f\nrelease Move\nwait 0.4"), Seconds));
		PlayTesting::Then([Test, What, Arrived]()
		{
			const ACheeseStrikeCharacter* Character = PlayTesting::Character(0);
			const FVector Now = Character != nullptr ? Character->GetActorLocation() : FVector::ZeroVector;
			Test->TestTrue(FString::Printf(TEXT("%s (at %.0f, %.0f, %.0f)"), *What, Now.X, Now.Y, Now.Z), Character != nullptr && Arrived(Now));
		});
	}

	/** Which part of the cellar a point is in: the vault, the maze, the gutter, or a stair or room behind a corner. */
	const TCHAR* PartOf(const FVector& At)
	{
		const float R = FVector2D(At.X, At.Y).Size();
		for (int32 Face = 0; Face < 8; ++Face)
		{
			const float A = FMath::DegreesToRadians(45.0f * Face);
			if (At.X * FMath::Cos(A) + At.Y * FMath::Sin(A) > Apothem)
			{
				return TEXT("the stairs and rooms");
			}
		}
		return R < 930.0f ? TEXT("the vault") : R < GutterFrom ? TEXT("the maze") : TEXT("the gutter");
	}

	template <typename T>
	TArray<T*> Every()
	{
		TArray<T*> Found;
		if (UWorld* World = PlayTesting::World(0))
		{
			for (TActorIterator<T> It(World); It; ++It)
			{
				Found.Add(*It);
			}
		}
		return Found;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLabyrinthHoldsTogether, "CheeseStrike.Play.TheLabyrinthHoldsTogether", LabyrinthTests::Flags)

bool FTheLabyrinthHoldsTogether::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace LabyrinthTests;
	FSession Session;
	Session.Level = Labyrinth;
	StartPlaying(Session);
	Then([this]()
	{
		UWorld* Here = World(0);
		// Eight starts a side, in its stair, and free-for-all's through the maze.
		TMap<FName, int32> Tags;
		for (TActorIterator<APlayerStart> It(Here); It; ++It)
		{
			Tags.FindOrAdd(It->PlayerStartTag)++;
		}
		TestEqual(TEXT("eight Gouda starts"), Tags.FindRef(FName(TEXT("Gouda"))), 8);
		TestEqual(TEXT("eight Mozza starts"), Tags.FindRef(FName(TEXT("Mozza"))), 8);
		TestTrue(FString::Printf(TEXT("free-for-all's starts through the maze (%d)"), Tags.FindRef(FName(TEXT("Free")))), Tags.FindRef(FName(TEXT("Free"))) >= 14);
		// Its cheese: the maze's walls the mice eat, the spines and the Mother Wheel in wax that they never do.
		TMap<FName, int32> Blocks;
		TMap<FName, int32> Kept;
		int32 Edible = 0;
		for (TActorIterator<ACheeseStrikeCheese> It(Here); It; ++It)
		{
			Blocks.FindOrAdd(It->GetStructure())++;
			Kept.FindOrAdd(It->GetStructure()) += It->IsEdible() ? 0 : 1;
			Edible += It->IsEdible() ? 1 : 0;
		}
		TestTrue(FString::Printf(TEXT("six to nine hundred blocks to eat, for eight mice (%d)"), Edible), Edible >= 600 && Edible <= 900);
		for (const TCHAR* Side : {TEXT("West"), TEXT("North"), TEXT("South"), TEXT("East")})
		{
			for (int32 Ring = 1; Ring <= 3; ++Ring)
			{
				const FName Structure(*FString::Printf(TEXT("%sWall%d"), Side, Ring));
				TestTrue(FString::Printf(TEXT("the %s is there, and edible (%d)"), *Structure.ToString(), Blocks.FindRef(Structure)), Blocks.FindRef(Structure) > 0 && Kept.FindRef(Structure) == 0);
			}
		}
		for (const TCHAR* Structure : {TEXT("GoudaSpine"), TEXT("MozzaSpine"), TEXT("MotherWheel")})
		{
			TestTrue(FString::Printf(TEXT("the %s is all wax (%d of %d)"), Structure, Kept.FindRef(FName(Structure)), Blocks.FindRef(FName(Structure))),
				Blocks.FindRef(FName(Structure)) > 0 && Kept.FindRef(FName(Structure)) == Blocks.FindRef(FName(Structure)));
		}
		for (const TCHAR* Structure : {TEXT("ParapetGouda"), TEXT("ParapetMozza"), TEXT("BridgeGouda"), TEXT("BridgeMozza"), TEXT("GutterRacksGouda"), TEXT("GutterRacksMozza"),
			TEXT("BrineRoom"), TEXT("SmokeRoom"), TEXT("AmbulatoryStacks"), TEXT("VaultStacks"), TEXT("Plug3_135"), TEXT("Plug3_315")})
		{
			TestTrue(FString::Printf(TEXT("the %s is there (%d)"), Structure, Blocks.FindRef(FName(Structure))), Blocks.FindRef(FName(Structure)) > 0);
		}
		FBox Walls(ForceInit);
		for (TActorIterator<AActor> It(Here); It; ++It)
		{
			if (It->GetRootComponent() != nullptr && It->GetFolderPath().ToString().StartsWith(TEXT("Cellar/Walls")))
			{
				Walls += It->GetComponentsBoundingBox();
			}
		}
		TestTrue(FString::Printf(TEXT("an octagon about 70 m across (%.0f by %.0f)"), Walls.GetSize().X / 100.0f, Walls.GetSize().Y / 100.0f), Walls.GetSize().X >= 7000.0f && Walls.GetSize().Y >= 7000.0f);

		// The lines its walls close, before the mice.
		TestEqual(TEXT("the spine's wax wall hides it from the gutter"), LabyrinthStructure(LabyrinthHit(Polar(3200, 250, 150), Polar(Aisle3, 250, 150))), FName(TEXT("GoudaSpine")));
		TestEqual(TEXT("wall 2 stands between West 3 and West 2"), LabyrinthStructure(LabyrinthHit(Polar(Aisle3, 172, 150), Polar(Aisle2, 172, 150))), FName(TEXT("WestWall2")));
		TestEqual(TEXT("wall 3 stands between the gutter and North 3"), LabyrinthStructure(LabyrinthHit(Polar(3200, 127, 150), Polar(Aisle3, 127, 150))), FName(TEXT("NorthWall3")));
		TestEqual(TEXT("a wall of wheels closes aisle 3 between North 3 and West 3"), LabyrinthStructure(LabyrinthHit(Polar(Aisle3, 128, 150), Polar(Aisle3, 142, 150))), FName(TEXT("Plug3_135")));
		TestEqual(TEXT("the Mother Wheel stops the line between the spines through the vault"), LabyrinthStructure(LabyrinthHit(Polar(Aisle1, 225, 150), Polar(Aisle1, 45, 150))), FName(TEXT("MotherWheel")));
		const AActor* Gallery = LabyrinthHit(Polar(820, 200, 680), Polar(820, 200, 0));
		TestTrue(TEXT("the gallery is a floor over the vault's"), Gallery != nullptr && Gallery->GetActorLabel().StartsWith(TEXT("Gallery_")));
		// Nothing outside a side's stair sees a start in it: not the gutter in front of it, nor the spine's way in.
		int32 Seen = 0;
		int32 Looked = 0;
		for (TActorIterator<APlayerStart> It(Here); It; ++It)
		{
			const bool bGouda = It->PlayerStartTag == FName(TEXT("Gouda"));
			if (!bGouda && It->PlayerStartTag != FName(TEXT("Mozza")))
			{
				continue;
			}
			const float Phi = bGouda ? 225.0f : 45.0f;
			for (float U = -1300.0f; U <= 1300.0f; U += 260.0f)
			{
				for (float V : {2950.0f, 3200.0f, 3450.0f})
				{
					++Looked;
					if (LabyrinthHit(OnFace(Phi, U, V, 160.0f), It->GetActorLocation()) == nullptr)
					{
						++Seen;
						AddError(FString::Printf(TEXT("%s is seen from (%.0f, %.0f) outside"), *It->GetActorLabel(), U, V));
					}
				}
			}
		}
		TestEqual(FString::Printf(TEXT("no start seen from outside its stair (of %d lines)"), Looked), Seen, 0);
	});
	// Up a gallery stair, over a bridge to the Mother Wheel's top, and through a door into the Brine Room.
	{
		const FVector Foot = Polar(780, 340, 100);
		const FVector Top = Polar(780, 300, 100);
		const FVector Along = (Top - Foot).GetSafeNormal2D();
		LabyrinthWalk(this, TEXT("up the stair to the gallery"), Foot - Along * 80.0f, Along.Rotation().Yaw, 2.6f, [](const FVector& At) { return At.Z > 400.0f; });
	}
	LabyrinthWalk(this, TEXT("over the bridge to the Mother Wheel's top"), FVector(0, -780, 440), 90.0f, 1.3f, [](const FVector& At) { return At.Y > -280.0f && At.Z > 400.0f; });
	LabyrinthWalk(this, TEXT("through a door into the Brine Room"), OnFace(135, 600, 3150, 100), 135.0f, 2.0f, [](const FVector& At)
	{
		const float A = FMath::DegreesToRadians(135.0f);
		return At.X * FMath::Cos(A) + At.Y * FMath::Sin(A) > Apothem + 150.0f;
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsAndMiceInTheLabyrinth, "CheeseStrike.Play.BotsAndMiceInTheLabyrinth", LabyrinthTests::Flags)

bool FBotsAndMiceInTheLabyrinth::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace LabyrinthTests;
	// A player and fifteen bots, and the mice three seconds in.
	static TMap<FString, FVector> Spawned;
	static TMap<FString, float> Furthest;
	static TSet<FString> Fought;
	static TSet<FString> Parts;
	static TMap<TWeakObjectPtr<ACheeseStrikeCheese>, float> Cheese;
	static float StartedAt = 0.0f;
	Spawned.Reset();
	Furthest.Reset();
	Fought.Reset();
	Parts.Reset();
	Cheese.Reset();
	FSession Session;
	Session.Level = Labyrinth;
	Session.Options = TEXT("?Bots=15?KillLimit=50?Mice=1?MiceAt=3");
	StartPlaying(Session);
	WaitUntil(TEXT("fifteen bots"), 10.0f, []() { return Every<ACheeseStrikeBotController>().Num() == 15; });
	WaitUntil(TEXT("the navigation mesh"), 30.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		FNavLocation Spot;
		return Navigation != nullptr && !Navigation->IsNavigationBeingBuilt(World(0)) && Navigation->ProjectPointToNavigation(OnFace(225, 0, 4150, 100), Spot, FVector(100, 100, 300));
	});
	Then([this]()
	{
		// Every part of it can be walked to from Gouda's stair, by the navigation mesh the bots and the mice go by.
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		const FVector Stair = OnFace(225, 0, 4150, 100);
		struct FPlace { const TCHAR* Name; FVector At; };
		for (const FPlace& Place : {FPlace{TEXT("the vault"), FVector(600, 0, 100)}, FPlace{TEXT("the gallery"), Polar(770, 200, 440)}, FPlace{TEXT("the Mother Wheel's top"), FVector(0, 0, 440)},
			FPlace{TEXT("the ambulatory"), Polar(Aisle1, 180, 100)}, FPlace{TEXT("West 3"), Polar(Aisle3, 170, 100)}, FPlace{TEXT("North 3"), Polar(Aisle3, 125, 100)},
			FPlace{TEXT("South 2"), Polar(Aisle2, 300, 100)}, FPlace{TEXT("East 3"), Polar(Aisle3, 351, 100)}, FPlace{TEXT("the West Gutter"), FVector(-3200, 300, 100)},
			FPlace{TEXT("the Brine Room"), OnFace(135, 0, 4200, 100)}, FPlace{TEXT("the Smoke Room"), OnFace(315, 0, 3800, 100)}, FPlace{TEXT("Mozza's stair"), OnFace(45, 0, 4150, 100)}})
		{
			UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(World(0), Stair, Place.At);
			TestTrue(FString::Printf(TEXT("%s can be walked to from Gouda's stair (%s)"), Place.Name, Path == nullptr ? TEXT("no path") : Path->IsPartial() ? TEXT("partly") : TEXT("whole")),
				Path != nullptr && Path->IsValid() && !Path->IsPartial());
		}
		for (ACheeseStrikeCheese* Each : Every<ACheeseStrikeCheese>())
		{
			if (Each->IsEdible())
			{
				Cheese.Add(Each, FVector2D(Each->GetActorLocation()).Size());
			}
		}
		StartedAt = Now(0);
	});
	WaitUntil(TEXT("eight mice in"), 20.0f, []() { return Every<ACheeseStrikeMouse>().Num() == 8; });
	// The bots find their way through the maze to one another, and the mice eat.
	WaitUntil(TEXT("the bots roaming the cellar and killing, and the mice eating"), 120.0f, []()
	{
		int32 Kills = 0;
		for (ACheeseStrikeBotController* Bot : Every<ACheeseStrikeBotController>())
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
			const FString Name = State->GetPlayerName();
			if (!Spawned.Contains(Name))
			{
				Spawned.Add(Name, Body->GetActorLocation());
			}
			Furthest.FindOrAdd(Name) = FMath::Max(Furthest.FindRef(Name), FVector::Dist2D(Spawned[Name], Body->GetActorLocation()));
			if (Bot->SeesTarget())
			{
				Fought.Add(Name);
			}
			Parts.Add(PartOf(Body->GetActorLocation()));
		}
		int32 Moved = 0;
		for (const TPair<FString, float>& Each : Furthest)
		{
			Moved += Each.Value > 1000.0f ? 1 : 0;
		}
		int32 Eaten = 0;
		for (const TPair<TWeakObjectPtr<ACheeseStrikeCheese>, float>& Each : Cheese)
		{
			Eaten += Each.Key.IsValid() ? 0 : 1;
		}
		return Now(0) - StartedAt >= 40.0f && Moved >= 14 && Kills >= 8 && Fought.Num() >= 10 && Parts.Contains(TEXT("the vault")) && Parts.Contains(TEXT("the maze"))
			&& Parts.Contains(TEXT("the gutter")) && Eaten >= 20;
	});
	Then([this]()
	{
		int32 Kills = 0;
		for (ACheeseStrikeBotController* Bot : Every<ACheeseStrikeBotController>())
		{
			if (const ACheeseStrikePlayerState* State = Bot->GetPlayerState<ACheeseStrikePlayerState>())
			{
				Kills += State->GetKills();
				AddInfo(FString::Printf(TEXT("%s: %d kills, %d deaths, went %.0f m, %s"), *State->GetPlayerName(), State->GetKills(), State->GetDeaths(), Furthest.FindRef(State->GetPlayerName()) / 100.0f, Bot->Doing()));
			}
		}
		TestTrue(FString::Printf(TEXT("the bots killed one another (%d)"), Kills), Kills >= 8);
		TestTrue(FString::Printf(TEXT("most bots found someone to fight (%d)"), Fought.Num()), Fought.Num() >= 10);
		for (const TCHAR* Part : {TEXT("the vault"), TEXT("the maze"), TEXT("the gutter")})
		{
			TestTrue(FString::Printf(TEXT("bots went into %s"), Part), Parts.Contains(Part));
		}
		// The mice eat from the outside in: what they have eaten so far lies further out than the cheese as a whole.
		double EatenAt = 0.0, AllAt = 0.0;
		int32 Eaten = 0;
		for (const TPair<TWeakObjectPtr<ACheeseStrikeCheese>, float>& Each : Cheese)
		{
			AllAt += Each.Value;
			if (!Each.Key.IsValid())
			{
				EatenAt += Each.Value;
				++Eaten;
			}
		}
		AllAt /= FMath::Max(1, Cheese.Num());
		EatenAt /= FMath::Max(1, Eaten);
		TestTrue(FString::Printf(TEXT("the mice ate (%d blocks)"), Eaten), Eaten >= 20);
		TestTrue(FString::Printf(TEXT("from the outside in (what they ate %.1f m from the middle, the cheese %.1f m)"), EatenAt / 100.0, AllAt / 100.0), EatenAt > AllAt + 300.0);
	});
	StopPlaying();
	return true;
}

#endif
