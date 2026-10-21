// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "CheeseStrikeGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "CheeseStrike.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "Rules/CheeseStrikeRules.h"
#include "Weapons/CheeseStrikeWeapon.h"
#include "Map/CheeseStrikeCheese.h"
#include "Map/CheeseStrikeMouse.h"
#include "NavigationSystem.h"
#include "UI/CheeseStrikeHUD.h"
#include "Variant_Shooter/AI/ShooterNPC.h"
#include "Variant_Shooter/AI/ShooterNPCSpawner.h"

ACheeseStrikeGameMode::ACheeseStrikeGameMode()
{
	DefaultPawnClass = ACheeseStrikeCharacter::StaticClass();
	PlayerControllerClass = ACheeseStrikePlayerController::StaticClass();
	PlayerStateClass = ACheeseStrikePlayerState::StaticClass();
	GameStateClass = ACheeseStrikeGameState::StaticClass();
	HUDClass = ACheeseStrikeHUD::StaticClass();
	KillLimit = CheeseStrikeRules::KillLimit;
	RespawnDelay = CheeseStrikeRules::RespawnDelay;
}

void ACheeseStrikeGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	KillLimit = CheeseStrikeRules::KillLimitFrom(Options);
	RespawnDelay = CheeseStrikeRules::RespawnDelayFrom(Options);
	// The match waits in the lobby for the host to start it, unless told not to.
	bDelayedStart = !CheeseStrikeRules::StartsAtOnce(Options);
	BotsWanted = CheeseStrikeRules::BotsFrom(Options);
	MapIndex = CheeseStrikeRules::MapFrom(Options, CurrentMapIndex());
	BotSeed = CheeseStrikeRules::BotSeedFrom(Options, FMath::Rand());
	BotSkill = CheeseStrikeBots::SkillFrom(Options);
	bMice = CheeseStrikeMice::HasMice(Options);
	MiceAfter = CheeseStrikeMice::ArrivalFrom(Options);
	// A match the lobby started on another map begins when everyone has come over (ReadyToStartMatch).
	PlayersExpected = CheeseStrikeRules::PlayersFrom(Options);
	if (PlayersExpected > 0)
	{
		bDelayedStart = false;
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Free-for-all to %d kills, back %.1f s after a death%s, %s bots"), KillLimit, RespawnDelay, bDelayedStart ? TEXT(", waiting in the lobby") : TEXT(""), BotsWanted < 0 ? TEXT("filling in with") : *FString::FromInt(BotsWanted));
	// The template's level comes with spawners for its own enemies, who would wander about the menu's view and the match: none of them.
	for (TActorIterator<AShooterNPCSpawner> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<AShooterNPC> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
}

FString ACheeseStrikeGameMode::TravelTo(const FURL& Url)
{
	// The map and the options only: travel is relative to where the server is, so it keeps listening on its port, and a URL
	// naming the host and port (":17777/Game/...") is refused by the engine (CanServerTravel allows no ':').
	FString To = Url.Map;
	for (const FString& Option : Url.Op)
	{
		To += TEXT("?") + Option;
	}
	return To;
}

int32 ACheeseStrikeGameMode::CurrentMapIndex() const
{
	return GetWorld() != nullptr ? CheeseStrikeRules::MapIndexFor(GetWorld()->GetOutermost()->GetName()) : INDEX_NONE;
}

void ACheeseStrikeGameMode::SetRules(int32 InKillLimit, float InRespawnDelay, int32 InMapIndex)
{
	KillLimit = InKillLimit;
	RespawnDelay = InRespawnDelay;
	if (InMapIndex >= 0 && InMapIndex < CheeseStrikeRules::NumMaps())
	{
		MapIndex = InMapIndex;
	}
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetLimits(KillLimit, RespawnDelay);
		State->SetMapIndex(MapIndex);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Rules: to %d kills, back %.1f s after a death, on %s"), KillLimit, RespawnDelay, MapIndex >= 0 ? CheeseStrikeRules::Maps[MapIndex].Name : TEXT("this level"));
}

FString ACheeseStrikeGameMode::RulesOptions() const
{
	FString Options = FString::Printf(TEXT("?KillLimit=%d?RespawnDelay=%.1f?BotSeed=%d?BotSkill=%d"), KillLimit, RespawnDelay, BotSeed, BotSkill);
	if (MapIndex >= 0)
	{
		Options += FString::Printf(TEXT("?Map=%d"), MapIndex);
	}
	// Always, -1 too: a travel keeps whatever options of the last URL the new one doesn't name.
	Options += FString::Printf(TEXT("?Bots=%d"), BotsWanted);
	return Options;
}

void ACheeseStrikeGameMode::CountdownOver()
{
	if (MapIndex < 0 || MapIndex == CurrentMapIndex() || GetWorld() == nullptr)
	{
		StartMatch();
		return;
	}
	// Everyone goes to the map the host picked, and the match starts there once they have all arrived. The URL is this
	// level's with the map changed, so that the host listens where the players reconnect (the same port).
	FURL Url = GetWorld()->URL;
	Url.Map = CheeseStrikeRules::Maps[MapIndex].Level;
	for (const TCHAR* Option : {TEXT("KillLimit"), TEXT("RespawnDelay"), TEXT("Map"), TEXT("BotSeed"), TEXT("BotSkill"), TEXT("Bots"), TEXT("Players"), TEXT("StartNow")})
	{
		Url.RemoveOption(Option);
	}
	TArray<FString> Each;
	(RulesOptions() + FString::Printf(TEXT("?Players=%d"), FMath::Max(1, GetNumPlayers()))).ParseIntoArray(Each, TEXT("?"));
	for (const FString& Option : Each)
	{
		Url.AddOption(*Option);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("To %s: %s"), CheeseStrikeRules::Maps[MapIndex].Name, *Url.ToString());
	// The countdown is left at its end, not cleared: the lobby says "Starting" until the travel takes over, rather than
	// offering Start the Match again for the moment before it does.
	GetWorld()->ServerTravel(TravelTo(Url), GetTravelType());
}

bool ACheeseStrikeGameMode::ReadyToStartMatch_Implementation()
{
	if (PlayersExpected <= 0)
	{
		return Super::ReadyToStartMatch_Implementation();
	}
	const bool bEveryone = GetNumPlayers() >= PlayersExpected;
	const bool bWaitedEnough = GetWorld() != nullptr && GetWorld()->GetTimeSeconds() > CheeseStrikeRules::WaitForPlayers;
	return GetMatchState() == MatchState::WaitingToStart && GetNumPlayers() > 0 && (bEveryone || bWaitedEnough);
}

void ACheeseStrikeGameMode::RestartGame()
{
	if (GameSession == nullptr || !GameSession->CanRestartGame() || GetMatchState() == MatchState::LeavingMap || GetWorld() == nullptr)
	{
		return;
	}
	// The engine's restart would travel by the URL the match was hosted with; this one carries the rules as the host left them,
	// and goes back to the lobby on this map even if the match came here from another.
	FURL Url = GetWorld()->URL;
	for (const TCHAR* Option : {TEXT("KillLimit"), TEXT("RespawnDelay"), TEXT("Map"), TEXT("BotSeed"), TEXT("BotSkill"), TEXT("Bots"), TEXT("Players")})
	{
		Url.RemoveOption(Option);
	}
	// Back to waiting in the lobby: no count of players to wait for, which the last URL had if the match came from another map.
	FString Rules = RulesOptions() + TEXT("?Players=0");
	TArray<FString> Each;
	Rules.ParseIntoArray(Each, TEXT("?"));
	for (const FString& Option : Each)
	{
		Url.AddOption(*Option);
	}
	GetWorld()->ServerTravel(TravelTo(Url), GetTravelType());
}

void ACheeseStrikeGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ACheeseStrikePlayerState* State = NewPlayer != nullptr ? NewPlayer->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	const ACheeseStrikeGameState* Match = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr || Match == nullptr)
	{
		return;
	}
	int32 Counts[CheeseStrikeRules::Teams] = {};
	for (const APlayerState* Each : Match->PlayerArray)
	{
		const ACheeseStrikePlayerState* Other = Cast<ACheeseStrikePlayerState>(Each);
		if (Other != nullptr && Other != State && Other->GetTeam() >= 0 && Other->GetTeam() < CheeseStrikeRules::Teams)
		{
			++Counts[Other->GetTeam()];
		}
	}
	State->SetTeam(CheeseStrikeRules::TeamToJoin(Counts));
	// The host is the player at the server's own keyboard; the lobby crowns them.
	if (NewPlayer->IsLocalController())
	{
		if (ACheeseStrikeGameState* Ours = GetGameState<ACheeseStrikeGameState>())
		{
			Ours->SetHostName(State->GetPlayerName());
		}
	}
}

void ACheeseStrikeGameMode::SpawnBots()
{
	ACheeseStrikeGameState* Match = GetGameState<ACheeseStrikeGameState>();
	UWorld* World = GetWorld();
	if (Match == nullptr || World == nullptr)
	{
		return;
	}
	for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
	{
		for (const CheeseStrikeRules::FRosterEntry& Entry : Match->TeamColumn(Team))
		{
			if (!Entry.bBot)
			{
				continue;
			}
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ACheeseStrikeBotController* Bot = World->SpawnActor<ACheeseStrikeBotController>(ACheeseStrikeBotController::StaticClass(), FTransform::Identity, Parameters);
			if (Bot == nullptr)
			{
				continue;
			}
			if (ACheeseStrikePlayerState* State = Bot->GetPlayerState<ACheeseStrikePlayerState>())
			{
				State->SetPlayerName(Entry.Name);
				State->SetIsABot(true);
				State->SetTeam(Team);
			}
			RestartPlayer(Bot);
		}
	}
}

bool ACheeseStrikeGameMode::IsWaitingToStart() const
{
	return GetMatchState() == MatchState::WaitingToStart;
}

void ACheeseStrikeGameMode::AskToStart(bool bAtOnce)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (!IsWaitingToStart() || State == nullptr || State->IsCountingDown())
	{
		return;
	}
	if (bAtOnce)
	{
		CountdownOver();
		return;
	}
	State->SetCountdownEndsAt(State->GetServerWorldTimeSeconds() + CheeseStrikeRules::MatchCountdown);
	UE_LOG(LogCheeseStrike, Display, TEXT("The match starts in %.0f s"), CheeseStrikeRules::MatchCountdown);
	GetWorldTimerManager().SetTimer(CountdownTimer, this, &ACheeseStrikeGameMode::CountdownOver, CheeseStrikeRules::MatchCountdown, false);
}

void ACheeseStrikeGameMode::CancelStart()
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (!IsWaitingToStart() || State == nullptr || !State->IsCountingDown())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(CountdownTimer);
	State->SetCountdownEndsAt(0.0f);
	UE_LOG(LogCheeseStrike, Display, TEXT("The start is called off"));
}

void ACheeseStrikeGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetCountdownEndsAt(0.0f);
	}
	SpawnBots();
	// The mice come a minute in, when there is cheese for them.
	int32 Blocks = 0;
	for (TActorIterator<ACheeseStrikeCheese> It(GetWorld()); It; ++It)
	{
		++Blocks;
	}
	if (bMice && CheeseStrikeMice::MiceFor(Blocks) > 0)
	{
		if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
		{
			State->SetMiceAt(State->GetServerWorldTimeSeconds() + MiceAfter);
		}
		GetWorldTimerManager().SetTimer(MiceTimer, this, &ACheeseStrikeGameMode::SpawnMice, FMath::Max(0.01f, MiceAfter), false);
	}
}

void ACheeseStrikeGameMode::SpawnMice()
{
	UWorld* World = GetWorld();
	if (World == nullptr || !IsMatchInProgress())
	{
		return;
	}
	int32 Blocks = 0;
	for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
	{
		++Blocks;
	}
	const int32 Count = CheeseStrikeMice::MiceFor(Blocks);
	// Where they come in: the map's MouseEntry points (on top of its edge, facing in), shuffled.
	TArray<AActor*> Entries;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("MouseEntry"))))
		{
			Entries.Add(*It);
		}
	}
	if (Entries.Num() == 0)
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("No MouseEntry points on this map: no mice"));
		return;
	}
	for (int32 Index = Entries.Num() - 1; Index > 0; --Index)
	{
		Entries.Swap(Index, FMath::RandHelper(Index + 1));
	}
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	int32 Came = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const AActor* Entry = Entries[Index % Entries.Num()];
		const FVector Top = Entry->GetActorLocation();
		// Down to the ground some way in from the edge.
		FNavLocation Ground;
		const FVector Inside = Top + Entry->GetActorForwardVector() * (500.0f + 150.0f * (Index / Entries.Num()));
		if (Navigation == nullptr || !Navigation->ProjectPointToNavigation(Inside, Ground, FVector(300.0f, 300.0f, 2000.0f)))
		{
			continue;
		}
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ACheeseStrikeMouse* Mouse = World->SpawnActor<ACheeseStrikeMouse>(ACheeseStrikeMouse::StaticClass(), FTransform(Top), Parameters))
		{
			Mouse->Enter(Top, Ground.Location);
			++Came;
		}
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("The mice are here: %d for %d blocks of cheese"), Came, Blocks);
}

void ACheeseStrikeGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
	// The results stay up a while; then the level is played again from the lobby, the session kept.
	GetWorldTimerManager().SetTimer(ResultsTimer, this, &AGameMode::RestartGame, CheeseStrikeRules::ResultsTime, false);
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetLobbyAt(State->GetServerWorldTimeSeconds() + CheeseStrikeRules::ResultsTime);
	}
}

FString ACheeseStrikeGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	// The engine names a player after their machine (or, later, their Steam
	// account) through the URL's own Name option; this one is theirs to pick.
	const FString Nick = UGameplayStatics::ParseOption(Options, TEXT("Nick")).Left(20);
	if (!Nick.IsEmpty() && NewPlayerController != nullptr)
	{
		ChangeName(NewPlayerController, Nick, false);
	}
	return Error;
}

void ACheeseStrikeGameMode::InitGameState()
{
	Super::InitGameState();
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetLimits(KillLimit, RespawnDelay);
		// The bots' names are drawn once, so that the lobby and the match agree on them.
		State->SetBots(CheeseStrikeRules::BotNames(BotSeed, CheeseStrikeRules::Teams * CheeseStrikeRules::TeamSize), BotsWanted);
		State->SetMapIndex(MapIndex);
		State->SetBotSkill(BotSkill);
	}
}

void ACheeseStrikeGameMode::SetBotsWanted(int32 Wanted)
{
	BotsWanted = Wanted < 0 ? -1 : FMath::Min(Wanted, CheeseStrikeRules::Teams * CheeseStrikeRules::TeamSize);
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetBots(State->GetBotNames(), BotsWanted);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Bots: %s"), BotsWanted < 0 ? TEXT("every empty seat") : *FString::FromInt(BotsWanted));
}

void ACheeseStrikeGameMode::SetBotSkill(int32 Skill)
{
	BotSkill = FMath::Clamp(Skill, 0, CheeseStrikeBots::NumSkills - 1);
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetBotSkill(BotSkill);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Bots play at %s"), CheeseStrikeBots::Skill(BotSkill).Name);
}

void ACheeseStrikeGameMode::Killed(ACheeseStrikeCharacter* Killer, ACheeseStrikeCharacter* Victim, bool bHead)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (Victim == nullptr || State == nullptr)
	{
		return;
	}
	ACheeseStrikePlayerState* KillerState = Killer != nullptr ? Killer->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	ACheeseStrikePlayerState* VictimState = Victim->GetPlayerState<ACheeseStrikePlayerState>();
	if (KillerState != nullptr && KillerState != VictimState)
	{
		KillerState->AddKill();
		// Until there are ammunition crates to go to (docs/unreal/design.md, "Pickups"), a kill brings a magazine's worth, so that nobody runs dry.
		if (ACheeseStrikeWeapon* Weapon = Killer->GetWeapon())
		{
			Weapon->AddReserve(Weapon->GetStats().MagazineSize);
		}
	}
	if (VictimState != nullptr)
	{
		VictimState->AddDeath();
	}
	State->AddKill(Killer != nullptr ? Killer->DisplayName() : FString(), Victim->DisplayName(), bHead);
	UE_LOG(LogCheeseStrike, Display, TEXT("Kill: %s killed %s%s; %d of %d"), Killer != nullptr ? *Killer->DisplayName() : TEXT("nobody"), *Victim->DisplayName(), bHead ? TEXT(" on the head") : TEXT(""), State->MostKills(), KillLimit);

	if (IsMatchInProgress() && CheeseStrikeRules::IsOver(State->MostKills(), KillLimit))
	{
		State->SetWinnerName(CheeseStrikeRules::Leader(State->Standings()));
		UE_LOG(LogCheeseStrike, Display, TEXT("Match over: %s wins"), *State->GetWinnerName());
		EndMatch();
		return;
	}
	if (AController* Player = Victim->GetController())
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &ACheeseStrikeGameMode::Respawn, TWeakObjectPtr<AController>(Player)), RespawnDelay, false);
	}
}

void ACheeseStrikeGameMode::Respawn(TWeakObjectPtr<AController> Who)
{
	AController* Player = Who.Get();
	if (Player == nullptr || !IsMatchInProgress())
	{
		return;
	}
	APawn* Body = Player->GetPawn();
	Player->UnPossess();
	RestartPlayer(Player);
	if (Body != nullptr)
	{
		// The body lies there a while longer.
		Body->SetLifeSpan(5.0f);
	}
}

AActor* ACheeseStrikeGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	TArray<APlayerStart*> Starts;
	TArray<FVector> Spots;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Starts.Add(*It);
		Spots.Add(It->GetActorLocation());
	}
	if (Starts.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}
	TArray<FVector> Enemies;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		if (It->IsAlive() && It->GetController() != Player)
		{
			Enemies.Add(It->GetActorLocation());
		}
	}
	return Starts[CheeseStrikeRules::FurthestFrom(Spots, Enemies)];
}
