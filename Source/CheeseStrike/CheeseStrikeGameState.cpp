// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Map/CheeseStrikeHill.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Weapons/CheeseStrikeArsenal.h"

ACheeseStrikeGameState::ACheeseStrikeGameState()
{
	TeamScores.Init(0, CheeseStrikeRules::Teams);
	TeamRevealUntil.Init(0.0f, CheeseStrikeRules::Teams);
	TeamRevealFrom.Init(0.0f, CheeseStrikeRules::Teams);
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
	DOREPLIFETIME(ACheeseStrikeGameState, BotSides);
	DOREPLIFETIME(ACheeseStrikeGameState, HostName);
	DOREPLIFETIME(ACheeseStrikeGameState, MapIndex);
	DOREPLIFETIME(ACheeseStrikeGameState, BotSkill);
	DOREPLIFETIME(ACheeseStrikeGameState, LobbyAt);
	DOREPLIFETIME(ACheeseStrikeGameState, MiceAt);
	DOREPLIFETIME(ACheeseStrikeGameState, GoAt);
	DOREPLIFETIME(ACheeseStrikeGameState, PlayersExpected);
	DOREPLIFETIME(ACheeseStrikeGameState, PlayersReady);
	DOREPLIFETIME(ACheeseStrikeGameState, Mode);
	DOREPLIFETIME(ACheeseStrikeGameState, TeamScores);
	DOREPLIFETIME(ACheeseStrikeGameState, WinningTeam);
	DOREPLIFETIME(ACheeseStrikeGameState, RoundLimit);
	DOREPLIFETIME(ACheeseStrikeGameState, RoundTime);
	DOREPLIFETIME(ACheeseStrikeGameState, Round);
	DOREPLIFETIME(ACheeseStrikeGameState, RoundEndsAt);
	DOREPLIFETIME(ACheeseStrikeGameState, RoundBreakUntil);
	DOREPLIFETIME(ACheeseStrikeGameState, LastRoundWinner);
	DOREPLIFETIME(ACheeseStrikeGameState, TeamRevealUntil);
	DOREPLIFETIME(ACheeseStrikeGameState, TeamRevealFrom);
}

TArray<CheeseStrikeRules::FRosterEntry> ACheeseStrikeGameState::Roster() const
{
	// In the order they joined: a joiner's PlayerArray is in the order the player states happened to arrive.
	TArray<const ACheeseStrikePlayerState*> Players;
	for (const APlayerState* Player : PlayerArray)
	{
		if (const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Player))
		{
			Players.Add(Ours);
		}
	}
	Players.StableSort([](const ACheeseStrikePlayerState& A, const ACheeseStrikePlayerState& B) { return A.GetJoinOrder() < B.GetJoinOrder(); });
	TArray<CheeseStrikeRules::FRosterEntry> Entries;
	for (const ACheeseStrikePlayerState* Ours : Players)
	{
		CheeseStrikeRules::FRosterEntry Entry;
		Entry.Name = Ours->GetPlayerName();
		Entry.Team = Ours->GetTeam();
		Entry.bBot = Ours->IsABot();
		Entries.Add(Entry);
	}
	return Entries;
}

TArray<CheeseStrikeRules::FRosterEntry> ACheeseStrikeGameState::Seats() const
{
	return CheeseStrikeRules::Seats(Roster(), BotNames, BotsWanted, BotSides);
}

TArray<CheeseStrikeRules::FRosterEntry> ACheeseStrikeGameState::TeamColumn(int32 Team) const
{
	return CheeseStrikeRules::TeamColumn(Team, Roster(), BotNames, BotsWanted, BotSides);
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

void ACheeseStrikeGameState::AddKill(const FString& Killer, const FString& Victim, bool bHead, const FString& How, uint8 Badges)
{
	if (HasAuthority())
	{
		MulticastKill(Killer, Victim, bHead, How, Badges);
	}
}

void ACheeseStrikeGameState::MulticastKill_Implementation(const FString& Killer, const FString& Victim, bool bHead, const FString& How, uint8 Badges)
{
	FKillFeedEntry Entry;
	Entry.Killer = Killer;
	Entry.Victim = Victim;
	Entry.bHead = bHead;
	Entry.How = How;
	Entry.Badges = Badges;
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

const ACheeseStrikePlayerState* ACheeseStrikeGameState::GunGameLeader() const
{
	const ACheeseStrikePlayerState* Leader = nullptr;
	for (const APlayerState* Each : PlayerArray)
	{
		const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Each);
		if (Ours != nullptr && (Leader == nullptr || Ours->GetGunLevel() > Leader->GetGunLevel() || (Ours->GetGunLevel() == Leader->GetGunLevel() && Ours->GetKills() > Leader->GetKills())))
		{
			Leader = Ours;
		}
	}
	return Leader;
}

FString ACheeseStrikeGameState::GunGameWeaponName(int32 Level)
{
	const FWeaponStats* Weapon = CheeseStrikeArsenal::Find(CheeseStrikeArsenal::GunGameWeapon(Level));
	return Weapon != nullptr ? Weapon->DisplayName.ToString() : FString();
}

FString ACheeseStrikeGameState::ScoreSummary() const
{
	if (GetModeInfo().WinBy == CheeseStrikeModes::EWinBy::Ladder)
	{
		const ACheeseStrikePlayerState* Leader = GunGameLeader();
		return Leader != nullptr ? FString::Printf(TEXT("%s on the %s, %d of %d"), *Leader->GetPlayerName(), *GunGameWeaponName(Leader->GetGunLevel()), Leader->GetGunLevel() + 1, CheeseStrikeArsenal::GunGameLadder().Num()) : FString();
	}
	if (!HasTeams())
	{
		// Solo King of the Hill goes by the hill's points, the others by kills.
		const TArray<CheeseStrikeRules::FStanding> InOrder = GetModeInfo().WinBy == CheeseStrikeModes::EWinBy::Hill ? ACheeseStrikeHill::SoloStandings(this) : Standings();
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

float ACheeseStrikeGameState::RevealStartedFor(const APlayerState* Viewer) const
{
	const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Viewer);
	if (Ours == nullptr || RevealEndsFor(Viewer) <= 0.0f)
	{
		return 0.0f;
	}
	if (HasTeams())
	{
		return TeamRevealFrom.IsValidIndex(Ours->GetTeam()) ? TeamRevealFrom[Ours->GetTeam()] : 0.0f;
	}
	return Ours->GetRevealFrom();
}

float ACheeseStrikeGameState::RevealFor(ACheeseStrikePlayerState* Caller, float Now)
{
	if (Caller == nullptr)
	{
		return 0.0f;
	}
	// A second drone while the first is up adds all its time to the first's.
	if (HasTeams())
	{
		const int32 Team = Caller->GetTeam();
		if (!TeamRevealUntil.IsValidIndex(Team) || !TeamRevealFrom.IsValidIndex(Team))
		{
			return 0.0f;
		}
		const CheeseStrikeStreaks::FReveal Reveal = CheeseStrikeStreaks::AddReveal({TeamRevealFrom[Team], TeamRevealUntil[Team]}, Now);
		TeamRevealFrom[Team] = Reveal.From;
		TeamRevealUntil[Team] = Reveal.Until;
		return Reveal.Until;
	}
	const CheeseStrikeStreaks::FReveal Reveal = CheeseStrikeStreaks::AddReveal({Caller->GetRevealFrom(), Caller->GetRevealUntil()}, Now);
	Caller->SetReveal(Reveal.From, Reveal.Until);
	return Reveal.Until;
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
	SpeakStreak(Entry);
}

void ACheeseStrikeGameState::SpeakStreak(const FAnnouncement& Entry)
{
	// The Unmanned Aerial Cheese is spoken of: "UAC is online" for the caller's own side, "Enemy UAC online" for any other.
	if (Entry.Streak != static_cast<uint8>(CheeseStrikeStreaks::EStreak::AerialCheese) || GetNetMode() == NM_DedicatedServer || GetWorld() == nullptr)
	{
		return;
	}
	const APlayerController* Listener = GetWorld()->GetFirstPlayerController();
	const APlayerState* Hearing = Listener != nullptr ? Listener->PlayerState.Get() : nullptr;
	const APlayerState* Caller = nullptr;
	for (const APlayerState* Each : PlayerArray)
	{
		if (Each != nullptr && Each->GetPlayerName() == Entry.Who)
		{
			Caller = Each;
			break;
		}
	}
	if (Hearing == nullptr || Caller == nullptr)
	{
		return;
	}
	const bool bOurs = AreTeammates(Hearing, Caller);
	const TCHAR* Path = bOurs ? TEXT("/Game/CheeseStrike/Voice/SW_UacOnline.SW_UacOnline") : TEXT("/Game/CheeseStrike/Voice/SW_UacEnemy.SW_UacEnemy");
	if (USoundBase* Voice = LoadObject<USoundBase>(nullptr, Path))
	{
		UGameplayStatics::PlaySound2D(this, Voice, 1.0f);
	}
}
