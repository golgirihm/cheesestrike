// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CheeseStrikeSeatMemory.generated.h"

class APlayerState;

/**
 * The host's memory of who sits where, across the match's levels: every
 * travel (to the map the host picked, back to the lobby after the results)
 * has everyone join afresh, so the game mode looks each player up here as
 * they come in, to give them back the side they chose in the lobby and their
 * place in its order (ACheeseStrikePlayerState::GetJoinOrder). Kept on the
 * host's game instance, which outlives its levels; forgotten when a match is
 * hosted anew, and for a player who leaves the match.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeSeatMemory : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	struct FSeat
	{
		int32 Order = 0;
		int32 Team = 0;
	};

	/** The seat `Player` had, if they have been in this match before. */
	const FSeat* Find(const APlayerState* Player) const;
	/** Remembers `Player` as new: the next in order, on `Team`. */
	const FSeat& Add(const APlayerState* Player, int32 Team);
	/** `Player` has gone over to `Team`. */
	void SetTeam(const APlayerState* Player, int32 Team);
	/** `Player` has left the match. */
	void Forget(const APlayerState* Player);
	/** A new match: nobody has been in it. */
	void Clear();

	/** How a player is known here: their online id, or their name without one. */
	static FString KeyOf(const APlayerState* Player);

private:
	TMap<FString, FSeat> Seats;
	int32 NextOrder = 0;
};
