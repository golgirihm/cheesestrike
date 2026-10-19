// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Rules/CheeseStrikeRules.h"
#include "CheeseStrikePlayerState.generated.h"

/**
 * What a match knows about a player wherever their character is: their
 * kills and deaths, counted by the server and seen by everyone.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	int32 GetKills() const { return Kills; }
	int32 GetDeaths() const { return Deaths; }
	/** Server only. */
	void AddKill() { ++Kills; }
	void AddDeath() { ++Deaths; }

	/** This player's line on the scoreboard. */
	CheeseStrikeRules::FStanding Standing() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(Replicated)
	int32 Kills = 0;
	UPROPERTY(Replicated)
	int32 Deaths = 0;
};
