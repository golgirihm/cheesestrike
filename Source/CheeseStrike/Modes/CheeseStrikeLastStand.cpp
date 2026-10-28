// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeLastStand.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "Weapons/CheeseStrikeGunfire.h"
#include "Effects/CheeseStrikeCheeseBlood.h"
#include "Rules/CheeseStrikeBlood.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikeWeapon.h"

using CheeseStrikeModes::EInteract;

static TAutoConsoleVariable<int32> CVarTraceExecution(TEXT("CheeseStrike.TraceExecution"), 0,
	TEXT("1: logs, every frame of an execution on the server, where the executor's hands and knife point are in the body of the one down."));
using CheeseStrikeExecution::EKind;

namespace
{
	/** How the kill feed names a kill by execution. */
	const TCHAR* const ExecutedWord = TEXT("executed");
	/** The view down: dark, washed of colour and red, blurred past arm's length. */
	constexpr float HurtViewRate = 2.5f;
	constexpr float HurtFocus = 90.0f;
	/** The blows' sounds (the knife's own, in and out, made by tools/art/stab_sound.py), and the chunks' cheese. */
	const TCHAR* const ExecutionWhooshPath = TEXT("/Game/Audio/Sounds/SFX/Lyra_MeleeWhoosh_02.Lyra_MeleeWhoosh_02");
	const TCHAR* const ExecutionStabInPath = TEXT("/Game/OpenGameArt/Stab/SW_KnifeStabIn.SW_KnifeStabIn");
	const TCHAR* const ExecutionStabOutPath = TEXT("/Game/OpenGameArt/Stab/SW_KnifeStabOut.SW_KnifeStabOut");
	const TCHAR* const ExecutionStompPath = TEXT("/Game/Audio/Sounds/SFX/Lyra_MeleeImpact_03.Lyra_MeleeImpact_03");
	const TCHAR* const ExecutionBurstPath = TEXT("/Game/Audio/Sounds/SFX/Lyra_MeleeImpact_01.Lyra_MeleeImpact_01");
	const TCHAR* const ChunkCheesePath = TEXT("/Game/CheeseStrike/Materials/M_Cheese.M_Cheese");
	const TCHAR* const ChunkMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

	/** A beat of an execution: when, what is heard, how loud and at what pitch, and whether it is the blow. */
	struct FExecutionBeat
	{
		float At;
		const TCHAR* Sound;
		float Volume;
		float Pitch;
		bool bBlow;
	};
	TArray<FExecutionBeat> BeatsOf(EKind Kind)
	{
		using namespace CheeseStrikeExecution;
		if (Kind == EKind::Stab)
		{
			// The thrust's swish, the blade going in, and drawn out again.
			return {{StabThrust, ExecutionWhooshPath, 0.6f, 1.25f, false}, {StabLands - 0.02f, ExecutionStabInPath, 1.4f, 1.0f, true}, {StabOut, ExecutionStabOutPath, 1.1f, 1.0f, false}};
		}
		if (Kind == EKind::Stomp)
		{
			return {{StompLands - 0.15f, ExecutionWhooshPath, 0.9f, 0.85f, false}, {StompLands, ExecutionStompPath, 1.3f, 0.8f, true}, {StompLands + 0.01f, ExecutionBurstPath, 1.1f, 0.7f, true}};
		}
		return {};
	}

	/** Where a point in the body of `Down` (CheeseStrikeDowned's frame) is in the world. */
	FVector InBodyOf(const ACheeseStrikeCharacter* Down, const FVector& InBody)
	{
		const FVector Floor = Down->GetActorLocation() - FVector(0.0f, 0.0f, Down->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		return Floor + FRotator(0.0f, Down->GetActorRotation().Yaw, 0.0f).RotateVector(InBody);
	}

	/** Chunks of cheese flying from `At`: on this machine only, for show. */
	void BurstIntoCheese(UWorld* World, const FVector& At)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, ChunkMeshPath);
		UMaterialInterface* Cheese = LoadObject<UMaterialInterface>(nullptr, ChunkCheesePath);
		if (World == nullptr || Cube == nullptr)
		{
			return;
		}
		UMaterialInstanceDynamic* Look = Cheese != nullptr ? UMaterialInstanceDynamic::Create(Cheese, World) : nullptr;
		if (Look != nullptr)
		{
			// The holes are carved for blocks a metre across; a chunk is too small for them.
			Look->SetScalarParameterValue(TEXT("Holes"), 0.0f);
		}
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Parameters.ObjectFlags |= RF_Transient;
		for (int32 Each = 0; Each < CheeseStrikeExecution::Chunks; ++Each)
		{
			// Out and up, mostly sideways off the floor the head lay on.
			const FVector Way = FMath::VRandCone(FVector::UpVector, FMath::DegreesToRadians(75.0f));
			AStaticMeshActor* Chunk = World->SpawnActor<AStaticMeshActor>(At + Way * 6.0f, FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), 0.0f), Parameters);
			if (Chunk == nullptr)
			{
				continue;
			}
			Chunk->SetReplicates(false);
			UStaticMeshComponent* Mesh = Chunk->GetStaticMeshComponent();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetStaticMesh(Cube);
			if (Look != nullptr)
			{
				Mesh->SetMaterial(0, Look);
			}
			// Cubes 4 to 10 cm across, some flattened into crumbs.
			const float Size = FMath::FRandRange(0.04f, 0.1f);
			Mesh->SetWorldScale3D(FVector(Size, Size * FMath::FRandRange(0.6f, 1.2f), Size * FMath::FRandRange(0.4f, 1.0f)));
			// They bounce off the level and nothing else: no blocking players or shots.
			Mesh->SetCollisionObjectType(ECC_PhysicsBody);
			Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
			Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
			Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Mesh->SetCastShadow(false);
			Mesh->SetSimulatePhysics(true);
			Mesh->SetPhysicsLinearVelocity(Way * CheeseStrikeExecution::ChunkSpeed * FMath::FRandRange(0.4f, 1.2f));
			Mesh->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * FMath::FRandRange(200.0f, 900.0f));
			Chunk->SetLifeSpan(CheeseStrikeExecution::ChunkLife * FMath::FRandRange(0.8f, 1.2f));
		}
	}

	/** The look of a hurt player's view, put on the camera at a weight. */
	void MakeHurtLook(FPostProcessSettings& Look)
	{
		Look.bOverride_VignetteIntensity = true;
		Look.VignetteIntensity = 1.4f;
		Look.bOverride_ColorSaturation = true;
		Look.ColorSaturation = FVector4(0.35f, 0.35f, 0.35f, 1.0f);
		Look.bOverride_ColorGain = true;
		Look.ColorGain = FVector4(0.75f, 0.52f, 0.48f, 1.0f);
		Look.bOverride_SceneFringeIntensity = true;
		Look.SceneFringeIntensity = 4.0f;
		Look.bOverride_DepthOfFieldFocalDistance = true;
		Look.DepthOfFieldFocalDistance = HurtFocus;
		Look.bOverride_DepthOfFieldFstop = true;
		Look.DepthOfFieldFstop = 1.0f;
		Look.bOverride_DepthOfFieldMinFstop = true;
		Look.DepthOfFieldMinFstop = 1.0f;
	}
}

UCheeseStrikeLastStand::UCheeseStrikeLastStand()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UCheeseStrikeLastStand::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCheeseStrikeLastStand, bDowned);
	DOREPLIFETIME(UCheeseStrikeLastStand, DownedAt);
	DOREPLIFETIME(UCheeseStrikeLastStand, WorkingOn);
	DOREPLIFETIME(UCheeseStrikeLastStand, Work);
	DOREPLIFETIME(UCheeseStrikeLastStand, WorkStartedAt);
	DOREPLIFETIME(UCheeseStrikeLastStand, Execution);
	DOREPLIFETIME(UCheeseStrikeLastStand, bExecuted);
}

void UCheeseStrikeLastStand::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ExecutionCamera != nullptr)
	{
		if (APlayerController* Player = ViewPlayer.Get(); Player != nullptr && Player->GetViewTarget() == ExecutionCamera)
		{
			Player->SetViewTarget(Player->GetPawn() != nullptr ? static_cast<AActor*>(Player->GetPawn()) : static_cast<AActor*>(Player));
		}
		ExecutionCamera->Destroy();
		ExecutionCamera = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

ACheeseStrikeCharacter* UCheeseStrikeLastStand::Body() const
{
	return Cast<ACheeseStrikeCharacter>(GetOwner());
}

float UCheeseStrikeLastStand::ServerNow() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
}

bool UCheeseStrikeLastStand::HasLastStand() const
{
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	return Match != nullptr && Match->GetModeInfo().bLastStand;
}

float UCheeseStrikeLastStand::SelfReviveLeft() const
{
	return bDowned ? CheeseStrikeModes::SelfReviveLeft(DownedAt, ServerNow()) : 0.0f;
}

float UCheeseStrikeLastStand::WorkDone() const
{
	const float Takes = CheeseStrikeModes::InteractTime(GetWork());
	return WorkingOn != nullptr && Takes > 0.0f ? FMath::Clamp((ServerNow() - WorkStartedAt) / Takes, 0.0f, 1.0f) : 0.0f;
}

float UCheeseStrikeLastStand::ExecutionElapsed() const
{
	return Execution.Kind != 0 ? ServerNow() - Execution.StartedAt : 0.0f;
}

bool UCheeseStrikeLastStand::InExecution() const
{
	return Execution.Kind != 0 && CheeseStrikeExecution::IsPlaying(Execution.GetKind(), ExecutionElapsed());
}

ACheeseStrikeCharacter* UCheeseStrikeLastStand::WorkedOnBy() const
{
	const ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || GetWorld() == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		const UCheeseStrikeLastStand* Theirs = It->GetLastStand();
		if (*It != Me && It->IsAlive() && Theirs != nullptr && Theirs->GetWorkingOn() == Me)
		{
			return *It;
		}
	}
	return nullptr;
}

ACheeseStrikeCharacter* UCheeseStrikeLastStand::InteractTarget(EInteract& OutKind) const
{
	OutKind = EInteract::None;
	const ACheeseStrikeCharacter* Me = Body();
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Me == nullptr || Match == nullptr || !Me->IsAlive() || bDowned || InExecution() || !Match->GetModeInfo().bLastStand)
	{
		return nullptr;
	}
	ACheeseStrikeCharacter* Best = nullptr;
	float Nearest = TNumericLimits<float>::Max();
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		// Someone already being executed is past saving, and past executing again.
		if (*It == Me || !It->IsAlive() || !It->IsDowned() || (It->GetLastStand() != nullptr && It->GetLastStand()->InExecution()))
		{
			continue;
		}
		const float Distance = FVector::Dist(Me->GetActorLocation(), It->GetActorLocation());
		const EInteract Kind = CheeseStrikeModes::InteractWith(true, Match->AreTeammates(Me->GetPlayerState(), It->GetPlayerState()), Distance);
		if (Kind != EInteract::None && Distance < Nearest)
		{
			Nearest = Distance;
			Best = *It;
			OutKind = Kind;
		}
	}
	return Best;
}

bool UCheeseStrikeLastStand::Intercept(float& Damage, const ACheeseStrikeCharacter* From, ACheeseStrikePlayerState* By, bool& bOutHurt)
{
	ACheeseStrikeCharacter* Me = Body();
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Me == nullptr || Match == nullptr || bFinishing || !Match->GetModeInfo().bLastStand)
	{
		return false;
	}
	// An execution plays out: nothing hurts the executor, nor takes the one executed from them.
	if (InExecution())
	{
		bOutHurt = false;
		return true;
	}
	// Only a hand up close finishes: a shot or a stroke from an enemy within reach, never a streak.
	const bool bEnemyClose = From != nullptr && From != Me && Match->AreEnemies(From->GetPlayerState(), Me->GetPlayerState())
		&& FVector::Dist(From->GetActorLocation(), Me->GetActorLocation()) <= CheeseStrikeModes::FinishRange;
	switch (CheeseStrikeModes::LastStandHit(true, bDowned, Me->GetHealth(), Damage, bEnemyClose))
	{
	case CheeseStrikeModes::ELastStandHit::Finish:
		Damage = Me->GetHealth() + 1000.0f;
		return false;
	case CheeseStrikeModes::ELastStandHit::Absorbed:
		bOutHurt = false;
		return true;
	case CheeseStrikeModes::ELastStandHit::Down:
		GoDown(By != nullptr ? By : (From != nullptr ? From->GetPlayerState<ACheeseStrikePlayerState>() : nullptr));
		bOutHurt = true;
		return true;
	default:
		return false;
	}
}

void UCheeseStrikeLastStand::GoDown(ACheeseStrikePlayerState* By)
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->HasAuthority() || bDowned)
	{
		return;
	}
	bDowned = true;
	DownedAt = ServerNow();
	DownedBy = By;
	if (By != nullptr && By != Me->GetPlayerState<ACheeseStrikePlayerState>())
	{
		By->AddDown();
	}
	bInteractHeld = false;
	StopWork();
	// Only the sidearm, in hand at once.
	if (UCheeseStrikeInventory* Inventory = Me->GetInventory())
	{
		Inventory->RestrictTo(EWeaponSlot::Sidearm);
	}
	KeepDown();
	// Falling back from whatever is just ahead, so that the legs lie out on the floor.
	Me->MakeRoomToLieDown();
	UE_LOG(LogCheeseStrike, Display, TEXT("%s is down, by %s"), *Me->DisplayName(), By != nullptr ? *By->GetPlayerName() : TEXT("nobody"));
	if (ACheeseStrikeGameMode* Mode = GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>())
	{
		Mode->Downed(Me, By);
	}
}

void UCheeseStrikeLastStand::Revive(float Health)
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->HasAuthority() || !bDowned || !Me->IsAlive() || InExecution())
	{
		return;
	}
	bDowned = false;
	DownedBy = nullptr;
	Me->SetHealth(Health);
	if (UCheeseStrikeInventory* Inventory = Me->GetInventory())
	{
		Inventory->LiftRestriction();
	}
	OnRep_Downed();
	UE_LOG(LogCheeseStrike, Display, TEXT("%s is back up"), *Me->DisplayName());
	if (ACheeseStrikeGameMode* Mode = GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>())
	{
		Mode->GotUp(Me);
	}
}

bool UCheeseStrikeLastStand::Execute(ACheeseStrikeCharacter* Victim, EKind Kind)
{
	ACheeseStrikeCharacter* Me = Body();
	UWorld* World = GetWorld();
	UCheeseStrikeLastStand* Theirs = Victim != nullptr ? Victim->GetLastStand() : nullptr;
	if (Me == nullptr || World == nullptr || !Me->HasAuthority() || !Me->CanAct() || bDowned || InExecution()
		|| Theirs == nullptr || !Victim->IsAlive() || !Theirs->IsDowned() || Theirs->InExecution())
	{
		return false;
	}
	if (Kind == EKind::None)
	{
		Kind = CheeseStrikeExecution::Pick(FMath::Rand());
	}
	// Beside the one down, on their right if there is room there (the knife hand and the boot toward their feet, where the
	// camera watches from), else their left.
	const float Came = CheeseStrikeExecution::PreferredSide;
	const float HalfHeight = Me->GetDefaultHalfHeight();
	const float Radius = Me->GetCapsuleComponent()->GetScaledCapsuleRadius();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeExecutionSpot), false);
	Params.AddIgnoredActor(Me);
	Params.AddIgnoredActor(Victim);
	float Side = Came;
	FVector Spot = Me->GetActorLocation();
	bool bRoom = false;
	for (const float Try : {Came, -Came})
	{
		const FVector Floor = InBodyOf(Victim, CheeseStrikeExecution::SpotInBody(Kind, Try));
		const FVector Centre = Floor + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
		if (!World->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params))
		{
			Side = Try;
			Spot = Centre;
			bRoom = true;
			break;
		}
	}
	if (!bRoom)
	{
		// Hemmed in: from where it stands.
		Spot.Z = Victim->GetActorLocation().Z - Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + HalfHeight + 2.0f;
	}
	const float Now = ServerNow();
	Execution = FCheeseStrikeExecutionState();
	Execution.Other = Victim;
	Execution.Kind = static_cast<uint8>(Kind);
	Execution.bExecutor = true;
	Execution.Side = static_cast<int8>(Side);
	Execution.StartedAt = Now;
	Execution.Spot = Spot;
	Execution.Yaw = CheeseStrikeExecution::FacingYaw(Spot, InBodyOf(Victim, CheeseStrikeExecution::AimPoint(Kind)));
	Theirs->Execution = FCheeseStrikeExecutionState();
	Theirs->Execution.Other = Me;
	Theirs->Execution.Kind = static_cast<uint8>(Kind);
	Theirs->Execution.bExecutor = false;
	Theirs->Execution.Side = static_cast<int8>(Side);
	Theirs->Execution.StartedAt = Now;
	Theirs->bInteractHeld = false;
	Theirs->StopWork();
	bExecutionKilled = false;
	bInteractHeld = false;
	StopWork();
	// The knife out for a stab; the weapon in hand comes back after.
	bSlotBefore = false;
	if (UCheeseStrikeInventory* Inventory = Me->GetInventory())
	{
		EWeaponSlot Held;
		if (Inventory->GetHeldSlot(Held))
		{
			SlotBefore = static_cast<uint8>(Held);
			bSlotBefore = true;
		}
		if (Kind == EKind::Stab && Inventory->CanDraw(EWeaponSlot::Knife))
		{
			Inventory->HoldNow(EWeaponSlot::Knife);
		}
	}
	if (ACheeseStrikeWeapon* Weapon = Me->GetWeapon())
	{
		Weapon->StopFiring();
		Weapon->StopAiming();
	}
	// An execution is an attack: a fresh spawn's protection ends with it.
	Me->EndProtection();
	Me->GetCharacterMovement()->StopMovementImmediately();
	if (Me->bIsCrouched && Me->IsLocallyControlled())
	{
		Me->UnCrouch();
	}
	HoldExecutionSpot();
	if (Me->IsLocallyControlled() && Me->GetController() != nullptr)
	{
		Me->GetController()->SetControlRotation(FRotator(0.0f, Execution.Yaw, 0.0f));
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("%s executes %s: a %s, from (%.0f, %.0f) facing %.0f, on side %.0f of them at (%.0f, %.0f) facing %.0f"), *Me->DisplayName(), *Victim->DisplayName(), CheeseStrikeExecution::Name(Kind),
		Spot.X, Spot.Y, Execution.Yaw, Side, Victim->GetActorLocation().X, Victim->GetActorLocation().Y, Victim->GetActorRotation().Yaw);
	return true;
}

void UCheeseStrikeLastStand::HoldExecutionSpot()
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Execution.bExecutor || !InExecution() || !Me->IsAlive())
	{
		return;
	}
	const FRotator Facing(0.0f, Execution.Yaw, 0.0f);
	if (FVector::DistSquared(Me->GetActorLocation(), Execution.Spot) > 1.0f || !Me->GetActorRotation().Equals(Facing, 0.5f))
	{
		Me->SetActorLocationAndRotation(Execution.Spot, Facing, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UCheeseStrikeLastStand::RunExecution()
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || Execution.Kind == 0)
	{
		return;
	}
	ACheeseStrikeCharacter* Other = Execution.Other;
	const float Elapsed = ExecutionElapsed();
	const EKind Kind = Execution.GetKind();
	if (!Execution.bExecutor)
	{
		// The one executed: freed if the executor is gone before the blow kills (they left, or fell off the map).
		const UCheeseStrikeLastStand* Theirs = IsValid(Other) ? Other->GetLastStand() : nullptr;
		if (Me->IsAlive() && (Theirs == nullptr || !Other->IsAlive() || Theirs->Execution.Other != Me))
		{
			Execution = FCheeseStrikeExecutionState();
		}
		return;
	}
	if (!Me->IsAlive())
	{
		EndExecution();
		return;
	}
	HoldExecutionSpot();
	UCheeseStrikeLastStand* Theirs = IsValid(Other) ? Other->GetLastStand() : nullptr;
	if (!bExecutionKilled && Theirs != nullptr && Other->IsAlive() && Elapsed >= CheeseStrikeExecution::KillAt(Kind))
	{
		bExecutionKilled = true;
		UE_LOG(LogCheeseStrike, Display, TEXT("%s is executed by %s"), *Other->DisplayName(), *Me->DisplayName());
		Theirs->Finish(Me->GetPlayerState<ACheeseStrikePlayerState>(), true);
	}
	if (!CheeseStrikeExecution::IsPlaying(Kind, Elapsed) || (!bExecutionKilled && (Theirs == nullptr || !Other->IsAlive())))
	{
		EndExecution();
	}
}

void UCheeseStrikeLastStand::EndExecution()
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Execution.bExecutor)
	{
		return;
	}
	if (ACheeseStrikeCharacter* Other = Execution.Other; IsValid(Other) && Other->IsAlive())
	{
		// Never killed (a round that ended first): they stay down.
		if (UCheeseStrikeLastStand* Theirs = Other->GetLastStand(); Theirs != nullptr && Theirs->Execution.Other == Me)
		{
			Theirs->Execution = FCheeseStrikeExecutionState();
		}
	}
	Execution = FCheeseStrikeExecutionState();
	if (UCheeseStrikeInventory* Inventory = Me->GetInventory(); Inventory != nullptr && bSlotBefore && Me->IsAlive() && Inventory->CanDraw(static_cast<EWeaponSlot>(SlotBefore)))
	{
		Inventory->HoldNow(static_cast<EWeaponSlot>(SlotBefore));
	}
	bSlotBefore = false;
}

void UCheeseStrikeLastStand::Finish(ACheeseStrikePlayerState* By, bool bByExecution)
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->HasAuthority() || !Me->IsAlive())
	{
		return;
	}
	// The blow lands as the finisher's own (or, with them gone, as a kill of theirs from nowhere).
	bFinishing = true;
	if (bByExecution)
	{
		bExecuted = true;
	}
	ACheeseStrikeCharacter* From = By != nullptr ? By->GetPawn<ACheeseStrikeCharacter>() : nullptr;
	if (bByExecution && By != nullptr)
	{
		Me->TakeBlast(Me->GetHealth() + 1000.0f, By, ExecutedWord);
	}
	else if (From != nullptr && From->IsAlive())
	{
		Me->TakeShot(Me->GetHealth() + 1000.0f, From, false);
	}
	else if (By != nullptr)
	{
		Me->TakeBlast(Me->GetHealth() + 1000.0f, By, FString());
	}
	else
	{
		Me->TakeShot(Me->GetHealth() + 1000.0f, nullptr, false);
	}
	if (Me->IsAlive())
	{
		bExecuted = false;
	}
	bFinishing = false;
}

void UCheeseStrikeLastStand::SetInteracting(bool bHeld)
{
	const ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr)
	{
		return;
	}
	if (Me->HasAuthority())
	{
		ServerInteract_Implementation(bHeld);
	}
	else
	{
		ServerInteract(bHeld);
	}
}

void UCheeseStrikeLastStand::ServerInteract_Implementation(bool bHeld)
{
	bInteractHeld = bHeld;
	if (!bHeld)
	{
		StopWork();
		return;
	}
	// A press over an enemy down executes them at once; over a teammate, holding it revives them.
	EInteract Kind = EInteract::None;
	ACheeseStrikeCharacter* Target = InteractTarget(Kind);
	const ACheeseStrikeCharacter* Me = Body();
	if (Target != nullptr && Kind == EInteract::Execute && Me != nullptr && Me->CanAct())
	{
		Execute(Target);
	}
}

void UCheeseStrikeLastStand::StopWork()
{
	WorkingOn = nullptr;
	Work = static_cast<uint8>(EInteract::None);
	WorkStartedAt = 0.0f;
}

void UCheeseStrikeLastStand::DoWork(float DeltaTime)
{
	EInteract Kind = EInteract::None;
	ACheeseStrikeCharacter* Target = InteractTarget(Kind);
	// Held over someone to execute does nothing more: the press did it, or they came into reach after it.
	if (Target == nullptr || Kind != EInteract::Revive)
	{
		StopWork();
		return;
	}
	// A new one: from the start.
	if (Target != WorkingOn || static_cast<uint8>(Kind) != Work)
	{
		WorkingOn = Target;
		Work = static_cast<uint8>(Kind);
		WorkStartedAt = ServerNow();
		return;
	}
	if (ServerNow() - WorkStartedAt < CheeseStrikeModes::InteractTime(Kind))
	{
		return;
	}
	StopWork();
	bInteractHeld = false;
	if (UCheeseStrikeLastStand* Theirs = Target->GetLastStand())
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("%s revives %s"), *Body()->DisplayName(), *Target->DisplayName());
		Theirs->Revive();
	}
}

void UCheeseStrikeLastStand::KeepDown()
{
	ACheeseStrikeCharacter* Me = Body();
	// The machine that moves it: its player's, or the server's for a bot. Crouching there reaches the server with the moves.
	if (Me == nullptr || !Me->IsAlive() || !Me->IsLocallyControlled())
	{
		return;
	}
	UCheeseStrikeMovementComponent* Movement = Me->GetCheeseStrikeMovement();
	// Executing, it stands to deal the blow, and stays put.
	if (IsExecuting())
	{
		if (Movement != nullptr)
		{
			Movement->SetWantsToSprint(false);
		}
		if (Me->bIsCrouched || (Movement != nullptr && Movement->bWantsToCrouch))
		{
			Me->UnCrouch();
		}
		HoldExecutionSpot();
		return;
	}
	if (!bDowned)
	{
		return;
	}
	// From the hip only.
	if (const ACheeseStrikeWeapon* Held = Me->GetWeapon(); Held != nullptr && Held->IsAimed())
	{
		Me->DoAimDownSights(false);
	}
	if (Movement != nullptr)
	{
		Movement->SetWantsToSprint(false);
		if (!Movement->bWantsToCrouch)
		{
			Me->Crouch();
		}
	}
}

void UCheeseStrikeLastStand::OnRep_Downed()
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->IsLocallyControlled())
	{
		return;
	}
	if (bDowned)
	{
		KeepDown();
	}
	else
	{
		Me->UnCrouch();
	}
}

void UCheeseStrikeLastStand::OnRep_Execution()
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->IsLocallyControlled() || !IsExecuting())
	{
		return;
	}
	// The executor's own machine puts it where the server has it, facing the blow, so that the moves agree.
	Me->GetCharacterMovement()->StopMovementImmediately();
	HoldExecutionSpot();
	if (Me->GetController() != nullptr)
	{
		Me->GetController()->SetControlRotation(FRotator(0.0f, Execution.Yaw, 0.0f));
	}
}

void UCheeseStrikeLastStand::Bleed(float DeltaTime)
{
	ACheeseStrikeCharacter* Me = Body();
	ACheeseStrikeCheeseBlood* Blood = ACheeseStrikeCheeseBlood::For(GetWorld());
	if (Me == nullptr || Blood == nullptr || DeltaTime <= 0.0f)
	{
		return;
	}
	const EKind Kind = Execution.GetKind();
	const float Elapsed = ExecutionElapsed();
	const USkeletalMeshComponent* Mesh = Me->GetMesh();
	const ACheeseStrikeCharacter* Executor = Execution.Other.Get();
	if (Kind == EKind::Stab)
	{
		const float Rate = CheeseStrikeBlood::StabRate(Elapsed);
		BleedOwed += Rate * DeltaTime;
		const int32 Now = FMath::FloorToInt32(BleedOwed);
		if (Now <= 0)
		{
			return;
		}
		BleedOwed -= Now;
		// From the chest where the knife went in (on the body as it lies, or falls), up out of it; the gush as the knife
		// comes out leans toward the executor, who draws it.
		const FVector Wound = FMath::Lerp(Mesh->GetBoneLocation(TEXT("spine_03")), Mesh->GetBoneLocation(TEXT("pelvis")), 0.2f) + FVector(0.0f, 0.0f, 8.0f);
		const bool bGush = Elapsed >= CheeseStrikeExecution::StabOut;
		FVector Way = FVector::UpVector;
		if (bGush && IsValid(Executor))
		{
			Way += (Executor->GetActorLocation() - Wound).GetSafeNormal2D() * 0.5f;
		}
		Blood->Spray(Wound, Way, Now, CheeseStrikeBlood::StabSpeed(Elapsed), 0.35f, bGush ? 30.0f : 22.0f, bGush ? 1.5f : 1.1f);
		return;
	}
	if (Kind != EKind::Stomp)
	{
		return;
	}
	// The stomp: a firehose from the neck, the way the head was, whipping about as it throbs, and a pool spreading under it.
	const FVector Neck = Mesh->GetBoneLocation(TEXT("neck_01"));
	const FVector Head = Mesh->GetBoneLocation(TEXT("head"));
	const FVector Out = (Head - Neck).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	if (!bPooled && Elapsed >= CheeseStrikeExecution::StompLands)
	{
		bPooled = true;
		Blood->Pool(Neck + Out.GetSafeNormal2D() * 25.0f + FVector(0.0f, 0.0f, 40.0f), Me);
	}
	const float Rate = CheeseStrikeBlood::StompRate(Elapsed);
	BleedOwed += Rate * DeltaTime;
	const int32 Now = FMath::FloorToInt32(BleedOwed);
	if (Now <= 0)
	{
		return;
	}
	BleedOwed -= Now;
	const float Since = Elapsed - CheeseStrikeExecution::StompLands;
	const FVector Side = FVector::CrossProduct(Out, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
	const FVector Way = (Out * 0.7f + FVector(0.0f, 0.0f, 0.75f) + Side * 0.45f * FMath::Sin(Since * 5.3f)).GetSafeNormal();
	Blood->Spray(Neck + Out * 6.0f, Way, Now, CheeseStrikeBlood::StompSpeed(Elapsed), 0.3f, 16.0f, CheeseStrikeBlood::StompDropSize(Elapsed));
}

void UCheeseStrikeLastStand::UpdateBurst(float DeltaTime)
{
	ACheeseStrikeCharacter* Me = Body();
	UWorld* World = GetWorld();
	if (Me == nullptr || World == nullptr || Execution.Kind == 0 || Execution.bExecutor || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const EKind Kind = Execution.GetKind();
	const float Elapsed = ExecutionElapsed();
	const TArray<FExecutionBeat> Beats = BeatsOf(Kind);
	const FVector Head = Me->GetMesh()->GetBoneLocation(TEXT("head"));
	// The swings and the blows, heard by everyone near, from where they land. A machine that hears of it late skips what is past.
	while (BeatsDone < Beats.Num() && Elapsed >= Beats[BeatsDone].At)
	{
		if (Elapsed - Beats[BeatsDone].At < 0.4f)
		{
			const FExecutionBeat& Beat = Beats[BeatsDone];
			if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, Beat.Sound))
			{
				const FVector Where = Kind == EKind::Stomp ? Head : InBodyOf(Me, CheeseStrikeExecution::AimPoint(Kind));
				UGameplayStatics::PlaySoundAtLocation(this, Sound, Where, FRotator::ZeroRotator, Beat.Volume, Beat.Pitch, 0.0f, CheeseStrikeGunfire::AttenuationAsset());
			}
		}
		++BeatsDone;
	}
	// The boot lands: the head bursts into cheese, and is gone.
	if (Kind == EKind::Stomp && !bBurst && (Elapsed >= CheeseStrikeExecution::StompLands || (bExecuted && !Me->IsAlive())))
	{
		bBurst = true;
		static const FName HeadBone(TEXT("head"));
		for (USkeletalMeshComponent* Mesh : {Me->GetMesh(), Me->GetFirstPersonMesh()})
		{
			if (Mesh != nullptr && Mesh->GetBoneIndex(HeadBone) != INDEX_NONE)
			{
				Mesh->HideBoneByName(HeadBone, PBO_None);
			}
		}
		BurstIntoCheese(World, Head + FVector(0.0f, 0.0f, 6.0f));
	}
	Bleed(DeltaTime);
}

void UCheeseStrikeLastStand::UpdateView(float DeltaTime)
{
	ACheeseStrikeCharacter* Me = Body();
	UWorld* World = GetWorld();
	if (Me == nullptr || World == nullptr)
	{
		return;
	}
	APlayerController* Player = Cast<APlayerController>(Me->GetController());
	if (Player != nullptr && !Player->IsLocalController())
	{
		Player = nullptr;
	}
	const float Time = World->GetTimeSeconds();
	const bool bWatch = InExecution();
	const ACheeseStrikeCharacter* Down = Execution.bExecutor ? Execution.Other.Get() : Me;
	// Out, to watch it from a little way off: the body seen as others see it.
	if (bWatch && !bViewOut && Player != nullptr && IsValid(Down))
	{
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Parameters.ObjectFlags |= RF_Transient;
		ExecutionCamera = World->SpawnActor<ACameraActor>(Player->PlayerCameraManager != nullptr ? Player->PlayerCameraManager->GetCameraLocation() : Me->GetActorLocation(), FRotator::ZeroRotator, Parameters);
		if (ExecutionCamera != nullptr)
		{
			ExecutionCamera->GetCameraComponent()->SetFieldOfView(CheeseStrikeExecution::CameraFov);
			ExecutionCamera->GetCameraComponent()->bConstrainAspectRatio = false;
			bViewOut = true;
			ViewBackAt = -1.0f;
			ViewPlayer = Player;
			bViewToBlend = true;
			ViewFloor = InBodyOf(Down, FVector::ZeroVector);
			ViewYaw = Down->GetActorRotation().Yaw;
		}
	}
	if (bViewOut && ExecutionCamera != nullptr && bWatch && IsValid(Down))
	{
		const EKind Kind = Execution.GetKind();
		const float Side = Execution.Side;
		const FRotator Lying(0.0f, ViewYaw, 0.0f);
		const FVector Look = ViewFloor + Lying.RotateVector(CheeseStrikeExecution::LookAtInBody(Kind, Side));
		FVector At = ViewFloor + Lying.RotateVector(CheeseStrikeExecution::CameraInBody(Kind, Side, ExecutionElapsed()));
		// Not through a wall: as far out as there is room for.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeExecutionView), false);
		for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Look, At, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(15.0f), Params))
		{
			At = Hit.Location;
		}
		ExecutionCamera->SetActorLocationAndRotation(At, (Look - At).Rotation());
		// Once placed, the view blends out to it.
		if (APlayerController* Viewer = ViewPlayer.Get(); Viewer != nullptr && bViewToBlend)
		{
			bViewToBlend = false;
			Viewer->SetViewTargetWithBlend(ExecutionCamera, CheeseStrikeExecution::ViewBlendOut, VTBlend_EaseInOut, 2.0f);
			Me->SeeOwnBody(true);
		}
	}
	// Over: back to the eyes (or, killed, to where the body lies, until the watching begins).
	if (bViewOut && !bWatch && ViewBackAt < 0.0f)
	{
		APlayerController* Viewer = ViewPlayer.Get();
		if (Viewer != nullptr && ExecutionCamera != nullptr && Viewer->GetViewTarget() == ExecutionCamera)
		{
			Viewer->SetViewTargetWithBlend(Me, Me->IsAlive() ? CheeseStrikeExecution::ViewBlendBack : 0.0f, VTBlend_EaseInOut, 2.0f);
		}
		ViewBackAt = Time + CheeseStrikeExecution::ViewBlendBack;
	}
	if (bViewOut && ViewBackAt >= 0.0f && Time >= ViewBackAt)
	{
		if (Me->IsAlive())
		{
			Me->SeeOwnBody(false);
		}
		if (ExecutionCamera != nullptr)
		{
			ExecutionCamera->Destroy();
			ExecutionCamera = nullptr;
		}
		bViewOut = false;
		ViewBackAt = -1.0f;
	}

	// Down: the view dark, washed out and blurred past arm's length, and swaying heavily, which the aim follows.
	if (Player == nullptr || !Me->IsLocallyControlled())
	{
		return;
	}
	const bool bHurt = bDowned && Me->IsAlive() && !InExecution();
	HurtView = FMath::FInterpConstantTo(HurtView, bHurt ? 1.0f : 0.0f, DeltaTime, HurtViewRate);
	if (UCameraComponent* Camera = Me->GetFirstPersonCameraComponent())
	{
		if (HurtView > 0.0f && !Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance)
		{
			MakeHurtLook(Camera->PostProcessSettings);
		}
		Camera->PostProcessBlendWeight = FMath::SmoothStep(0.0f, 1.0f, HurtView);
	}
	if (HurtView > 0.0f || !SwayShown.IsZero())
	{
		SwayClock += DeltaTime;
		const FRotator Sway = CheeseStrikeModes::DownedSway(SwayClock) * HurtView;
		const FRotator Step = Sway - SwayShown;
		SwayShown = HurtView > 0.0f ? Sway : FRotator::ZeroRotator;
		if (!Me->TakesInput())
		{
			return;
		}
		Player->SetControlRotation(Player->GetControlRotation() + Step);
	}
}

void UCheeseStrikeLastStand::TraceExecution() const
{
	const ACheeseStrikeCharacter* Me = Body();
	const ACheeseStrikeCharacter* Down = Execution.Other;
	const USkeletalMeshComponent* Theirs = Down->GetMesh();
	const USkeletalMeshComponent* Mine = Me->GetMesh();
	const FVector Floor = Down->GetActorLocation() - FVector(0.0f, 0.0f, Down->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FRotator Lying(0.0f, Down->GetActorRotation().Yaw, 0.0f);
	const auto InBody = [&](const FVector& World) { return Lying.UnrotateVector(World - Floor); };
	const FVector Chest = InBody(FMath::Lerp(Theirs->GetBoneLocation(TEXT("spine_03")), Theirs->GetBoneLocation(TEXT("pelvis")), 0.2f));
	FVector Point = FVector::ZeroVector;
	if (const ACheeseStrikeWeapon* Weapon = Me->GetWeapon(); Weapon != nullptr && Weapon->GetModel(false) != nullptr)
	{
		Point = InBody(Weapon->GetModel(false)->GetComponentTransform().TransformPosition(Weapon->GetModelAlong() * 21.0f));
	}
	const FVector HandR = InBody(Mine->GetBoneLocation(TEXT("hand_r")));
	const FVector HandL = InBody(Mine->GetBoneLocation(TEXT("hand_l")));
	const FVector Head = InBody(Mine->GetBoneLocation(TEXT("upperarm_r")));
	const FVector TheirHead = InBody(Theirs->GetBoneLocation(TEXT("head")));
	UE_LOG(LogCheeseStrike, Display, TEXT("Execution %.2f s: point (%.0f, %.0f, %.0f) chest (%.0f, %.0f, %.0f) %.0f cm apart; hands (%.0f, %.0f, %.0f) (%.0f, %.0f, %.0f); shoulder (%.0f, %.0f, %.0f), their head (%.0f, %.0f, %.0f)"),
		ExecutionElapsed(), Point.X, Point.Y, Point.Z, Chest.X, Chest.Y, Chest.Z, FVector::Dist(Point, Chest), HandR.X, HandR.Y, HandR.Z, HandL.X, HandL.Y, HandL.Z,
		Head.X, Head.Y, Head.Z, TheirHead.X, TheirHead.Y, TheirHead.Z);
}

void UCheeseStrikeLastStand::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr)
	{
		return;
	}
	KeepDown();
	UpdateBurst(DeltaTime);
	UpdateView(DeltaTime);
	if (!Me->HasAuthority())
	{
		return;
	}
	if (CVarTraceExecution.GetValueOnGameThread() != 0 && IsExecuting() && IsValid(Execution.Other))
	{
		TraceExecution();
	}
	if (bDowned && !Me->IsAlive())
	{
		bDowned = false;
	}
	RunExecution();
	// Left down and not executed, it gets up by itself in the end, hurt.
	if (bDowned && Me->IsAlive() && !InExecution() && ServerNow() - DownedAt >= CheeseStrikeModes::SelfReviveTime)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("%s gets up by themself"), *Me->DisplayName());
		Revive(CheeseStrikeModes::SelfReviveHealth);
	}
	if (bInteractHeld && Me->CanAct() && !bDowned && !InExecution())
	{
		DoWork(DeltaTime);
	}
	else if (WorkingOn != nullptr)
	{
		StopWork();
	}
}
