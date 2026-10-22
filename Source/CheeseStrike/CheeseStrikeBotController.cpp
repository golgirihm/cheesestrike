// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBotController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Modes/CheeseStrikeLastStand.h"
#include "HAL/IConsoleManager.h"
#include "Rules/CheeseStrikeFalls.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikeWeapon.h"

/** Every bot that comes in after this is set carries this primary, rather than one picked for its skill. */
static TAutoConsoleVariable<FString> CVarBotPrimary(TEXT("CheeseStrike.BotPrimary"), TEXT(""), TEXT("The primary every bot coming in carries (blaster, carbine, grater, munster, shotgun, roquefort), or empty for one suited to its skill."));

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
	if (bSeesTarget)
	{
		return TEXT("fighting");
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

void ACheeseStrikeBotController::BeginPlay()
{
	Super::BeginPlay();
	// Its primary, for every character it is given, chosen for the skill it plays at.
	UWorld* World = GetWorld();
	UCheeseStrikeLoadouts* Loadouts = World != nullptr ? World->GetSubsystem<UCheeseStrikeLoadouts>() : nullptr;
	const ACheeseStrikeGameMode* Mode = World != nullptr ? World->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	if (Loadouts != nullptr && !Loadouts->HasChosen(this))
	{
		// For watching one weapon in bots' hands: CheeseStrike.BotPrimary <weapon>.
		const FWeaponStats* Forced = CheeseStrikeArsenal::Find(FName(*CVarBotPrimary.GetValueOnGameThread()));
		FRandomStream Random(FMath::Rand());
		Loadouts->Choose(this, Forced != nullptr && Forced->Slot == EWeaponSlot::Primary ? Forced->Id : CheeseStrikeBots::PrimaryFor(Mode != nullptr ? Mode->GetBotSkill() : SkillIndex, Random));
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
	MovedAt = Now();
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
	for (const FVector& To : {Other->GetEyes()->GetComponentLocation(), Other->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f)})
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
	if (Mine == nullptr || Match == nullptr || !Match->GetModeInfo().bLastStand || Me->IsDowned() || bSeesTarget)
	{
		LetGo();
		return false;
	}
	// In reach of someone down: hold Interact over them, still, looking at them.
	CheeseStrikeModes::EInteract Kind = CheeseStrikeModes::EInteract::None;
	if (const ACheeseStrikeCharacter* Over = Mine->InteractTarget(Kind))
	{
		if (bGoing)
		{
			StopMovement();
			bGoing = false;
		}
		Wanted = (Over->GetActorLocation() - EyesAt()).Rotation();
		if (!bInteracting)
		{
			Mine->SetInteracting(true);
			bInteracting = true;
		}
		return true;
	}
	LetGo();
	// Someone down not far off, a teammate to revive or an enemy to finish: go to them.
	const ACheeseStrikeCharacter* Nearest = nullptr;
	float NearestAt = TendRange;
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
		if (*It != Me && It->IsAlive() && It->IsDowned() && Distance < NearestAt)
		{
			Nearest = *It;
			NearestAt = Distance;
		}
	}
	if (Nearest == nullptr)
	{
		return false;
	}
	if (!bGoing || FVector::Dist(Goal, Nearest->GetActorLocation()) > 100.0f)
	{
		GoTo(Nearest->GetActorLocation(), false);
	}
	return true;
}

void ACheeseStrikeBotController::Think()
{
	Perceive();
	bTending = ThinkAboutTheDown();
	ChooseWhereToGo();
	ChooseWeapon();
	ThinkAboutStreaks();
	ACheeseStrikeCharacter* Me = Body();
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
		// Someone down can be harmed only up close: no use shooting them from further (ThinkAboutTheDown goes to them).
		if (Other->IsDowned() && Distance > CheeseStrikeModes::FinishRange)
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

bool ACheeseStrikeBotController::GoTo(const FVector& Where, bool bSprint)
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr)
	{
		return false;
	}
	// The view is its own, turned by the bot: the path only moves the feet.
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Where, GoalReached * 0.5f, true, true, true, true);
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
	// Stuck: going somewhere and getting nowhere. A jump, and somewhere else.
	if (!bGoing || Me->GetVelocity().Size2D() > ProgressSpeed)
	{
		MovedAt = Time;
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
		FVector Elsewhere;
		if (PickRoamGoal(Elsewhere))
		{
			GoTo(Elsewhere, false);
		}
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
	// Tending someone down (ThinkAboutTheDown): its feet are taken care of.
	if (bTending)
	{
		return;
	}
	bRetreating = bRetreating && bGoing;
	if (bRetreating)
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

bool ACheeseStrikeBotController::PickRoamGoal(FVector& Out) const
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ACheeseStrikeCharacter* Me = Body();
	if (Navigation == nullptr || Me == nullptr)
	{
		return false;
	}
	const FVector Here = Me->GetActorLocation();
	// Sometimes toward someone else, as a player looks for a fight; their place on the mesh.
	if (FMath::FRand() < 0.35f)
	{
		TArray<const ACheeseStrikeCharacter*> Others;
		for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != Me && It->IsAlive() && IsEnemy(*It))
			{
				Others.Add(*It);
			}
		}
		if (Others.Num() > 0)
		{
			FNavLocation Near;
			const FVector Toward = Others[FMath::RandHelper(Others.Num())]->GetActorLocation();
			if (Navigation->GetRandomReachablePointInRadius(Toward, 800.0f, Near))
			{
				Out = Near.Location;
				return true;
			}
		}
	}
	// Else somewhere new: of a couple of spots anywhere on the mesh, the further from here.
	bool bFound = false;
	float Furthest = -1.0f;
	for (int32 Try = 0; Try < 2; ++Try)
	{
		FNavLocation Spot;
		if (Navigation->GetRandomReachablePointInRadius(Here, RoamRadius, Spot))
		{
			const float Distance = FVector::Dist2D(Here, Spot.Location);
			if (Distance > Furthest)
			{
				Furthest = Distance;
				Out = Spot.Location;
				bFound = true;
			}
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
		const FVector Chest = Target->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
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
		// Where it is going.
		Wanted = FRotator(0.0f, Me->GetVelocity().Rotation().Yaw, 0.0f);
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
	if (Me == nullptr || !bSeesTarget || !Target.IsValid() || bGoing)
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
