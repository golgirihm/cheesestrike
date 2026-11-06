// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Console commands for looking at a map: CheeseStrikeHideTagged hides what a
// map script tagged (tools/art/map_kit.py, overhead: a cellar's ceiling), so
// that a roofed map can be seen from above (tools/play/labyrinth_tour.txt);
// CheeseStrikeMouseTo puts a mouse somewhere, to watch how it gets on from there
// (tools/play/grotto_arch.txt: mice left in Gruyere Grotto's arch).

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "CheeseStrikeMouse.h"

namespace
{
	/** Hides every actor with a tag (or shows it again), in this machine's world only. */
	FAutoConsoleCommandWithWorldAndArgs HideTaggedCommand(TEXT("CheeseStrikeHideTagged"),
		TEXT("CheeseStrikeHideTagged <tag> [0]: hides every actor tagged <tag> (a map's Ceiling), or with 0 shows it again."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() == 0)
		{
			return;
		}
		const FName Tag(*Args[0]);
		const bool bHide = Args.Num() < 2 || Args[1] != TEXT("0");
		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Tag))
			{
				It->SetActorHiddenInGame(bHide);
				++Count;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("CheeseStrikeHideTagged: %s %d actors tagged %s"), bHide ? TEXT("hid") : TEXT("showed"), Count, *Tag.ToString());
	}));

	/** Puts the n-th mouse (from 0, as CheeseStrikeMice lists them) at a point on the ground, on the server. */
	FAutoConsoleCommandWithWorldAndArgs MouseToCommand(TEXT("CheeseStrikeMouseTo"),
		TEXT("CheeseStrikeMouseTo <n> <x> <y> <z>: puts the n-th mouse (from 0) with its feet at x, y, z, looking for cheese afresh from there."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() < 4 || World->GetNetMode() == NM_Client)
		{
			return;
		}
		const int32 Wanted = FCString::Atoi(*Args[0]);
		int32 Index = 0;
		for (TActorIterator<ACheeseStrikeMouse> It(World); It; ++It, ++Index)
		{
			if (Index == Wanted)
			{
				const FVector Feet(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]));
				It->PutAt(Feet);
				UE_LOG(LogTemp, Display, TEXT("CheeseStrikeMouseTo: %s put at %s"), *It->GetName(), *Feet.ToCompactString());
				return;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("CheeseStrikeMouseTo: no mouse %d (%d in all)"), Wanted, Index);
	}));
}
