// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePickup.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "CheeseStrikeArsenal.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikeInventory.h"
#include "CheeseStrikeWeapon.h"
#include "CheeseStrikeWeaponLooks.h"

namespace
{
	/** The plinth (cm): across and high; the pad on it, across. */
	constexpr float PlinthAcross = 110.0f;
	constexpr float PlinthHigh = 8.0f;
	constexpr float PadAcross = 84.0f;
	/** The column of light over the pad: across and high. */
	constexpr float BeamAcross = 70.0f;
	constexpr float BeamHigh = 170.0f;
	/** How much bigger than in the hand its weapon floats, to be read from afar. */
	constexpr float PickupModelScale = 1.25f;
	/** The pad's glow, there and while empty. */
	constexpr float GlowThere = 0.5f;
	constexpr float GlowEmpty = 0.12f;
	/** Its light (lumens), there, and how far it reaches (cm). */
	constexpr float PickupLight = 110.0f;
	constexpr float PickupLightReach = 320.0f;
	/** How often the server looks for anyone walking over it (s). */
	constexpr float WalkOverEvery = 0.1f;

	TAutoConsoleVariable<float> CVarPickupSeconds(TEXT("CheeseStrike.PickupSeconds"), 0.0f,
		TEXT("Seconds a weapon's stand stays empty once taken, in place of CheeseStrikePickups::RespawnSeconds (for the play tests); 0 for the usual."), ECVF_Cheat);

	template <typename T>
	T* LoadPickupAsset(const TCHAR* Path)
	{
		return Path != nullptr ? LoadObject<T>(nullptr, Path) : nullptr;
	}

	FVector PickupFeetOf(const ACheeseStrikeCharacter* Character)
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		return Character->GetActorLocation() - FVector(0.0f, 0.0f, Capsule != nullptr ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f);
	}
}

ACheeseStrikePickup::ACheeseStrikePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.0f);
	SetReplicatingMovement(false);

	Floor = CreateDefaultSubobject<USceneComponent>(TEXT("Floor"));
	RootComponent = Floor;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const auto MakePart = [this](const TCHAR* Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetGenerateOverlapEvents(false);
		return Part;
	};
	// The engine's cylinder is a metre across and a metre high, about its middle.
	Plinth = MakePart(TEXT("Plinth"), Floor);
	Plinth->SetStaticMesh(Cylinder.Object);
	Plinth->SetRelativeLocation(FVector(0.0f, 0.0f, PlinthHigh / 2.0f));
	Plinth->SetRelativeScale3D(FVector(PlinthAcross / 100.0f, PlinthAcross / 100.0f, PlinthHigh / 100.0f));
	Pad = MakePart(TEXT("Pad"), Floor);
	Pad->SetStaticMesh(Cylinder.Object);
	Pad->SetRelativeLocation(FVector(0.0f, 0.0f, PlinthHigh + 0.5f));
	Pad->SetRelativeScale3D(FVector(PadAcross / 100.0f, PadAcross / 100.0f, 0.012f));
	Pad->SetCastShadow(false);
	// Lit by nothing but its own glow, so that its colour reads whatever falls on it.
	Pad->SetLightingChannels(false, true, false);

	Beam = MakePart(TEXT("Beam"), Floor);
	Beam->SetStaticMesh(Cylinder.Object);
	Beam->SetRelativeLocation(FVector(0.0f, 0.0f, PlinthHigh + BeamHigh / 2.0f));
	Beam->SetRelativeScale3D(FVector(BeamAcross / 100.0f, BeamAcross / 100.0f, BeamHigh / 100.0f));
	Beam->SetCastShadow(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Floor);
	Light->SetRelativeLocation(FVector(0.0f, 0.0f, CheeseStrikePickups::FloatHeight + 40.0f));
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetIntensity(PickupLight);
	Light->SetAttenuationRadius(PickupLightReach);
	Light->SetCastShadows(false);

	Spinner = CreateDefaultSubobject<USceneComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(Floor);
	Spinner->SetRelativeLocation(FVector(0.0f, 0.0f, CheeseStrikePickups::FloatHeight));
	SkinnedModel = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkinnedModel"));
	SkinnedModel->SetupAttachment(Spinner);
	SkinnedModel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkinnedModel->SetCanEverAffectNavigation(false);
	SkinnedModel->SetGenerateOverlapEvents(false);
	PlainModel = MakePart(TEXT("PlainModel"), Spinner);
}

void ACheeseStrikePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACheeseStrikePickup, Weapon, COND_InitialOnly);
	DOREPLIFETIME(ACheeseStrikePickup, BackAt);
}

void ACheeseStrikePickup::SetWeapon(FName Id)
{
	const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Id);
	Weapon = Stats != nullptr ? Stats->Id : Id;
	bLookApplied = false;
	if (HasActorBegunPlay())
	{
		ApplyLook();
	}
}

FText ACheeseStrikePickup::GetWeaponName() const
{
	const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Weapon);
	return Stats != nullptr ? Stats->DisplayName : FText::FromName(Weapon);
}

float ACheeseStrikePickup::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
	return State != nullptr ? static_cast<float>(State->GetServerWorldTimeSeconds()) : (World != nullptr ? World->GetTimeSeconds() : 0.0f);
}

bool ACheeseStrikePickup::IsInPlay() const
{
	const UWorld* World = GetWorld();
	const ACheeseStrikeGameState* Match = World != nullptr ? World->GetGameState<ACheeseStrikeGameState>() : nullptr;
	return Match != nullptr && CheeseStrikePickups::InMode(Match->GetMode()) && CheeseStrikePickups::CanLieOnAStand(Weapon);
}

bool ACheeseStrikePickup::IsThere() const
{
	return IsInPlay() && SecondsLeft() <= 0.0f;
}

float ACheeseStrikePickup::SecondsLeft() const
{
	return CheeseStrikePickups::SecondsLeft(ServerNow(), BackAt);
}

CheeseStrikePickups::ETake ACheeseStrikePickup::HowFor(const ACheeseStrikeCharacter* Character) const
{
	using CheeseStrikePickups::ETake;
	const UCheeseStrikeInventory* Inventory = Character != nullptr ? Character->GetInventory() : nullptr;
	if (Inventory == nullptr || !IsThere()
		|| !CheeseStrikePickups::MayTake(Character->IsAlive(), Character->CanAct(), Character->IsDowned(), Inventory->IsAllowed(EWeaponSlot::Primary)))
	{
		return ETake::None;
	}
	const ACheeseStrikeWeapon* Carried = Inventory->GetInSlot(EWeaponSlot::Primary);
	const bool bFull = Carried != nullptr && (Carried->GetStats().HasEndlessReserve() || Carried->GetReserve() >= Carried->GetStats().ReserveMax);
	return CheeseStrikePickups::HowToTake(Carried != nullptr ? Carried->GetWeaponId() : NAME_None, Weapon, bFull);
}

bool ACheeseStrikePickup::IsInReach(const ACheeseStrikeCharacter* Character, float Across) const
{
	return Character != nullptr && CheeseStrikePickups::InReach(FloorAt(), PickupFeetOf(Character), Across);
}

ACheeseStrikePickup* ACheeseStrikePickup::OfferedTo(const ACheeseStrikeCharacter* Character)
{
	UWorld* World = Character != nullptr ? Character->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	ACheeseStrikePickup* Nearest = nullptr;
	float NearestAt = TNumericLimits<float>::Max();
	for (TActorIterator<ACheeseStrikePickup> It(World); It; ++It)
	{
		if (!It->IsInReach(Character, CheeseStrikePickups::PressReach) || It->HowFor(Character) != CheeseStrikePickups::ETake::Press)
		{
			continue;
		}
		const float Distance = FVector::DistSquared2D(It->FloorAt(), Character->GetActorLocation());
		if (Distance < NearestAt)
		{
			Nearest = *It;
			NearestAt = Distance;
		}
	}
	return Nearest;
}

void ACheeseStrikePickup::Take()
{
	if (!HasAuthority())
	{
		return;
	}
	const float Seconds = CVarPickupSeconds.GetValueOnGameThread() > 0.0f ? CVarPickupSeconds.GetValueOnGameThread() : CheeseStrikePickups::RespawnSeconds;
	BackAt = ServerNow() + Seconds;
	ForceNetUpdate();
	UpdateLook();
}

void ACheeseStrikePickup::PutBack()
{
	if (!HasAuthority())
	{
		return;
	}
	BackAt = 0.0f;
	ForceNetUpdate();
	UpdateLook();
}

void ACheeseStrikePickup::OnRep_Weapon()
{
	bLookApplied = false;
	ApplyLook();
}

void ACheeseStrikePickup::OnRep_BackAt()
{
	UpdateLook();
}

void ACheeseStrikePickup::BeginPlay()
{
	Super::BeginPlay();
	// Loaded by path at run time, not in the constructor, so that the scripts may rebuild them.
	if (UMaterialInterface* Dark = LoadPickupAsset<UMaterialInterface>(TEXT("/Game/CheeseStrike/Materials/MI_Painted_Dark.MI_Painted_Dark")))
	{
		Plinth->SetMaterial(0, Dark);
	}
	if (UMaterialInterface* Glow = LoadPickupAsset<UMaterialInterface>(TEXT("/Game/CheeseStrike/Materials/M_Glow.M_Glow")))
	{
		PadGlow = UMaterialInstanceDynamic::Create(Glow, this);
		Pad->SetMaterial(0, PadGlow);
	}
	if (UMaterialInterface* GhostMaterial = LoadPickupAsset<UMaterialInterface>(TEXT("/Game/CheeseStrike/Effects/M_Ghost.M_Ghost")))
	{
		Ghost = UMaterialInstanceDynamic::Create(GhostMaterial, this);
		Ghost->SetScalarParameterValue(TEXT("Amount"), 0.85f);
		BeamGlow = UMaterialInstanceDynamic::Create(GhostMaterial, this);
		BeamGlow->SetScalarParameterValue(TEXT("Amount"), 0.5f);
		Beam->SetMaterial(0, BeamGlow);
	}
	// Taken, it is heard near it: the weapons' draw sound, which carries no attenuation of its own.
	TakenSound = LoadPickupAsset<USoundBase>(CheeseStrikeWeaponLooks::DrawSounds[0]);
	TakenReach = NewObject<USoundAttenuation>(this);
	TakenReach->Attenuation.bAttenuate = true;
	TakenReach->Attenuation.bSpatialize = true;
	TakenReach->Attenuation.AttenuationShapeExtents = FVector(300.0f);
	TakenReach->Attenuation.FalloffDistance = 2000.0f;
	// Each stand turns from its own start, so that they don't all turn as one.
	Spin = FMath::Fmod(GetActorLocation().X * 0.37f + GetActorLocation().Y * 0.61f, 360.0f);
	// Again, now that its pad's glow is made, if its weapon arrived before.
	bLookApplied = false;
	ApplyLook();
	UpdateLook();
	if (HasAuthority())
	{
		UE_LOG(LogCheeseStrike, Verbose, TEXT("%s holds the %s"), *GetName(), *GetWeaponName().ToString());
	}
}

void ACheeseStrikePickup::ApplyLook()
{
	if (bLookApplied || !CheeseStrikePickups::CanLieOnAStand(Weapon))
	{
		return;
	}
	bLookApplied = true;
	const FWeaponLook& Look = CheeseStrikeWeaponLooks::For(Weapon);
	USkeletalMesh* Skinned = LoadPickupAsset<USkeletalMesh>(Look.SkinnedModel);
	UStaticMesh* Plain = Skinned == nullptr ? LoadPickupAsset<UStaticMesh>(Look.PlainModel) : nullptr;
	SkinnedModel->SetSkeletalMeshAsset(Skinned);
	PlainModel->SetStaticMesh(Plain);
	UMeshComponent* Model = Skinned != nullptr ? static_cast<UMeshComponent*>(SkinnedModel) : static_cast<UMeshComponent*>(PlainModel);
	SkinnedModel->SetVisibility(Skinned != nullptr);
	PlainModel->SetVisibility(Plain != nullptr);
	if (Skinned == nullptr && Plain == nullptr)
	{
		return;
	}
	for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(Look.Materials); ++Slot)
	{
		if (UMaterialInterface* Material = LoadPickupAsset<UMaterialInterface>(Look.Materials[Slot]))
		{
			if (Slot < Model->GetNumMaterials())
			{
				Model->SetMaterial(Slot, Material);
			}
		}
	}
	OwnMaterials.Reset();
	for (int32 Slot = 0; Slot < Model->GetNumMaterials(); ++Slot)
	{
		OwnMaterials.Add(Model->GetMaterial(Slot));
	}
	// As the hand holds it (its grip frame), larger, its middle on the axis it turns about.
	const FBoxSphereBounds Bounds = Skinned != nullptr ? Skinned->GetBounds() : Plain->GetBounds();
	FTransform Placed = Look.Model;
	Placed.SetScale3D(Placed.GetScale3D() * PickupModelScale);
	Placed.SetLocation(FVector::ZeroVector);
	Placed.SetLocation(-Placed.TransformPosition(Bounds.Origin));
	Model->SetRelativeTransform(Placed);
	if (PadGlow != nullptr)
	{
		PadGlow->SetVectorParameterValue(TEXT("Colour"), CheeseStrikePickups::WorthColour(CheeseStrikePickups::Worth(Weapon)));
	}
	Light->SetLightColor(CheeseStrikePickups::WorthColour(CheeseStrikePickups::Worth(Weapon)));
	if (BeamGlow != nullptr)
	{
		BeamGlow->SetVectorParameterValue(TEXT("Colour"), CheeseStrikePickups::WorthColour(CheeseStrikePickups::Worth(Weapon)));
	}
	UpdateLook(true);
}

void ACheeseStrikePickup::UpdateLook(bool bForce)
{
	const bool bInPlay = IsInPlay();
	const bool bThere = bInPlay && IsThere();
	// Only while playing: in the editor (a map's script setting its weapon) there is no mode, and it would be saved hidden.
	const UWorld* World = GetWorld();
	if (World != nullptr && World->IsGameWorld() && bInPlay == IsHidden())
	{
		SetActorHiddenInGame(!bInPlay);
	}
	if ((bThere == bShownThere && !bForce) || !bLookApplied)
	{
		return;
	}
	const bool bTaken = bShownThere && !bThere && !bForce;
	bShownThere = bThere;
	UMeshComponent* Model = SkinnedModel->GetSkinnedAsset() != nullptr ? static_cast<UMeshComponent*>(SkinnedModel) : static_cast<UMeshComponent*>(PlainModel);
	for (int32 Slot = 0; Slot < Model->GetNumMaterials(); ++Slot)
	{
		UMaterialInterface* Own = OwnMaterials.IsValidIndex(Slot) ? OwnMaterials[Slot].Get() : nullptr;
		Model->SetMaterial(Slot, bThere || Ghost == nullptr ? Own : static_cast<UMaterialInterface*>(Ghost));
	}
	Model->SetCastShadow(bThere);
	if (PadGlow != nullptr)
	{
		PadGlow->SetScalarParameterValue(TEXT("Strength"), bThere ? GlowThere : GlowEmpty);
	}
	Light->SetIntensity(bThere ? PickupLight : PickupLight * 0.15f);
	Beam->SetVisibility(bThere);
	if (bTaken && TakenSound != nullptr && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, TakenSound, Spinner->GetComponentLocation(), 1.0f, 1.0f, 0.0f, TakenReach);
	}
}

FString ACheeseStrikePickup::DescribeLook() const
{
	const UMeshComponent* Model = SkinnedModel->GetSkinnedAsset() != nullptr ? static_cast<const UMeshComponent*>(SkinnedModel) : static_cast<const UMeshComponent*>(PlainModel);
	return FString::Printf(TEXT("hidden %d, plinth %s visible %d at %s, model %s visible %d at %s scale %s"), IsHidden(),
		Plinth->GetStaticMesh() != nullptr ? *Plinth->GetStaticMesh()->GetName() : TEXT("none"), Plinth->IsVisible(), *Plinth->GetComponentLocation().ToCompactString(),
		*GetNameSafe(SkinnedModel->GetSkinnedAsset() != nullptr ? static_cast<const UObject*>(SkinnedModel->GetSkinnedAsset()) : static_cast<const UObject*>(PlainModel->GetStaticMesh())),
		Model->IsVisible(), *Model->GetComponentLocation().ToCompactString(), *Model->GetComponentScale().ToCompactString());
}

void ACheeseStrikePickup::GiveToWhoeverWalksOver()
{
	using CheeseStrikePickups::ETake;
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It && IsThere(); ++It)
	{
		ACheeseStrikeCharacter* Character = *It;
		if (!IsInReach(Character, CheeseStrikePickups::Reach))
		{
			continue;
		}
		const ETake How = HowFor(Character);
		if ((How == ETake::WalkOver || How == ETake::TopUp) && Character->GetInventory() != nullptr)
		{
			Character->GetInventory()->TakePickup(this, false);
		}
	}
}

void ACheeseStrikePickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = ServerNow();
	if (HasAuthority())
	{
		// A new round: every weapon back.
		const ACheeseStrikeGameState* Match = GetWorld()->GetGameState<ACheeseStrikeGameState>();
		const int32 Round = Match != nullptr ? Match->GetRound() : 0;
		if (Round != SeenRound)
		{
			if (SeenRound >= 0 && BackAt > 0.0f)
			{
				PutBack();
			}
			SeenRound = Round;
		}
		if (BackAt > 0.0f && Now >= BackAt)
		{
			PutBack();
		}
		if (Now >= NextCheckAt)
		{
			NextCheckAt = Now + WalkOverEvery;
			GiveToWhoeverWalksOver();
		}
	}
	UpdateLook();
	if (GetNetMode() == NM_DedicatedServer || IsHidden())
	{
		return;
	}
	// It turns and bobs, there or not.
	Spin = FMath::Fmod(Spin + CheeseStrikePickups::TurnRate * DeltaSeconds, 360.0f);
	const float Bob = FMath::Sin(Now * 2.0f + Spin * 0.01f) * CheeseStrikePickups::Bob;
	Spinner->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, CheeseStrikePickups::FloatHeight + Bob), FRotator(0.0f, Spin, 0.0f));
}

#if WITH_EDITOR
void ACheeseStrikePickup::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	// The weapon shown in the editor too, as the map's script names it.
	bLookApplied = false;
	ApplyLook();
}
#endif

namespace
{
	/** `CheeseStrikePickups`: every stand in the log, where it is, what it holds and how it is shown. */
	void ListPickups(UWorld* World)
	{
		for (TActorIterator<ACheeseStrikePickup> It(World); World != nullptr && It; ++It)
		{
			UE_LOG(LogCheeseStrike, Display, TEXT("%s: the %s at %s, %s, %.1f s to go, %s"), *It->GetName(), *It->GetWeaponName().ToString(), *It->FloorAt().ToCompactString(),
				It->IsInPlay() ? (It->IsThere() ? TEXT("there") : TEXT("empty")) : TEXT("out of play"), It->SecondsLeft(), *It->DescribeLook());
		}
	}

	FAutoConsoleCommandWithWorld PickupsCommand(
		TEXT("CheeseStrikePickups"),
		TEXT("CheeseStrikePickups: every weapon's stand in the log."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ListPickups));
}
