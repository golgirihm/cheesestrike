// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGrenades.h"
#include "Algo/Find.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGrenade.h"
#include "CheeseStrikeGrenadeRules.h"
#include "CheeseStrikeWeapon.h"

namespace
{

	/** The hand a grenade is held and thrown with, and how deep in its palm it sits (cm from the palm's middle, out of it). */
	const FName GrenadeHandBone(TEXT("hand_r"));
	constexpr float GrenadeInPalmDepth = 4.0f;
	/** A grenade held, with no arms to hold it in: low on the right of its player's view (cm from the eye: ahead, right, up). */
	const FVector GrenadeInViewAt(40.0f, 14.0f, -10.0f);
	const FRotator GrenadeInViewTurn(-15.0f, 160.0f, -10.0f);
	/** The grenade (tools/art/weapon_models.py) and its materials (tools/art/weapon_materials.py), as ACheeseStrikeGrenade has them. */
	const TCHAR* ThrowRoundPath = TEXT("/Game/CheeseStrike/Weapons/Grenade/SM_FragGrenade.SM_FragGrenade");
	const TCHAR* const ThrowMaterialPaths[] = {TEXT("/Game/CheeseStrike/Weapons/Materials/MI_GrenadeCheese.MI_GrenadeCheese"),
		TEXT("/Game/CheeseStrike/Weapons/Materials/MI_GrenadeMetal.MI_GrenadeMetal")};
	/** The arc shown while a throw is held: its glow, its colour (the cheese's), how big its dots are and the mark where it comes down (cm across). */
	const TCHAR* PreviewGlowPath = TEXT("/Game/CheeseStrike/Materials/M_Glow.M_Glow");
	const FLinearColor PreviewColour(1.0f, 0.78f, 0.2f);
	constexpr float PreviewDotSize = 2.4f;
	constexpr float PreviewDotAngle = 0.009f;
	constexpr float PreviewMarkAcross = 44.0f;
}

UCheeseStrikeGrenades::UCheeseStrikeGrenades()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UCheeseStrikeGrenades::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UCheeseStrikeGrenades, Count, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UCheeseStrikeGrenades, bHoldShown, COND_SkipOwner);
}

ACheeseStrikeCharacter* UCheeseStrikeGrenades::Holder() const
{
	return Cast<ACheeseStrikeCharacter>(GetOwner());
}

float UCheeseStrikeGrenades::ServerNow() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void UCheeseStrikeGrenades::SetCount(int32 InCount)
{
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		Count = FMath::Clamp(InCount, 0, CheeseStrikeGrenades::MostCarried);
	}
}

bool UCheeseStrikeGrenades::IsThrowing() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimeSeconds() < ThrowingUntil;
}

float UCheeseStrikeGrenades::ThrowPhase() const
{
	if (!IsThrowing())
	{
		return -1.0f;
	}
	return FMath::Clamp(1.0f - (ThrowingUntil - GetWorld()->GetTimeSeconds()) / CheeseStrikeGrenades::ThrowTime, 0.0f, 1.0f);
}

bool UCheeseStrikeGrenades::CanThrow() const
{
	const ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr || Count <= 0 || IsThrowing() || !Character->CanAct() || Character->IsDowned())
	{
		return false;
	}
	// A reload under way is finished first.
	const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
	return Weapon == nullptr || !Weapon->IsReloading();
}

void UCheeseStrikeGrenades::Throw(const FVector& Aim)
{
	ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr || !Character->IsLocallyControlled() || !CanThrow())
	{
		return;
	}
	CancelHold();
	// The hands go to the throw: the trigger and the sights let go.
	if (ACheeseStrikeWeapon* Weapon = Character->GetWeapon())
	{
		Weapon->StopFiring();
		Weapon->StopAiming();
	}
	ShowThrow();
	if (Character->HasAuthority())
	{
		ServerThrowNow(Aim.GetSafeNormal());
	}
	else
	{
		ServerThrow(Aim.GetSafeNormal());
	}
}

void UCheeseStrikeGrenades::BeginHold()
{
	ACheeseStrikeCharacter* Character = Holder();
	if (bHolding || Character == nullptr || !Character->IsLocallyControlled() || !CanThrow())
	{
		return;
	}
	bHolding = true;
	HeldWith = Character->GetWeapon();
	if (ACheeseStrikeWeapon* Weapon = Character->GetWeapon())
	{
		Weapon->StopFiring();
		Weapon->StopAiming();
	}
	// The weapon goes down and the grenade comes up in the right hand (the switch, each tick), for the others to see too.
	ShowHold(true);
	UpdateHold();
}

void UCheeseStrikeGrenades::EndHold(const FVector& Aim)
{
	ACheeseStrikeCharacter* Character = Holder();
	if (!bHolding || Character == nullptr)
	{
		return;
	}
	if (!CanThrow())
	{
		CancelHold();
		return;
	}
	bHolding = false;
	ShowPreview(false);
	ShowThrow();
	if (Character->HasAuthority())
	{
		ServerThrowNow(Aim.GetSafeNormal());
	}
	else
	{
		ServerThrow(Aim.GetSafeNormal());
	}
}

void UCheeseStrikeGrenades::CancelHold()
{
	if (!bHolding)
	{
		return;
	}
	bHolding = false;
	ShowPreview(false);
	ShowHold(false);
}

void UCheeseStrikeGrenades::ShowHold(bool bShow)
{
	const ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr)
	{
		return;
	}
	if (Character->HasAuthority())
	{
		bHoldShown = bShow;
	}
	else
	{
		ServerShowHold(bShow);
	}
}

void UCheeseStrikeGrenades::ServerShowHold_Implementation(bool bShow)
{
	bHoldShown = bShow;
}

void UCheeseStrikeGrenades::ServerThrow_Implementation(FVector_NetQuantizeNormal Aim)
{
	ServerThrowNow(Aim);
}

void UCheeseStrikeGrenades::ServerThrowNow(const FVector& Aim)
{
	ACheeseStrikeCharacter* Character = Holder();
	// Thrown or not, the grenade is no longer held up.
	bHoldShown = false;
	// A throw arrives no sooner than the last one's hands are free, allowing for the network.
	if (Character == nullptr || Count <= 0 || !Character->CanAct() || Character->IsDowned() || ServerNow() - LastThrowAt < CheeseStrikeGrenades::ThrowTime * 0.8f || Aim.IsNearlyZero())
	{
		return;
	}
	--Count;
	LastThrowAt = ServerNow();
	Character->EndProtection();
	MulticastThrow();
	GetWorld()->GetTimerManager().SetTimer(ReleaseTimer, FTimerDelegate::CreateUObject(this, &UCheeseStrikeGrenades::Release, Aim), CheeseStrikeGrenades::ReleaseDelay, false);
}

void UCheeseStrikeGrenades::MulticastThrow_Implementation()
{
	const ACheeseStrikeCharacter* Character = Holder();
	// Its own player's machine has shown it already.
	if (Character != nullptr && !Character->IsLocallyControlled())
	{
		ShowThrow();
	}
}

void UCheeseStrikeGrenades::ShowThrow()
{
	if (Holder() == nullptr)
	{
		return;
	}
	// Both bodies throw with the right hand (FCheeseStrikeGrenadeHold, by ThrowPhase), the grenade in it until it goes.
	const float Time = GetWorld()->GetTimeSeconds();
	ThrowingUntil = Time + CheeseStrikeGrenades::ThrowTime;
	InHandUntil = Time + CheeseStrikeGrenades::ReleaseDelay;
}

bool UCheeseStrikeGrenades::InPalm(const USkeletalMeshComponent* Body, FTransform& OutInHand)
{
	const USkeletalMesh* Mesh = Body != nullptr ? Body->GetSkeletalMeshAsset() : nullptr;
	if (Mesh == nullptr)
	{
		return false;
	}
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	const int32 Hand = Skeleton.FindBoneIndex(GrenadeHandBone);
	const int32 Knuckles[] = {Skeleton.FindBoneIndex(TEXT("index_01_r")), Skeleton.FindBoneIndex(TEXT("middle_01_r")), Skeleton.FindBoneIndex(TEXT("ring_01_r")),
		Skeleton.FindBoneIndex(TEXT("pinky_01_r"))};
	if (Hand == INDEX_NONE || Algo::Find(Knuckles, INDEX_NONE) != nullptr)
	{
		return false;
	}
	// The knuckles from the wrist, in the hand bone's space, as the skeleton's own pose has them.
	const FTransform HandAt = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Hand);
	FVector From[4];
	for (int32 Each = 0; Each < 4; ++Each)
	{
		From[Each] = HandAt.InverseTransformPositionNoScale(FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Knuckles[Each]).GetLocation());
	}
	const FVector Fingers = From[1].GetSafeNormal();
	// The palm faces the way the fingers cross the index's side (FCheeseStrikeGrenadeHold turns the hand by the same).
	const FVector Palm = FVector::CrossProduct(Fingers, (From[0] - From[3]).GetSafeNormal()).GetSafeNormal();
	if (Fingers.IsNearlyZero() || Palm.IsNearlyZero())
	{
		return false;
	}
	// In the palm, most of the way to the knuckles where the fingers close round it, and out of it by the grenade's depth;
	// its fuse along the fingers.
	const FVector Middle = (From[0] + From[1] + From[2] + From[3]) * 0.25f * 0.85f;
	OutInHand = FTransform(FRotationMatrix::MakeFromZX(Fingers, Palm).ToQuat(), Middle + Palm * GrenadeInPalmDepth);
	return true;
}

UStaticMeshComponent* UCheeseStrikeGrenades::MakeInHand(USkeletalMeshComponent* Body, bool bFirstPerson)
{
	ACheeseStrikeCharacter* Character = Holder();
	UStaticMesh* Round = LoadObject<UStaticMesh>(nullptr, ThrowRoundPath);
	if (Character == nullptr || Round == nullptr)
	{
		return nullptr;
	}
	// The frag, in its cheese and steel.
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(GetOwner());
	Mesh->SetStaticMesh(Round);
	for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(ThrowMaterialPaths); ++Slot)
	{
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, ThrowMaterialPaths[Slot]))
		{
			Mesh->SetMaterial(Slot, Material);
		}
	}
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(!bFirstPerson);
	Mesh->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	Mesh->bOnlyOwnerSee = bFirstPerson;
	Mesh->bOwnerNoSee = !bFirstPerson;
	Mesh->SetVisibility(false);
	Mesh->RegisterComponent();
	// In the right palm; without a hand to hold it (or fingers to find the palm by), in the view or at the hand.
	FTransform InHand;
	const bool bHand = Body != nullptr && Body->GetSkeletalMeshAsset() != nullptr && Body->GetBoneIndex(GrenadeHandBone) != INDEX_NONE;
	if (bHand)
	{
		Mesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GrenadeHandBone);
		if (InPalm(Body, InHand))
		{
			Mesh->SetRelativeTransform(InHand);
		}
	}
	else if (bFirstPerson && Character->GetFirstPersonCameraComponent() != nullptr)
	{
		Mesh->AttachToComponent(Character->GetFirstPersonCameraComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Mesh->SetRelativeLocationAndRotation(GrenadeInViewAt, GrenadeInViewTurn);
	}
	Mesh->SetWorldScale3D(FVector(1.0f));
	return Mesh;
}

void UCheeseStrikeGrenades::UpdateInHand()
{
	ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Up in the right hand once the weapon is down, and through a throw until it goes.
	const bool bShow = Character->IsAlive() && ((IsHoldShown() && Switch >= 1.0f) || GetWorld()->GetTimeSeconds() < InHandUntil);
	const auto Show = [this, bShow](TObjectPtr<UStaticMeshComponent>& Mesh, USkeletalMeshComponent* Body, bool bFirstPerson)
	{
		if (bShow && Mesh == nullptr)
		{
			Mesh = MakeInHand(Body, bFirstPerson);
		}
		if (Mesh != nullptr && Mesh->IsVisible() != bShow)
		{
			Mesh->SetVisibility(bShow);
		}
	};
	// The view's only for its own player.
	if (Character->IsLocallyControlled() && Character->IsPlayerControlled())
	{
		Show(HeldInView, Character->GetFirstPersonMesh(), true);
	}
	Show(InHandThird, Character->GetMesh(), false);
}

bool UCheeseStrikeGrenades::PredictPath(const FVector& Aim, FPredictProjectilePathResult& OutResult, float SimFrequency) const
{
	const ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr || GetWorld() == nullptr)
	{
		return false;
	}
	// As ACheeseStrikeGrenade flies: from the hand, at its speed, under its gravity, its size, off the world and the cheese, through players.
	const FVector Along = Aim.GetSafeNormal();
	FPredictProjectilePathParams Path(CheeseStrikeGrenades::CollisionRadius, ReleasePoint(Along), CheeseStrikeGrenades::LaunchVelocity(Along, Character->GetVelocity()), CheeseStrikeGrenades::Fuse);
	Path.bTraceWithCollision = true;
	Path.bTraceWithChannel = false;
	Path.ObjectTypes = {UEngineTypes::ConvertToObjectType(ECC_WorldStatic), UEngineTypes::ConvertToObjectType(ECC_WorldDynamic)};
	Path.ActorsToIgnore.Add(const_cast<ACheeseStrikeCharacter*>(Character));
	Path.SimFrequency = SimFrequency;
	Path.OverrideGravityZ = GetWorld()->GetGravityZ() * CheeseStrikeGrenades::GravityScale;
	return UGameplayStatics::PredictProjectilePath(const_cast<UCheeseStrikeGrenades*>(this), Path, OutResult) && OutResult.HitResult.bBlockingHit;
}

void UCheeseStrikeGrenades::UpdateHold()
{
	ACheeseStrikeCharacter* Character = Holder();
	if (!bHolding || Character == nullptr)
	{
		return;
	}
	const bool bChanged = Character->GetWeapon() != HeldWith.Get();
	if (CheeseStrikeGrenades::HeldThrow(true, CanThrow(), bChanged) == CheeseStrikeGrenades::EHeld::PutAway)
	{
		UE_LOG(LogCheeseStrike, Verbose, TEXT("Grenade: %s puts the throw away"), *Character->DisplayName());
		CancelHold();
		return;
	}
	// Its arc, as it would fly thrown now.
	FPredictProjectilePathResult Result;
	bPreviewLands = PredictPath(Character->GetControlRotation().Vector(), Result);
	TArray<FVector> Path;
	Path.Reserve(Result.PathData.Num() + 1);
	for (const FPredictProjectilePathPointData& Point : Result.PathData)
	{
		Path.Add(Point.Location);
	}
	if (bPreviewLands)
	{
		Path.Add(Result.HitResult.Location);
		PreviewLanding = Result.HitResult.ImpactPoint;
	}
	PreviewAt = CheeseStrikeGrenades::PreviewPoints(Path);
	ShowPreview(true);
	if (bPreviewLands && PreviewMark != nullptr)
	{
		PreviewMark->SetWorldLocationAndRotation(Result.HitResult.ImpactPoint + Result.HitResult.ImpactNormal * 0.6f, FRotationMatrix::MakeFromZ(Result.HitResult.ImpactNormal).ToQuat());
	}
}

void UCheeseStrikeGrenades::ShowPreview(bool bShow)
{
	const ACheeseStrikeCharacter* Character = Holder();
	if (GetNetMode() == NM_DedicatedServer || Character == nullptr || !Character->IsPlayerControlled())
	{
		return;
	}
	if (bShow && PreviewMark == nullptr)
	{
		// Dots and a mark that glow cheese yellow, seen by the thrower alone, casting nothing.
		UStaticMesh* Dot = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, PreviewGlowPath))
		{
			PreviewGlow = UMaterialInstanceDynamic::Create(Glow, this);
			PreviewGlow->SetVectorParameterValue(TEXT("Colour"), PreviewColour);
			PreviewGlow->SetScalarParameterValue(TEXT("Strength"), 0.7f);
		}
		const auto Make = [this](UStaticMesh* Mesh, const FVector& Scale)
		{
			UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
			Part->SetStaticMesh(Mesh);
			if (PreviewGlow != nullptr)
			{
				Part->SetMaterial(0, PreviewGlow);
			}
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetCastShadow(false);
			Part->SetCanEverAffectNavigation(false);
			Part->bOnlyOwnerSee = true;
			Part->SetUsingAbsoluteLocation(true);
			Part->SetUsingAbsoluteRotation(true);
			Part->SetUsingAbsoluteScale(true);
			Part->SetupAttachment(GetOwner()->GetRootComponent());
			Part->RegisterComponent();
			Part->SetWorldScale3D(Scale);
			Part->SetVisibility(false);
			return Part;
		};
		for (int32 Index = 0; Index < CheeseStrikeGrenades::PreviewDots; ++Index)
		{
			PreviewDots.Add(Make(Dot, FVector(PreviewDotSize / 100.0f)));
		}
		PreviewMark = Make(Disc, FVector(PreviewMarkAcross / 100.0f, PreviewMarkAcross / 100.0f, 0.012f));
	}
	const FVector Eye = Character->GetFirstPersonCameraComponent() != nullptr ? Character->GetFirstPersonCameraComponent()->GetComponentLocation() : Character->GetActorLocation();
	for (int32 Index = 0; Index < PreviewDots.Num(); ++Index)
	{
		UStaticMeshComponent* Dot = PreviewDots[Index];
		const bool bDot = bShow && PreviewAt.IsValidIndex(Index);
		if (bDot)
		{
			// About as big in view however far along the arc, so that its far end reads as well as its near.
			const float Size = FMath::Clamp(FVector::Dist(PreviewAt[Index], Eye) * PreviewDotAngle, PreviewDotSize, PreviewDotSize * 3.0f);
			Dot->SetWorldLocation(PreviewAt[Index]);
			Dot->SetWorldScale3D(FVector(Size / 100.0f));
		}
		if (Dot->IsVisible() != bDot)
		{
			Dot->SetVisibility(bDot);
		}
	}
	if (PreviewMark != nullptr && PreviewMark->IsVisible() != (bShow && bPreviewLands))
	{
		PreviewMark->SetVisibility(bShow && bPreviewLands);
	}
	if (!bShow)
	{
		PreviewAt.Reset();
		bPreviewLands = false;
	}
}

FVector UCheeseStrikeGrenades::ReleasePoint(const FVector& Aim) const
{
	const ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr)
	{
		return FVector::ZeroVector;
	}
	const FVector Eyes = Character->GetEyes()->GetComponentLocation();
	const FRotator Facing(0.0f, Aim.Rotation().Yaw, 0.0f);
	// Where the right hand lets it go (CheeseStrikeGrenades::ThrowHand at ReleasePhase), on every machine alike.
	const FVector Wanted = Eyes + Aim * CheeseStrikeGrenades::ReleaseAhead + Facing.RotateVector(FVector::RightVector) * CheeseStrikeGrenades::ReleaseRight
		- FVector(0.0f, 0.0f, CheeseStrikeGrenades::ReleaseDown);
	// Never through a wall right in front: short of it.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeGrenadeRelease), false, Character);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Eyes, Wanted, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(CheeseStrikeGrenades::CollisionRadius), Params))
	{
		return Hit.Location;
	}
	return Wanted;
}

void UCheeseStrikeGrenades::Release(FVector Aim)
{
	ACheeseStrikeCharacter* Character = Holder();
	UWorld* World = GetWorld();
	if (Character == nullptr || World == nullptr || !Character->IsAlive())
	{
		return;
	}
	const FTransform At(FRotator(0.0f, Aim.Rotation().Yaw, 0.0f), ReleasePoint(Aim));
	ACheeseStrikeGrenade* Grenade = World->SpawnActorDeferred<ACheeseStrikeGrenade>(ACheeseStrikeGrenade::StaticClass(), At, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Grenade == nullptr)
	{
		return;
	}
	Grenade->Launch(Character, CheeseStrikeGrenades::LaunchVelocity(Aim, Character->GetVelocity()));
	UGameplayStatics::FinishSpawningActor(Grenade, At);
	Thrown.RemoveAll([](const TWeakObjectPtr<ACheeseStrikeGrenade>& Each) { return !Each.IsValid(); });
	Thrown.Add(Grenade);
}

void UCheeseStrikeGrenades::UpdateGunAway()
{
	const ACheeseStrikeCharacter* Character = Holder();
	// The weapon is out of sight, on both bodies, from when it has gone down until the grenade hand is down again after.
	ACheeseStrikeWeapon* Wanted = Character != nullptr && Character->IsAlive() && CheeseStrikeGrenades::IsWeaponAway(Switch, IsThrowing()) ? Character->GetWeapon() : nullptr;
	ACheeseStrikeWeapon* Was = AwayGun.Get();
	if (Was != Wanted)
	{
		if (Was != nullptr)
		{
			Was->SetAway(false);
		}
		if (Wanted != nullptr)
		{
			Wanted->SetAway(true);
		}
		AwayGun = Wanted;
	}
}

void UCheeseStrikeGrenades::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateHold();
	// The weapon down and the grenade up while it is held or thrown, and back after.
	const ACheeseStrikeCharacter* Character = Holder();
	const bool bBusy = Character != nullptr && Character->IsAlive() && (IsHoldShown() || IsThrowing());
	Switch = CheeseStrikeGrenades::SwitchStep(Switch, bBusy, DeltaTime);
	UpdateGunAway();
	UpdateInHand();
}

void UCheeseStrikeGrenades::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ACheeseStrikeWeapon* Gun = AwayGun.Get())
	{
		Gun->SetAway(false);
	}
	ShowPreview(false);
	Super::EndPlay(EndPlayReason);
}
