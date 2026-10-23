// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGrenades.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
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
	/** The hand the throw is made with (Lyra's toss throws with the left, the weapon kept in the right), and how the round sits in it. */
	const FName ThrowHandBone(TEXT("hand_l"));
	const FVector ThrowInHandAt(9.0f, 3.0f, -1.0f);
	const TCHAR* ThrowRoundPath = TEXT("/Game/CheeseStrike/Meshes/SM_CheeseWheel.SM_CheeseWheel");
	const TCHAR* ThrowWaxPath = TEXT("/Game/CheeseStrike/Materials/M_CheeseWax.M_CheeseWax");
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
	// The round in the throwing hand until it goes.
	UStaticMesh* Round = LoadObject<UStaticMesh>(nullptr, ThrowRoundPath);
	UMaterialInterface* Wax = LoadObject<UMaterialInterface>(nullptr, ThrowWaxPath);
	const auto InHand = [this, Round, Wax](TObjectPtr<UStaticMeshComponent>& Mesh, USkeletalMeshComponent* Body, bool bFirstPerson)
	{
		if (Body == nullptr || Round == nullptr)
		{
			return;
		}
		if (Mesh == nullptr)
		{
			Mesh = NewObject<UStaticMeshComponent>(GetOwner());
			Mesh->SetStaticMesh(Round);
			if (Wax != nullptr)
			{
				Mesh->SetMaterial(0, Wax);
			}
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(!bFirstPerson);
			Mesh->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::WorldSpaceRepresentation);
			Mesh->bOnlyOwnerSee = bFirstPerson;
			Mesh->bOwnerNoSee = !bFirstPerson;
			Mesh->RegisterComponent();
			Mesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ThrowHandBone);
			Mesh->SetRelativeLocation(ThrowInHandAt);
			Mesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
			Mesh->SetWorldScale3D(FVector(0.09f, 0.09f, 0.045f));
		}
		Mesh->SetVisibility(true);
	};
	InHand(InHandFirst, Character->GetFirstPersonMesh(), true);
	InHand(InHandThird, Character->GetMesh(), false);
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

void UCheeseStrikeGrenades::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
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
