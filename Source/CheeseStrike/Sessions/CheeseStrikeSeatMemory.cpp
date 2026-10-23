// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSeatMemory.h"
#include "GameFramework/PlayerState.h"

FString UCheeseStrikeSeatMemory::KeyOf(const APlayerState* Player)
{
	if (Player == nullptr)
	{
		return FString();
	}
	const FUniqueNetIdRepl& Id = Player->GetUniqueId();
	return Id.IsValid() ? Id.ToString() : TEXT("name:") + Player->GetPlayerName();
}

const UCheeseStrikeSeatMemory::FSeat* UCheeseStrikeSeatMemory::Find(const APlayerState* Player) const
{
	return Seats.Find(KeyOf(Player));
}

const UCheeseStrikeSeatMemory::FSeat& UCheeseStrikeSeatMemory::Add(const APlayerState* Player, int32 Team)
{
	FSeat Seat;
	Seat.Order = NextOrder++;
	Seat.Team = Team;
	return Seats.Add(KeyOf(Player), Seat);
}

void UCheeseStrikeSeatMemory::SetTeam(const APlayerState* Player, int32 Team)
{
	if (FSeat* Seat = Seats.Find(KeyOf(Player)))
	{
		Seat->Team = Team;
	}
}

void UCheeseStrikeSeatMemory::Forget(const APlayerState* Player)
{
	Seats.Remove(KeyOf(Player));
}

void UCheeseStrikeSeatMemory::Clear()
{
	Seats.Reset();
	NextOrder = 0;
}
