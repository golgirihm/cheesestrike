// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHung.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "CheeseStrike.h"
#include "Map/CheeseStrikeCheese.h"
#include "Rules/CheeseStrikeMice.h"

namespace
{
	/** How often the structures' cheese is counted (s). */
	constexpr float HungCountEvery = 0.25f;

	/** For looking at a map (development): lists what hangs in cheese, the blocks round each and how many are left. */
	FAutoConsoleCommandWithWorld HungCommand(TEXT("CheeseStrikeHung"), TEXT("Lists what hangs in cheese, with the blocks round each and how many of them are left."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const UCheeseStrikeHungSubsystem* Hung = World != nullptr ? World->GetSubsystem<UCheeseStrikeHungSubsystem>() : nullptr;
		if (Hung == nullptr)
		{
			return;
		}
		for (const TPair<TWeakObjectPtr<AActor>, FName>& Each : Hung->GetHung())
		{
			const AActor* Actor = Each.Key.Get();
			if (Actor == nullptr)
			{
				continue;
			}
			UE_LOG(LogCheeseStrike, Display, TEXT("Hung: %s on %s at %s: %d of %d blocks round it left%s"), *Actor->GetName(), *Each.Value.ToString(), *Actor->GetActorLocation().ToCompactString(),
				Hung->GetRoundLeft(Actor), Hung->GetRound(Actor), Hung->IsGone(Actor) ? TEXT(", gone") : TEXT(""));
		}
	}));
}

bool UCheeseStrikeHungSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCheeseStrikeHungSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCheeseStrikeHungSubsystem, STATGROUP_Tickables);
}

void UCheeseStrikeHungSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Gather();
}

void UCheeseStrikeHungSubsystem::Gather()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	bGathered = true;
	Hung.Reset();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			const FName Structure = CheeseStrikeMice::StructureOfTag(Tag);
			if (!Structure.IsNone())
			{
				FHung Each;
				Each.Actor = *It;
				Each.Structure = Structure;
				Hung.Add(MoveTemp(Each));
				break;
			}
		}
	}
	// The cheese round each: the blocks of its structure touching its bounds or within reach of them.
	for (FHung& Each : Hung)
	{
		Each.Bounds = Each.Actor->GetComponentsBoundingBox(true);
		if (!Each.Bounds.IsValid)
		{
			// A light alone has no bounds: where it is.
			Each.Bounds = FBox(Each.Actor->GetActorLocation(), Each.Actor->GetActorLocation());
		}
	}
	int32 WithCheeseRound = 0;
	for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
	{
		const FBox Block = It->GetComponentsBoundingBox();
		for (FHung& Each : Hung)
		{
			if (Each.Structure == It->GetStructure() && CheeseStrikeMice::IsRoundHung(Each.Bounds, Block))
			{
				Each.Round.Add(*It);
			}
		}
	}
	for (const FHung& Each : Hung)
	{
		WithCheeseRound += Each.Round.Num() > 0 ? 1 : 0;
	}
	if (Hung.Num() > 0)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Hung on cheese: %d things, %d with cheese round them"), Hung.Num(), WithCheeseRound);
	}
}

int32 UCheeseStrikeHungSubsystem::RoundLeft(const FHung& Each)
{
	int32 Left = 0;
	for (const TWeakObjectPtr<ACheeseStrikeCheese>& Block : Each.Round)
	{
		// Eaten, it is gone; fallen away, it is out of reach.
		if (const ACheeseStrikeCheese* Cheese = Block.Get(); Cheese != nullptr && !Cheese->IsActorBeingDestroyed() && CheeseStrikeMice::IsRoundHung(Each.Bounds, Cheese->GetComponentsBoundingBox()))
		{
			++Left;
		}
	}
	return Left;
}

void UCheeseStrikeHungSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (World == nullptr || !World->HasBegunPlay())
	{
		return;
	}
	if (!bGathered)
	{
		Gather();
	}
	if (Hung.Num() == 0)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (CountedAt < 0.0f || Now - CountedAt >= HungCountEvery)
	{
		CountedAt = Now;
		// The blocks left of each structure something hangs on: a block eaten is gone on every machine.
		TMap<FName, int32> Left;
		for (const FHung& Each : Hung)
		{
			Left.Add(Each.Structure, 0);
		}
		for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
		{
			if (int32* Count = !It->IsActorBeingDestroyed() ? Left.Find(It->GetStructure()) : nullptr)
			{
				++*Count;
			}
		}
		for (FHung& Each : Hung)
		{
			if (Each.GoneAt < 0.0f && Each.Actor.IsValid() && CheeseStrikeMice::IsHungOnEatenCheese(Each.Structure, Left.FindRef(Each.Structure), Each.Round.Num(), RoundLeft(Each)))
			{
				Each.GoneAt = Now;
				StartGoing(Each);
			}
		}
	}
	for (FHung& Each : Hung)
	{
		if (Each.GoneAt >= 0.0f && !Each.bDone)
		{
			ShowGoing(Each);
		}
	}
}

void UCheeseStrikeHungSubsystem::StartGoing(FHung& Each)
{
	AActor* Actor = Each.Actor.Get();
	USceneComponent* Root = Actor != nullptr ? Actor->GetRootComponent() : nullptr;
	if (Root == nullptr)
	{
		Each.bDone = true;
		return;
	}
	// It does nothing more of its own (a door stops watching for whoever comes, and sliding), and nothing walks or shoots
	// into it as it goes.
	Actor->SetActorTickEnabled(false);
	if (UWorld* World = Actor->GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(Actor);
	}
	Actor->SetActorEnableCollision(false);
	Each.Was = Root->GetComponentTransform();
	Root->SetMobility(EComponentMobility::Movable);
	TArray<ULightComponent*> Lights;
	Actor->GetComponents(Lights);
	for (ULightComponent* Light : Lights)
	{
		Each.Lights.Add({Light, Light->Intensity});
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("%s goes with the cheese of %s (%d blocks round it)"), *Actor->GetName(), *Each.Structure.ToString(), Each.Round.Num());
}

void UCheeseStrikeHungSubsystem::ShowGoing(FHung& Each)
{
	AActor* Actor = Each.Actor.Get();
	USceneComponent* Root = Actor != nullptr ? Actor->GetRootComponent() : nullptr;
	if (Root == nullptr || GetWorld() == nullptr)
	{
		Each.bDone = true;
		return;
	}
	const float Seconds = GetWorld()->GetTimeSeconds() - Each.GoneAt;
	const float Scale = CheeseStrikeMice::VanishScale(Seconds);
	FTransform Now = Each.Was;
	Now.SetScale3D(Each.Was.GetScale3D() * FMath::Max(Scale, 0.01f));
	Now.AddToTranslation(FVector(0.0f, 0.0f, -CheeseStrikeMice::VanishDrop(Seconds)));
	Root->SetWorldTransform(Now);
	for (const TPair<TWeakObjectPtr<ULightComponent>, float>& Light : Each.Lights)
	{
		if (ULightComponent* Shining = Light.Key.Get(); Shining != nullptr && Shining->Mobility != EComponentMobility::Static)
		{
			Shining->SetIntensity(Light.Value * Scale);
		}
	}
	if (Seconds >= CheeseStrikeMice::VanishSeconds)
	{
		Actor->SetActorHiddenInGame(true);
		for (const TPair<TWeakObjectPtr<ULightComponent>, float>& Light : Each.Lights)
		{
			if (ULightComponent* Shining = Light.Key.Get())
			{
				Shining->SetVisibility(false);
			}
		}
		Each.bDone = true;
	}
}

bool UCheeseStrikeHungSubsystem::IsGone(const AActor* Actor) const
{
	const FHung* Found = Hung.FindByPredicate([Actor](const FHung& Each) { return Each.Actor.Get() == Actor; });
	return Found != nullptr && Found->GoneAt >= 0.0f;
}

TArray<TPair<TWeakObjectPtr<AActor>, FName>> UCheeseStrikeHungSubsystem::GetHung() const
{
	TArray<TPair<TWeakObjectPtr<AActor>, FName>> Out;
	for (const FHung& Each : Hung)
	{
		Out.Add({Each.Actor, Each.Structure});
	}
	return Out;
}

int32 UCheeseStrikeHungSubsystem::GetRound(const AActor* Actor) const
{
	const FHung* Found = Hung.FindByPredicate([Actor](const FHung& Each) { return Each.Actor.Get() == Actor; });
	return Found != nullptr ? Found->Round.Num() : 0;
}

int32 UCheeseStrikeHungSubsystem::GetRoundLeft(const AActor* Actor) const
{
	const FHung* Found = Hung.FindByPredicate([Actor](const FHung& Each) { return Each.Actor.Get() == Actor; });
	return Found != nullptr ? RoundLeft(*Found) : 0;
}
