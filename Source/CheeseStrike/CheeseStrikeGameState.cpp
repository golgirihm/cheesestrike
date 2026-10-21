// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Net/UnrealNetwork.h"

void ACheeseStrikeGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeGameState, KillLimit);
	DOREPLIFETIME(ACheeseStrikeGameState, RespawnDelay);
	DOREPLIFETIME(ACheeseStrikeGameState, WinnerName);
	DOREPLIFETIME(ACheeseStrikeGameState, CountdownEndsAt);
	DOREPLIFETIME(ACheeseStrikeGameState, BotNames);
	DOREPLIFETIME(ACheeseStrikeGameState, BotsWanted);
	DOREPLIFETIME(ACheeseStrikeGameState, HostName);
	DOREPLIFETIME(ACheeseStrikeGameState, MapIndex);
	DOREPLIFETIME(ACheeseStrikeGameState, BotSkill);
	DOREPLIFETIME(ACheeseStrikeGameState, LobbyAt);
	DOREPLIFETIME(ACheeseStrikeGameState, MiceAt);
}

TArray<CheeseStrikeRules::FRosterEntry> ACheeseStrikeGameState::Roster() const
{
	TArray<CheeseStrikeRules::FRosterEntry> Entries;
	for (const APlayerState* Player : PlayerArray)
	{
		if (const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Player))
		{
			CheeseStrikeRules::FRosterEntry Entry;
			Entry.Name = Ours->GetPlayerName();
			Entry.Team = Ours->GetTeam();
			Entry.bBot = Ours->IsABot();
			Entries.Add(Entry);
		}
	}
	return Entries;
}

TArray<CheeseStrikeRules::FRosterEntry> ACheeseStrikeGameState::TeamColumn(int32 Team) const
{
	return CheeseStrikeRules::TeamColumn(Team, Roster(), BotNames, BotsWanted);
}

TArray<CheeseStrikeRules::FStanding> ACheeseStrikeGameState::Standings() const
{
	TArray<CheeseStrikeRules::FStanding> Lines;
	for (const APlayerState* Player : PlayerArray)
	{
		if (const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Player))
		{
			Lines.Add(Ours->Standing());
		}
	}
	return CheeseStrikeRules::Sorted(Lines);
}

int32 ACheeseStrikeGameState::MostKills() const
{
	int32 Most = 0;
	for (const APlayerState* Player : PlayerArray)
	{
		if (const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Player))
		{
			Most = FMath::Max(Most, Ours->GetKills());
		}
	}
	return Most;
}

void ACheeseStrikeGameState::SetLimits(int32 InKillLimit, float InRespawnDelay)
{
	KillLimit = InKillLimit;
	RespawnDelay = InRespawnDelay;
}

void ACheeseStrikeGameState::AddKill(const FString& Killer, const FString& Victim, bool bHead)
{
	if (HasAuthority())
	{
		MulticastKill(Killer, Victim, bHead);
	}
}

void ACheeseStrikeGameState::MulticastKill_Implementation(const FString& Killer, const FString& Victim, bool bHead)
{
	FKillFeedEntry Entry;
	Entry.Killer = Killer;
	Entry.Victim = Victim;
	Entry.bHead = bHead;
	Entry.At = GetServerWorldTimeSeconds();
	KillFeed.Add(Entry);
	while (KillFeed.Num() > CheeseStrikeRules::KillFeedLength)
	{
		KillFeed.RemoveAt(0);
	}
}
