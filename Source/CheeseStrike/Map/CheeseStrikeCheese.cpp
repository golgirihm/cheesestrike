// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCheese.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Rules/CheeseStrikeFalls.h"
#include "Rules/CheeseStrikeMice.h"
#include "Rules/CheeseStrikeRules.h"

namespace
{
	constexpr float CheeseFallGravity = 980.0f;
	/** How near a block's bottom is to another's top for it to rest on it (cm), and how far into it it may have sunk. */
	constexpr float Resting = 8.0f;
	constexpr float Sunk = 40.0f;
}

ACheeseStrikeCheese::ACheeseStrikeCheese()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicatingMovement(true);
	NetDormancy = DORM_Initial;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Cheese(TEXT("/Game/CheeseStrike/Materials/M_Cheese.M_Cheese"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetStaticMesh(Cube.Object);
	if (Cheese.Object != nullptr)
	{
		Mesh->SetMaterial(0, Cheese.Object);
	}
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// Movable: what rests on eaten cheese falls.
	Mesh->SetMobility(EComponentMobility::Movable);
	Health = CheeseStrikeRules::CheeseBlockHealth;
}

void ACheeseStrikeCheese::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeCheese, Stage);
}

void ACheeseStrikeCheese::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ShowStage();
}

void ACheeseStrikeCheese::OnRep_Stage()
{
	ShowStage();
}

void ACheeseStrikeCheese::ShowStage()
{
	// The cheese material's holes grow with it (tools/art/cheese_material.py).
	if (Mesh != nullptr)
	{
		Mesh->SetCustomPrimitiveDataFloat(0, Stage);
	}
}

int32 ACheeseStrikeCheese::CountEdible(const UWorld* World)
{
	int32 Count = 0;
	if (World != nullptr)
	{
		for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
		{
			Count += It->IsEdible() && !It->IsActorBeingDestroyed() ? 1 : 0;
		}
	}
	return Count;
}

float ACheeseStrikeCheese::EdibleHealth(const UWorld* World)
{
	float Total = 0.0f;
	if (World != nullptr)
	{
		for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
		{
			Total += It->IsEdible() && !It->IsActorBeingDestroyed() ? FMath::Max(0.0f, It->GetHealth()) : 0.0f;
		}
	}
	return Total;
}

void ACheeseStrikeCheese::Bite(float Amount)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || Health <= 0.0f)
	{
		return;
	}
	Health = FMath::Max(0.0f, Health - Amount);
	const uint8 Now = static_cast<uint8>(CheeseStrikeMice::Stage(Health, CheeseStrikeRules::CheeseBlockHealth));
	if (Now != Stage)
	{
		Stage = Now;
		ShowStage();
		FlushNetDormancy();
	}
	if (Health > 0.0f)
	{
		return;
	}
	// Gone: what rested on it falls, unless something else under it holds it.
	const TArray<ACheeseStrikeCheese*> Above = RestingOnIt();
	Destroy();
	for (ACheeseStrikeCheese* Each : Above)
	{
		Each->FallUnlessHeld();
	}
}

TArray<ACheeseStrikeCheese*> ACheeseStrikeCheese::RestingOnIt() const
{
	TArray<ACheeseStrikeCheese*> Above;
	const FBox Top = GetComponentsBoundingBox();
	for (TActorIterator<ACheeseStrikeCheese> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->Health <= 0.0f || It->bFalling || It->IsActorBeingDestroyed())
		{
			continue;
		}
		const FBox Box = It->GetComponentsBoundingBox();
		const bool bOver = Box.Min.X < Top.Max.X - 2.0f && Box.Max.X > Top.Min.X + 2.0f && Box.Min.Y < Top.Max.Y - 2.0f && Box.Max.Y > Top.Min.Y + 2.0f;
		// Its bottom at this one's top, or a little into it (blocks falling together can land a frame's fall into each other).
		if (bOver && Box.Min.Z - Top.Max.Z < Resting && Box.Min.Z - Top.Max.Z > -Sunk)
		{
			Above.Add(*It);
		}
	}
	return Above;
}

bool ACheeseStrikeCheese::Holds(const UPrimitiveComponent* Component)
{
	// Something a player could stand on: not a volume, which is static too.
	if (Component == nullptr || Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
	{
		return false;
	}
	const ACheeseStrikeCheese* Cheese = Cast<ACheeseStrikeCheese>(Component->GetOwner());
	return Cheese == nullptr || (Cheese->Health > 0.0f && !Cheese->bFalling && !Cheese->IsActorBeingDestroyed());
}

bool ACheeseStrikeCheese::IsHeld(FString* Why) const
{
	// What is under it now: the level and cheese (not people, mice or doors, which are not the level's static world).
	const FBox Box = GetComponentsBoundingBox();
	const FVector Extent = Box.GetExtent();
	const FBox2D Footprint(FVector2D(Box.Min.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Max.Y));
	TArray<FOverlapResult> Under;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseHeld), false, this);
	const FVector Below(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z - Resting * 0.5f);
	GetWorld()->OverlapMultiByObjectType(Under, Below, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeBox(FVector(Extent.X - 2.0f, Extent.Y - 2.0f, Resting * 0.5f)), Params);
	TArray<FBox2D> Contacts;
	for (const FOverlapResult& Each : Under)
	{
		const UPrimitiveComponent* Component = Each.GetComponent();
		if (!Holds(Component))
		{
			continue;
		}
		const FBox Bounds = Component->Bounds.GetBox();
		// Its top at this one's bottom: something beside it, reaching lower, doesn't hold it.
		if (Bounds.Max.Z > Box.Min.Z + Resting)
		{
			continue;
		}
		Contacts.Add(FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y)));
		if (Why != nullptr)
		{
			Why->Appendf(TEXT(" on %s"), *Component->GetOwner()->GetName());
		}
	}
	return CheeseStrikeMice::IsHeld(Footprint, Contacts);
}

void ACheeseStrikeCheese::FallUnlessHeld()
{
	if (bFalling || Health <= 0.0f || IsActorBeingDestroyed())
	{
		return;
	}
	if (!IsHeld())
	{
		Fall();
	}
}

TArray<ACheeseStrikeCheese*> ACheeseStrikeCheese::Unheld(const UWorld* World)
{
	TArray<ACheeseStrikeCheese*> Found;
	if (World != nullptr)
	{
		for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
		{
			if (!It->bFalling && It->Health > 0.0f && !It->IsActorBeingDestroyed() && !It->IsHeld())
			{
				Found.Add(*It);
			}
		}
	}
	return Found;
}

namespace
{
	/** Counts the cheese left in each structure: how far the mice have got. */
	FAutoConsoleCommandWithWorld CheeseLeftCommand(TEXT("CheeseStrikeCheeseLeft"), TEXT("Counts the blocks of cheese left, in each structure."), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		TMap<FName, int32> Left;
		int32 Total = 0;
		for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
		{
			if (!It->IsActorBeingDestroyed() && It->IsEdible())
			{
				Left.FindOrAdd(It->GetStructure())++;
				++Total;
			}
		}
		Left.KeySort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
		FString Each;
		for (const TPair<FName, int32>& Structure : Left)
		{
			Each += FString::Printf(TEXT(" %s %d"), *Structure.Key.ToString(), Structure.Value);
		}
		UE_LOG(LogTemp, Display, TEXT("CheeseStrikeCheeseLeft: %d blocks at %.0f s:%s"), Total, World != nullptr ? World->GetTimeSeconds() : 0.0f, *Each);
	}));

	/** Lists every block of cheese that stands on nothing that holds it: a map's mistake, or a fall that went wrong. */
	FAutoConsoleCommandWithWorld UnheldCommand(TEXT("CheeseStrikeUnheld"), TEXT("Lists the blocks of cheese that nothing under them holds up."), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const TArray<ACheeseStrikeCheese*> Found = ACheeseStrikeCheese::Unheld(World);
		UE_LOG(LogTemp, Display, TEXT("CheeseStrikeUnheld: %d blocks of cheese stand on nothing that holds them"), Found.Num());
		for (const ACheeseStrikeCheese* Each : Found)
		{
			FString Why;
			Each->IsHeld(&Why);
#if WITH_EDITOR
			const FString Name = Each->GetActorLabel(false);
#else
			const FString Name = Each->GetName();
#endif
			UE_LOG(LogTemp, Display, TEXT("  %s (%s) at %s:%s"), *Name, *Each->GetStructure().ToString(), *Each->GetComponentsBoundingBox().ToString(), *Why);
		}
	}));
}

namespace
{
	/**
	 * For looking at a map as the mice leave it (development): eats a share (0 to 1) of the cheese the mice may eat,
	 * at random, at once, as the mice would (what stood on it falls), on the server; with a structure's name, only
	 * that structure's.
	 */
	FAutoConsoleCommandWithWorldAndArgs EatCheeseCommand(TEXT("CheeseStrikeEatCheese"), TEXT("Eats a share (0 to 1) of the cheese at random, as the mice would: CheeseStrikeEatCheese 0.5 [structure]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const float Share = Args.Num() > 0 ? FMath::Clamp(FCString::Atof(*Args[0]), 0.0f, 1.0f) : 1.0f;
		TArray<ACheeseStrikeCheese*> Edible;
		for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
		{
			if (It->HasAuthority() && It->IsEdible() && !It->IsActorBeingDestroyed() && (Args.Num() < 2 || It->GetStructure().ToString() == Args[1]))
			{
				Edible.Add(*It);
			}
		}
		for (int32 Index = Edible.Num() - 1; Index > 0; --Index)
		{
			Edible.Swap(Index, FMath::RandHelper(Index + 1));
		}
		const int32 Count = FMath::RoundToInt(Share * Edible.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (IsValid(Edible[Index]) && !Edible[Index]->IsActorBeingDestroyed())
			{
				Edible[Index]->Bite(CheeseStrikeRules::CheeseBlockHealth);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("CheeseStrikeEatCheese: ate %d of %d blocks"), Count, Edible.Num());
	}));
}

void ACheeseStrikeCheese::Fall()
{
	if (bFalling)
	{
		return;
	}
	const TArray<ACheeseStrikeCheese*> Above = RestingOnIt();
	bFalling = true;
	FallSpeed = 0.0f;
	SetActorTickEnabled(true);
	FlushNetDormancy();
	for (ACheeseStrikeCheese* Each : Above)
	{
		Each->FallUnlessHeld();
	}
}

void ACheeseStrikeCheese::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bFalling)
	{
		return;
	}
	FlushNetDormancy();
	FallSpeed += CheeseFallGravity * DeltaSeconds;
	const float Drop = FallSpeed * DeltaSeconds;
	// Down until what it comes onto holds it, as CheeseStrikeMice::IsHeld has it: the level or cheese that isn't falling,
	// under its middle or on either side of it; not people, mice or doors, and not a post or the edge of a wall that it
	// would only teeter on, which it goes on past.
	const FBox Box = GetComponentsBoundingBox();
	const FBox2D Footprint(FVector2D(Box.Min.X, Box.Min.Y), FVector2D(Box.Max.X, Box.Max.Y));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseFall), false, this);
	// What it already touches or has fallen into doesn't hold it up (the Wheel Stack's rows lean on each other).
	Params.bFindInitialOverlaps = false;
	for (TActorIterator<ACheeseStrikeCheese> It(GetWorld()); It; ++It)
	{
		if (It->bFalling)
		{
			Params.AddIgnoredActor(*It);
		}
	}
	// The block less a little all round, so that what is beside it isn't under it.
	constexpr float Inset = 2.0f;
	const FCollisionShape Shape = FCollisionShape::MakeBox(Box.GetExtent() - FVector(Inset));
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Box.GetCenter(), Box.GetCenter() - FVector(0.0f, 0.0f, Drop), FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Shape, Params);
	Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });
	float Landing = -1.0f;
	for (int32 First = 0; First < Hits.Num() && Landing < 0.0f; ++First)
	{
		if (!Holds(Hits[First].GetComponent()))
		{
			continue;
		}
		// Coming down onto this: what else it would be on there, the tops within Resting of this one's.
		const float Top = Hits[First].GetComponent()->Bounds.GetBox().Max.Z;
		TArray<FBox2D> Contacts;
		for (const FHitResult& Each : Hits)
		{
			const FBox Bounds = Holds(Each.GetComponent()) ? Each.GetComponent()->Bounds.GetBox() : FBox(ForceInit);
			if (Bounds.IsValid && FMath::Abs(Bounds.Max.Z - Top) < Resting)
			{
				Contacts.Add(FBox2D(FVector2D(Bounds.Min.X, Bounds.Min.Y), FVector2D(Bounds.Max.X, Bounds.Max.Y)));
			}
		}
		if (CheeseStrikeMice::IsHeld(Footprint, Contacts))
		{
			Landing = Hits[First].Distance;
		}
	}
	if (Landing >= 0.0f)
	{
		// The swept shape's bottom is Inset above the block's: the block comes to rest with its bottom on what it hit.
		AddActorWorldOffset(FVector(0.0f, 0.0f, Inset - Landing));
		bFalling = false;
		FallSpeed = 0.0f;
		SetActorTickEnabled(false);
		// Come to rest under the map's kill height: sunk into the fondue, and gone.
		if (CheeseStrikeFalls::IsBelowKillHeight(GetWorld(), GetComponentsBoundingBox().Max.Z))
		{
			Destroy();
			return;
		}
		// What fell with it on top of it, and has already come down this frame into it, goes back up onto it.
		const FBox Landed = GetComponentsBoundingBox();
		for (TActorIterator<ACheeseStrikeCheese> It(GetWorld()); It; ++It)
		{
			if (!It->bFalling)
			{
				continue;
			}
			const FBox Other = It->GetComponentsBoundingBox();
			const bool bOver = Other.Min.X < Landed.Max.X - 2.0f && Other.Max.X > Landed.Min.X + 2.0f && Other.Min.Y < Landed.Max.Y - 2.0f && Other.Max.Y > Landed.Min.Y + 2.0f;
			if (bOver && Other.Min.Z < Landed.Max.Z && Other.Min.Z > Landed.Max.Z - Sunk && Other.GetCenter().Z > Landed.GetCenter().Z)
			{
				It->AddActorWorldOffset(FVector(0.0f, 0.0f, Landed.Max.Z - Other.Min.Z));
			}
		}
		return;
	}
	AddActorWorldOffset(FVector(0.0f, 0.0f, -Drop));
	// Nothing under it at all (off the map): gone.
	if (GetActorLocation().Z < -5000.0f)
	{
		Destroy();
	}
}
