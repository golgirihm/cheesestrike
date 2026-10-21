// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Net/UnrealNetwork.h"

ACheeseStrikeGameState::ACheeseStrikeGameState()
{
	TeamScores.Init(0, CheeseStrikeRules::Teams);
	TeamRevealUntil.Init(0.0f, CheeseStrikeRules::Teams);
}

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
	DOREPLIFETIME(ACheeseStrikeGameState, PlayersExpected);
	DOREPLIFETIME(ACheeseStrikeGameState, PlayersReady);
	DOREPLIFETIME(ACheeseStrikeGameState, Mode);
	DOREPLIFETIME(ACheeseStrikeGameState, TeamScores);
	DOREPLIFETIME(ACheeseStrikeGameState, WinningTeam);
	DOREPLIFETIME(ACheeseStrikeGameState, RoundLimit);
	DOREPLIFETIME(ACheeseStrikeGameState, Round);
	DOREPLIFETIME(ACheeseStrikeGameState, RoundEndsAt);
	DOREPLIFETIME(ACheeseStrikeGameState, RoundBreakUntil);
	DOREPLIFETIME(ACheeseStrikeGameState, LastRoundWinner);
	DOREPLIFETIME(ACheeseStrikeGameState, TeamRevealUntil);
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

void ACheeseStrikeGameState::AddKill(const FString& Killer, const FString& Victim, bool bHead, const FString& How)
{
	if (HasAuthority())
	{
		MulticastKill(Killer, Victim, bHead, How);
	}
}

void ACheeseStrikeGameState::MulticastKill_Implementation(const FString& Killer, const FString& Victim, bool bHead, const FString& How)
{
	FKillFeedEntry Entry;
	Entry.Killer = Killer;
	Entry.Victim = Victim;
	Entry.bHead = bHead;
	Entry.How = How;
	Entry.At = GetServerWorldTimeSeconds();
	KillFeed.Add(Entry);
	while (KillFeed.Num() > CheeseStrikeRules::KillFeedLength)
	{
		KillFeed.RemoveAt(0);
	}
}

int32 ACheeseStrikeGameState::TeamOf(const APlayerState* Player) const
{
	const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Player);
	return Ours != nullptr && HasTeams() ? Ours->GetTeam() : CheeseStrikeModes::NoTeam;
}

bool ACheeseStrikeGameState::AreTeammates(const APlayerState* A, const APlayerState* B) const
{
	if (A == nullptr || B == nullptr)
	{
		return false;
	}
	if (A == B)
	{
		return true;
	}
	return HasTeams() && TeamOf(A) == TeamOf(B);
}

bool ACheeseStrikeGameState::AreEnemies(const APlayerState* A, const APlayerState* B) const
{
	if (A == nullptr || B == nullptr)
	{
		return false;
	}
	return CheeseStrikeModes::AreEnemies(HasTeams(), TeamOf(A), TeamOf(B), A == B);
}

void ACheeseStrikeGameState::AddTeamScore(int32 Team, int32 Points)
{
	if (TeamScores.IsValidIndex(Team))
	{
		TeamScores[Team] += Points;
	}
}

FString ACheeseStrikeGameState::ScoreSummary() const
{
	if (!HasTeams())
	{
		const TArray<CheeseStrikeRules::FStanding> InOrder = Standings();
		return InOrder.Num() > 0 ? FString::Printf(TEXT("%s leads with %d"), *InOrder[0].Name, InOrder[0].Kills) : FString();
	}
	FString Line = FString::Printf(TEXT("%s %d  ·  %d %s"), CheeseStrikeRules::TeamNames[0], GetTeamScore(0), GetTeamScore(1), CheeseStrikeRules::TeamNames[1]);
	if (GetModeInfo().WinBy == CheeseStrikeModes::EWinBy::Rounds && Round > 0)
	{
		Line += FString::Printf(TEXT("  ·  round %d"), Round);
	}
	return Line;
}

void ACheeseStrikeGameState::StartRound(int32 Number, float EndsAt)
{
	Round = Number;
	RoundEndsAt = EndsAt;
	RoundBreakUntil = 0.0f;
}

void ACheeseStrikeGameState::EndRound(int32 Winner, float BreakUntil)
{
	LastRoundWinner = Winner;
	RoundBreakUntil = BreakUntil;
	RoundEndsAt = 0.0f;
}

bool ACheeseStrikeGameState::IsRevealedTo(const APlayerState* Viewer) const
{
	return RevealEndsFor(Viewer) > GetServerWorldTimeSeconds();
}

float ACheeseStrikeGameState::RevealEndsFor(const APlayerState* Viewer) const
{
	const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Viewer);
	if (Ours == nullptr)
	{
		return 0.0f;
	}
	float Until = 0.0f;
	if (HasTeams())
	{
		Until = TeamRevealUntil.IsValidIndex(Ours->GetTeam()) ? TeamRevealUntil[Ours->GetTeam()] : 0.0f;
	}
	else
	{
		Until = Ours->GetRevealUntil();
	}
	return Until > GetServerWorldTimeSeconds() ? Until : 0.0f;
}

void ACheeseStrikeGameState::RevealFor(ACheeseStrikePlayerState* Caller, float Until)
{
	if (Caller == nullptr)
	{
		return;
	}
	if (HasTeams())
	{
		if (TeamRevealUntil.IsValidIndex(Caller->GetTeam()))
		{
			TeamRevealUntil[Caller->GetTeam()] = FMath::Max(TeamRevealUntil[Caller->GetTeam()], Until);
		}
	}
	else
	{
		Caller->SetRevealUntil(FMath::Max(Caller->GetRevealUntil(), Until));
	}
}

void ACheeseStrikeGameState::Announce(const FString& Text, CheeseStrikeStreaks::EStreak Streak, int32 Team, const FString& Who)
{
	if (HasAuthority())
	{
		MulticastAnnounce(Text, static_cast<uint8>(Streak), Team, Who);
	}
}

void ACheeseStrikeGameState::MulticastAnnounce_Implementation(const FString& Text, uint8 Streak, int32 Team, const FString& Who)
{
	FAnnouncement Entry;
	Entry.Text = Text;
	Entry.Streak = Streak;
	Entry.Team = Team;
	Entry.Who = Who;
	Entry.At = GetServerWorldTimeSeconds();
	Announcements.Add(Entry);
	while (Announcements.Num() > AnnouncementsKept)
	{
		Announcements.RemoveAt(0);
	}
}
