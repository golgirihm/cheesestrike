// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "Rules/CheeseStrikeCredit.h"

#include "CheeseStrikeGameMode.h"
#include "Modes/CheeseStrikeLastStand.h"
#include "Components/CapsuleComponent.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
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
#include "Rules/CheeseStrikePodiums.h"
#include "UI/CheeseStrikePodium.h"
#include "Rules/CheeseStrikeStart.h"
#include "Sessions/CheeseStrikeSeatMemory.h"
#include "Weapons/CheeseStrikeWeapon.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeGrenades.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Map/CheeseStrikeCheese.h"
#include "Map/CheeseStrikeHill.h"
#include "Map/CheeseStrikeMouse.h"
#include "NavigationSystem.h"
#include "Streaks/CheeseStrikeAerialCheese.h"
#include "Streaks/CheeseStrikeBombs.h"
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
	Mode = CheeseStrikeModes::ModeFrom(Options);
	KillLimit = CheeseStrikeModes::KillLimitFor(Mode, Options);
	RespawnDelay = CheeseStrikeRules::RespawnDelayFrom(Options);
	RoundLimit = CheeseStrikeModes::RoundLimitFrom(Options);
	RoundTime = CheeseStrikeModes::RoundTimeFrom(Options);
	// The match waits in the lobby for the host to start it, unless told not to.
	bDelayedStart = !CheeseStrikeRules::StartsAtOnce(Options);
	BotsWanted = CheeseStrikeRules::BotsFrom(Options);
	BotSides = CheeseStrikeRules::BotSidesFrom(Options);
	MapIndex = CheeseStrikeRules::MapFrom(Options, CurrentMapIndex());
	BotSeed = CheeseStrikeRules::BotSeedFrom(Options, FMath::Rand());
	BotSkill = CheeseStrikeBots::SkillFrom(Options);
	bMice = CheeseStrikeMice::HasMice(Options);
	MiceAfter = CheeseStrikeMice::ArrivalFrom(Options);
	StartCountdown = CheeseStrikeStart::CountdownFrom(Options);
	// A match the lobby started on another map begins when everyone has come over (ReadyToStartMatch).
	PlayersExpected = CheeseStrikeRules::PlayersFrom(Options);
	if (PlayersExpected > 0)
	{
		bDelayedStart = false;
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("%s, %s, back %.1f s after a death%s, %s bots"), CheeseStrikeModes::Info(Mode).Name, *CheeseStrikeModes::Goal(Mode, KillLimit, RoundLimit), RespawnDelay, bDelayedStart ? TEXT(", waiting in the lobby") : TEXT(""), BotsWanted < 0 ? TEXT("filling in with") : *FString::FromInt(BotsWanted));
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
	Options += FString::Printf(TEXT("?Mode=%s?Rounds=%d?RoundTime=%.0f"), CheeseStrikeModes::Info(Mode).Key, RoundLimit, RoundTime);
	if (MapIndex >= 0)
	{
		Options += FString::Printf(TEXT("?Map=%d"), MapIndex);
	}
	// Always, -1 too: a travel keeps whatever options of the last URL the new one doesn't name.
	Options += FString::Printf(TEXT("?Bots=%d"), BotsWanted);
	if (BotSides.Num() > 0)
	{
		Options += TEXT("?BotSides=") + CheeseStrikeRules::BotSidesOption(BotSides);
	}
	return Options;
}

void ACheeseStrikeGameMode::CountdownEnded()
{
	// Someone left during the countdown, and the match can no longer be played: back to waiting.
	const FString WhyNot = WhyNotStart();
	if (!WhyNot.IsEmpty() && IsWaitingToStart())
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("The start is called off: %s"), *WhyNot);
		if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
		{
			State->SetCountdownEndsAt(0.0f);
		}
		return;
	}
	CountdownOver();
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
	for (const TCHAR* Option : {TEXT("KillLimit"), TEXT("RespawnDelay"), TEXT("Map"), TEXT("BotSeed"), TEXT("BotSkill"), TEXT("Bots"), TEXT("BotSides"), TEXT("Players"), TEXT("StartNow"), TEXT("Mode"), TEXT("Rounds"), TEXT("RoundTime")})
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
	if (GetMatchState() != MatchState::WaitingToStart || GetWorld() == nullptr)
	{
		return false;
	}
	// Ready: in, with the level loaded and the view able to be shown (ACheeseStrikePlayerController::ServerReady). Everyone
	// sees how many are, behind their loading cover.
	int32 Ready = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const ACheeseStrikePlayerController* Player = Cast<ACheeseStrikePlayerController>(It->Get());
		Ready += Player != nullptr && Player->HasSaidReady() ? 1 : 0;
	}
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetPlayersReady(Ready, PlayersExpected);
	}
	const float Waited = GetWorld()->GetTimeSeconds();
	if (!CheeseStrikeRules::ShouldBegin(PlayersExpected, Ready, GetNumPlayers(), Waited))
	{
		return false;
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("The match begins: %d of %d ready after %.1f s"), Ready, PlayersExpected, Waited);
	return true;
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
	for (const TCHAR* Option : {TEXT("KillLimit"), TEXT("RespawnDelay"), TEXT("Map"), TEXT("BotSeed"), TEXT("BotSkill"), TEXT("Bots"), TEXT("BotSides"), TEXT("Players"), TEXT("Mode"), TEXT("Rounds"), TEXT("RoundTime")})
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
	// Back from another of the match's levels, a player has the side and the place in the lobby's order they had;
	// a newcomer joins the side with fewer, after everyone already in.
	UCheeseStrikeSeatMemory* Memory = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikeSeatMemory>() : nullptr;
	const UCheeseStrikeSeatMemory::FSeat* Known = Memory != nullptr ? Memory->Find(State) : nullptr;
	if (Known != nullptr)
	{
		State->SetTeam(Known->Team);
		State->SetJoinOrder(Known->Order);
	}
	else
	{
		State->SetTeam(CheeseStrikeRules::TeamToJoin(Counts));
		State->SetJoinOrder(Memory != nullptr ? Memory->Add(State, State->GetTeam()).Order : Match->PlayerArray.Num());
	}
	// The host is the player at the server's own keyboard; the lobby crowns them.
	if (NewPlayer->IsLocalController())
	{
		if (ACheeseStrikeGameState* Ours = GetGameState<ACheeseStrikeGameState>())
		{
			Ours->SetHostName(State->GetPlayerName());
		}
	}
}

void ACheeseStrikeGameMode::KickPlayer(ACheeseStrikePlayerController* Player)
{
	if (!HasAuthority() || Player == nullptr)
	{
		return;
	}
	const APlayerState* Who = Player->GetPlayerState<APlayerState>();
	UE_LOG(LogCheeseStrike, Display, TEXT("%s is removed from the game by the host"), Who != nullptr ? *Who->GetPlayerName() : TEXT("someone"));
	Player->ClientKicked();
	// A moment for the word to reach them before the connection closes.
	// Removed by the engine, which also drops their player and their connection: the connection is closed here too, since
	// it stays with the player list until it is.
	const TWeakObjectPtr<UNetConnection> Link = Cast<UNetConnection>(Player->Player);
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Player, Link]()
	{
		if (IsValid(Player) && GameSession != nullptr)
		{
			GameSession->KickPlayer(Player, FText::FromString(TEXT("The host removed you from the game.")));
		}
		if (Link.IsValid())
		{
			Link->Close();
		}
	}), 0.5f, false);
}

void ACheeseStrikeGameMode::SpawnBots()
{
	ACheeseStrikeGameState* Match = GetGameState<ACheeseStrikeGameState>();
	UWorld* World = GetWorld();
	if (Match == nullptr || World == nullptr)
	{
		return;
	}
	// Both sides' bots are dealt before any comes in: a bot in is one of the roster's players, and dealing the second side
	// after the first's had come would deal it the first's again.
	TArray<CheeseStrikeRules::FRosterEntry> Bots;
	for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
	{
		for (const CheeseStrikeRules::FRosterEntry& Entry : Match->TeamColumn(Team))
		{
			if (Entry.bBot)
			{
				Bots.Add(Entry);
			}
		}
	}
	for (const CheeseStrikeRules::FRosterEntry& Entry : Bots)
	{
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
			State->SetTeam(Entry.Team);
			// After the players, in the order the lobby showed them.
			State->SetJoinOrder(CheeseStrikeRules::SeatCount * 4 + Entry.Bot);
		}
		RestartPlayer(Bot);
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
	const FString WhyNot = WhyNotStart();
	if (!WhyNot.IsEmpty())
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("The match can't start: %s"), *WhyNot);
		return;
	}
	State->SetCountdownEndsAt(State->GetServerWorldTimeSeconds() + CheeseStrikeRules::MatchCountdown);
	UE_LOG(LogCheeseStrike, Display, TEXT("The match starts in %.0f s"), CheeseStrikeRules::MatchCountdown);
	GetWorldTimerManager().SetTimer(CountdownTimer, this, &ACheeseStrikeGameMode::CountdownEnded, CheeseStrikeRules::MatchCountdown, false);
}

FString ACheeseStrikeGameMode::WhyNotStart() const
{
	const ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	return State != nullptr ? CheeseStrikeRules::WhyNotStart(State->Seats(), CheeseStrikeModes::Info(Mode).bTeams) : FString();
}

bool ACheeseStrikeGameMode::CanChangeLobby() const
{
	const ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	return IsWaitingToStart() && State != nullptr && !State->IsCountingDown();
}

bool ACheeseStrikeGameMode::SwitchSide(APlayerController* Player)
{
	ACheeseStrikePlayerState* Mine = Player != nullptr ? Player->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	const ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (Mine == nullptr || State == nullptr || !CanChangeLobby() || !CheeseStrikeModes::Info(Mode).bTeams)
	{
		return false;
	}
	const int32 To = 1 - FMath::Clamp(Mine->GetTeam(), 0, 1);
	if (!CheeseStrikeRules::HasRoomForAPlayer(State->Roster(), To))
	{
		return false;
	}
	Mine->SetTeam(To);
	if (UCheeseStrikeSeatMemory* Memory = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikeSeatMemory>() : nullptr)
	{
		Memory->SetTeam(Mine, To);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("%s goes over to %s"), *Mine->GetPlayerName(), CheeseStrikeRules::TeamNames[To]);
	return true;
}

bool ACheeseStrikeGameMode::MoveBot(int32 Bot)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr || !CanChangeLobby() || !CheeseStrikeModes::Info(Mode).bTeams)
	{
		return false;
	}
	TArray<int32> Sides = BotSides;
	if (!CheeseStrikeRules::MoveBot(State->Roster(), State->GetBotNames(), BotsWanted, Sides, Bot))
	{
		return false;
	}
	BotSides = Sides;
	State->SetBotSides(BotSides);
	UE_LOG(LogCheeseStrike, Display, TEXT("Bot %d goes over to %s"), Bot, CheeseStrikeRules::TeamNames[BotSides.IsValidIndex(Bot) ? FMath::Clamp(BotSides[Bot], 0, 1) : 0]);
	return true;
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
	// Everyone comes in held where they stand for the count (3, 2, 1, FIGHT): set before the players' characters come in
	// (in the engine's HandleMatchHasStarted), whose protection is counted from its end.
	float CountLeft = 0.0f;
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		CountLeft = StartCountdown;
		State->SetGoAt(StartCountdown > 0.0f ? State->GetServerWorldTimeSeconds() + StartCountdown : 0.0f);
		if (StartCountdown > 0.0f)
		{
			UE_LOG(LogCheeseStrike, Display, TEXT("The match counts %.0f s before it begins"), StartCountdown);
		}
	}
	Super::HandleMatchHasStarted();
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetCountdownEndsAt(0.0f);
	}
	SpawnBots();
	if (CheeseStrikeModes::Info(Mode).WinBy == CheeseStrikeModes::EWinBy::Rounds)
	{
		StartRound(true);
	}
	if (CheeseStrikeModes::Info(Mode).WinBy == CheeseStrikeModes::EWinBy::Hill)
	{
		StartHill(CountLeft);
	}
	// The mice come a minute in (counted from the end of the count), when there is cheese for them.
	const int32 Blocks = ACheeseStrikeCheese::CountEdible(GetWorld());
	if (bMice && CheeseStrikeMice::MiceFor(Blocks) > 0)
	{
		if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
		{
			State->SetMiceAt(State->GetServerWorldTimeSeconds() + CountLeft + MiceAfter);
		}
		GetWorldTimerManager().SetTimer(MiceTimer, this, &ACheeseStrikeGameMode::SpawnMice, FMath::Max(0.01f, CountLeft + MiceAfter), false);
	}
}

void ACheeseStrikeGameMode::SpawnMice()
{
	UWorld* World = GetWorld();
	if (World == nullptr || !IsMatchInProgress())
	{
		return;
	}
	const int32 Blocks = ACheeseStrikeCheese::CountEdible(World);
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
	// They eat it all in about five minutes from now, at a steady pace (CheeseStrikeMice::Pace).
	MiceCameAt = World->GetTimeSeconds();
	MiceCameTo = Blocks;
	const float FullHealth = ACheeseStrikeCheese::EdibleHealth(World);
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
			Mouse->SetSchedule(MiceCameAt, FullHealth);
			Mouse->Enter(Top, Ground.Location);
			++Came;
		}
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("The mice are here: %d for %d blocks of cheese"), Came, Blocks);
	GetWorldTimerManager().SetTimer(MiceProgressTimer, this, &ACheeseStrikeGameMode::LogMiceProgress, 10.0f, true);
}

void ACheeseStrikeGameMode::LogMiceProgress()
{
	const int32 Left = ACheeseStrikeCheese::CountEdible(GetWorld());
	const float Since = GetWorld()->GetTimeSeconds() - MiceCameAt;
	if (Left == 0)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Mice: all %d blocks eaten, %.0f s after they came"), MiceCameTo, Since);
		GetWorldTimerManager().ClearTimer(MiceProgressTimer);
		return;
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Mice: %d of %d blocks left, %.0f s after they came"), Left, MiceCameTo, Since);
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

void ACheeseStrikeGameMode::BackToLobbyNow()
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr || !HasMatchEnded() || GetMatchState() == MatchState::LeavingMap)
	{
		return;
	}
	const float At = State->GetServerWorldTimeSeconds() + CheeseStrikePodiums::SkipSeconds;
	if (State->GetLobbyAt() <= At)
	{
		return;
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Podium: the host goes back to the lobby now, %.1f s early"), State->GetLobbyAt() - At);
	State->SetLobbyAt(At);
	State->ForceNetUpdate();
	GetWorldTimerManager().SetTimer(ResultsTimer, this, &AGameMode::RestartGame, CheeseStrikePodiums::SkipSeconds, false);
}

void ACheeseStrikeGameMode::BackToLobbyMidMatch()
{
	if (!IsMatchInProgress() || GetMatchState() == MatchState::LeavingMap)
	{
		return;
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("The host ends the match and takes everyone back to the lobby"));
	GetWorldTimerManager().ClearTimer(ResultsTimer);
	RestartGame();
}

FString ACheeseStrikeGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	// The engine names a player after their machine (or their Steam account)
	// through the URL's own Name option, cut to 20 letters; `?Nick=` is theirs
	// to pick. Either is kept to Steam's 32 (CheeseStrikeRules::MaxNameLength).
	FString Nick = UGameplayStatics::ParseOption(Options, TEXT("Nick")).Left(CheeseStrikeRules::MaxNameLength);
	if (Nick.IsEmpty())
	{
		const FString Name = UGameplayStatics::ParseOption(Options, TEXT("Name")).Left(CheeseStrikeRules::MaxNameLength);
		Nick = Name.Len() > 20 ? Name : FString();
	}
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
		State->SetMode(Mode);
		State->SetRoundLimit(RoundLimit);
		// The bots' names are drawn once, so that the lobby and the match agree on them.
		State->SetBots(CheeseStrikeRules::BotNames(BotSeed, CheeseStrikeRules::Teams * CheeseStrikeRules::TeamSize), BotsWanted);
		State->SetBotSides(BotSides);
		State->SetMapIndex(MapIndex);
		State->SetBotSkill(BotSkill);
		State->SetPlayersReady(0, PlayersExpected);
	}
}

void ACheeseStrikeGameMode::SetBotsWanted(int32 Wanted)
{
	BotsWanted = Wanted < 0 ? -1 : FMath::Min(Wanted, CheeseStrikeRules::SeatCount - 1);
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

void ACheeseStrikeGameMode::Killed(ACheeseStrikeCharacter* Killer, ACheeseStrikeCharacter* Victim, bool bHead, ACheeseStrikePlayerState* By, const FString& How)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (Victim == nullptr || State == nullptr)
	{
		return;
	}
	const CheeseStrikeModes::FModeInfo& Rules = CheeseStrikeModes::Info(Mode);
	ACheeseStrikePlayerState* KillerState = By != nullptr ? By : (Killer != nullptr ? Killer->GetPlayerState<ACheeseStrikePlayerState>() : nullptr);
	ACheeseStrikePlayerState* VictimState = Victim->GetPlayerState<ACheeseStrikePlayerState>();
	if (KillerState != nullptr && KillerState != VictimState)
	{
		// A hand's kill counts toward the next streak, where the mode has streaks; a streak's doesn't.
		const CheeseStrikeStreaks::EStreak Earned = KillerState->AddKill(Rules.bStreaks && !CheeseStrikeStreaks::IsStreakName(How));
		if (Earned != CheeseStrikeStreaks::EStreak::None)
		{
			UE_LOG(LogCheeseStrike, Display, TEXT("Streak: %s has %d in a row and earns %s %s"), *KillerState->GetPlayerName(), KillerState->GetStreak(), CheeseStrikeStreaks::Info(Earned).Article, CheeseStrikeStreaks::Info(Earned).Name);
		}
		if (Rules.WinBy == CheeseStrikeModes::EWinBy::TeamKills && State->AreEnemies(KillerState, VictimState))
		{
			State->AddTeamScore(KillerState->GetTeam(), 1);
		}
		// Until there are ammunition crates to go to (docs/unreal/design.md, "Pickups"), a kill brings a magazine's worth, so that nobody runs dry.
		ACheeseStrikeCharacter* KillerBody = Killer != nullptr ? Killer : KillerState->GetPawn<ACheeseStrikeCharacter>();
		if (ACheeseStrikeWeapon* Weapon = KillerBody != nullptr ? KillerBody->GetWeapon() : nullptr)
		{
			Weapon->AddReserve(Weapon->GetStats().MagazineSize);
		}
	}
	// The assists: whoever hurt the victim lately, but not the killer. Their hits are forgotten with the death.
	const float Now = State->GetServerWorldTimeSeconds();
	for (const int32 Helper : CheeseStrikeCredit::Assisters(Victim->GetHitBy(), KillerState != nullptr ? KillerState->GetPlayerId() : -1, VictimState != nullptr ? VictimState->GetPlayerId() : -1, Now))
	{
		for (APlayerState* Each : State->PlayerArray)
		{
			if (Each != nullptr && Each->GetPlayerId() == Helper)
			{
				if (ACheeseStrikePlayerState* Assisting = Cast<ACheeseStrikePlayerState>(Each))
				{
					Assisting->AddAssist();
				}
			}
		}
	}
	Victim->ClearHitBy();
	if (VictimState != nullptr)
	{
		VictimState->AddDeath();
	}
	const FString KillerName = KillerState != nullptr ? KillerState->GetPlayerName() : (Killer != nullptr ? Killer->DisplayName() : FString());
	State->AddKill(KillerName, Victim->DisplayName(), bHead, How);
	if (KillerName.IsEmpty() && !How.IsEmpty())
	{
		// Nobody did it: a fall into a map's fondue ("fell into the fondue").
		UE_LOG(LogCheeseStrike, Display, TEXT("Kill: %s %s; %s"), *Victim->DisplayName(), *How, *State->ScoreSummary());
	}
	else
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Kill: %s killed %s%s%s; %s"), KillerName.IsEmpty() ? TEXT("nobody") : *KillerName, *Victim->DisplayName(), bHead ? TEXT(" on the head") : TEXT(""), How.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" with the %s"), *How), *State->ScoreSummary());
	}

	if (!IsMatchInProgress())
	{
		return;
	}
	switch (Rules.WinBy)
	{
	case CheeseStrikeModes::EWinBy::PlayerKills:
		if (CheeseStrikeRules::IsOver(State->MostKills(), KillLimit))
		{
			State->SetWinnerName(CheeseStrikeRules::Leader(State->Standings()));
			UE_LOG(LogCheeseStrike, Display, TEXT("Match over: %s wins"), *State->GetWinnerName());
			EndMatch();
			return;
		}
		break;
	case CheeseStrikeModes::EWinBy::TeamKills:
	{
		const int32 Scores[CheeseStrikeRules::Teams] = {State->GetTeamScore(0), State->GetTeamScore(1)};
		const int32 Winner = CheeseStrikeModes::TeamAtLimit(Scores, KillLimit);
		if (Winner != CheeseStrikeModes::NoTeam)
		{
			EndMatchFor(Winner);
			return;
		}
		break;
	}
	case CheeseStrikeModes::EWinBy::Ladder:
		if (KillerState != nullptr && KillerState != VictimState)
		{
			const int32 Steps = CheeseStrikeArsenal::GunGameLadder().Num();
			const CheeseStrikeModes::FGunGameKill Step = CheeseStrikeModes::GunGameKill(KillerState->GetGunLevel(), Steps);
			if (Step.bWins)
			{
				State->SetWinnerName(KillerState->GetPlayerName());
				UE_LOG(LogCheeseStrike, Display, TEXT("Match over: %s wins Gun Game with the %s"), *KillerState->GetPlayerName(), *ACheeseStrikeGameState::GunGameWeaponName(KillerState->GetGunLevel()));
				EndMatch();
				return;
			}
			KillerState->SetGunLevel(Step.KillerLevel);
			UE_LOG(LogCheeseStrike, Display, TEXT("Gun Game: %s up to the %s, %d of %d"), *KillerState->GetPlayerName(), *ACheeseStrikeGameState::GunGameWeaponName(Step.KillerLevel), Step.KillerLevel + 1, Steps);
			if (Step.KillerLevel == Steps - 1)
			{
				State->Announce(FString::Printf(TEXT("%s has the knife: one kill from winning"), *KillerState->GetPlayerName()), CheeseStrikeStreaks::EStreak::None, CheeseStrikeModes::NoTeam, KillerState->GetPlayerName());
			}
			// The next weapon in hand at once, once the shot that earned it is done with the one it came from.
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ACheeseStrikeGameMode::ArmForGunGame, TWeakObjectPtr<ACheeseStrikeCharacter>(KillerState->GetPawn<ACheeseStrikeCharacter>())));
		}
		break;
	case CheeseStrikeModes::EWinBy::Hill:
		// Won by the hill's points (ACheeseStrikeHill), not by kills.
		break;
	case CheeseStrikeModes::EWinBy::Rounds:
		// One life a round: out until the next.
		if (VictimState != nullptr)
		{
			VictimState->SetOut(true);
		}
		CheckRound(false);
		return;
	}
	AController* Player = Victim->GetController();
	if (Player != nullptr && Rules.bRespawns)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &ACheeseStrikeGameMode::Respawn, TWeakObjectPtr<AController>(Player)), RespawnDelay, false);
	}
}

void ACheeseStrikeGameMode::EndMatchFor(int32 Team)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr || Team < 0 || Team >= CheeseStrikeRules::Teams)
	{
		return;
	}
	State->SetWinningTeam(Team);
	State->SetWinnerName(CheeseStrikeRules::TeamNames[Team]);
	UE_LOG(LogCheeseStrike, Display, TEXT("Match over: %s win, %s"), CheeseStrikeRules::TeamNames[Team], *State->ScoreSummary());
	EndMatch();
}

void ACheeseStrikeGameMode::StartHill(float CountLeft)
{
	UWorld* World = GetWorld();
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (World == nullptr || State == nullptr)
	{
		return;
	}
	TArray<FVector> Spots;
	TArray<float> Radii;
	ACheeseStrikeHill::GatherSpots(World, Spots, Radii);
	if (Spots.Num() == 0)
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("Hill: this level has no hill spots and no starts; no hill"));
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACheeseStrikeHill* Made = World->SpawnActor<ACheeseStrikeHill>(ACheeseStrikeHill::StaticClass(), Spots[0], FRotator::ZeroRotator, Params);
	if (Made != nullptr)
	{
		Made->Begin(Spots, Radii, State->GetServerWorldTimeSeconds() + CountLeft + CheeseStrikeHills::HillTime);
		Hill = Made;
	}
}

void ACheeseStrikeGameMode::HillWon(int32 Team, ACheeseStrikePlayerState* Player)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr || !IsMatchInProgress())
	{
		return;
	}
	if (Player != nullptr)
	{
		State->SetWinnerName(Player->GetPlayerName());
		UE_LOG(LogCheeseStrike, Display, TEXT("Match over: %s wins King of the Hill with %d points"), *Player->GetPlayerName(), Player->GetHillPoints());
		EndMatch();
		return;
	}
	EndMatchFor(Team);
}

void ACheeseStrikeGameMode::CheeseStrikeEndMatch()
{
#if !UE_BUILD_SHIPPING
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr || !IsMatchInProgress())
	{
		return;
	}
	if (State->HasTeams())
	{
		const int32 First = State->GetTeamScore(0), Second = State->GetTeamScore(1);
		if (First != Second)
		{
			EndMatchFor(First > Second ? 0 : 1);
			return;
		}
	}
	else
	{
		const TArray<FPodiumPlace> Places = ACheeseStrikePodium::PlacesFor(State, nullptr);
		State->SetWinnerName(Places.IsEmpty() ? FString() : Places[0].Name);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Match over (from the console): %s, %s"), State->GetWinnerName().IsEmpty() ? TEXT("nobody wins") : *State->GetWinnerName(), *State->ScoreSummary());
	EndMatch();
#endif
}

void ACheeseStrikeGameMode::CheeseStrikeMoveHill()
{
#if !UE_BUILD_SHIPPING
	if (ACheeseStrikeHill* Ours = Hill.Get())
	{
		Ours->MoveNow();
	}
#endif
}

void ACheeseStrikeGameMode::CheeseStrikeViewHill(float Distance, float Yaw, float Pitch)
{
#if !UE_BUILD_SHIPPING
	const ACheeseStrikeHill* Ours = Hill.Get();
	const APlayerController* Host = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
	ACheeseStrikeCharacter* Body = Host != nullptr ? Cast<ACheeseStrikeCharacter>(Host->GetPawn()) : nullptr;
	if (Ours != nullptr && Body != nullptr)
	{
		const FRotator Look(-Pitch, Yaw, 0.0f);
		const FVector From = Ours->GetCentre() - Look.Vector() * Distance;
		Body->CheeseStrikeViewFrom(From.X, From.Y, From.Z, -Pitch, Yaw);
	}
#endif
}

bool ACheeseStrikeGameMode::CanHurt(const ACheeseStrikePlayerState* Attacker, const ACheeseStrikePlayerState* Victim, bool bSelf) const
{
	if (Attacker == nullptr || Victim == nullptr)
	{
		return true;
	}
	const ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	return CheeseStrikeModes::CanHurt(CheeseStrikeModes::Info(Mode).bTeams, Attacker->GetTeam(), Victim->GetTeam(), bSelf || Attacker == Victim, State != nullptr && State->IsRoundBreak());
}

void ACheeseStrikeGameMode::SetMode(CheeseStrikeModes::EMode InMode)
{
	// A mode played to kills starts from its own limit (more for teams than for one player).
	const int32 Usual = CheeseStrikeModes::Info(InMode).DefaultKillLimit;
	if (InMode != Mode && Usual > 0)
	{
		KillLimit = Usual;
	}
	Mode = InMode;
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetMode(Mode);
		State->SetLimits(KillLimit, RespawnDelay);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Mode: %s, %s"), CheeseStrikeModes::Info(Mode).Name, *CheeseStrikeModes::Goal(Mode, KillLimit, RoundLimit));
}

void ACheeseStrikeGameMode::SetRoundLimit(int32 Rounds)
{
	RoundLimit = FMath::Clamp(Rounds, 1, 15);
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetRoundLimit(RoundLimit);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Rounds: first to win %d"), RoundLimit);
}

void ACheeseStrikeGameMode::Downed(ACheeseStrikeCharacter* Victim, ACheeseStrikePlayerState* By)
{
	// Down is never out: someone down must be executed, or gets up by themself in the end, even with their whole side
	// down. The round is looked at again once the hit that put them down is done with.
	GetWorldTimerManager().SetTimerForNextTick(this, &ACheeseStrikeGameMode::CheckRoundNow);
}

void ACheeseStrikeGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);
	if (CheeseStrikeModes::Info(Mode).WinBy == CheeseStrikeModes::EWinBy::Ladder)
	{
		ArmForGunGame(Cast<ACheeseStrikeCharacter>(PlayerPawn));
	}
}

void ACheeseStrikeGameMode::ArmForGunGame(TWeakObjectPtr<ACheeseStrikeCharacter> Body)
{
	ACheeseStrikeCharacter* Character = Body.Get();
	const ACheeseStrikePlayerState* Player = Character != nullptr ? Character->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	UCheeseStrikeInventory* Inventory = Character != nullptr ? Character->GetInventory() : nullptr;
	if (Player == nullptr || Inventory == nullptr || !Character->IsAlive())
	{
		return;
	}
	// The step's weapon and nothing else: no changing to a sidearm or the knife. It never runs dry (it still reloads), so
	// that a step is never lost to an empty reserve.
	Inventory->Give(FCheeseStrikeLoadout::Only(CheeseStrikeArsenal::GunGameWeapon(Player->GetGunLevel())));
	for (int32 Slot = 0; Slot < NumWeaponSlots; ++Slot)
	{
		if (ACheeseStrikeWeapon* Weapon = Inventory->GetInSlot(static_cast<EWeaponSlot>(Slot)))
		{
			Weapon->MakeReserveEndless();
		}
	}
	// The ladder is the guns': no grenades.
	if (UCheeseStrikeGrenades* Grenades = Character->GetGrenades())
	{
		Grenades->SetCount(0);
	}
}

bool ACheeseStrikeGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	if (!Super::PlayerCanRestart_Implementation(Player))
	{
		return false;
	}
	const ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (CheeseStrikeModes::Info(Mode).WinBy != CheeseStrikeModes::EWinBy::Rounds || State == nullptr || !IsMatchInProgress() || State->GetRound() == 0)
	{
		return true;
	}
	// Into a round well under way, or between rounds: out until the next.
	const bool bLate = State->IsRoundBreak() || GetWorld()->GetTimeSeconds() - RoundStartedAt > CheeseStrikeModes::LateJoin;
	if (bLate)
	{
		if (ACheeseStrikePlayerState* Theirs = Player != nullptr ? Player->GetPlayerState<ACheeseStrikePlayerState>() : nullptr)
		{
			Theirs->SetOut(true);
		}
		return false;
	}
	return true;
}

void ACheeseStrikeGameMode::Logout(AController* Exiting)
{
	// A player who leaves the match is forgotten; one leaving with everyone for the match's next level is not.
	UCheeseStrikeSeatMemory* Memory = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikeSeatMemory>() : nullptr;
	if (Memory != nullptr && Exiting != nullptr && Exiting->PlayerState != nullptr && GetMatchState() != MatchState::LeavingMap && !Exiting->PlayerState->IsABot())
	{
		Memory->Forget(Exiting->PlayerState);
	}
	Super::Logout(Exiting);
	if (CheeseStrikeModes::Info(Mode).WinBy == CheeseStrikeModes::EWinBy::Rounds && IsMatchInProgress())
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ACheeseStrikeGameMode::CheckRoundNow);
	}
}

void ACheeseStrikeGameMode::StartRound(bool bFirst)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	UWorld* World = GetWorld();
	if (State == nullptr || World == nullptr || !IsMatchInProgress())
	{
		return;
	}
	const int32 Number = bFirst ? 1 : State->GetRound() + 1;
	RoundStartedAt = World->GetTimeSeconds();
	// The first round's count is the match's, already running; each later one has a shorter count of its own, set before
	// everyone comes back so that their protection is counted from its end. The round's clock runs from the count's end.
	const float Now = State->GetServerWorldTimeSeconds();
	if (!bFirst)
	{
		const float Count = CheeseStrikeStart::RoundCountdownFor(StartCountdown);
		State->SetGoAt(Count > 0.0f ? Now + Count : 0.0f);
	}
	const float CountLeft = FMath::Max(0.0f, State->GetGoAt() - Now);
	State->StartRound(Number, Now + CountLeft + RoundTime);
	if (!bFirst)
	{
		// Everyone back at their side's starts with full health, the living too, and the fallen cleared away.
		TArray<AController*> Everyone;
		for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
		{
			AController* Each = It->Get();
			if (Each != nullptr && Each->GetPlayerState<ACheeseStrikePlayerState>() != nullptr)
			{
				Everyone.Add(Each);
			}
		}
		for (AController* Each : Everyone)
		{
			Each->GetPlayerState<ACheeseStrikePlayerState>()->SetOut(false);
			if (APawn* Body = Each->GetPawn())
			{
				Each->UnPossess();
				Body->Destroy();
			}
		}
		TArray<ACheeseStrikeCharacter*> Fallen;
		for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
		{
			Fallen.Add(*It);
		}
		for (ACheeseStrikeCharacter* Body : Fallen)
		{
			Body->Destroy();
		}
		for (AController* Each : Everyone)
		{
			RestartPlayer(Each);
		}
	}
	GetWorldTimerManager().SetTimer(RoundTimer, this, &ACheeseStrikeGameMode::RoundTimeUp, CountLeft + RoundTime, false);
	bRoundTimeUp = false;
	GetWorldTimerManager().ClearTimer(RoundWarnTimer);
	if (const float WarnIn = CheeseStrikeModes::NextRoundWarning(RoundTime + CountLeft, CountLeft); WarnIn > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RoundWarnTimer, this, &ACheeseStrikeGameMode::WarnRoundTime, WarnIn, false);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Round %d: %s"), Number, *State->ScoreSummary());
	if (!bFirst)
	{
		State->Announce(FString::Printf(TEXT("Round %d"), Number));
	}
}

void ACheeseStrikeGameMode::WarnRoundTime()
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr || !IsMatchInProgress() || State->IsRoundBreak() || State->GetRound() == 0)
	{
		return;
	}
	// The round's clock is on the score at the bottom all along; as its end nears, everyone is told in words too.
	const float Left = State->SecondsLeftInRound();
	const FString Word = CheeseStrikeModes::RoundWarning(Left);
	if (!Word.IsEmpty())
	{
		State->Announce(Word);
		UE_LOG(LogCheeseStrike, Display, TEXT("Round %d: %s"), State->GetRound(), *Word);
	}
	if (const float WarnIn = CheeseStrikeModes::NextRoundWarning(Left, 0.0f); WarnIn > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RoundWarnTimer, this, &ACheeseStrikeGameMode::WarnRoundTime, WarnIn, false);
	}
}

void ACheeseStrikeGameMode::RoundTimeUp()
{
	// No overtime: the round is over now, a draw if both sides still have someone in (CheeseStrikeModes::RoundOver).
	bRoundTimeUp = true;
	CheckRound(true);
}

void ACheeseStrikeGameMode::GotUp(ACheeseStrikeCharacter* Body)
{
	if (bRoundTimeUp)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ACheeseStrikeGameMode::CheckRoundNow);
	}
}

void ACheeseStrikeGameMode::CheckRound(bool bTimeUp)
{
	bTimeUp = bTimeUp || bRoundTimeUp;
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr || !IsMatchInProgress() || State->IsRoundBreak() || State->GetRound() == 0)
	{
		return;
	}
	CheeseStrikeModes::FSide Sides[CheeseStrikeRules::Teams];
	for (const APlayerState* Each : State->PlayerArray)
	{
		const ACheeseStrikePlayerState* Player = Cast<ACheeseStrikePlayerState>(Each);
		if (Player == nullptr || Player->GetTeam() < 0 || Player->GetTeam() >= CheeseStrikeRules::Teams || Player->IsInactive())
		{
			continue;
		}
		CheeseStrikeModes::FSide& Side = Sides[Player->GetTeam()];
		++Side.Members;
		ACheeseStrikeCharacter* Body = Player->GetPawn<ACheeseStrikeCharacter>();
		// Still in: alive, down or not (someone down is in until executed, and gets up by themself in the end); their
		// health counts only standing, for the time's tie rule.
		if (!Player->IsOut() && Body != nullptr && Body->IsAlive())
		{
			++Side.Alive;
			Side.Health += Body->IsDowned() ? 0.0f : Body->GetHealth();
			if (Body->IsDowned())
			{
				++Side.Down;
			}
		}
	}
	int32 Winner = CheeseStrikeModes::NoTeam;
	if (!CheeseStrikeModes::RoundOver(Sides[0], Sides[1], bTimeUp, Winner))
	{
		return;
	}
	EndRound(Winner);
}

void ACheeseStrikeGameMode::EndRound(int32 Winner)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(RoundTimer);
	GetWorldTimerManager().ClearTimer(RoundWarnTimer);
	bRoundTimeUp = false;
	if (Winner != CheeseStrikeModes::NoTeam)
	{
		State->AddTeamScore(Winner, 1);
	}
	const FString Word = Winner != CheeseStrikeModes::NoTeam ? FString::Printf(TEXT("%s take round %d"), CheeseStrikeRules::TeamNames[Winner], State->GetRound()) : FString::Printf(TEXT("Round %d is a draw"), State->GetRound());
	UE_LOG(LogCheeseStrike, Display, TEXT("%s: %s"), *Word, *State->ScoreSummary());
	if (Winner != CheeseStrikeModes::NoTeam && State->GetTeamScore(Winner) >= RoundLimit)
	{
		State->EndRound(Winner, 0.0f);
		EndMatchFor(Winner);
		return;
	}
	State->EndRound(Winner, State->GetServerWorldTimeSeconds() + CheeseStrikeModes::RoundBreak);
	State->Announce(Word, CheeseStrikeStreaks::EStreak::None, Winner);
	GetWorldTimerManager().SetTimer(RoundTimer, this, &ACheeseStrikeGameMode::NextRound, CheeseStrikeModes::RoundBreak, false);
}

bool ACheeseStrikeGameMode::CallStreak(ACheeseStrikePlayerState* Caller, CheeseStrikeStreaks::EStreak Kind, const FVector& Spot, float Yaw)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	UWorld* World = GetWorld();
	const ACheeseStrikeCharacter* Body = Caller != nullptr ? Caller->GetPawn<ACheeseStrikeCharacter>() : nullptr;
	if (State == nullptr || World == nullptr || !IsMatchInProgress() || State->IsRoundBreak() || State->IsFrozen() || Body == nullptr || !Body->IsAlive() || Caller->NextStreak() != Kind)
	{
		return false;
	}
	if (Kind == CheeseStrikeStreaks::EStreak::CheeseStrike && FVector::Dist(Spot, Body->GetActorLocation()) > CheeseStrikeStreaks::StrikeRange + 500.0f)
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("Cheese Strike from %s refused: out of range"), *Caller->GetPlayerName());
		return false;
	}
	Caller->TakeStreak(Kind);
	// Calling one in is an attack: it gives up the protection of a fresh spawn.
	if (ACheeseStrikeCharacter* CallerBody = Caller->GetPawn<ACheeseStrikeCharacter>())
	{
		CallerBody->EndProtection();
	}
	const float Now = State->GetServerWorldTimeSeconds();
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	switch (Kind)
	{
	case CheeseStrikeStreaks::EStreak::AerialCheese:
	{
		// A second one while the side's first is up adds its time to it; the new drone flies until the end of it all.
		const float Until = State->RevealFor(Caller, Now);
		if (ACheeseStrikeAerialCheese* Drone = World->SpawnActor<ACheeseStrikeAerialCheese>(ACheeseStrikeAerialCheese::StaticClass(), FTransform::Identity, Parameters))
		{
			Drone->FlyOver(Now, FMath::Max(Until, Now + CheeseStrikeStreaks::RevealTime));
		}
		break;
	}
	case CheeseStrikeStreaks::EStreak::CheeseStrike:
		if (ACheeseStrikeBombs* Bombs = World->SpawnActor<ACheeseStrikeBombs>(ACheeseStrikeBombs::StaticClass(), FTransform(Spot), Parameters))
		{
			Bombs->StartStrike(Caller, Spot, Yaw);
		}
		break;
	case CheeseStrikeStreaks::EStreak::GoudaOverload:
		if (ACheeseStrikeBombs* Bombs = World->SpawnActor<ACheeseStrikeBombs>(ACheeseStrikeBombs::StaticClass(), FTransform(Body->GetActorLocation()), Parameters))
		{
			Bombs->StartOverload(Caller);
		}
		break;
	default:
		return false;
	}
	const FString Word = CheeseStrikeStreaks::CalledIn(Caller->GetPlayerName(), Kind);
	UE_LOG(LogCheeseStrike, Display, TEXT("Streak: %s"), *Word);
	State->Announce(Word, Kind, State->HasTeams() ? Caller->GetTeam() : CheeseStrikeModes::NoTeam, Caller->GetPlayerName());
	return true;
}

void ACheeseStrikeGameMode::CheeseStrikeGiveStreak(int32 Kind, const FString& Player)
{
	const ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr)
	{
		return;
	}
	// The host: the player at the server's own keyboard.
	const ACheeseStrikePlayerState* Host = nullptr;
	for (APlayerState* Each : State->PlayerArray)
	{
		const APlayerController* Controller = Each != nullptr ? Each->GetPlayerController() : nullptr;
		if (Controller != nullptr && Controller->IsLocalController())
		{
			Host = Cast<ACheeseStrikePlayerState>(Each);
		}
	}
	for (APlayerState* Each : State->PlayerArray)
	{
		ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Each);
		bool bWanted = Ours != nullptr && Ours->GetPlayerName().Equals(Player, ESearchCase::IgnoreCase);
		if (Player.IsEmpty())
		{
			bWanted = Ours != nullptr && Ours == Host;
		}
		else if (Player.Equals(TEXT("enemy"), ESearchCase::IgnoreCase))
		{
			bWanted = Ours != nullptr && Ours->GetPawn() != nullptr && State->AreEnemies(Host, Ours);
		}
		if (bWanted)
		{
			Ours->GiveStreak(CheeseStrikeStreaks::StreakAt(Kind));
			UE_LOG(LogCheeseStrike, Display, TEXT("Streak: %s is given %s"), *Ours->GetPlayerName(), CheeseStrikeStreaks::Info(CheeseStrikeStreaks::StreakAt(Kind)).Name);
			return;
		}
	}
}

void ACheeseStrikeGameMode::CheeseStrikeDown(const FString& Player)
{
#if !UE_BUILD_SHIPPING
	const ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr)
	{
		return;
	}
	const ACheeseStrikePlayerState* Host = nullptr;
	for (APlayerState* Each : State->PlayerArray)
	{
		const APlayerController* Controller = Each != nullptr ? Each->GetPlayerController() : nullptr;
		if (Controller != nullptr && Controller->IsLocalController())
		{
			Host = Cast<ACheeseStrikePlayerState>(Each);
		}
	}
	for (APlayerState* Each : State->PlayerArray)
	{
		const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Each);
		ACheeseStrikeCharacter* Body = Ours != nullptr ? Ours->GetPawn<ACheeseStrikeCharacter>() : nullptr;
		if (Body == nullptr || !Body->IsAlive() || Body->IsDowned() || Body->GetLastStand() == nullptr)
		{
			continue;
		}
		bool bWanted = Ours->GetPlayerName().Equals(Player, ESearchCase::IgnoreCase);
		if (Player.IsEmpty())
		{
			bWanted = Ours == Host;
		}
		else if (Player.Equals(TEXT("enemy"), ESearchCase::IgnoreCase))
		{
			bWanted = State->AreEnemies(Host, Ours);
		}
		else if (Player.Equals(TEXT("mate"), ESearchCase::IgnoreCase))
		{
			bWanted = Ours != Host && State->AreTeammates(Host, Ours);
		}
		if (bWanted)
		{
			Body->GetLastStand()->GoDown(nullptr);
			return;
		}
	}
#endif
}

void ACheeseStrikeGameMode::CheeseStrikeRevive(const FString& Player)
{
#if !UE_BUILD_SHIPPING
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && It->IsDowned() && It->GetLastStand() != nullptr && (Player.IsEmpty() || It->DisplayName().Equals(Player, ESearchCase::IgnoreCase)))
		{
			It->GetLastStand()->Revive();
		}
	}
#endif
}

void ACheeseStrikeGameMode::CheeseStrikeExecute(int32 Kind, const FString& Player)
{
#if !UE_BUILD_SHIPPING
	const ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	ACheeseStrikeCharacter* Executor = nullptr;
	const ACheeseStrikeCharacter* Host = nullptr;
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		const APlayerController* Controller = Cast<APlayerController>(It->GetController());
		if (Controller != nullptr && Controller->IsLocalController())
		{
			Host = *It;
		}
	}
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		const APlayerController* Controller = Cast<APlayerController>(It->GetController());
		const bool bMate = Player.Equals(TEXT("mate"), ESearchCase::IgnoreCase) && Host != nullptr && *It != Host && State != nullptr && State->AreTeammates(Host->GetPlayerState(), It->GetPlayerState());
		const bool bEnemy = Player.Equals(TEXT("enemy"), ESearchCase::IgnoreCase) && Host != nullptr && State != nullptr && State->AreEnemies(Host->GetPlayerState(), It->GetPlayerState());
		const bool bWanted = bMate || bEnemy || (Player.IsEmpty() ? Controller != nullptr && Controller->IsLocalController() : It->DisplayName().Equals(Player, ESearchCase::IgnoreCase));
		if (bWanted && It->IsAlive() && !It->IsDowned())
		{
			Executor = *It;
		}
	}
	if (Executor == nullptr || State == nullptr || Executor->GetLastStand() == nullptr)
	{
		return;
	}
	ACheeseStrikeCharacter* Nearest = nullptr;
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != Executor && It->IsAlive() && It->IsDowned() && State->AreEnemies(Executor->GetPlayerState(), It->GetPlayerState())
			&& (Nearest == nullptr || FVector::Dist(It->GetActorLocation(), Executor->GetActorLocation()) < FVector::Dist(Nearest->GetActorLocation(), Executor->GetActorLocation())))
		{
			Nearest = *It;
		}
	}
	if (Nearest != nullptr)
	{
		Executor->GetLastStand()->Execute(Nearest, CheeseStrikeExecution::KindAt(Kind));
	}
#endif
}

void ACheeseStrikeGameMode::CheeseStrikeWatchDown(float Yaw, float Distance, float Height)
{
#if !UE_BUILD_SHIPPING
	ACheeseStrikeCharacter* Host = nullptr;
	const ACheeseStrikeCharacter* Down = nullptr;
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		const APlayerController* Controller = Cast<APlayerController>(It->GetController());
		if (Controller != nullptr && Controller->IsLocalController())
		{
			Host = *It;
		}
		const UCheeseStrikeLastStand* Theirs = It->GetLastStand();
		if (It->IsAlive() && Theirs != nullptr && (Theirs->IsBeingExecuted() || (It->IsDowned() && Down == nullptr)))
		{
			Down = *It;
		}
	}
	if (Host == nullptr || Down == nullptr)
	{
		return;
	}
	const FVector Floor = Down->GetActorLocation() - FVector(0.0f, 0.0f, Down->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Chest = Floor + FVector(0.0f, 0.0f, 45.0f);
	const FVector At = Floor + FRotator(0.0f, Down->GetActorRotation().Yaw + Yaw, 0.0f).Vector() * Distance + FVector(0.0f, 0.0f, Height);
	const FRotator Look = (Chest - At).Rotation();
	Host->CheeseStrikeViewFrom(At.X, At.Y, At.Z, Look.Pitch, Look.Yaw);
#endif
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
	const bool bTeams = CheeseStrikeModes::Info(Mode).bTeams;
	const ACheeseStrikePlayerState* Mine = Player != nullptr ? Player->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	// In a team mode, the side's own starts: those the map marks for it, or without marks, its half of them. In rounds,
	// the sides swap ends each round.
	TArray<int32> Allowed;
	if (bTeams && Mine != nullptr && Mine->GetTeam() >= 0 && Mine->GetTeam() < CheeseStrikeRules::Teams)
	{
		const ACheeseStrikeGameState* Match = GetGameState<ACheeseStrikeGameState>();
		const int32 Side = CheeseStrikeModes::StartSide(Mine->GetTeam(), Match != nullptr ? Match->GetRound() : 0, CheeseStrikeModes::Info(Mode).WinBy == CheeseStrikeModes::EWinBy::Rounds);
		const FName Tag(CheeseStrikeModes::TeamStartTags[Side]);
		for (int32 Index = 0; Index < Starts.Num(); ++Index)
		{
			if (Starts[Index]->PlayerStartTag == Tag)
			{
				Allowed.Add(Index);
			}
		}
		if (Allowed.Num() == 0)
		{
			const TArray<int32> Sides = CheeseStrikeModes::SidesOf(Spots);
			for (int32 Index = 0; Index < Starts.Num(); ++Index)
			{
				if (Sides[Index] == Side)
				{
					Allowed.Add(Index);
				}
			}
		}
	}
	if (Allowed.Num() == 0)
	{
		for (int32 Index = 0; Index < Starts.Num(); ++Index)
		{
			Allowed.Add(Index);
		}
	}
	// King of the Hill: away from the hill, and from where it is about to go.
	if (const ACheeseStrikeHill* Ours = Hill.Get())
	{
		TArray<FVector> SideSpots;
		for (const int32 Index : Allowed)
		{
			SideSpots.Add(Spots[Index]);
		}
		TArray<int32> Away;
		for (const int32 Kept : CheeseStrikeHills::AwayFromHill(SideSpots, Ours->GetCentre(), CheeseStrikeHills::SpawnClear, Ours->IsWarning(), Ours->GetNextCentre()))
		{
			Away.Add(Allowed[Kept]);
		}
		Allowed = Away;
	}
	TArray<FVector> Choices;
	for (const int32 Index : Allowed)
	{
		Choices.Add(Spots[Index]);
	}
	// Furthest from the enemies, and not on top of anyone.
	TArray<FVector> Enemies;
	TArray<FVector> Others;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		if (!It->IsAlive() || It->GetController() == Player)
		{
			continue;
		}
		Others.Add(It->GetActorLocation());
		const ACheeseStrikePlayerState* Theirs = It->GetPlayerState<ACheeseStrikePlayerState>();
		if (!bTeams || Mine == nullptr || Theirs == nullptr || Theirs->GetTeam() != Mine->GetTeam())
		{
			Enemies.Add(It->GetActorLocation());
		}
	}
	const int32 Chosen = CheeseStrikeModes::ChooseStart(Choices, Enemies, Others, 120.0f);
	return Starts[Allowed[FMath::Max(0, Chosen)]];
}
