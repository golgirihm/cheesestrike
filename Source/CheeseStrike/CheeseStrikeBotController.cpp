// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBotController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Modes/CheeseStrikeLastStand.h"
#include "HAL/IConsoleManager.h"
#include "Rules/CheeseStrikeFalls.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Kismet/GameplayStatics.h"
#include "Weapons/CheeseStrikeGrenade.h"
#include "Weapons/CheeseStrikeGrenadeRules.h"
#include "Weapons/CheeseStrikeGrenades.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikePickup.h"
#include "Weapons/CheeseStrikeWeapon.h"

/** Every bot that comes in after this is set carries this primary, rather than one picked for its skill. */
static TAutoConsoleVariable<FString> CVarBotPrimary(TEXT("CheeseStrike.BotPrimary"), TEXT(""), TEXT("The primary every bot coming in carries (blaster, carbine, grater, munster, shotgun, burratas, roquefort), or empty for the spawn primary everyone has."));

namespace
{
	constexpr float ThinkInterval = 0.1f;
	/** How close to a goal counts as there. */
	constexpr float GoalReached = 150.0f;
	/** How far it roams in one go, at most: across most of a map, so that it goes everywhere in time. */
	constexpr float RoamRadius = 8000.0f;
	/** A target out of sight for longer than this is noticed afresh when seen again (its reaction runs again). */
	constexpr float ForgetsTracking = 1.0f;
	/** How near the aim must be to the target, beyond its own error, to fire (degrees). */
	constexpr float FireCone = 2.5f;
	/** Beyond this, it aims down the sights. */
	constexpr float SightsBeyond = 1500.0f;
	/** Moving slower than this (cm/s) while going somewhere is no progress. */
	constexpr float ProgressSpeed = 40.0f;
	/** Seconds after changing weapons before it changes again. */
	constexpr float ChangeRest = 0.6f;
}

ACheeseStrikeBotController::ACheeseStrikeBotController()
{
	// A bot is on the scoreboard and in the lobby like anyone else.
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
}

ACheeseStrikeCharacter* ACheeseStrikeBotController::Body() const
{
	return Cast<ACheeseStrikeCharacter>(GetPawn());
}

float ACheeseStrikeBotController::Now() const
{
	return GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
}

const TCHAR* ACheeseStrikeBotController::Doing() const
{
	const ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->CanAct())
	{
		return TEXT("idle");
	}
	if (bRetreating)
	{
		return TEXT("retreating");
	}
	if (bHill)
	{
		switch (HillPlan)
		{
		case CheeseStrikeHills::EBotPlan::HoldHill:
			return TEXT("holding the hill");
		case CheeseStrikeHills::EBotPlan::GoToNext:
			return TEXT("going to the next hill");
		default:
			return TEXT("going to the hill");
		}
	}
	if (bSeesTarget)
	{
		return TEXT("fighting");
	}
	if (bTending)
	{
		return bInteracting ? TEXT("reviving") : TEXT("going to someone down");
	}
	return bSearching ? TEXT("searching") : TEXT("roaming");
}

FString ACheeseStrikeBotController::Describe() const
{
	const ACheeseStrikeCharacter* Me = Body();
	const ACheeseStrikeWeapon* Weapon = Me != nullptr ? Me->GetWeapon() : nullptr;
	const APlayerState* State = GetPlayerState<APlayerState>();
	return FString::Printf(TEXT("%s %s, health %.0f, ammo %d/%d%s%s, speed %.0f at %s, target %s%s"),
		State != nullptr ? *State->GetPlayerName() : *GetName(), Doing(), Me != nullptr ? Me->GetHealth() : 0.0f,
		Weapon != nullptr ? Weapon->GetMagazine() : -1, Weapon != nullptr ? Weapon->GetReserve() : -1,
		Weapon != nullptr && Weapon->IsReloading() ? *(TEXT(" reloading, ") + Weapon->DescribeReload()) : TEXT(""), bTriggerHeld ? TEXT(" firing") : TEXT(""),
		Me != nullptr ? Me->GetVelocity().Size2D() : 0.0f, Me != nullptr ? *Me->GetActorLocation().ToCompactString() : TEXT("-"),
		Target.IsValid() ? *Target->DisplayName() : TEXT("none"), bSeesTarget ? TEXT(" (seen)") : TEXT(""));
}

void ACheeseStrikeBotController::InitPlayerState()
{
	Super::InitPlayerState();
	const AGameModeBase* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode() : nullptr;
	if (PlayerState != nullptr && Mode != nullptr && Mode->GameSession != nullptr)
	{
		PlayerState->SetPlayerId(Mode->GameSession->GetNextPlayerID());
	}
}

void ACheeseStrikeBotController::BeginPlay()
{
	Super::BeginPlay();
	// It comes in with the spawn primary, as everyone does, and goes for better ones on the map's stands (ThinkAboutPickups);
	// for watching one weapon in bots' hands, CheeseStrike.BotPrimary <weapon> gives them that instead.
	UWorld* World = GetWorld();
	UCheeseStrikeLoadouts* Loadouts = World != nullptr ? World->GetSubsystem<UCheeseStrikeLoadouts>() : nullptr;
	const FWeaponStats* Forced = CheeseStrikeArsenal::Find(FName(*CVarBotPrimary.GetValueOnGameThread()));
	if (Loadouts != nullptr && !Loadouts->HasChosen(this) && Forced != nullptr && Forced->Slot == EWeaponSlot::Primary)
	{
		Loadouts->Choose(this, Forced->Id);
	}
}

void ACheeseStrikeBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ForgetAll();
	if (const ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		SkillIndex = Mode->GetBotSkill();
	}
	if (InPawn != nullptr)
	{
		FaceTo(InPawn->GetActorRotation());
	}
	// On a map where a fall kills (a vat of fondue), a bot never walks off an edge; it can still fall when the floor
	// under it is eaten.
	if (const ACheeseStrikeCharacter* Me = Body())
	{
		const bool bMayDrop = !CheeseStrikeFalls::HasKillHeight(CheeseStrikeFalls::KillHeight(GetWorld()));
		Me->GetCharacterMovement()->bCanWalkOffLedges = bMayDrop;
		Me->GetCharacterMovement()->bCanWalkOffLedgesWhenCrouching = bMayDrop;
	}
	// A moment's pause on coming in, a little different for each, so that they don't all move as one.
	NextThinkAt = Now() + FMath::FRandRange(0.2f, 0.6f);
}

void ACheeseStrikeBotController::FaceTo(const FRotator& Rotation)
{
	Wanted = FRotator(Rotation.Pitch, Rotation.Yaw, 0.0f);
	SetControlRotation(Wanted);
	if (APawn* MyPawn = GetPawn())
	{
		MyPawn->FaceRotation(Wanted, 0.0f);
	}
}

void ACheeseStrikeBotController::OnUnPossess()
{
	LetGoOfTrigger();
	StopMovement();
	Super::OnUnPossess();
}

void ACheeseStrikeBotController::ForgetAll()
{
	Target = nullptr;
	bSeesTarget = false;
	bShotAtTarget = false;
	bGoing = false;
	bSearching = false;
	bRetreating = false;
	Bursts = CheeseStrikeBots::FBursts();
	bTriggerHeld = false;
	bThrowPending = false;
	MovedAt = Now();
	MovedFrom = Body() != nullptr ? Body()->GetActorLocation() : FVector::ZeroVector;
	Wobble = FMath::FRandRange(0.0f, 2.0f * PI);
}

FVector ACheeseStrikeBotController::EyesAt() const
{
	const ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr)
	{
		return FVector::ZeroVector;
	}
	// Its eyes as the server keeps them, on the capsule: the head bone's pose isn't kept up on a server nobody watches from.
	return Me->GetEyes() != nullptr ? Me->GetEyes()->GetComponentLocation() : Me->GetPawnViewLocation();
}

bool ACheeseStrikeBotController::CanSeeInto(const ACheeseStrikeCharacter* Other) const
{
	UWorld* World = GetWorld();
	if (World == nullptr || Other == nullptr)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeBotSight), false, GetPawn());
	// Other people don't hide anyone: a shot through a crowd hits the nearest.
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	const FVector From = EyesAt();
	FHitResult Hit;
	for (const FVector& To : {Other->GetEyes()->GetComponentLocation(), Other->ChestAt()})
	{
		if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
		{
			return true;
		}
	}
	return false;
}

void ACheeseStrikeBotController::Tick(float DeltaSeconds)
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->CanAct())
	{
		if (bTriggerHeld)
		{
			LetGoOfTrigger();
		}
		// Held by the count before the match or its round: going nowhere either.
		if (Me != nullptr && Me->IsFrozen())
		{
			StopMovement();
		}
		Super::Tick(DeltaSeconds);
		return;
	}
	if (Now() >= NextThinkAt)
	{
		NextThinkAt = Now() + ThinkInterval;
		Think();
	}
	AimAndFire(DeltaSeconds);
	Step(DeltaSeconds);
	// The engine's tick turns the view (UpdateControlRotation) and follows the path.
	Super::Tick(DeltaSeconds);
}

void ACheeseStrikeBotController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	APawn* MyPawn = GetPawn();
	if (MyPawn == nullptr)
	{
		return;
	}
	SetControlRotation(CheeseStrikeBots::TurnToward(GetControlRotation(), Wanted, CheeseStrikeBots::Skill(SkillIndex).TurnRate, DeltaTime));
	if (bUpdatePawn)
	{
		MyPawn->FaceRotation(ControlRotation, DeltaTime);
	}
}

bool ACheeseStrikeBotController::IsEnemy(const ACheeseStrikeCharacter* Other) const
{
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	return Match == nullptr || Other == nullptr || Match->AreEnemies(PlayerState, Other->GetPlayerState());
}

void ACheeseStrikeBotController::ThinkAboutStreaks()
{
	ACheeseStrikePlayerState* Mine = GetPlayerState<ACheeseStrikePlayerState>();
	ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	const ACheeseStrikeCharacter* Me = Body();
	const CheeseStrikeStreaks::EStreak Next = Mine != nullptr ? Mine->NextStreak() : CheeseStrikeStreaks::EStreak::None;
	if (Next == CheeseStrikeStreaks::EStreak::None || Mode == nullptr || Me == nullptr)
	{
		StreakHeldSince = -1.0f;
		return;
	}
	if (StreakHeldSince < 0.0f)
	{
		StreakHeldSince = Now();
	}
	// A moment's pause, as a player takes, then: the drone and the Overload at once; the strike on an enemy just seen, or,
	// held a while with nobody seen, on one of them wherever they are.
	if (Now() - StreakHeldSince < 1.5f)
	{
		return;
	}
	if (Next != CheeseStrikeStreaks::EStreak::CheeseStrike)
	{
		Mode->CallStreak(Mine, Next, FVector::ZeroVector, 0.0f);
		return;
	}
	FVector Spot = FVector::ZeroVector;
	bool bSpot = false;
	if (Target.IsValid() && Now() - LastSeenAt < 4.0f && FVector::Dist(LastSeenWhere, Me->GetActorLocation()) > CheeseStrikeStreaks::StrikeBlast.Radius)
	{
		Spot = LastSeenWhere;
		bSpot = true;
	}
	else if (Now() - StreakHeldSince > 12.0f)
	{
		TArray<const ACheeseStrikeCharacter*> Enemies;
		for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != Me && It->IsAlive() && IsEnemy(*It) && FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) < CheeseStrikeStreaks::StrikeRange)
			{
				Enemies.Add(*It);
			}
		}
		if (Enemies.Num() > 0)
		{
			Spot = Enemies[FMath::RandHelper(Enemies.Num())]->GetActorLocation();
			bSpot = true;
		}
	}
	if (bSpot)
	{
		Mode->CallStreak(Mine, Next, Spot, (Spot - Me->GetActorLocation()).Rotation().Yaw);
	}
}

bool ACheeseStrikeBotController::ThinkAboutTheDown()
{
	ACheeseStrikeCharacter* Me = Body();
	UCheeseStrikeLastStand* Mine = Me != nullptr ? Me->GetLastStand() : nullptr;
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const auto LetGo = [this, Mine]()
	{
		if (bInteracting && Mine != nullptr)
		{
			Mine->SetInteracting(false);
		}
		bInteracting = false;
	};
	if (Mine == nullptr || Match == nullptr || !Match->GetModeInfo().bLastStand || Me->IsDowned())
	{
		LetGo();
		return false;
	}
	// In an execution: it plays out.
	if (Mine->InExecution())
	{
		LetGo();
		return true;
	}
	// In reach of an enemy down: a press, and they are executed, whatever else is about. A teammate: held over them, still,
	// looking at them, while nobody is in sight to shoot it.
	CheeseStrikeModes::EInteract Kind = CheeseStrikeModes::EInteract::None;
	if (const ACheeseStrikeCharacter* Over = Mine->InteractTarget(Kind); Over != nullptr && (Kind == CheeseStrikeModes::EInteract::Execute || !bSeesTarget))
	{
		if (bGoing)
		{
			StopMovement();
			bGoing = false;
		}
		Wanted = (Over->GetActorLocation() - EyesAt()).Rotation();
		if (Kind == CheeseStrikeModes::EInteract::Execute)
		{
			LetGo();
			Mine->SetInteracting(true);
			Mine->SetInteracting(false);
			return true;
		}
		if (!bInteracting)
		{
			Mine->SetInteracting(true);
			bInteracting = true;
		}
		return true;
	}
	LetGo();
	// An enemy down to execute (even mid-fight, if near: left alone, they get up), else a teammate down near it, with
	// nobody in sight, to revive.
	const float Fighting = bSeesTarget && Target.IsValid() ? FVector::Dist(Target->GetActorLocation(), Me->GetActorLocation()) : TNumericLimits<float>::Max();
	const ACheeseStrikeCharacter* Nearest = nullptr;
	float NearestAt = TNumericLimits<float>::Max();
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == Me || !It->IsAlive() || !It->IsDowned() || (It->GetLastStand() != nullptr && It->GetLastStand()->InExecution()))
		{
			continue;
		}
		const float Distance = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
		const bool bWanted = IsEnemy(*It) ? CheeseStrikeBots::GoesToExecute(Distance, bSeesTarget, Fighting) : !bSeesTarget && Distance < TendRange;
		// Enemies first: an execution ends a player, a revive only saves one.
		const float Rank = IsEnemy(*It) ? Distance : Distance + CheeseStrikeBots::ExecuteRange;
		if (bWanted && Rank < NearestAt)
		{
			Nearest = *It;
			NearestAt = Rank;
		}
	}
	if (Nearest == nullptr)
	{
		return false;
	}
	// A teammate: straight to them. An enemy, who may still shoot: from outside their aim, swerving while in it.
	FVector Where = Nearest->GetActorLocation();
	bool bInAim = false;
	if (IsEnemy(Nearest))
	{
		const float Yaw = Nearest->GetActorRotation().Yaw;
		bInAim = CheeseStrikeBots::InDownedAim(Me->GetActorLocation(), Where, Yaw, CheeseStrikeBots::FlankMargin);
		Where = CheeseStrikeBots::FinishApproach(Me->GetActorLocation(), Where, Yaw, Now(), FlankSide);
	}
	if (!bGoing || FVector::Dist(Goal, Where) > 100.0f)
	{
		// Somewhere it can't get to: straight to them after all.
		if (!GoTo(Where, bInAim, true))
		{
			GoTo(Nearest->GetActorLocation(), false);
		}
	}
	return true;
}

void ACheeseStrikeBotController::Think()
{
	ACheeseStrikeCharacter* Me = Body();
	Perceive();
	bTending = ThinkAboutTheDown();
	// In an execution, dealing it or dealt it: nothing to do but let it play.
	if (Me != nullptr && Me->GetLastStand() != nullptr && Me->GetLastStand()->InExecution())
	{
		if (bGoing)
		{
			StopMovement();
			bGoing = false;
		}
		return;
	}
	// Down, it stays where it lies and fights from there, turning round as its view takes it (its feet are no use for paths).
	if (Me != nullptr && Me->IsDowned())
	{
		if (bGoing)
		{
			StopMovement();
			bGoing = false;
		}
	}
	else if (!GetAwayFromGrenades())
	{
		ChooseWhereToGo();
	}
	ChooseWeapon();
	ThinkAboutStreaks();
	ThinkAboutGrenades();
	ACheeseStrikeWeapon* Weapon = Me != nullptr ? Me->GetWeapon() : nullptr;
	if (Weapon == nullptr)
	{
		return;
	}
	// Between fights, a magazine half gone is filled.
	if (!bSeesTarget && !Weapon->IsReloading() && Weapon->GetReserve() != 0 && Weapon->GetStats().UsesRounds() && Weapon->GetMagazine() < Weapon->GetStats().MagazineSize / 2)
	{
		Me->DoReload();
	}
	// At range, down the sights; never while running.
	const bool bSights = bSeesTarget && Target.IsValid() && FVector::Dist(Me->GetActorLocation(), Target->GetActorLocation()) > SightsBeyond;
	if (bSights != Weapon->IsAimed())
	{
		Me->DoAimDownSights(bSights);
	}
}

void ACheeseStrikeBotController::Perceive()
{
	ACheeseStrikeCharacter* Me = Body();
	UWorld* World = GetWorld();
	if (Me == nullptr || World == nullptr)
	{
		return;
	}
	const FVector Eyes = EyesAt();
	const FVector Facing = GetControlRotation().Vector();
	const float Time = Now();
	ACheeseStrikeCharacter* Attacker = Me->GetLastHurtBy();
	const AGameStateBase* State = World->GetGameState();
	const float ServerTime = State != nullptr ? State->GetServerWorldTimeSeconds() : Time;
	const bool bJustHurt = Attacker != nullptr && Attacker->IsAlive() && ServerTime - Me->GetLastHurtAt() < 2.0f;

	// Whom it can see, and the best of them to fight: whoever it was fighting, then whoever is shooting it, then the nearest.
	ACheeseStrikeCharacter* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	ACheeseStrikeCharacter* Heard = nullptr;
	float HeardAt = CheeseStrikeBots::HearingRange;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		ACheeseStrikeCharacter* Other = *It;
		if (Other == Me || !Other->IsAlive() || !IsEnemy(Other))
		{
			continue;
		}
		const FVector ToOther = Other->GetEyes()->GetComponentLocation() - Eyes;
		const float Distance = ToOther.Size();
		// Someone down is no one to fight: harmed only up close, they are finished by hand (ThinkAboutTheDown goes to them
		// from outside their aim and holds Interact over them), not shot at from wherever it stands.
		if (Other->IsDowned())
		{
			continue;
		}
		// A shot nearby is heard, whoever fired it.
		const ACheeseStrikeWeapon* TheirWeapon = Other->GetWeapon();
		if (TheirWeapon != nullptr && (TheirWeapon->IsFiring() || TheirWeapon->SinceShot() < 0.5f) && Distance < HeardAt)
		{
			Heard = Other;
			HeardAt = Distance;
		}
		// Someone just come back can't be hurt: no use shooting at them.
		if (Other->IsProtected() || !CheeseStrikeBots::InSight(Facing, ToOther) || !CanSeeInto(Other))
		{
			continue;
		}
		float Score = Distance;
		if (Other == Target.Get())
		{
			Score *= 0.5f;
		}
		if (bJustHurt && Other == Attacker)
		{
			Score *= 0.3f;
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Other;
		}
	}

	const bool bWasSeeing = bSeesTarget;
	bShotByTarget = false;
	if (Best != nullptr)
	{
		// A new enemy, or the same one back in sight after a while, is noticed afresh: the reaction runs from here.
		if (Best != Target.Get() || !bWasSeeing && Time - LastSeenAt > ForgetsTracking)
		{
			if (Best != Target.Get())
			{
				bShotAtTarget = false;
			}
			TrackingSince = Time;
		}
		Target = Best;
		bShotByTarget = bJustHurt && Best == Attacker;
		bSeesTarget = true;
		bSearching = false;
		LastSeenAt = Time;
		LastSeenWhere = Best->GetActorLocation();
		return;
	}
	bSeesTarget = false;
	if (Target.IsValid() && (!Target->IsAlive() || Time - LastSeenAt > CheeseStrikeBots::SearchTime))
	{
		Target = nullptr;
		bSearching = false;
	}
	// Nobody in sight: turn to whoever is shooting it, or to a shot heard, and go looking.
	ACheeseStrikeCharacter* Lead = bJustHurt ? Attacker : Heard;
	if (Lead != nullptr && (!Target.IsValid() || Lead == Attacker))
	{
		if (Lead != Target.Get())
		{
			bShotAtTarget = false;
		}
		Target = Lead;
		LastSeenAt = Time;
		LastSeenWhere = Lead->GetActorLocation();
		bSearching = false;
	}
}

bool ACheeseStrikeBotController::GoTo(const FVector& Where, bool bSprint, bool bRightThere)
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr)
	{
		return false;
	}
	// The view is its own, turned by the bot: the path only moves the feet.
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Where, bRightThere ? 10.0f : GoalReached * 0.5f, !bRightThere, true, true, true);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		return false;
	}
	Goal = Where;
	GoalSetAt = Now();
	bGoing = Result == EPathFollowingRequestResult::RequestSuccessful;
	Me->DoSprint(bSprint);
	return true;
}

void ACheeseStrikeBotController::ChooseWhereToGo()
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr)
	{
		return;
	}
	const float Time = Now();
	const FVector Here = Me->GetActorLocation();
	const bool bFollowing = GetMoveStatus() == EPathFollowingStatus::Moving;
	if (!bFollowing)
	{
		bGoing = false;
	}
	// Stuck: going somewhere and getting nowhere (by where it is, not its velocity, which runs on into a wall). A jump, and somewhere else.
	if (!bGoing || FVector::Dist2D(Here, MovedFrom) > ProgressSpeed * CheeseStrikeBots::StuckTime * 0.5f)
	{
		MovedAt = Time;
		MovedFrom = Here;
	}
	else if (Time - MovedAt > CheeseStrikeBots::StuckTime)
	{
		Me->DoJumpStart();
		Me->DoJumpEnd();
		StopMovement();
		bGoing = false;
		bSearching = false;
		bRetreating = false;
		MovedAt = Time;
		// Tending someone down: the other way round to them for a while (ThinkAboutTheDown), rather than off elsewhere.
		if (bTending)
		{
			FlankSide = -FlankSide;
			return;
		}
		FVector Elsewhere;
		if (PickRoamGoal(Elsewhere))
		{
			GoTo(Elsewhere, false);
		}
		return;
	}
	// King of the Hill: the hill first, fighting on the way and from it.
	bHill = ThinkAboutTheHill();
	if (bHill)
	{
		return;
	}

	// Tending someone down (ThinkAboutTheDown), even in a fight: its feet are taken care of.
	if (bTending)
	{
		return;
	}
	if (bSeesTarget && Target.IsValid())
	{
		const float Distance = FVector::Dist(Here, Target->GetActorLocation());
		// No map has wedges yet: hurt or not, it fights on; with nothing left to shoot, it keeps away.
		const ACheeseStrikeWeapon* Weapon = Me->GetWeapon();
		const bool bDry = Weapon != nullptr && Weapon->IsDry();
		Move = bDry ? CheeseStrikeBots::EMove::Retreat : CheeseStrikeBots::MoveFor(Distance, Me->GetWeapon() != nullptr ? Me->GetWeapon()->GetStats().MaxRange : 5000.0f, Me->GetHealth(), false);
		Me->DoSprint(false);
		switch (Move)
		{
		case CheeseStrikeBots::EMove::Approach:
			bRetreating = false;
			// Toward them, a fresh path now and then as they move.
			if (!bGoing || Time - GoalSetAt > 0.5f)
			{
				GoTo(Target->GetActorLocation(), false);
			}
			break;
		case CheeseStrikeBots::EMove::Retreat:
		{
			FVector Away;
			if ((!bRetreating || !bGoing) && PickRetreatGoal(Target->GetActorLocation(), Away))
			{
				bRetreating = GoTo(Away, false);
			}
			break;
		}
		default:
			// Holding or backing off: the feet are its own (Step), not a path's.
			bRetreating = false;
			if (bGoing)
			{
				StopMovement();
				bGoing = false;
			}
			break;
		}
		return;
	}
	bRetreating = bRetreating && bGoing;
	if (bRetreating)
	{
		return;
	}
	// A better weapon on a stand near it, with nobody in sight: there first.
	if (ThinkAboutPickups())
	{
		return;
	}
	// Lost them: to where they were last seen, and look there.
	if (Target.IsValid())
	{
		if (!bSearching || !bGoing)
		{
			if (FVector::Dist2D(Here, LastSeenWhere) < GoalReached)
			{
				Target = nullptr;
				bSearching = false;
			}
			else
			{
				bSearching = GoTo(LastSeenWhere, false);
			}
		}
		return;
	}
	bSearching = false;
	// Nobody about: roam.
	if (!bGoing || FVector::Dist2D(Here, Goal) < GoalReached)
	{
		FVector Next;
		if (PickRoamGoal(Next))
		{
			GoTo(Next, true);
		}
	}
}

bool ACheeseStrikeBotController::ThinkAboutPickups()
{
	ACheeseStrikeCharacter* Me = Body();
	UCheeseStrikeInventory* Inventory = Me != nullptr ? Me->GetInventory() : nullptr;
	if (Inventory == nullptr || bSeesTarget || Me->IsDowned())
	{
		WantedPickup = nullptr;
		return false;
	}
	const float Time = Now();
	const FVector Here = Me->GetActorLocation();
	const ACheeseStrikeWeapon* Carried = Inventory->GetInSlot(EWeaponSlot::Primary);
	const FName Mine = Carried != nullptr ? Carried->GetWeaponId() : NAME_None;
	ACheeseStrikePickup* Best = nullptr;
	float BestWant = 0.0f;
	for (TActorIterator<ACheeseStrikePickup> It(GetWorld()); It; ++It)
	{
		ACheeseStrikePickup* Pickup = *It;
		if ((Pickup == GaveUpOn.Get() && Time < GaveUpUntil) || Pickup->HowFor(Me) == CheeseStrikePickups::ETake::None)
		{
			continue;
		}
		// The one it is going for is kept unless another is plainly better.
		const float Want = CheeseStrikePickups::BotWants(Mine, Pickup->GetWeapon(), FVector::Dist(Here, Pickup->FloorAt())) * (Pickup == WantedPickup.Get() ? 1.25f : 1.0f);
		if (Want > BestWant)
		{
			Best = Pickup;
			BestWant = Want;
		}
	}
	if (Best == nullptr)
	{
		WantedPickup = nullptr;
		return false;
	}
	// There: it presses Interact for it, as a player must (walking over it takes nothing).
	if (Best->IsInReach(Me, CheeseStrikePickups::Reach))
	{
		Inventory->TakePickup(Best);
		WantedPickup = nullptr;
		return false;
	}
	if (Best != WantedPickup.Get())
	{
		WantedPickup = Best;
		WantedSince = Time;
	}
	// Getting nowhere near it, or no way there: something else for a while.
	if (Time - WantedSince > PickupPatience || ((!bGoing || FVector::Dist(Goal, Best->FloorAt()) > 1.0f) && !GoTo(Best->FloorAt(), true, true)))
	{
		GaveUpOn = Best;
		GaveUpUntil = Time + PickupPatience;
		WantedPickup = nullptr;
		return false;
	}
	return true;
}

bool ACheeseStrikeBotController::PickRoamGoal(FVector& Out) const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ACheeseStrikeCharacter* Me = Body();
	if (Navigation == nullptr || Me == nullptr)
	{
		return false;
	}
	const FVector Here = Me->GetActorLocation();
	// Where its teammates are, and where those that are bots are going: it keeps away from both, so that a side spreads out.
	TArray<FVector> Mates;
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		const AController* Other = It->Get();
		const ACheeseStrikeCharacter* Theirs = Other != nullptr && Other != this ? Cast<ACheeseStrikeCharacter>(Other->GetPawn()) : nullptr;
		if (Theirs == nullptr || !Theirs->IsAlive() || IsEnemy(Theirs))
		{
			continue;
		}
		Mates.Add(Theirs->GetActorLocation());
		if (const ACheeseStrikeBotController* Bot = Cast<ACheeseStrikeBotController>(Other); Bot != nullptr && Bot->bGoing)
		{
			Mates.Add(Bot->Goal);
		}
	}
	const auto NearestMate = [&Mates](const FVector& Spot)
	{
		float Nearest = CheeseStrikeBots::AloneAt;
		for (const FVector& Mate : Mates)
		{
			Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist2D(Mate, Spot)));
		}
		return Nearest;
	};
	// Sometimes toward an enemy, as a player looks for a fight (their place on the mesh), else somewhere new; of a few
	// spots, the one furthest from here and from its teammates (CheeseStrikeBots::RoamScore).
	TArray<const ACheeseStrikeCharacter*> Enemies;
	if (FMath::FRand() < CheeseStrikeBots::HuntChance)
	{
		for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != Me && It->IsAlive() && IsEnemy(*It))
			{
				Enemies.Add(*It);
			}
		}
	}
	bool bFound = false;
	float Best = -1.0f;
	for (int32 Try = 0; Try < 4; ++Try)
	{
		FNavLocation Spot;
		const bool bHunt = Enemies.Num() > 0;
		const bool bGot = bHunt ? Navigation->GetRandomReachablePointInRadius(Enemies[FMath::RandHelper(Enemies.Num())]->GetActorLocation(), 800.0f, Spot)
			: Navigation->GetRandomReachablePointInRadius(Here, RoamRadius, Spot);
		if (!bGot)
		{
			continue;
		}
		const float Score = CheeseStrikeBots::RoamScore(bHunt ? CheeseStrikeBots::RoamFar : FVector::Dist2D(Here, Spot.Location), NearestMate(Spot.Location));
		if (Score > Best)
		{
			Best = Score;
			Out = Spot.Location;
			bFound = true;
		}
	}
	return bFound;
}

bool ACheeseStrikeBotController::PickRetreatGoal(const FVector& From, FVector& Out) const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ACheeseStrikeCharacter* Me = Body();
	if (Navigation == nullptr || Me == nullptr)
	{
		return false;
	}
	const FVector Here = Me->GetActorLocation();
	bool bFound = false;
	float Best = -1.0f;
	for (int32 Try = 0; Try < 6; ++Try)
	{
		FNavLocation Spot;
		if (Navigation->GetRandomReachablePointInRadius(Here, 2000.0f, Spot))
		{
			// Away from them, and not far round past them.
			const float Score = FVector::Dist2D(Spot.Location, From) - 0.5f * FVector::Dist2D(Spot.Location, Here);
			if (Score > Best)
			{
				Best = Score;
				Out = Spot.Location;
				bFound = true;
			}
		}
	}
	return bFound;
}

bool ACheeseStrikeBotController::GetAwayFromGrenades()
{
	ACheeseStrikeCharacter* Me = Body();
	UWorld* World = GetWorld();
	const ACheeseStrikeGameState* Match = World != nullptr ? World->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Me == nullptr || World == nullptr)
	{
		return false;
	}
	// The nearest grenade that can hurt it: its own, or an enemy's (a teammate's can't, friendly fire being off).
	const ACheeseStrikeGrenade* Nearest = nullptr;
	float NearestAt = TNumericLimits<float>::Max();
	for (TActorIterator<ACheeseStrikeGrenade> It(World); It; ++It)
	{
		const ACheeseStrikePlayerState* Thrower = It->GetThrower();
		const bool bCanHurt = Thrower == nullptr || Thrower == PlayerState || Match == nullptr || Match->AreEnemies(Thrower, PlayerState);
		const float Distance = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
		if (CheeseStrikeGrenades::BotGetsAway(Distance, bCanHurt, It->HasBurst()) && Distance < NearestAt)
		{
			Nearest = *It;
			NearestAt = Distance;
		}
	}
	if (Nearest == nullptr)
	{
		bFleeingGrenade = false;
		return false;
	}
	if (!bFleeingGrenade)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Grenade: %s runs from one %.0f cm off"), *Me->DisplayName(), NearestAt);
	}
	// Running, to somewhere out of its reach; a new way if the last one has ended.
	FVector Away;
	if ((!bFleeingGrenade || GetMoveStatus() != EPathFollowingStatus::Moving) && PickRetreatGoal(Nearest->GetActorLocation(), Away))
	{
		GoTo(Away, true);
		bSearching = false;
		bRetreating = false;
	}
	bFleeingGrenade = true;
	return true;
}

void ACheeseStrikeBotController::ThinkAboutGrenades()
{
	ACheeseStrikeCharacter* Me = Body();
	const UCheeseStrikeGrenades* Grenades = Me != nullptr ? Me->GetGrenades() : nullptr;
	if (Grenades == nullptr || bThrowPending || bTending || Now() < NextGrenadeThoughtAt)
	{
		return;
	}
	NextGrenadeThoughtAt = Now() + 1.0f;
	if (!Target.IsValid() || !Target->IsAlive() || !Grenades->CanThrow())
	{
		return;
	}
	CheeseStrikeGrenades::FBotThrow Thought;
	Thought.Carried = Grenades->GetCount();
	const AGameStateBase* State = GetWorld()->GetGameState();
	Thought.SinceLastThrow = (State != nullptr ? State->GetServerWorldTimeSeconds() : Now()) - Grenades->GetLastThrowAt();
	Thought.SinceSeen = Now() - LastSeenAt;
	Thought.Distance = FVector::Dist2D(LastSeenWhere, Me->GetActorLocation());
	Thought.bSeesThem = bSeesTarget;
	Thought.Roll = FMath::FRand();
	if (!CheeseStrikeGrenades::BotShouldThrow(Thought))
	{
		return;
	}
	// Onto the floor where they were (or are).
	const FVector Where = bSeesTarget ? Target->GetActorLocation() : LastSeenWhere;
	FVector Spot = Where - FVector(0.0f, 0.0f, 88.0f);
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeBotGrenadeFloor), false, Me);
	Params.AddIgnoredActor(Target.Get());
	if (GetWorld()->LineTraceSingleByChannel(Floor, Where, Where - FVector(0.0f, 0.0f, 400.0f), ECC_Visibility, Params))
	{
		Spot = Floor.ImpactPoint;
	}
	// Short of them, toward here, as it bounces and rolls on.
	const FVector Toward = (Me->GetActorLocation() - Spot).GetSafeNormal2D();
	const FVector Landing = Spot + Toward * CheeseStrikeGrenades::BotLandingShort(FVector::Dist2D(Spot, Me->GetActorLocation()));
	FVector Aim;
	if (AimToLandAt(Landing, Aim))
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Grenade: %s means one for %s at (%.0f, %.0f, %.0f), %.0f cm off"), *Me->DisplayName(), *Target->DisplayName(), Spot.X, Spot.Y, Spot.Z, FVector::Dist2D(Spot, Me->GetActorLocation()));
		bThrowPending = true;
		ThrowAim = Aim;
		ThrowPendingSince = Now();
		LetGoOfTrigger();
	}
}

bool ACheeseStrikeBotController::AimToLandAt(const FVector& Spot, FVector& OutAim) const
{
	const ACheeseStrikeCharacter* Me = Body();
	const UCheeseStrikeGrenades* Grenades = Me != nullptr ? Me->GetGrenades() : nullptr;
	if (Grenades == nullptr)
	{
		return false;
	}
	// Tries the throws from low to high, along the grenade's own arc against the world, and keeps the one that comes
	// down nearest the spot.
	const float Yaw = (Spot - Me->GetActorLocation()).Rotation().Yaw;
	float Best = CheeseStrikeGrenades::BotLandsWithin;
	bool bFound = false;
	for (float Pitch = -15.0f; Pitch <= 60.0f; Pitch += 2.5f)
	{
		const FVector Aim = FRotator(Pitch, Yaw, 0.0f).Vector();
		FPredictProjectilePathResult Result;
		if (!Grenades->PredictPath(Aim, Result, 15.0f))
		{
			continue;
		}
		// Never one that comes down within reach of itself.
		const float Off = FVector::Dist(Result.HitResult.ImpactPoint, Spot);
		if (Off < Best && FVector::Dist(Result.HitResult.ImpactPoint, Me->GetActorLocation()) > CheeseStrikeGrenades::BotOwnMargin)
		{
			Best = Off;
			OutAim = Aim;
			bFound = true;
		}
	}
	return bFound;
}

void ACheeseStrikeBotController::AimAndFire(float DeltaSeconds)
{
	ACheeseStrikeCharacter* Me = Body();
	ACheeseStrikeWeapon* Weapon = Me != nullptr ? Me->GetWeapon() : nullptr;
	const float Time = Now();
	const CheeseStrikeBots::FSkill& Skill = CheeseStrikeBots::Skill(SkillIndex);
	const FVector Eyes = EyesAt();
	bool bWantsToFire = false;
	if (bSeesTarget && Target.IsValid() && Target->IsAlive())
	{
		// At their chest, off by the error its tracking has steadied to, the error drifting round.
		const FVector Chest = Target->ChestAt();
		const FRotator Straight = (Chest - Eyes).Rotation();
		const float Tracking = Time - TrackingSince;
		const float Error = CheeseStrikeBots::AimError(Skill, Tracking);
		Wobble += DeltaSeconds * 1.7f;
		Wanted = FRotator(Straight.Pitch + Error * 0.6f * FMath::Sin(Wobble * 1.3f), Straight.Yaw + Error * FMath::Cos(Wobble), 0.0f);
		// It fires once its reaction is over, when the aim is near enough, at what is in reach.
		const float Off = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(GetControlRotation().Vector(), Straight.Vector()), -1.0f, 1.0f)));
		const float Range = Weapon != nullptr ? Weapon->GetStats().MaxRange : 5000.0f;
		const bool bHasAmmo = Weapon != nullptr && !Weapon->IsDry();
		const bool bInReach = Weapon != nullptr && Weapon->GetStats().IsMelee()
			? CheeseStrikeBots::WouldStrikeAt(FVector::Dist(Eyes, Chest), Weapon->GetStats().Reach)
			: CheeseStrikeBots::WouldFireAt(FVector::Dist(Eyes, Chest), Range, bShotByTarget);
		bWantsToFire = bHasAmmo && Tracking >= Skill.Reaction && Off <= Error + FireCone && bInReach;
	}
	else if (Target.IsValid())
	{
		// Toward where they were.
		const FRotator Toward = (LastSeenWhere - Eyes).Rotation();
		Wanted = FRotator(FMath::Clamp(Toward.Pitch, -20.0f, 20.0f), Toward.Yaw, 0.0f);
	}
	else if (Me != nullptr && Me->GetVelocity().Size2D() > 50.0f)
	{
		// Where it is going: its heading eased in, since the velocity's own direction swings about with every step up a stair or
		// round a corner, and a view that follows it shakes.
		const float Heading = Me->GetVelocity().Rotation().Yaw;
		const float Ease = 1.0f - FMath::Exp(-DeltaSeconds * 3.0f);
		WalkingYaw = FRotator::NormalizeAxis(WalkingYaw + FRotator::NormalizeAxis(Heading - WalkingYaw) * Ease);
		Wanted = FRotator(0.0f, WalkingYaw, 0.0f);
	}
	else
	{
		// Standing: the next walk starts from where it faces.
		WalkingYaw = GetControlRotation().Yaw;
	}
	// A throw decided on: it turns to it, and lets go once it faces it (or has tried a moment).
	if (bThrowPending && Me != nullptr)
	{
		Wanted = ThrowAim.Rotation();
		const float Off = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(GetControlRotation().Vector(), ThrowAim), -1.0f, 1.0f)));
		if (Off <= 4.0f || Time - ThrowPendingSince > 1.0f)
		{
			bThrowPending = false;
			if (UCheeseStrikeGrenades* Grenades = Me->GetGrenades())
			{
				Grenades->Throw(ThrowAim);
			}
		}
		bWantsToFire = false;
	}
	if (Me == nullptr || Weapon == nullptr)
	{
		return;
	}
	const bool bHold = CheeseStrikeBots::HoldsTrigger(Bursts, Skill, bWantsToFire && !Weapon->IsReloading(), Weapon->GetStats().FireInterval, Time);
	if (bHold && !bShotAtTarget)
	{
		bShotAtTarget = true;
		LastReaction = Time - TrackingSince;
	}
	if (!Weapon->GetStats().bAutomatic)
	{
		// A weapon that fires a shot (or a burst, or a stroke) a pull is pulled each time it is ready.
		if (bTriggerHeld)
		{
			bTriggerHeld = false;
			Me->DoFire(false);
		}
		if (bHold && Weapon->IsReady())
		{
			Me->DoFire(true);
			Me->DoFire(false);
		}
		return;
	}
	if (bHold != bTriggerHeld)
	{
		bTriggerHeld = bHold;
		Me->DoFire(bHold);
	}
}

void ACheeseStrikeBotController::ChooseWeapon()
{
	ACheeseStrikeCharacter* Me = Body();
	UCheeseStrikeInventory* Inventory = Me != nullptr ? Me->GetInventory() : nullptr;
	EWeaponSlot Current;
	if (Inventory == nullptr || !Inventory->GetHeldSlot(Current) || Now() - ChangedWeaponAt < ChangeRest)
	{
		return;
	}
	CheeseStrikeBots::FArmed Armed;
	Armed.bPrimary = Inventory->CanDraw(EWeaponSlot::Primary);
	Armed.bSidearm = Inventory->CanDraw(EWeaponSlot::Sidearm);
	Armed.bKnife = Inventory->CanDraw(EWeaponSlot::Knife);
	if (const ACheeseStrikeWeapon* Primary = Inventory->GetInSlot(EWeaponSlot::Primary))
	{
		Armed.PrimaryMagazine = Primary->GetMagazine();
		Armed.bPrimaryReserve = Primary->GetReserve() != 0;
	}
	const bool bFighting = bSeesTarget && Target.IsValid();
	const float Distance = bFighting ? FVector::Dist(Me->GetActorLocation(), Target->GetActorLocation()) : 0.0f;
	const EWeaponSlot Best = CheeseStrikeBots::ChooseWeapon(Armed, bFighting, Distance, Current);
	if (Best != Current)
	{
		LetGoOfTrigger();
		Me->DoDrawWeapon(Best);
		ChangedWeaponAt = Now();
	}
}

void ACheeseStrikeBotController::Step(float DeltaSeconds)
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !bSeesTarget || !Target.IsValid() || bGoing || Me->IsDowned())
	{
		return;
	}
	const float Time = Now();
	if (Time >= NextStepAt)
	{
		NextStepAt = Time + FMath::FRandRange(CheeseStrikeBots::StepMin, CheeseStrikeBots::StepMax);
		StepSide = FMath::RandBool() ? 1.0f : -1.0f;
	}
	const FVector Away = (Me->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	FVector Side = FVector::CrossProduct(FVector::UpVector, Away) * StepSide;
	// Not off the mesh: a step toward a wall or a drop goes the other way, and with neither way clear (a narrow path
	// over the fondue), it stands; it backs off only where there is room behind.
	bool bSideClear = true;
	bool bBackClear = true;
	if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FVector Blocked;
		const FVector Feet = Me->GetNavAgentLocation();
		if (Navigation->NavigationRaycast(GetWorld(), Feet, Feet + Side * 150.0f, Blocked))
		{
			StepSide = -StepSide;
			Side = -Side;
			NextStepAt = Time + CheeseStrikeBots::StepMin;
			bSideClear = !Navigation->NavigationRaycast(GetWorld(), Feet, Feet + Side * 150.0f, Blocked);
		}
		bBackClear = !Navigation->NavigationRaycast(GetWorld(), Feet, Feet + Away * 150.0f, Blocked);
	}
	if (bSideClear)
	{
		Me->AddMovementInput(Side, 1.0f);
	}
	if (Move == CheeseStrikeBots::EMove::BackOff && bBackClear)
	{
		Me->AddMovementInput(Away, 1.0f);
	}
}

void ACheeseStrikeBotController::LetGoOfTrigger()
{
	bTriggerHeld = false;
	Bursts = CheeseStrikeBots::FBursts();
	if (ACheeseStrikeCharacter* Me = Body())
	{
		Me->DoFire(false);
	}
}
