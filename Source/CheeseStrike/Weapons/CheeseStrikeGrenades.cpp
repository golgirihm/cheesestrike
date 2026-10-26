// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGrenades.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGrenade.h"
#include "CheeseStrikeGrenadeRules.h"
#include "CheeseStrikeWeapon.h"

namespace
{

	/** Where a grenade leaves the hand from the eyes: ahead, to the right and down (cm), along the aim. */
	constexpr float ThrowFromAhead = 35.0f;
	constexpr float ThrowFromRight = 14.0f;
	constexpr float ThrowFromDown = 10.0f;
	/** The hand the throw is made with (Lyra's toss throws with the left, the weapon kept in the right), and how the grenade sits in it. */
	const FName ThrowHandBone(TEXT("hand_l"));
	const FVector ThrowInHandAt(9.0f, 3.0f, -1.0f);
	const FRotator ThrowInHandTurn(0.0f, 0.0f, 90.0f);
	/** A throw held: the grenade up in the free hand, low on the left of its player's view (cm from the eye: ahead, right, up), its spoon toward them. */
	const FVector HeldInViewAt(40.0f, -17.0f, -14.0f);
	const FRotator HeldInViewTurn(-15.0f, 200.0f, 10.0f);
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
	// The grenade taken up in the free hand, low on the left of the view.
	ShowHeldInView(true);
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
	ShowHeldInView(false);
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
	ShowHeldInView(false);
}

void UCheeseStrikeGrenades::ServerThrow_Implementation(FVector_NetQuantizeNormal Aim)
{
	ServerThrowNow(Aim);
}

void UCheeseStrikeGrenades::ServerThrowNow(const FVector& Aim)
{
	ACheeseStrikeCharacter* Character = Holder();
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
	ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr)
	{
		return;
	}
	const float Time = GetWorld()->GetTimeSeconds();
	ThrowingUntil = Time + CheeseStrikeGrenades::ThrowTime;
	InHandUntil = Time + CheeseStrikeGrenades::ReleaseDelay;
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Each body plays its own skeleton's toss, over the weapon it holds, the throw made in ThrowTime.
	const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
	const ECheeseStrikeHold Hold = Weapon != nullptr ? Weapon->GetHold() : ECheeseStrikeHold::Rifle;
	for (USkeletalMeshComponent* Body : {Character->GetFirstPersonMesh(), Character->GetMesh()})
	{
		UCheeseStrikeAnimInstance* Instance = Body != nullptr ? Cast<UCheeseStrikeAnimInstance>(Body->GetAnimInstance()) : nullptr;
		UAnimSequence* Toss = Instance != nullptr ? Instance->GetSet(Hold).Throw.Get() : nullptr;
		if (Toss != nullptr)
		{
			const float Rate = FMath::Max(1.0f, Toss->GetPlayLength() / CheeseStrikeGrenades::ThrowTime);
			Instance->PlaySlotAnimationAsDynamicMontage(Toss, UCheeseStrikeAnimInstance::SlotName(), 0.08f, 0.15f, Rate);
		}
	}
	ShowInHand();
}

void UCheeseStrikeGrenades::ShowHeldInView(bool bShow)
{
	ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr || GetNetMode() == NM_DedicatedServer || !Character->IsPlayerControlled())
	{
		return;
	}
	if (bShow && HeldInView == nullptr && Character->GetFirstPersonCameraComponent() != nullptr)
	{
		HeldInView = NewObject<UStaticMeshComponent>(GetOwner());
		HeldInView->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, ThrowRoundPath));
		for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(ThrowMaterialPaths); ++Slot)
		{
			if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, ThrowMaterialPaths[Slot]))
			{
				HeldInView->SetMaterial(Slot, Material);
			}
		}
		HeldInView->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HeldInView->SetCastShadow(false);
		HeldInView->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		HeldInView->bOnlyOwnerSee = true;
		HeldInView->SetupAttachment(Character->GetFirstPersonCameraComponent());
		HeldInView->RegisterComponent();
		HeldInView->SetRelativeLocationAndRotation(HeldInViewAt, HeldInViewTurn);
	}
	if (HeldInView != nullptr)
	{
		HeldInView->SetVisibility(bShow);
	}
}

void UCheeseStrikeGrenades::ShowInHand()
{
	ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// The grenade in the throwing hand until it goes: the frag, in its cheese and steel.
	UStaticMesh* Round = LoadObject<UStaticMesh>(nullptr, ThrowRoundPath);
	const auto InHand = [this, Round](TObjectPtr<UStaticMeshComponent>& Mesh, USkeletalMeshComponent* Body, bool bFirstPerson)
	{
		if (Body == nullptr || Round == nullptr)
		{
			return;
		}
		if (Mesh == nullptr)
		{
			Mesh = NewObject<UStaticMeshComponent>(GetOwner());
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
			Mesh->RegisterComponent();
			Mesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ThrowHandBone);
			Mesh->SetRelativeLocation(ThrowInHandAt);
			Mesh->SetRelativeRotation(ThrowInHandTurn);
			Mesh->SetWorldScale3D(FVector(1.0f));
		}
		Mesh->SetVisibility(true);
	};
	InHand(InHandFirst, Character->GetFirstPersonMesh(), true);
	InHand(InHandThird, Character->GetMesh(), false);
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
	const FVector Wanted = Eyes + Aim * ThrowFromAhead + Facing.RotateVector(FVector::RightVector) * ThrowFromRight - FVector(0.0f, 0.0f, ThrowFromDown);
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
	// The gun is put away from its player's view for as long as a grenade is held or being thrown, and comes back after.
	ACheeseStrikeWeapon* Wanted = Character != nullptr && Character->IsLocallyControlled() && Character->IsAlive() && IsBusy() ? Character->GetWeapon() : nullptr;
	ACheeseStrikeWeapon* Was = AwayGun.Get();
	if (Was != Wanted)
	{
		if (Was != nullptr)
		{
			Was->SetFirstPersonAway(false);
		}
		if (Wanted != nullptr)
		{
			Wanted->SetFirstPersonAway(true);
		}
		AwayGun = Wanted;
	}
}

void UCheeseStrikeGrenades::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateHold();
	UpdateGunAway();
	// The round in hand goes as it is let go (or the throw is cut short by death).
	const bool bShow = GetWorld()->GetTimeSeconds() < InHandUntil && Holder() != nullptr && Holder()->IsAlive();
	for (UStaticMeshComponent* Mesh : {InHandFirst.Get(), InHandThird.Get()})
	{
		if (Mesh != nullptr && Mesh->IsVisible() != bShow)
		{
			Mesh->SetVisibility(bShow);
		}
	}
}

void UCheeseStrikeGrenades::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ACheeseStrikeWeapon* Gun = AwayGun.Get())
	{
		Gun->SetFirstPersonAway(false);
	}
	ShowPreview(false);
	ShowHeldInView(false);
	Super::EndPlay(EndPlayReason);
}
