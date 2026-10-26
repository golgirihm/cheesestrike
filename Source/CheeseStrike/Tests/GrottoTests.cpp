// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Gruyère Grotto played in the editor (see PlayTesting.h), as its notes say
// (docs/unreal/maps/grotto.md): its levels and decks, the lines its rock
// stops whatever the mice eat, the mice's ways in, a walk up
// and down every flight and over the bridges; a deck's slab coming down whole
// onto the level below when its pillar is eaten, safely for whoever is under
// it or on it; and bots finding their way to every level and fighting there.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
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

namespace GrottoTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const TCHAR* const Grotto = TEXT("/Game/CheeseStrike/Maps/Lvl_Grotto");
	/** The levels' floors (tools/art/build_grotto.py): the cave's floor, the Ledge, the Pulpit and the hollows, and the galleries. */
	constexpr float L1 = 400.0f;
	constexpr float L2 = 800.0f;

	TArray<ACheeseStrikeCheese*> AllCheese()
	{
		TArray<ACheeseStrikeCheese*> Found;
		if (UWorld* World = PlayTesting::World(0))
		{
			for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					Found.Add(*It);
				}
			}
		}
		return Found;
	}

	ACheeseStrikeCheese* CheeseLabelled(const FString& Label)
	{
		for (ACheeseStrikeCheese* Each : AllCheese())
		{
			if (Each->GetActorLabel() == Label)
			{
				return Each;
			}
		}
		return nullptr;
	}

	/** What a straight line first hits in the host's world, passing through every player and, if `bThroughCheese`, all the cheese: what is left once the mice have eaten it. */
	AActor* GrottoHit(const FVector& From, const FVector& To, bool bThroughCheese)
	{
		UWorld* World = PlayTesting::World(0);
		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("GrottoTests")), true);
		for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		if (bThroughCheese)
		{
			for (ACheeseStrikeCheese* Each : AllCheese())
			{
				Params.AddIgnoredActor(Each);
			}
		}
		return World != nullptr && World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params) ? Hit.GetActor() : nullptr;
	}

	FString LabelOf(const AActor* Actor)
	{
		return Actor != nullptr ? Actor->GetActorLabel() : FString(TEXT("nothing"));
	}

	/** The structure of the cheese a line first hits, or none if it first hits something else or nothing. */
	FName StructureHit(const FVector& From, const FVector& To)
	{
		const ACheeseStrikeCheese* Cheese = Cast<ACheeseStrikeCheese>(GrottoHit(From, To, false));
		return Cheese != nullptr ? Cheese->GetStructure() : NAME_None;
	}

	/** What blocking a player the character stands inside (a hair smaller than its capsule), or empty if it is free. */
	FString InsideOf(const ACheeseStrikeCharacter* Character)
	{
		UWorld* World = PlayTesting::World(0);
		const UCapsuleComponent* Capsule = Character != nullptr ? Character->GetCapsuleComponent() : nullptr;
		if (World == nullptr || Capsule == nullptr)
		{
			return TEXT("no character");
		}
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GrottoInside), false, Character);
		World->OverlapMultiByChannel(Overlaps, Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), ECC_Pawn,
			FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 3.0f, Capsule->GetScaledCapsuleHalfHeight() - 3.0f), Params);
		for (const FOverlapResult& Each : Overlaps)
		{
			const UPrimitiveComponent* Component = Each.GetComponent();
			if (Component != nullptr && Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block && Cast<APawn>(Component->GetOwner()) == nullptr)
			{
				return LabelOf(Component->GetOwner());
			}
		}
		return FString();
	}

	/** Puts the player at `At` facing `Yaw`, plays `Script` (or walks forward for so many seconds, if it is a number), then checks where they are with `Arrived`. */
	void WalkThrough(FAutomationTestBase* Test, const FString& What, const FVector& At, float Yaw, const FString& Script, TFunction<bool(const FVector&)> Arrived)
	{
		PlayTesting::PutAt(0, At, Yaw);
		PlayTesting::Play(0, Script.IsNumeric() ? FString::Printf(TEXT("hold Move 0 1\nwait %.2f\nrelease Move\nwait 0.4"), FCString::Atof(*Script) * PlayTesting::WalkTimeScale) : Script);
		PlayTesting::Then([Test, What, Arrived]()
		{
			const ACheeseStrikeCharacter* Character = PlayTesting::Character(0);
			const FVector Now = Character != nullptr ? Character->GetActorLocation() : FVector::ZeroVector;
			Test->TestTrue(FString::Printf(TEXT("%s (at %.0f, %.0f, %.0f)"), *What, Now.X, Now.Y, Now.Z), Character != nullptr && Arrived(Now));
		});
	}

	bool NavigationReady()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(PlayTesting::World(0));
		return Navigation != nullptr && Navigation->GetDefaultNavDataInstance() != nullptr && !Navigation->IsNavigationBeingBuilt(PlayTesting::World(0));
	}

	/** Which level a height is on: 0 the floor, 1 the middle, 2 the galleries. */
	int32 LevelAt(float Z)
	{
		return Z > L2 - 100.0f ? 2 : Z > L1 - 100.0f ? 1 : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheGrottoHoldsTogether, "CheeseStrike.Play.TheGrottoHoldsTogether", GrottoTests::Flags)

bool FTheGrottoHoldsTogether::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace GrottoTests;
	FSession Session;
	Session.Level = Grotto;
	StartPlaying(Session);
	Then([this]()
	{
		UWorld* Here = World(0);
		// Starts for sixteen, eight a side in its hollow at the middle level, and more through every level for free-for-all.
		TMap<FName, TArray<APlayerStart*>> Starts;
		for (TActorIterator<APlayerStart> It(Here); It; ++It)
		{
			Starts.FindOrAdd(It->PlayerStartTag).Add(*It);
		}
		TestEqual(TEXT("eight Gouda starts"), Starts.FindRef(FName(TEXT("Gouda"))).Num(), 8);
		TestEqual(TEXT("eight Mozza starts"), Starts.FindRef(FName(TEXT("Mozza"))).Num(), 8);
		TSet<int32> FreeLevels;
		for (const APlayerStart* Start : Starts.FindRef(FName(TEXT("Free"))))
		{
			FreeLevels.Add(LevelAt(Start->GetActorLocation().Z - 100.0f));
		}
		TestEqual(TEXT("free-for-all starts on all three levels"), FreeLevels.Num(), 3);
		for (const TCHAR* Team : {TEXT("Gouda"), TEXT("Mozza")})
		{
			for (const APlayerStart* Start : Starts.FindRef(FName(Team)))
			{
				TestEqual(FString::Printf(TEXT("%s is in a hollow at the middle level"), *Start->GetActorLabel()), LevelAt(Start->GetActorLocation().Z - 100.0f), 1);
				TestTrue(FString::Printf(TEXT("%s is at its side's end"), *Start->GetActorLabel()), FMath::Abs(Start->GetActorLocation().X) > 3600.0f && (Start->GetActorLocation().X < 0.0f) == (FString(Team) == TEXT("Gouda")));
			}
		}
		// Its size, from its ground and walls.
		FBox Extent(ForceInit);
		for (TActorIterator<AActor> It(Here); It; ++It)
		{
			if (It->GetRootComponent() != nullptr && It->GetFolderPath().ToString().StartsWith(TEXT("Ground/Boundary")))
			{
				Extent += It->GetComponentsBoundingBox();
			}
		}
		TestTrue(FString::Printf(TEXT("about 88 by 56 m inside its walls (%.0f by %.0f with them)"), Extent.GetSize().X / 100.0f, Extent.GetSize().Y / 100.0f), Extent.GetSize().X >= 9000.0f && Extent.GetSize().Y >= 6000.0f);

		// The cheese: every structure there, whole.
		TMap<FName, int32> Blocks;
		for (ACheeseStrikeCheese* Each : AllCheese())
		{
			Blocks.FindOrAdd(Each->GetStructure())++;
			TestTrue(FString::Printf(TEXT("%s is whole"), *Each->GetActorLabel()), FMath::IsNearlyEqual(Each->GetHealth(), CheeseStrikeRules::CheeseBlockHealth));
		}
		for (const TCHAR* Structure : {TEXT("GalleryDeckGouda"), TEXT("GalleryDeckMozza"), TEXT("LedgeDeckGouda"), TEXT("LedgeDeckMozza"), TEXT("RindTunnel"), TEXT("NorthSpan"), TEXT("SouthSpan"), TEXT("CheeseStoreGouda"), TEXT("CheeseStoreMozza"),
			TEXT("FloorGouda"), TEXT("FloorMozza"), TEXT("HollowGouda"), TEXT("HollowMozza"), TEXT("PulpitEdge"), TEXT("WalksGouda"), TEXT("WalksMozza")})
		{
			TestTrue(FString::Printf(TEXT("the %s is there (%d)"), Structure, Blocks.FindRef(FName(Structure))), Blocks.FindRef(FName(Structure)) > 0);
		}
		// Every slab of the decks stands on one pillar of cheese under its middle, which holds it up.
		int32 Slabs = 0;
		for (ACheeseStrikeCheese* Slab : AllCheese())
		{
			const FString Label = Slab->GetActorLabel();
			if (!Label.Contains(TEXT("_Slab_")))
			{
				continue;
			}
			++Slabs;
			const ACheeseStrikeCheese* Pillar = CheeseLabelled(Label.Replace(TEXT("_Slab_"), TEXT("_Pillar_")));
			const FBox Box = Slab->GetComponentsBoundingBox();
			const FBox Under = Pillar != nullptr ? Pillar->GetComponentsBoundingBox() : FBox(ForceInit);
			TestTrue(FString::Printf(TEXT("%s stands on its pillar, under its middle"), *Label), Pillar != nullptr && FMath::Abs(Under.Max.Z - Box.Min.Z) < 2.0f && FVector::Dist2D(Under.GetCenter(), Box.GetCenter()) < 5.0f && Under.Min.Z < 5.0f);
			TestTrue(FString::Printf(TEXT("%s is held up"), *Label), Slab->IsHeld());
		}
		TestTrue(FString::Printf(TEXT("the decks are slabs (%d)"), Slabs), Slabs >= 40);
		TestEqual(TEXT("the gallery's deck is cheese, 8 m up"), StructureHit(FVector(-2800, 1700, 1100), FVector(-2800, 1700, -100)), FName(TEXT("GalleryDeckGouda")));
		TestEqual(TEXT("and the Ledge's, 4 m up"), StructureHit(FVector(-2400, -1600, 700), FVector(-2400, -1600, -100)), FName(TEXT("LedgeDeckGouda")));

		// The lines the rock stops, whatever the mice eat.
		const FString FloorLine = LabelOf(GrottoHit(FVector(2600, 0, 150), FVector(-2600, 0, 150), true));
		TestTrue(FString::Printf(TEXT("the Spine stops the floor's middle line between the hollows' stairs (%s)"), *FloorLine), FloorLine.Contains(TEXT("Spine")));
		TestTrue(TEXT("and the line between the hollows' openings"), LabelOf(GrottoHit(FVector(3700, 0, 560), FVector(-3700, 0, 560), true)).Contains(TEXT("Spine")));
		TestTrue(TEXT("the Curtain stops the line along the galleries"), LabelOf(GrottoHit(FVector(3000, 2550, 960), FVector(-3000, 2550, 960), true)).StartsWith(TEXT("Curtain")));
		TestTrue(TEXT("and across the gallery decks"), LabelOf(GrottoHit(FVector(3000, 1700, 960), FVector(-3000, 1700, 960), true)).StartsWith(TEXT("Curtain")));
		for (const float Y : {-2700.0f, -2550.0f, -2400.0f, -2300.0f})
		{
			TestNotNull(*FString::Printf(TEXT("the walk's buttress and fin stop its line from end to end at y %.0f"), Y), GrottoHit(FVector(3350, Y, 560), FVector(-3350, Y, 560), true));
		}
		for (const APlayerStart* Gouda : Starts.FindRef(FName(TEXT("Gouda"))))
		{
			for (const APlayerStart* Mozza : Starts.FindRef(FName(TEXT("Mozza"))))
			{
				const AActor* Between = GrottoHit(Gouda->GetActorLocation() + FVector(0, 0, 60), Mozza->GetActorLocation() + FVector(0, 0, 60), true);
				TestNotNull(*FString::Printf(TEXT("rock between %s and %s"), *Gouda->GetActorLabel(), *Mozza->GetActorLabel()), Between);
			}
		}
		// Before the mice, the cheese hides: the tunnel's walls and the decks' edges.
		TestEqual(TEXT("the Rind Tunnel hides who is in it from the Ledge"), StructureHit(FVector(-1000, -1800, 560), FVector(-1000, -2500, 560)), FName(TEXT("RindTunnel")));
		TestEqual(TEXT("and its roof from the gallery"), StructureHit(FVector(-1000, -2500, 1000), FVector(-1000, -2500, 500)), FName(TEXT("RindTunnel")));
		TestEqual(*FString::Printf(TEXT("the Ledge's edge hides who crouches behind it from the floor (%s)"), *LabelOf(GrottoHit(FVector(-1200, -500, 150), FVector(-1200, -1600, 480), false))),
			StructureHit(FVector(-1200, -500, 150), FVector(-1200, -1600, 480)), FName(TEXT("LedgeDeckGouda")));
	});
	// The mice's ways in: each lands on the navigation mesh, and on every level.
	WaitUntil(TEXT("the navigation mesh"), 30.0f, &NavigationReady);
	Then([this]()
	{
		UWorld* Here = World(0);
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Here);
		TSet<int32> Levels;
		int32 Entries = 0;
		for (TActorIterator<AActor> It(Here); It; ++It)
		{
			if (!It->ActorHasTag(FName(TEXT("MouseEntry"))))
			{
				continue;
			}
			++Entries;
			// As the game mode brings a mouse in (ACheeseStrikeGameMode::SpawnMice).
			FNavLocation Ground;
			const bool bLands = Navigation->ProjectPointToNavigation(It->GetActorLocation() + It->GetActorForwardVector() * 500.0f, Ground, FVector(300.0f, 300.0f, 2000.0f));
			TestTrue(FString::Printf(TEXT("a mouse coming out at %s lands (on level %d)"), *It->GetActorLabel(), bLands ? LevelAt(Ground.Location.Z) : -1), bLands && FMath::Abs(Ground.Location.Z - It->GetActorLocation().Z) < 400.0f);
			if (bLands)
			{
				Levels.Add(LevelAt(Ground.Location.Z));
			}
		}
		TestTrue(FString::Printf(TEXT("ten ways in (%d)"), Entries), Entries >= 10);
		TestTrue(TEXT("the mice come in on the floor and the middle level, not onto the galleries"), Levels.Contains(0) && Levels.Contains(1) && !Levels.Contains(2));
	});
	// Every flight and both bridges can be walked.
	WalkThrough(this, TEXT("up the hollow's stairs to its landing by the gallery"), FVector(-3900, 1000, 500), 90.0f, TEXT("3.0"), [](const FVector& At) { return At.Z > L2 + 50.0f; });
	WalkThrough(this, TEXT("out of the hollow's opening and down its stairs to the floor"), FVector(-3500, 0, 500), 0.0f, TEXT("2.5"), [](const FVector& At) { return At.Z < 150.0f && At.X > -2600.0f; });
	WalkThrough(this, TEXT("along the gallery and down its stairs to the Pulpit"), FVector(-2100, 2550, 900), 0.0f, TEXT("2.5"), [](const FVector& At) { return At.Z < L1 + 150.0f && At.Z > L1 && At.X > -1100.0f; });
	WalkThrough(this, TEXT("up the stairs from the Well to the Pulpit"), FVector(-800, 350, 100), 90.0f, TEXT("2.5"), [](const FVector& At) { return At.Z > L1 + 50.0f; });
	WalkThrough(this, TEXT("up the stairs from the Well to the landing"), FVector(-600, -450, 100), -90.0f, TEXT("2.5"), [](const FVector& At) { return At.Z > L1 + 50.0f; });
	WalkThrough(this, TEXT("from the Pulpit over both bridges through the Spine's arch to the landing"), FVector(0, 1700, 500), -90.0f, TEXT("8.0"), [](const FVector& At) { return At.Z > L1 + 50.0f && At.Y < -1450.0f; });
	WalkThrough(this, TEXT("from the landing through the Rind Tunnel's door"), FVector(0, -1800, 500), -90.0f, TEXT("2.0"), [](const FVector& At) { return At.Z > L1 + 50.0f && At.Y < -2350.0f; });
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAGrottoDeckFallsOntoTheLevelBelow, "CheeseStrike.Play.AGrottoDeckFallsOntoTheLevelBelow", GrottoTests::Flags)

bool FAGrottoDeckFallsOntoTheLevelBelow::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace GrottoTests;
	static TWeakObjectPtr<ACheeseStrikeCheese> Slab;
	static TArray<TWeakObjectPtr<ACheeseStrikeCheese>> Edge;
	static FBox SlabWas;
	FSession Session;
	Session.Level = Grotto;
	StartPlaying(Session);
	// A slab of the Ledge, 4 m up, with the player standing under it beside its pillar: the pillar is eaten.
	auto Prepare = [](const TCHAR* Name)
	{
		Slab = CheeseLabelled(FString::Printf(TEXT("Cheese_%s_Slab_2_1"), Name));
		Edge.Reset();
		if (Slab.IsValid())
		{
			SlabWas = Slab->GetComponentsBoundingBox();
			for (int32 Index = 0; Index < 2; ++Index)
			{
				if (ACheeseStrikeCheese* Each = CheeseLabelled(FString::Printf(TEXT("Cheese_%s_Edge_2_1_%d"), Name, Index)))
				{
					Edge.Add(Each);
				}
			}
		}
	};
	Then([this, Prepare]()
	{
		Prepare(TEXT("LedgeDeck_Gouda"));
		TestTrue(TEXT("a slab of the Ledge, 4 m up, with an edge on it"), Slab.IsValid() && FMath::IsNearlyEqual(SlabWas.Max.Z, L1, 1.0f) && Edge.Num() == 2);
	});
	PutAt(0, FVector(-2228, -1500, 100), 0.0f);
	Wait(0.5f);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = Character(0);
		TestTrue(TEXT("the player stands under it, on the floor"), Me != nullptr && Me->GetActorLocation().Z < 200.0f && SlabWas.IsInsideXY(Me->GetActorLocation()));
		ACheeseStrikeCheese* Pillar = CheeseLabelled(TEXT("Cheese_LedgeDeck_Gouda_Pillar_2_1"));
		if (TestNotNull(TEXT("its pillar"), Pillar))
		{
			Pillar->Bite(CheeseStrikeRules::CheeseBlockHealth);
		}
		TestTrue(TEXT("the slab falls as soon as its pillar is gone"), Slab.IsValid() && Slab->IsFalling());
	});
	WaitUntil(TEXT("the slab coming down onto the floor"), 5.0f, []() { return Slab.IsValid() && !Slab->IsFalling() && Slab->GetComponentsBoundingBox().Min.Z < 10.0f; });
	Wait(0.5f);
	Then([this]()
	{
		const FBox Now = Slab->GetComponentsBoundingBox();
		TestTrue(FString::Printf(TEXT("whole, straight down, its bottom on the floor (%.0f)"), Now.Min.Z), FMath::Abs(Now.Min.Z) < 3.0f && FVector::Dist2D(Now.GetCenter(), SlabWas.GetCenter()) < 1.0f && Now.GetSize().Equals(SlabWas.GetSize(), 1.0));
		for (const TWeakObjectPtr<ACheeseStrikeCheese>& Each : Edge)
		{
			TestTrue(TEXT("its edge came down with it, onto it"), Each.IsValid() && !Each->IsFalling() && FMath::Abs(Each->GetComponentsBoundingBox().Min.Z - Now.Max.Z) < 3.0f);
		}
		const ACheeseStrikeCharacter* Me = Character(0);
		const FString Inside = InsideOf(Me);
		TestTrue(FString::Printf(TEXT("the player under it is not trapped inside anything (%s; feet at %.0f)"), Inside.IsEmpty() ? TEXT("free") : *Inside, Me != nullptr ? Me->GetActorLocation().Z - 96.0f : -1.0f), Inside.IsEmpty());
	});
	Play(0, TEXT("hold Move 0 1\nwait 1\nrelease Move\nwait 0.3"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = Character(0);
		TestTrue(TEXT("and walks away"), Me != nullptr && FVector::Dist2D(Me->GetActorLocation(), FVector(-2228, -1500, 0)) > 150.0f);
	});
	// A slab of the gallery, 8 m up, with the player standing on it: they come down with it.
	Then([this]()
	{
		Slab = CheeseLabelled(TEXT("Cheese_GalleryDeck_Gouda_Slab_1_0"));
		Edge.Reset();
		for (int32 Index = 0; Index < 2; ++Index)
		{
			if (ACheeseStrikeCheese* Each = CheeseLabelled(FString::Printf(TEXT("Cheese_GalleryDeck_Gouda_Edge_1_0_%d"), Index)))
			{
				Edge.Add(Each);
			}
		}
		SlabWas = Slab.IsValid() ? Slab->GetComponentsBoundingBox() : FBox(ForceInit);
		TestTrue(TEXT("a slab of the gallery, 8 m up, with an edge on it"), Slab.IsValid() && FMath::IsNearlyEqual(SlabWas.Max.Z, L2, 1.0f) && Edge.Num() == 2);
	});
	PutAt(0, FVector(-2700, 1750, L2 + 100), 0.0f);
	Wait(0.5f);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = Character(0);
		TestTrue(TEXT("the player stands on it"), Me != nullptr && Me->GetActorLocation().Z > L2 + 50.0f);
		if (ACheeseStrikeCheese* Pillar = CheeseLabelled(TEXT("Cheese_GalleryDeck_Gouda_Pillar_1_0")))
		{
			Pillar->Bite(CheeseStrikeRules::CheeseBlockHealth);
		}
	});
	WaitUntil(TEXT("the slab coming down onto the floor"), 6.0f, []() { return Slab.IsValid() && !Slab->IsFalling() && Slab->GetComponentsBoundingBox().Min.Z < 10.0f; });
	Wait(1.0f);
	Then([this]()
	{
		const FBox Now = Slab->GetComponentsBoundingBox();
		TestTrue(FString::Printf(TEXT("whole, straight down 8 m, its bottom on the floor (%.0f)"), Now.Min.Z), FMath::Abs(Now.Min.Z) < 3.0f && FVector::Dist2D(Now.GetCenter(), SlabWas.GetCenter()) < 1.0f);
		for (const TWeakObjectPtr<ACheeseStrikeCheese>& Each : Edge)
		{
			TestTrue(TEXT("its edge came down with it, onto it"), Each.IsValid() && !Each->IsFalling() && FMath::Abs(Each->GetComponentsBoundingBox().Min.Z - Now.Max.Z) < 3.0f);
		}
		const ACheeseStrikeCharacter* Me = Character(0);
		const FVector At = Me != nullptr ? Me->GetActorLocation() : FVector::ZeroVector;
		TestTrue(FString::Printf(TEXT("the player came down with it, alive, standing on it (feet at %.0f)"), At.Z - 96.0f), Me != nullptr && Me->IsAlive() && FMath::Abs(At.Z - 96.0f - Now.Max.Z) < 15.0f && Now.IsInsideXY(At));
		const FString Inside = InsideOf(Me);
		TestTrue(FString::Printf(TEXT("and is not inside anything (%s)"), Inside.IsEmpty() ? TEXT("free") : *Inside), Inside.IsEmpty());
		for (ACheeseStrikeCheese* Each : ACheeseStrikeCheese::Unheld(World(0)))
		{
			AddError(FString::Printf(TEXT("%s stands on nothing at %s"), *Each->GetActorLabel(), *Each->GetComponentsBoundingBox().ToString()));
		}
	});
	Play(0, TEXT("hold Move 0 1\nwait 1\nrelease Move\nwait 0.3"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = Character(0);
		TestTrue(TEXT("and walks off the fallen slab"), Me != nullptr && FVector::Dist2D(Me->GetActorLocation(), FVector(-2700, 1750, 0)) > 150.0f);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsPlayTheGrotto, "CheeseStrike.Play.BotsPlayTheGrotto", GrottoTests::Flags)

bool FBotsPlayTheGrotto::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace GrottoTests;
	static TMap<FString, FVector> Spawned;
	static TMap<FString, float> Furthest;
	static TSet<FString> Fought;
	static TMap<int32, int32> OnLevel;
	static float StartedAt = 0.0f;
	Spawned.Reset();
	Furthest.Reset();
	Fought.Reset();
	OnLevel.Reset();
	auto Bots = []()
	{
		TArray<ACheeseStrikeBotController*> Found;
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			Found.Add(*It);
		}
		return Found;
	};
	FSession Session;
	Session.Level = Grotto;
	Session.Options = TEXT("?Bots=15?KillLimit=50");
	StartPlaying(Session);
	WaitUntil(TEXT("fifteen bots"), 10.0f, [Bots]() { return Bots().Num() == 15; });
	WaitUntil(TEXT("the navigation mesh"), 30.0f, &NavigationReady);
	// Every level, and every place on it, can be walked to from a hollow by the navigation mesh the bots and mice go by.
	Then([this]()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		const FVector Hollow(-4200, -1900, 420);
		struct FPlace { const TCHAR* Name; FVector At; };
		for (const FPlace& Place : {FPlace{TEXT("the other hollow"), FVector(4200, -1900, 420)}, FPlace{TEXT("the floor"), FVector(-1500, -800, 20)},
			FPlace{TEXT("the Well"), FVector(0, 900, 20)}, FPlace{TEXT("the Colonnade, under the gallery"), FVector(-2600, 2000, 20)},
			FPlace{TEXT("the Undercroft, under the Ledge"), FVector(-1800, -1700, 20)}, FPlace{TEXT("the gallery's walk"), FVector(-2600, 2600, 820)},
			FPlace{TEXT("a gallery deck"), FVector(-2800, 1700, 820)}, FPlace{TEXT("the Pulpit"), FVector(-600, 1900, 420)},
			FPlace{TEXT("the Spine's arch"), FVector(0, 0, 420)}, FPlace{TEXT("the landing"), FVector(-500, -1800, 420)},
			FPlace{TEXT("the Rind Tunnel"), FVector(-800, -2500, 420)}, FPlace{TEXT("a Ledge deck"), FVector(-2400, -1600, 420)},
			FPlace{TEXT("the south walk"), FVector(-3000, -2500, 420)}, FPlace{TEXT("the other gallery"), FVector(2600, 2600, 820)}})
		{
			FNavLocation From, To;
			const bool bOn = Navigation->ProjectPointToNavigation(Hollow, From, FVector(100, 100, 200)) && Navigation->ProjectPointToNavigation(Place.At, To, FVector(100, 100, 200));
			UNavigationPath* Path = bOn ? Navigation->FindPathToLocationSynchronously(World(0), From.Location, To.Location) : nullptr;
			TestTrue(FString::Printf(TEXT("%s can be walked to from a hollow (%s)"), Place.Name, !bOn ? TEXT("off the mesh") : Path == nullptr ? TEXT("no path") : Path->IsPartial() ? TEXT("partly") : TEXT("whole")),
				Path != nullptr && Path->IsValid() && !Path->IsPartial());
		}
		StartedAt = Now(0);
	});
	// Half a minute at least: long enough to leave the hollows, spread over the levels and fight there.
	WaitUntil(TEXT("the bots spreading over the three levels, finding each other and killing"), 90.0f, [Bots]()
	{
		int32 Kills = 0;
		for (ACheeseStrikeBotController* Bot : Bots())
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
			Furthest.FindOrAdd(Name) = FMath::Max(Furthest.FindRef(Name), FVector::Dist(Spawned[Name], Body->GetActorLocation()));
			if (Bot->SeesTarget())
			{
				Fought.Add(Name);
			}
			// Out of the hollows, on which level.
			if (FMath::Abs(Body->GetActorLocation().X) < 3400.0f)
			{
				OnLevel.FindOrAdd(LevelAt(Body->GetActorLocation().Z - 96.0f))++;
			}
		}
		int32 Moved = 0;
		for (const TPair<FString, float>& Each : Furthest)
		{
			Moved += Each.Value > 1000.0f ? 1 : 0;
		}
		return Now(0) - StartedAt >= 30.0f && Moved >= 14 && Kills >= 8 && Fought.Num() >= 10 && OnLevel.FindRef(0) > 0 && OnLevel.FindRef(1) > 0 && OnLevel.FindRef(2) > 0;
	});
	Then([this, Bots]()
	{
		int32 Kills = 0;
		for (ACheeseStrikeBotController* Bot : Bots())
		{
			if (const ACheeseStrikePlayerState* State = Bot->GetPlayerState<ACheeseStrikePlayerState>())
			{
				Kills += State->GetKills();
				AddInfo(FString::Printf(TEXT("%s: %d kills, went %.0f m, %s"), *State->GetPlayerName(), State->GetKills(), Furthest.FindRef(State->GetPlayerName()) / 100.0f, Bot->Doing()));
			}
		}
		AddInfo(FString::Printf(TEXT("seen on the floor %d times, the middle level %d, the galleries %d"), OnLevel.FindRef(0), OnLevel.FindRef(1), OnLevel.FindRef(2)));
		TestTrue(FString::Printf(TEXT("the bots killed one another (%d)"), Kills), Kills >= 8);
		TestTrue(FString::Printf(TEXT("most bots found someone to fight (%d)"), Fought.Num()), Fought.Num() >= 10);
		for (int32 Level = 0; Level < 3; ++Level)
		{
			TestTrue(FString::Printf(TEXT("bots went onto level %d"), Level), OnLevel.FindRef(Level) > 0);
		}
	});
	StopPlaying();
	return true;
}

#endif
