// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CheeseStrikePlayerController.generated.h"

class UCheeseStrikePlayScript;
class UInputMappingContext;

/**
 * The player's controller: puts the game's input bindings on for a local
 * player (see UCheeseStrikeInputSet), and limits how far the view looks up
 * and down through its camera manager.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACheeseStrikePlayerController();

protected:
	/** Mapping contexts a Blueprint of this controller adds, besides the game's own. */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** The script playing the game in place of a person, if -PlayScript named one. */
	UPROPERTY()
	TObjectPtr<UCheeseStrikePlayScript> PlayScript;
};
