// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Console commands for looking at a map: CheeseStrikeHideTagged hides what a
// map script tagged (tools/art/map_kit.py, overhead: a cellar's ceiling), so
// that a roofed map can be seen from above (tools/play/labyrinth_tour.txt).

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

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
}
