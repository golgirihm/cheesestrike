// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePlayerState.h"
#include "Net/UnrealNetwork.h"

void ACheeseStrikePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikePlayerState, Kills);
	DOREPLIFETIME(ACheeseStrikePlayerState, Deaths);
}

CheeseStrikeRules::FStanding ACheeseStrikePlayerState::Standing() const
{
	CheeseStrikeRules::FStanding Line;
	Line.Name = GetPlayerName();
	Line.Kills = Kills;
	Line.Deaths = Deaths;
	Line.Order = GetPlayerId();
	return Line;
}
