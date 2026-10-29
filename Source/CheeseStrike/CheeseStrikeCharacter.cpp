// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "CheeseStrikeCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/SkeletalMesh.h"
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "CheeseStrike.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Animation/CheeseStrikeArmsPose.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Materials/MaterialInterface.h"
#include "Input/CheeseStrikeInput.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Map/CheeseStrikeMouse.h"
#include "Map/CheeseStrikePlace.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "Rules/CheeseStrikeFalls.h"
#include "Rules/CheeseStrikeFootsteps.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/CheeseStrikeHUD.h"
#include "Weapons/CheeseStrikeGunfire.h"
#include "Weapons/CheeseStrikeGrenades.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Modes/CheeseStrikeLastStand.h"
#include "Effects/CheeseStrikeCheeseBlood.h"
#include "Weapons/CheeseStrikeWeapon.h"
#include "Weapons/CheeseStrikeWeaponLooks.h"
#include "Streaks/CheeseStrikeSynth.h"
#include "Sound/SoundWaveProcedural.h"

/** With this on, every character writes its state to the log four times a second, for watching a run from outside. */
static TAutoConsoleVariable<int32> CVarTraceState(TEXT("CheeseStrike.TraceState"), 0, TEXT("Log each character's state four times a second."));

const FVector ACheeseStrikeCharacter::DefaultCameraAtHead(-2.8f, 5.89f, 0.0f);
const FVector ACheeseStrikeCharacter::CameraFromEyes(-7.8f, -1.0f, 0.0f);

namespace
{
	/** Words in the names of a body's material slots that are its head, hidden from its own player: the camera is inside it. */
	const TCHAR* const HeadSlotWords[] = {TEXT("head"), TEXT("face"), TEXT("hair"), TEXT("eye"), TEXT("brow"), TEXT("mouth"), TEXT("teeth"), TEXT("tear"), TEXT("lash"), TEXT("glass")};
}

ACheeseStrikeCharacter::ACheeseStrikeCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCheeseStrikeMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	using namespace CheeseStrikeMovement;
	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	// Every body a character can have, and the looks that dress them in each
	// side's colours; it starts as the first, and puts on its player's side's
	// as soon as it knows it (UpdateBody).
	for (const CheeseStrikeBodies::FBody& Each : CheeseStrikeBodies::All())
	{
		ConstructorHelpers::FObjectFinder<USkeletalMesh> Model(Each.Mesh);
		BodyMeshes.Add(Model.Object);
		const int32 Index = BodyMeshes.Num() - 1;
		for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
		{
			for (int32 Slot = 0; Slot < Each.NumTeamSlots; ++Slot)
			{
				const FString Path = CheeseStrikeBodies::TeamLook(Index, Team, Each.TeamSlots[Slot]);
				ConstructorHelpers::FObjectFinder<UMaterialInterface> Look(*Path);
				TeamLooks.Add(FName(*Path), Look.Object);
			}
		}
	}

	// The body everyone else sees, and the copy of it its own player sees
	// (the same mesh and animation, so the arms in view are the character's
	// own), each animated in code with Lyra's rifle set retargeted to it,
	// standing on the capsule's floor, turned to face its front.
	USkeletalMesh* const First = BodyMeshes.IsEmpty() ? nullptr : BodyMeshes[0].Get();
	const TSubclassOf<UAnimInstance> BodyAnimation = UCheeseStrikeHeroAnimInstance::StaticClass();
	// No cloth simulation: Twinblast's coat would otherwise be simulated, and the engine's cloth vertex factory can render the body
	// swapped onto a live component before any simulated data is written (an assert in GetClothBufferIndexForReading). With no
	// cloth actors there is never any simulated data, so the vertex factory is never double buffered and never reads it.
	GetMesh()->SetAllowClothActors(false);
	GetMesh()->SetSkeletalMeshAsset(First);
	GetMesh()->SetAnimInstanceClass(BodyAnimation);
	GetMesh()->SetRelativeLocationAndRotation(FVector(-10.0f, 0.0f, -CapsuleHalfHeight), FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	// The cheese bled on the floor (its splats and pools, Effects/CheeseStrikeCheeseBlood) stays on the floor, not on a body lying in it.
	GetMesh()->SetReceivesDecals(false);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetAllowClothActors(false);
	FirstPersonMesh->SetSkeletalMeshAsset(First);
	FirstPersonMesh->SetAnimInstanceClass(BodyAnimation);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetReceivesDecals(false);

	// The eyes stand on the capsule at eye height, standing or crouched (the
	// engine moves BaseEyeHeight between the two), and the view follows them
	// smoothly rather than at once.
	BaseEyeHeight = EyeHeight;
	CrouchedEyeHeight = CrouchedEyeHeightAboveCentre;
	EyeOffset = EyeHeight;
	Eyes = CreateDefaultSubobject<USceneComponent>(TEXT("Eyes"));
	Eyes->SetupAttachment(GetCapsuleComponent());
	Eyes->SetRelativeLocation(FVector(0.0f, 0.0f, EyeHeight));

	// The camera sits on the first-person body's head, between its eyes
	// (FitFirstPersonBody); crouching lowers that body, and the camera with it.
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(CameraAtHead, FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	// The first-person body is drawn at a narrower field of view than the
	// world, which keeps a rifle this close from stretching; it isn't shrunk
	// towards the camera, which would pull it out of the hands' line.
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = false;
	FirstPersonCameraComponent->FirstPersonFieldOfView = FirstPersonFieldOfViewUp;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;
	BaseFieldOfView = FirstPersonCameraComponent->FieldOfView;
	Health = CheeseStrikeWeapons::MaxHealth;

	Inventory = CreateDefaultSubobject<UCheeseStrikeInventory>(TEXT("Inventory"));
	LastStand = CreateDefaultSubobject<UCheeseStrikeLastStand>(TEXT("LastStand"));
	Grenades = CreateDefaultSubobject<UCheeseStrikeGrenades>(TEXT("Grenades"));
}

bool ACheeseStrikeCharacter::IsDowned() const
{
	return LastStand != nullptr && LastStand->IsDowned();
}

bool ACheeseStrikeCharacter::InExecution() const
{
	return LastStand != nullptr && LastStand->InExecution();
}

float ACheeseStrikeCharacter::MeasureLegRoom() const
{
	using namespace CheeseStrikeDowned;
	const UWorld* World = GetWorld();
	const float Plenty = LegLength + ToeGap + 10.0f;
	if (World == nullptr)
	{
		return Plenty;
	}
	// A ball a hand over the floor swept from the capsule's axis (which has room) past where the feet would be, through
	// whatever stops the character but other players.
	constexpr float Ball = 8.0f;
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector From = GetActorLocation() + FVector(0.0f, 0.0f, 18.0f - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeLegRoom), false, this);
	FCollisionResponseParams Responses;
	GetCapsuleComponent()->InitSweepCollisionParams(Params, Responses);
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	FHitResult Hit;
	if (World->SweepSingleByChannel(Hit, From, From + Forward * (HipsAhead + Plenty - Ball), FQuat::Identity, GetCapsuleComponent()->GetCollisionObjectType(), FCollisionShape::MakeSphere(Ball), Params, Responses))
	{
		return FMath::Max(0.0f, Hit.Distance + Ball - HipsAhead);
	}
	return Plenty;
}

float ACheeseStrikeCharacter::GetDownAimYaw() const
{
	// Where the player looks, from the controls where there are any (its own machine, the server), else as the server sent it.
	if (Controller != nullptr)
	{
		return FMath::Clamp(FRotator::NormalizeAxis(Controller->GetControlRotation().Yaw - GetActorRotation().Yaw), -CheeseStrikeDowned::ConeYaw, CheeseStrikeDowned::ConeYaw);
	}
	return DownAimYawSent;
}

FVector ACheeseStrikeCharacter::DownBodyFrame(const FVector& World) const
{
	const FVector Floor = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return FRotator(0.0f, GetActorRotation().Yaw, 0.0f).UnrotateVector(World - Floor);
}

CheeseStrikeDowned::FShape ACheeseStrikeCharacter::GetDownShape() const
{
	const FVector Floor = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return CheeseStrikeDowned::Shape(Floor, GetActorRotation().Yaw, DownLegRoom);
}

FVector ACheeseStrikeCharacter::ChestAt() const
{
	return DownBlend > 0.5f ? CheeseStrikeDowned::Middle(GetDownShape()) : GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
}

void ACheeseStrikeCharacter::MakeRoomToLieDown()
{
	if (!HasAuthority())
	{
		return;
	}
	// Short of room for the legs ahead: back as far as is short, or as far as there is room behind.
	const float Short = CheeseStrikeDowned::LegLength + CheeseStrikeDowned::ToeGap - MeasureLegRoom();
	if (Short > 1.0f)
	{
		SetActorLocation(GetActorLocation() - GetActorForwardVector().GetSafeNormal2D() * Short, true);
	}
	DownLegRoom = MeasureLegRoom();
}

void ACheeseStrikeCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	// In an execution the body stays as it plays (the executor's facing the blow), whatever the view does.
	if (InExecution() && IsAlive())
	{
		return;
	}
	const bool bDown = IsAlive() && IsDowned();
	if (!IsAlive() || (!bDown && DownBlend <= 0.0f))
	{
		Super::FaceRotation(NewControlRotation, DeltaTime);
		return;
	}
	const float BodyYaw = GetActorRotation().Yaw;
	if (!bDown)
	{
		// Getting up: round to the view, quickly.
		SetActorRotation(FRotator(0.0f, CheeseStrikeDowned::TurnRising(BodyYaw, NewControlRotation.Yaw, DeltaTime), 0.0f));
		return;
	}
	// Down: the body turns round after the view, slowly, and its own machine keeps the view to the cone in front of it
	// (going down, the pitch eases into it rather than jumping).
	const float Turned = CheeseStrikeDowned::TurnBody(BodyYaw, NewControlRotation.Yaw, DeltaTime);
	SetActorRotation(FRotator(0.0f, Turned, 0.0f));
	if (IsLocallyControlled() && Controller != nullptr && !CheeseStrikeDowned::InCone(NewControlRotation, Turned, 0.01f))
	{
		FRotator Kept = CheeseStrikeDowned::ClampView(NewControlRotation, Turned);
		if (DownBlend < 1.0f)
		{
			Kept.Pitch = FMath::FInterpTo(FRotator::NormalizeAxis(NewControlRotation.Pitch), Kept.Pitch, DeltaTime, 6.0f);
		}
		Controller->SetControlRotation(Kept);
	}
}

void ACheeseStrikeCharacter::UpdateDown(float DeltaSeconds)
{
	const bool bDown = IsAlive() && IsDowned();
	DownBlend = FMath::FInterpConstantTo(DownBlend, bDown ? 1.0f : 0.0f, DeltaSeconds, 1.0f / (bDown ? CheeseStrikeDowned::Seconds : CheeseStrikeDowned::RiseSeconds));
	DownInView = FMath::SmoothStep(0.0f, 1.0f, DownBlend);
	if (bDown || DownBlend > 0.0f)
	{
		DownLegRoom = MeasureLegRoom();
	}
	if (bDown && HasAuthority())
	{
		DownAimYawSent = static_cast<int8>(FMath::RoundToInt(GetDownAimYaw()));
	}
	// The first-person body is drawn as the world is while down, so that its legs lie on the floor in view.
	if (IsLocallyControlled())
	{
		FirstPersonCameraComponent->FirstPersonFieldOfView = FMath::Lerp(FirstPersonFieldOfViewUp, FirstPersonCameraComponent->FieldOfView, DownInView);
		// The arm it is propped on is out of the way, beside and behind the view, but too close to it to show.
		static const FName PropArm(TEXT("upperarm_l"));
		const bool bHideArm = DownInView > 0.5f && IsAlive();
		if (FirstPersonMesh->GetSkeletalMeshAsset() != nullptr && FirstPersonMesh->GetBoneIndex(PropArm) != INDEX_NONE && bHideArm != FirstPersonMesh->IsBoneHiddenByName(PropArm))
		{
			if (bHideArm)
			{
				FirstPersonMesh->HideBoneByName(PropArm, PBO_None);
			}
			else
			{
				FirstPersonMesh->UnHideBoneByName(PropArm);
			}
		}
	}
}

void ACheeseStrikeCharacter::SetHealth(float InHealth)
{
	if (HasAuthority())
	{
		Health = FMath::Clamp(InHealth, 0.0f, CheeseStrikeWeapons::MaxHealth);
		HearHealth();
	}
}

void ACheeseStrikeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeCharacter, Health);
	DOREPLIFETIME(ACheeseStrikeCharacter, KilledBy);
	DOREPLIFETIME(ACheeseStrikeCharacter, DiedHow);
	DOREPLIFETIME(ACheeseStrikeCharacter, DiedAt);
	DOREPLIFETIME(ACheeseStrikeCharacter, ProtectedUntil);
	DOREPLIFETIME_CONDITION(ACheeseStrikeCharacter, DownAimYawSent, COND_SkipOwner);
}

UCheeseStrikeMovementComponent* ACheeseStrikeCharacter::GetCheeseStrikeMovement() const
{
	return Cast<UCheeseStrikeMovementComponent>(GetCharacterMovement());
}

bool ACheeseStrikeCharacter::FindEyes(const USkeletalMesh* Body, FName& Left, FName& Right)
{
	if (Body == nullptr)
	{
		return false;
	}
	const TPair<const TCHAR*, const TCHAR*> Pairs[] = {{TEXT("eyeball_l"), TEXT("eyeball_r")}, {TEXT("L_eye"), TEXT("R_eye")}};
	for (const TPair<const TCHAR*, const TCHAR*>& Pair : Pairs)
	{
		if (Body->GetRefSkeleton().FindBoneIndex(FName(Pair.Key)) != INDEX_NONE && Body->GetRefSkeleton().FindBoneIndex(FName(Pair.Value)) != INDEX_NONE)
		{
			Left = FName(Pair.Key);
			Right = FName(Pair.Value);
			return true;
		}
	}
	return false;
}

void ACheeseStrikeCharacter::FitFirstPersonBody()
{
	const USkeletalMesh* Asset = FirstPersonMesh->GetSkeletalMeshAsset();
	if (Asset == nullptr)
	{
		return;
	}
	const FName Head(TEXT("head"));
	FName LeftEye;
	FName RightEye;
	if (Asset->GetRefSkeleton().FindBoneIndex(Head) != INDEX_NONE && FindEyes(Asset, LeftEye, RightEye))
	{
		// Just under the eyes in the reference pose, in the head bone's own space (whose X is up the head and Y forward):
		// a shooter's view, which frames the arms and rifle and keeps the eye height of the movement's design.
		const FVector Between = (Asset->GetComposedRefPoseMatrix(LeftEye).GetOrigin() + Asset->GetComposedRefPoseMatrix(RightEye).GetOrigin()) * 0.5;
		CameraAtHead = Asset->GetComposedRefPoseMatrix(Head).InverseTransformPosition(Between) + CameraFromEyes;
	}
	else
	{
		CameraAtHead = DefaultCameraAtHead;
	}
	FirstPersonCameraComponent->SetRelativeLocation(CameraAtHead);
	// The head, which the camera is inside, is not drawn for its own player, nor what else its body hides from it.
	const int32 Hero = CheeseStrikeBodies::IndexOfMesh(Asset->GetPathName());
	const TArray<FSkeletalMaterial>& Slots = Asset->GetMaterials();
	for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
	{
		const FString Name = Slots[Slot].MaterialSlotName.ToString();
		bool bHead = false;
		for (const TCHAR* Word : HeadSlotWords)
		{
			bHead |= Name.Contains(Word, ESearchCase::IgnoreCase);
		}
		for (int32 Hidden = 0; Hero != INDEX_NONE && Hidden < CheeseStrikeBodies::All()[Hero].NumFirstPersonHiddenSlots; ++Hidden)
		{
			bHead |= Name == CheeseStrikeBodies::All()[Hero].FirstPersonHiddenSlots[Hidden];
		}
		if (bHead)
		{
			for (int32 Level = 0; Level < Asset->GetLODNum(); ++Level)
			{
				FirstPersonMesh->ShowMaterialSection(Slot, INDEX_NONE, false, Level);
			}
		}
	}
	ACheeseStrikeWeapon::HideBuiltInWeapons(FirstPersonMesh);
}

void ACheeseStrikeCharacter::UpdateBody()
{
	if (bDeathShown)
	{
		return;
	}
	int32 Wanted = ForcedBody;
	int32 Team = ForcedTeam;
	if (Wanted == INDEX_NONE)
	{
		const ACheeseStrikePlayerState* State = GetPlayerState<ACheeseStrikePlayerState>();
		if (State == nullptr)
		{
			return;
		}
		Wanted = BodyOf(GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr, State, Team);
	}
	if (Wanted != BodyIndex || Team != BodyTeam)
	{
		ApplyBody(Wanted, Team);
	}
}

int32 ACheeseStrikeCharacter::BodyOf(const ACheeseStrikeGameState* Match, const ACheeseStrikePlayerState* State, int32& OutTeam)
{
	OutTeam = INDEX_NONE;
	if (State == nullptr)
	{
		return INDEX_NONE;
	}
	// A team mode's sides, or everyone for themself (CheeseStrikeModes).
	const bool bTeams = Match == nullptr || Match->HasTeams();
	const int32 Side = Match != nullptr ? Match->TeamOf(State) : State->GetTeam();
	// Its rank: how many players come before it on its side (or in the match), by their ids, which every
	// machine knows.
	int32 Rank = 0;
	for (const APlayerState* Other : Match != nullptr ? Match->PlayerArray : TArray<TObjectPtr<APlayerState>>())
	{
		const ACheeseStrikePlayerState* Player = Cast<ACheeseStrikePlayerState>(Other);
		if (Player != nullptr && Player != State && (!bTeams || Match->TeamOf(Player) == Side) && Player->GetPlayerId() < State->GetPlayerId())
		{
			++Rank;
		}
	}
	OutTeam = bTeams ? Side : INDEX_NONE;
	return CheeseStrikeBodies::For(bTeams, Side, Rank);
}

void ACheeseStrikeCharacter::ApplyBody(int32 Index, int32 Team)
{
	if (!CheeseStrikeBodies::All().IsValidIndex(Index) || !BodyMeshes.IsValidIndex(Index) || BodyMeshes[Index] == nullptr)
	{
		return;
	}
	const CheeseStrikeBodies::FBody& Look = CheeseStrikeBodies::All()[Index];
	USkeletalMesh* const Model = BodyMeshes[Index];
	for (USkeletalMeshComponent* Shown : {GetMesh(), FirstPersonMesh.Get()})
	{
		if (Shown->GetSkeletalMeshAsset() != Model)
		{
			Shown->SetSkeletalMeshAsset(Model);
			// A fresh animation, which plays the new skeleton's set.
			Shown->SetAnimInstanceClass(nullptr);
			Shown->SetAnimInstanceClass(UCheeseStrikeHeroAnimInstance::StaticClass());
		}
		// Nothing the last body hid stays hidden.
		Shown->EmptyOverrideMaterials();
		for (int32 Level = 0; Level < Model->GetLODNum(); ++Level)
		{
			Shown->ShowAllMaterialSections(Level);
		}
		for (int32 Bone = 0; Bone < Model->GetRefSkeleton().GetNum(); ++Bone)
		{
			if (Shown->IsBoneHiddenByName(Model->GetRefSkeleton().GetBoneName(Bone)))
			{
				Shown->UnHideBoneByName(Model->GetRefSkeleton().GetBoneName(Bone));
			}
		}
		// Dressed in the side's colours, if it is on one.
		for (int32 Slot = 0; Team >= 0 && Team < CheeseStrikeRules::Teams && Slot < Look.NumTeamSlots; ++Slot)
		{
			if (UMaterialInterface* Dress = TeamLooks.FindRef(FName(*CheeseStrikeBodies::TeamLook(Index, Team, Look.TeamSlots[Slot]))))
			{
				Shown->SetMaterialByName(FName(Look.TeamSlots[Slot]), Dress);
			}
		}
		// The weapons it was modelled holding are hidden: only the game's own show.
		for (int32 Slot = 0; Slot < Look.NumWeaponSlots; ++Slot)
		{
			const FName Name(Look.WeaponSlots[Slot]);
			const int32 Material = Model->GetMaterials().IndexOfByPredicate([&Name](const FSkeletalMaterial& Each) { return Each.MaterialSlotName == Name; });
			for (int32 Level = 0; Material != INDEX_NONE && Level < Model->GetLODNum(); ++Level)
			{
				Shown->ShowMaterialSection(Material, INDEX_NONE, false, Level);
			}
		}
	}
	ACheeseStrikeWeapon::HideBuiltInWeapons(GetMesh());
	FitFirstPersonBody();
	BodyIndex = Index;
	BodyTeam = Team;
	// Its weapons, held as this body holds them.
	TArray<AActor*> Attached;
	GetAttachedActors(Attached);
	for (AActor* Each : Attached)
	{
		if (ACheeseStrikeWeapon* Weapon = Cast<ACheeseStrikeWeapon>(Each))
		{
			Weapon->FitToHands();
		}
	}
}

void ACheeseStrikeCharacter::CheeseStrikeBody(int32 Index, int32 Team)
{
	ForcedBody = CheeseStrikeBodies::All().IsValidIndex(Index) ? Index : INDEX_NONE;
	ForcedTeam = Team;
	UpdateBody();
}

void ACheeseStrikeCharacter::BeginPlay()
{
	Super::BeginPlay();
	ACheeseStrikeWeapon::HideBuiltInWeapons(GetMesh());
	FitFirstPersonBody();
	FirstPersonBase = FirstPersonMesh->GetRelativeTransform();
	// The server finds heads by the body's bones (SampleNow), so its copy of every body is posed whether or not it is drawn.
	if (HasAuthority())
	{
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	// A fresh character can't be hurt for its first seconds, counted from the end of the count before the match or its
	// round, unless it attacks.
	if (HasAuthority())
	{
		const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
		ProtectedUntil = CheeseStrikeStart::ProtectedUntil(ServerNow(), Match != nullptr ? Match->GetGoAt() : 0.0f);
	}
}

void ACheeseStrikeCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// The server gives every character its weapons as it is possessed, with the primary its player chose; they arrive on
	// every machine as actors of this character's (and go with it). A mode may give others after (SetPlayerDefaults).
	if (HasAuthority() && Inventory->GetHeld() == nullptr)
	{
		Inventory->GiveSpawnLoadout(NewController);
	}
}

ACheeseStrikeWeapon* ACheeseStrikeCharacter::GetWeapon() const
{
	return Inventory != nullptr ? Inventory->GetHeld() : nullptr;
}

float ACheeseStrikeCharacter::ServerNow() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
}

bool ACheeseStrikeCharacter::CanAct() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	const ACheeseStrikeGameState* Match = Cast<ACheeseStrikeGameState>(State);
	return CheeseStrikeStart::MayAct(IsAlive(), IsFrozen(), Match != nullptr && Match->IsRoundBreak(), State != nullptr && State->HasMatchEnded());
}

bool ACheeseStrikeCharacter::IsFrozen() const
{
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	return Match != nullptr && Match->IsFrozen();
}

bool ACheeseStrikeCharacter::TakesInput() const
{
	const ACheeseStrikePlayerController* Player = Cast<ACheeseStrikePlayerController>(GetController());
	return CanAct() && !InExecution() && (Player == nullptr || !Player->IsPaused());
}

bool ACheeseStrikeCharacter::IsProtected() const
{
	return IsAlive() && CheeseStrikeStart::IsProtected(ServerNow(), ProtectedUntil);
}

void ACheeseStrikeCharacter::EndProtection()
{
	ProtectedUntil = CheeseStrikeStart::AfterAttack(ProtectedUntil, ServerNow());
}

void ACheeseStrikeCharacter::UpdateGhost()
{
	const float Amount = IsAlive() ? CheeseStrikeStart::GhostAmount(ServerNow(), ProtectedUntil) : 0.0f;
	if (Amount <= 0.0f && GhostShown <= 0.0f)
	{
		return;
	}
	// The overlay is loaded by path (tools/art/ghost_material.py rebuilds it), instances of it per character for its fade:
	// one for the body, one with a softer rim for the first-person arms and weapon, which are seen up close and mostly
	// edge-on, where a full rim would turn them white.
	if (Amount > 0.0f && GhostMaterial == nullptr)
	{
		if (UMaterialInterface* Ghost = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CheeseStrike/Effects/M_Ghost.M_Ghost")))
		{
			GhostMaterial = UMaterialInstanceDynamic::Create(Ghost, this);
			GhostViewMaterial = UMaterialInstanceDynamic::Create(Ghost, this);
			GhostViewMaterial->SetScalarParameterValue(TEXT("RimCover"), 0.22f);
			GhostViewMaterial->SetScalarParameterValue(TEXT("RimGlow"), 0.45f);
			GhostViewMaterial->SetScalarParameterValue(TEXT("Cover"), 0.18f);
			GhostViewMaterial->SetScalarParameterValue(TEXT("Wash"), 0.14f);
		}
	}
	for (UMaterialInstanceDynamic* Ghost : {GhostMaterial.Get(), GhostViewMaterial.Get()})
	{
		if (Ghost != nullptr)
		{
			Ghost->SetScalarParameterValue(TEXT("Amount"), Amount);
		}
	}
	UMaterialInterface* Overlay = Amount > 0.0f ? GhostMaterial.Get() : nullptr;
	UMaterialInterface* ViewOverlay = Amount > 0.0f ? GhostViewMaterial.Get() : nullptr;
	if (GetMesh()->GetOverlayMaterial() != Overlay)
	{
		GetMesh()->SetOverlayMaterial(Overlay);
	}
	if (FirstPersonMesh->GetOverlayMaterial() != ViewOverlay)
	{
		FirstPersonMesh->SetOverlayMaterial(ViewOverlay);
	}
	// Its weapons too, the one in its own view and the one others see.
	TArray<AActor*> Attached;
	GetAttachedActors(Attached);
	for (AActor* Each : Attached)
	{
		if (ACheeseStrikeWeapon* Weapon = Cast<ACheeseStrikeWeapon>(Each))
		{
			Weapon->SetOverlay(Overlay, ViewOverlay);
		}
	}
	GhostShown = Amount;
}

FString ACheeseStrikeCharacter::DisplayName() const
{
	const APlayerState* Player = GetPlayerState();
	return Player != nullptr ? Player->GetPlayerName() : GetName();
}

FHitHistory::FSample ACheeseStrikeCharacter::SampleNow(float Time) const
{
	FHitHistory::FSample Sample;
	Sample.Time = Time;
	Sample.Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	Sample.Height = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
	Sample.Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Sample.Eyes = Eyes->GetComponentLocation();
	Sample.Yaw = GetActorRotation().Yaw;
	// The head where the body's head bone is now (standing, crouched, lying or mid-fall), shaped as its hero's head; without
	// one, a ball round the eyes.
	static const FName HeadBone(TEXT("head"));
	const int32 HeadIndex = CheeseStrikeBodies::All().IsValidIndex(BodyIndex) ? GetMesh()->GetBoneIndex(HeadBone) : INDEX_NONE;
	if (HeadIndex != INDEX_NONE)
	{
		const CheeseStrikeBodies::FBody& Hero = CheeseStrikeBodies::All()[BodyIndex];
		const FTransform Head = GetMesh()->GetBoneTransform(HeadIndex);
		Sample.HeadLow = Head.TransformPosition(Hero.HeadLow);
		Sample.HeadHigh = Head.TransformPosition(Hero.HeadHigh);
		Sample.HeadRadius = Hero.HeadRadius * GetMesh()->GetComponentScale().GetMax();
	}
	else
	{
		Sample.HeadLow = Sample.Eyes;
		Sample.HeadHigh = Sample.Eyes;
		Sample.HeadRadius = CheeseStrikeWeapons::HeadRadius;
	}
	// Lying (most of the way down), shots find the body where it lies.
	if (DownBlend > 0.5f)
	{
		const CheeseStrikeDowned::FShape Lying = GetDownShape();
		Sample.bLying = true;
		Sample.Hips = Lying.Hips;
		Sample.Shoulders = Lying.Shoulders;
		Sample.LegsTo = Lying.Feet;
	}
	return Sample;
}

void ACheeseStrikeCharacter::RecordHitBy(ACheeseStrikePlayerState* By)
{
	const float Now = ServerNow();
	HitBy.RemoveAll([Now](const CheeseStrikeCredit::FHit& Hit) { return Now - Hit.At > CheeseStrikeCredit::AssistWindow; });
	if (By != nullptr)
	{
		HitBy.Add({By->GetPlayerId(), Now});
	}
}

bool ACheeseStrikeCharacter::CanBeHurtBy(const ACheeseStrikeCharacter* From, const ACheeseStrikePlayerState* By) const
{
	const ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	return Mode == nullptr || Mode->CanHurt(By != nullptr ? By : (From != nullptr ? From->GetPlayerState<ACheeseStrikePlayerState>() : nullptr), GetPlayerState<ACheeseStrikePlayerState>(), From == this);
}

void ACheeseStrikeCharacter::NoteHit(FName Weapon, float Distance, bool bFromBehind)
{
	NotedHit = CheeseStrikeBadges::FKillFacts();
	NotedHit.Weapon = Weapon;
	NotedHit.Distance = Distance;
	NotedHit.bFromBehind = bFromBehind;
	bHitNoted = true;
}

bool ACheeseStrikeCharacter::TakeShot(float Damage, ACheeseStrikeCharacter* From, bool bHead)
{
	// What this shot is, for the kill's badges if it kills: the weapon's note, if it sent one.
	CheeseStrikeBadges::FKillFacts Facts = bHitNoted ? NotedHit : CheeseStrikeBadges::FKillFacts();
	bHitNoted = false;
	Facts.bHead = bHead;
	const float HealthBefore = Health;
	if (!HasAuthority() || !CanAct() || IsProtected() || !CanBeHurtBy(From, nullptr))
	{
		return false;
	}
	// Execution's last stand may take the hit: down instead of dead, or down already and out of reach.
	bool bHurt = false;
	if (LastStand != nullptr && LastStand->Intercept(Damage, From, nullptr, bHurt))
	{
		if (bHurt)
		{
			LastHurtBy = From;
			LastHurtAt = ServerNow();
		}
		return bHurt;
	}
	Health = FMath::Max(0.0f, Health - Damage);
	HearHealth();
	RecordHitBy(From != nullptr ? From->GetPlayerState<ACheeseStrikePlayerState>() : nullptr);
	LastHurtBy = From;
	LastHurtAt = ServerNow();
	if (From != nullptr && IsPlayerControlled())
	{
		ClientHurtFrom(From->GetActorLocation(), From);
	}
	if (!IsAlive())
	{
		Facts.bOneHit = HealthBefore >= CheeseStrikeWeapons::MaxHealth - 0.01f;
		DeathFacts = Facts;
		bDeathFactsFresh = true;
		Die(From, bHead);
	}
	return true;
}

bool ACheeseStrikeCharacter::TakeBlast(float Damage, ACheeseStrikePlayerState* By, const FString& How)
{
	ACheeseStrikeCharacter* From = By != nullptr ? By->GetPawn<ACheeseStrikeCharacter>() : nullptr;
	if (!HasAuthority() || !CanAct() || IsProtected() || By == nullptr || !CanBeHurtBy(nullptr, By))
	{
		return false;
	}
	// A streak never finishes anyone down: no hand from up close.
	bool bHurt = false;
	if (LastStand != nullptr && LastStand->Intercept(Damage, nullptr, By, bHurt))
	{
		return bHurt;
	}
	Health = FMath::Max(0.0f, Health - Damage);
	HearHealth();
	LastHurtBy = From;
	LastHurtAt = ServerNow();
	if (!IsAlive())
	{
		DeathFacts = CheeseStrikeBadges::FKillFacts();
		DeathFacts.bExecuted = LastStand != nullptr && LastStand->WasExecuted();
		bDeathFactsFresh = true;
		Die(From, false, By, How);
	}
	return true;
}

void ACheeseStrikeCharacter::Die(ACheeseStrikeCharacter* From, bool bHead, ACheeseStrikePlayerState* By, const FString& How)
{
	KilledBy = By != nullptr ? By->GetPlayerName() : (From != nullptr ? From->DisplayName() : FString());
	DiedHow = KilledBy.IsEmpty() ? How : FString();
	DiedAt = ServerNow();
	if (KilledBy.IsEmpty() && !How.IsEmpty())
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("%s %s"), *DisplayName(), *How);
	}
	else
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("%s is killed by %s%s"), *DisplayName(), KilledBy.IsEmpty() ? TEXT("nobody") : *KilledBy, How.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("'s %s"), *How));
	}
	ShowDeath();
	// A death by anything but a shot or a blast (a fall) has no blow's facts.
	if (!bDeathFactsFresh)
	{
		DeathFacts = CheeseStrikeBadges::FKillFacts();
	}
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		Mode->Killed(From, this, bHead, By, How);
	}
	bDeathFactsFresh = false;
}

void ACheeseStrikeCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	// Under the map's kill height (into The Fondue Floor's vat): killed by the fall, not destroyed as the engine
	// would, so that the body is seen going in and its player watches from where they fell. It counts as a kill for
	// whoever hurt them just before; else the feed says how they died. The engine asks again every frame the body
	// stays down there.
	if (!HasAuthority() || !CanAct())
	{
		return;
	}
	ACheeseStrikeCharacter* Pusher = FallCreditedTo();
	const FString How = Pusher != nullptr ? FString() : CheeseStrikeFalls::Words(ACheeseStrikePlace::NameAt(GetWorld(), GetActorLocation()));
	UE_LOG(LogCheeseStrike, Display, TEXT("%s fell below the kill height at %s"), *DisplayName(), *GetActorLocation().ToCompactString());
	Health = 0.0f;
	Die(Pusher, false, nullptr, How);
}

ACheeseStrikeCharacter* ACheeseStrikeCharacter::FallCreditedTo() const
{
	ACheeseStrikeCharacter* Pusher = LastHurtBy.Get();
	if (Pusher == this || !CheeseStrikeFalls::CreditsHurter(LastHurtAt, ServerNow()) || !CanBeHurtBy(Pusher, nullptr))
	{
		return nullptr;
	}
	return Pusher;
}

void ACheeseStrikeCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	// The speed it comes down at (the server's own simulation of the fall, a player's as their moves arrive) tells
	// the drop, a jump's rise included.
	if (!HasAuthority() || GetCharacterMovement() == nullptr)
	{
		return;
	}
	const float Drop = CheeseStrikeFalls::DropFor(-GetCharacterMovement()->Velocity.Z, -GetCharacterMovement()->GetGravityZ());
	const float Damage = CheeseStrikeFalls::DamageFor(Drop);
	if (Damage > 0.0f)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("%s lands after a drop of %.0f cm: %.0f damage"), *DisplayName(), Drop, Damage);
		TakeFall(Damage);
	}
}

bool ACheeseStrikeCharacter::TakeFall(float Damage)
{
	if (!HasAuthority() || !CanAct() || IsProtected() || Damage <= 0.0f)
	{
		return false;
	}
	ACheeseStrikeCharacter* Pusher = FallCreditedTo();
	MulticastHardLanding(Damage);
	// Execution's last stand takes a fall as any hit: down instead of dead (to whoever pushed them), and once down, a
	// fall never finishes them.
	bool bHurt = false;
	if (LastStand != nullptr && LastStand->Intercept(Damage, nullptr, Pusher != nullptr ? Pusher->GetPlayerState<ACheeseStrikePlayerState>() : nullptr, bHurt))
	{
		return bHurt;
	}
	Health = FMath::Max(0.0f, Health - Damage);
	HearHealth();
	if (!IsAlive())
	{
		Die(Pusher, false, nullptr, Pusher != nullptr ? FString() : CheeseStrikeFalls::HardLandingWords());
	}
	return true;
}

bool ACheeseStrikeCharacter::TakeOwnBlast(float Damage, const FString& How)
{
	if (!HasAuthority() || !CanAct() || IsProtected() || Damage <= 0.0f)
	{
		return false;
	}
	ACheeseStrikeCharacter* Pusher = FallCreditedTo();
	bool bHurt = false;
	if (LastStand != nullptr && LastStand->Intercept(Damage, nullptr, Pusher != nullptr ? Pusher->GetPlayerState<ACheeseStrikePlayerState>() : nullptr, bHurt))
	{
		return bHurt;
	}
	Health = FMath::Max(0.0f, Health - Damage);
	HearHealth();
	if (!IsAlive())
	{
		Die(Pusher, false, nullptr, Pusher != nullptr ? FString() : How);
	}
	return true;
}

void ACheeseStrikeCharacter::MulticastHardLanding_Implementation(float Damage)
{
	if (IsLocallyControlled())
	{
		FallFlashAt = GetWorld()->GetTimeSeconds();
	}
	static TSoftObjectPtr<USoundBase> Thud(FSoftObjectPath(TEXT("/Game/Audio/Sounds/SFX/Lyra_MeleeImpact_02.Lyra_MeleeImpact_02")));
	USoundBase* Sound = Thud.LoadSynchronous();
	if (Sound == nullptr || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// A heavy, low thud: the knife's impact pitched down, louder and lower the harder the landing.
	const float Hard = FMath::Clamp(Damage / CheeseStrikeFalls::DeadlyDamage, 0.0f, 1.0f);
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	UGameplayStatics::PlaySoundAtLocation(this, Sound, Feet, FRotator::ZeroRotator, 0.8f + 0.7f * Hard, 0.62f - 0.2f * Hard, 0.0f, CheeseStrikeGunfire::AttenuationAsset());
}

FVector ACheeseStrikeCharacter::FeetNow() const
{
	return GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void ACheeseStrikeCharacter::MulticastBleed_Implementation(FVector_NetQuantize10 FromFeet, FVector_NetQuantizeNormal Way, uint8 Damage)
{
	// Where the hit was on the body as the shooter saw it, put on the body where this machine has it now.
	if (ACheeseStrikeCheeseBlood* Blood = ACheeseStrikeCheeseBlood::For(GetWorld()))
	{
		Blood->ShotSpray(FeetNow() + FVector(FromFeet), FVector(Way), static_cast<float>(Damage));
	}
}

void ACheeseStrikeCharacter::CheeseStrikeBleed(float Damage, float Yaw)
{
#if !UE_BUILD_SHIPPING
	// A shot from `Yaw` off ahead, level, into the chest: it goes out the other way.
	const FVector Way = -FRotator(0.0f, GetActorRotation().Yaw + Yaw, 0.0f).Vector();
	const FVector Chest = FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 1.45f) - Way * 12.0f;
	MulticastBleed_Implementation(Chest, Way, static_cast<uint8>(FMath::Clamp(Damage, 0.0f, 255.0f)));
#endif
}

void ACheeseStrikeCharacter::CheeseStrikeDie()
{
	if (HasAuthority())
	{
		ServerDie_Implementation();
	}
	else
	{
		ServerDie();
	}
}

void ACheeseStrikeCharacter::CheeseStrikeProtect(float Seconds)
{
	if (HasAuthority())
	{
		ServerProtect_Implementation(Seconds);
	}
	else
	{
		ServerProtect(Seconds);
	}
}

void ACheeseStrikeCharacter::ServerProtect_Implementation(float Seconds)
{
#if !UE_BUILD_SHIPPING
	ProtectedUntil = ServerNow() + FMath::Clamp(Seconds, 0.0f, 600.0f);
#endif
}

void ACheeseStrikeCharacter::SeeOwnBody(bool bOutside)
{
	// The body and its weapons, seen as others see them, or kept from the player as in play.
	GetMesh()->SetOwnerNoSee(!bOutside);
	GetMesh()->SetFirstPersonPrimitiveType(bOutside ? EFirstPersonPrimitiveType::None : EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	FirstPersonMesh->SetVisibility(!bOutside, true);
	for (const EWeaponSlot Slot : {EWeaponSlot::Primary, EWeaponSlot::Sidearm, EWeaponSlot::Knife})
	{
		if (ACheeseStrikeWeapon* Carried = Inventory->GetInSlot(Slot))
		{
			Carried->SetSeenFromOutside(bOutside);
		}
	}
}

void ACheeseStrikeCharacter::CheeseStrikeOutside(float Distance, float Yaw, float Height, float Rise)
{
	APlayerController* Player = Cast<APlayerController>(GetController());
	if (Player == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	const bool bOutside = Distance > 0.0f;
	SeeOwnBody(bOutside);
	if (OutsideCamera != nullptr)
	{
		OutsideCamera->Destroy();
		OutsideCamera = nullptr;
	}
	if (!bOutside)
	{
		Player->SetViewTarget(this);
		return;
	}
	// Once dead, the body lies where it fell, not where the character stood: the view is round its head.
	const FVector Middle = (bDeathShown ? GetMesh()->GetBoneLocation(TEXT("head")) : GetActorLocation()) + FVector(0.0f, 0.0f, Height);
	const FVector Way = FRotator(0.0f, GetActorRotation().Yaw + Yaw, 0.0f).Vector();
	const FVector At = Middle + Way * Distance + FVector(0.0f, 0.0f, Rise);
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	OutsideCamera = GetWorld()->SpawnActor<ACameraActor>(At, (Middle - At).Rotation(), Parameters);
	if (OutsideCamera != nullptr)
	{
		OutsideCamera->GetCameraComponent()->SetFieldOfView(60.0f);
		// Viewing through a camera turns the controls to face the way it does; the character keeps facing and aiming as before.
		const FRotator Kept = Player->GetControlRotation();
		Player->SetViewTarget(OutsideCamera);
		Player->SetControlRotation(Kept);
	}
}

void ACheeseStrikeCharacter::CheeseStrikeViewFrom(float X, float Y, float Z, float Pitch, float Yaw)
{
	APlayerController* Player = Cast<APlayerController>(GetController());
	if (Player == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	if (OutsideCamera != nullptr)
	{
		OutsideCamera->Destroy();
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	OutsideCamera = GetWorld()->SpawnActor<ACameraActor>(FVector(X, Y, Z), FRotator(Pitch, Yaw, 0.0f), Parameters);
	if (OutsideCamera != nullptr)
	{
		OutsideCamera->GetCameraComponent()->SetFieldOfView(70.0f);
		const FRotator Kept = Player->GetControlRotation();
		Player->SetViewTarget(OutsideCamera);
		Player->SetControlRotation(Kept);
	}
}

void ACheeseStrikeCharacter::CheeseStrikeViewMouse(float Distance, float Yaw, float Height)
{
	APlayerController* Player = Cast<APlayerController>(GetController());
	if (Player == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	TActorIterator<ACheeseStrikeMouse> Mouse(GetWorld());
	if (!Mouse)
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("CheeseStrikeViewMouse: no mice"));
		return;
	}
	const FVector Middle = Mouse->GetActorLocation();
	const FVector At = Middle + FRotator(0.0f, Mouse->GetActorRotation().Yaw + Yaw, 0.0f).Vector() * Distance + FVector(0.0f, 0.0f, Height);
	CheeseStrikeViewFrom(At.X, At.Y, At.Z, (Middle - At).Rotation().Pitch, (Middle - At).Rotation().Yaw);
	if (OutsideCamera != nullptr)
	{
		OutsideCamera->AttachToActor(*Mouse, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void ACheeseStrikeCharacter::ServerDie_Implementation()
{
	EndProtection();
	TakeShot(CheeseStrikeWeapons::MaxHealth, nullptr, false);
}

void ACheeseStrikeCharacter::ClientHurtFrom_Implementation(FVector_NetQuantize From, ACheeseStrikeCharacter* By)
{
	const APlayerController* Player = Cast<APlayerController>(GetController());
	if (ACheeseStrikeHUD* HUD = Player != nullptr ? Cast<ACheeseStrikeHUD>(Player->GetHUD()) : nullptr)
	{
		HUD->AddDamageFrom(From, By);
	}
}

void ACheeseStrikeCharacter::UpdateFootsteps(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer || GetCharacterMovement() == nullptr)
	{
		return;
	}
	const UCheeseStrikeMovementComponent* Moving = GetCheeseStrikeMovement();
	const float Speed = GetVelocity().Size2D();
	const CheeseStrikeFootsteps::EGait Gait = CheeseStrikeFootsteps::GaitFor(IsAlive(), GetCharacterMovement()->IsMovingOnGround(), Moving != nullptr && Moving->IsSliding(), bIsCrouched, Moving != nullptr && Moving->IsSprinting(), Speed);
	if (!CheeseStrikeFootsteps::Advance(StepProgress, Gait, Speed * DeltaSeconds))
	{
		return;
	}
	// Heard from the feet, fading with distance (the listener's own too, which are right under them); a few sounds, varied a little in pitch.
	static USoundAttenuation* Attenuation = nullptr;
	if (Attenuation == nullptr)
	{
		Attenuation = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("CheeseStrikeFootstepAttenuation"));
		Attenuation->Attenuation.bAttenuate = true;
		Attenuation->Attenuation.bSpatialize = true;
		Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Attenuation->Attenuation.AttenuationShapeExtents = FVector(CheeseStrikeFootsteps::FullWithin, 0.0f, 0.0f);
		Attenuation->Attenuation.FalloffDistance = CheeseStrikeFootsteps::FalloffDistance;
		Attenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		Attenuation->Attenuation.dBAttenuationAtMax = -40.0f;
		Attenuation->Attenuation.FalloffMode = ENaturalSoundFalloffMode::Silent;
		Attenuation->Attenuation.NonSpatializedRadiusStart = 100.0f;
		Attenuation->Attenuation.NonSpatializedRadiusEnd = 30.0f;
		Attenuation->AddToRoot();
	}
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	UGameplayStatics::SpawnSoundAtLocation(this, CheeseStrikeSynth::MakeSound(this, CheeseStrikeSynth::Footstep(StepsTaken)), Feet, FRotator::ZeroRotator, CheeseStrikeFootsteps::Volume(Gait), FMath::FRandRange(0.92f, 1.08f), 0.0f, Attenuation);
	++StepsTaken;
}

void ACheeseStrikeCharacter::HearHealth()
{
	// A grunt for any damage taken, by a shot, a blast, a fall or anything else: heard by the one hurt, whose health it is.
	const float Lost = HeardHealth - Health;
	HeardHealth = Health;
	if (Lost > 0.0f && IsAlive() && IsLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(this, CheeseStrikeSynth::MakeSound(this, CheeseStrikeSynth::Grunt(0.3f, FMath::FRandRange(0.9f, 1.1f))), 0.8f);
	}
}

void ACheeseStrikeCharacter::OnRep_Health()
{
	// A client hears its health change here; the server, which the replication never tells (a listen server's host, or a bot's
	// machine), hears it where the damage is dealt.
	HearHealth();
	if (!IsAlive())
	{
		ShowDeath();
	}
}

void ACheeseStrikeCharacter::ShowDeath()
{
	if (bDeathShown)
	{
		return;
	}
	bDeathShown = true;
	if (ACheeseStrikeWeapon* Held = GetWeapon())
	{
		Held->StopFiring();
		Held->StopAiming();
	}
	// The body goes on the way the character was going, falling as it goes.
	const FVector Going = GetVelocity();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The body falls where it is, and its own player sees it fall: the view
	// stays where the eyes were, free to look round, until a new character
	// comes.
	FirstPersonCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	FirstPersonMesh->SetVisibility(false, true);
	// The body is no longer the first-person view's stand-in for lighting: it is a body, seen by all.
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetAllPhysicsLinearVelocity(Going);
}

void ACheeseStrikeCharacter::ApplyStartAt()
{
	// For watching a run from outside: -StartAt=X,Y,Z,Yaw puts the local
	// player's first character there, facing that way, instead of wherever
	// the level starts them: once, on the first tick it is possessed; the
	// characters that follow a death come back where the rules say.
	FString Spec;
	ACheeseStrikePlayerController* Player = Cast<ACheeseStrikePlayerController>(GetController());
	if (!bStartAtApplied && IsLocallyControlled() && Player != nullptr && FParse::Value(FCommandLine::Get(), TEXT("StartAt="), Spec, false))
	{
		bStartAtApplied = true;
		if (!Player->TakeStartAt())
		{
			return;
		}
		TArray<FString> Parts;
		Spec.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 3)
		{
			const FVector At(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			const float Yaw = Parts.Num() >= 4 ? FCString::Atof(*Parts[3]) : 0.0f;
			if (HasAuthority())
			{
				StartAt(At, Yaw);
			}
			else
			{
				ServerStartAt(At, Yaw);
			}
		}
	}
}

void ACheeseStrikeCharacter::StartAt(const FVector& At, float Yaw)
{
	SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
	if (GetController() != nullptr)
	{
		GetController()->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
	}
}

void ACheeseStrikeCharacter::ServerStartAt_Implementation(FVector_NetQuantize At, float Yaw)
{
	StartAt(At, Yaw);
	// The client's own view turns too: the control rotation isn't sent back.
	ClientStartAt(At, Yaw);
}

void ACheeseStrikeCharacter::ClientStartAt_Implementation(FVector_NetQuantize At, float Yaw)
{
	if (GetController() != nullptr)
	{
		GetController()->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
	}
}

FVector ACheeseStrikeCharacter::LeftHandOnRifle() const
{
	const USkeletalMeshComponent* Rifle = GetWeapon() != nullptr ? GetWeapon()->GetFirstPersonMesh() : nullptr;
	if (Rifle == nullptr || FirstPersonMesh == nullptr)
	{
		return FVector::ZeroVector;
	}
	return Rifle->GetComponentTransform().InverseTransformPosition(FirstPersonMesh->GetBoneLocation(TEXT("hand_l")));
}

void ACheeseStrikeCharacter::NoteWeaponInView(const FTransform& InView)
{
	if (WeaponInViewKnown >= 2)
	{
		// How it moved and turned this frame against the last: a shake turns one way, then the other.
		const FVector Moved = InView.GetLocation() - WeaponInView[0].GetLocation();
		const FVector MovedBefore = WeaponInView[0].GetLocation() - WeaponInView[1].GetLocation();
		const FQuat Turned = InView.GetRotation() * WeaponInView[0].GetRotation().Inverse();
		const FQuat TurnedBefore = WeaponInView[0].GetRotation() * WeaponInView[1].GetRotation().Inverse();
		TrembleSum.X += (Moved - MovedBefore).Size();
		TrembleSum.Y += FMath::RadiansToDegrees((Turned * TurnedBefore.Inverse()).GetNormalized().GetAngle());
		++TrembleFrames;
	}
	WeaponInView[1] = WeaponInView[0];
	WeaponInView[0] = InView;
	WeaponInViewKnown = FMath::Min(WeaponInViewKnown + 1, 2);
}

FVector2D ACheeseStrikeCharacter::TakeViewTremble()
{
	const FVector2D Mean = TrembleFrames > 0 ? TrembleSum / TrembleFrames : FVector2D::ZeroVector;
	TrembleSum = FVector2D::ZeroVector;
	TrembleFrames = 0;
	return Mean;
}

static TAutoConsoleVariable<float> CVarSightSide(TEXT("CheeseStrike.SightSide"), ACheeseStrikeCharacter::SightSide, TEXT("Where the rifle's sight line is across it, from its bounds' middle (cm)."));
static TAutoConsoleVariable<float> CVarSightHeight(TEXT("CheeseStrike.SightHeight"), ACheeseStrikeCharacter::SightHeight, TEXT("Where the rifle's sight line is, down from its top (cm)."));
static TAutoConsoleVariable<float> CVarSightBack(TEXT("CheeseStrike.SightBack"), ACheeseStrikeCharacter::SightBack, TEXT("How far along the rifle from its back end the eye is when aiming (cm)."));

bool ACheeseStrikeCharacter::WeaponSight(FVector& OutSight) const
{
	const ACheeseStrikeWeapon* Held = GetWeapon();
	if (Held == nullptr)
	{
		return false;
	}
	if (Held->GetSight(OutSight))
	{
		return true;
	}
	// The rifle's: SightBack along it from its back end, behind the rear sight as a cheek on the stock puts it, a little
	// down from its top (the rifle points a little under the view), across it from its middle.
	const UPrimitiveComponent* Rifle = Held->GetModel(true);
	const USkeletalMeshComponent* Skinned = Cast<USkeletalMeshComponent>(Rifle);
	const UStaticMeshComponent* Plain = Cast<UStaticMeshComponent>(Rifle);
	if (Skinned != nullptr && Skinned->GetSkeletalMeshAsset() != nullptr)
	{
		const FBoxSphereBounds Bounds = Skinned->GetSkeletalMeshAsset()->GetBounds();
		OutSight = FVector(Bounds.Origin.X + CVarSightSide.GetValueOnGameThread(), Bounds.Origin.Y - Bounds.BoxExtent.Y + CVarSightBack.GetValueOnGameThread(), Bounds.Origin.Z + Bounds.BoxExtent.Z - CVarSightHeight.GetValueOnGameThread());
		return true;
	}
	if (Plain != nullptr && Plain->GetStaticMesh() != nullptr)
	{
		const FBoxSphereBounds Bounds = Plain->GetStaticMesh()->GetBounds();
		OutSight = FVector(Bounds.Origin.X + CVarSightSide.GetValueOnGameThread(), Bounds.Origin.Y - Bounds.BoxExtent.Y + CVarSightBack.GetValueOnGameThread(), Bounds.Origin.Z + Bounds.BoxExtent.Z - CVarSightHeight.GetValueOnGameThread());
		return true;
	}
	return false;
}

void ACheeseStrikeCharacter::PlaceView()
{
	// Only for its own player, alive, with the camera riding the arms (after a death it is left where the eyes were).
	if (!IsLocallyControlled() || FirstPersonMesh->GetAttachParent() == nullptr || !IsAlive() || FirstPersonCameraComponent->GetAttachParent() != FirstPersonMesh)
	{
		return;
	}
	// The body stays on the body others see: the animation places the arms in view, not the body.
	if (!FirstPersonMesh->GetRelativeTransform().Equals(FirstPersonBase, 0.01f))
	{
		FirstPersonMesh->SetRelativeTransform(FirstPersonBase);
	}
	// Just under the eyes; down, a little ahead of the face, out of the collar and the shoulders of a body lying propped up.
	const FVector Ahead = FirstPersonMesh->GetSocketTransform(FName("head")).InverseTransformVectorNoScale(GetControlRotation().Vector());
	FirstPersonCameraComponent->SetRelativeLocation(CameraAtHead + Ahead * DownCameraAhead * DownInView);
	if (DownInView > 0.0f)
	{
		KeepDownViewClear();
	}
}

void ACheeseStrikeCharacter::KeepDownViewClear()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const FVector Wanted = FirstPersonCameraComponent->GetComponentLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeDownView), false, this);
	FVector At = Wanted;
	FHitResult Hit;
	// From the capsule's middle (which the body lies round, with room) out to where the head has the eyes.
	if (World->SweepSingleByChannel(Hit, GetActorLocation(), Wanted, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(DownViewRoom), Params))
	{
		At = Hit.Location;
	}
	// And high enough over what is under it.
	if (World->LineTraceSingleByChannel(Hit, At + FVector(0.0f, 0.0f, DownViewRoom), At - FVector(0.0f, 0.0f, DownViewHeight), ECC_Camera, Params))
	{
		At.Z = FMath::Max(At.Z, Hit.ImpactPoint.Z + DownViewHeight);
	}
	if (!At.Equals(Wanted, 0.1f))
	{
		FirstPersonCameraComponent->SetWorldLocation(FMath::Lerp(Wanted, At, DownInView));
	}
}

void ACheeseStrikeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyStartAt();
	UpdateBody();
	UpdateGhost();
	UpdateDown(DeltaSeconds);
	UpdateFootsteps(DeltaSeconds);
	ResumeSprintKey();
	// The server keeps where everyone has been, for shots fired at where they were seen.
	if (HasAuthority() && GetWorld() != nullptr && GetWorld()->GetGameState() != nullptr)
	{
		HitHistory.Add(SampleNow(GetWorld()->GetGameState()->GetServerWorldTimeSeconds()));
	}
	// Aiming down the sights narrows the view, smoothly, and brings the rifle to the eye: the arms are
	// moved and turned so that the rifle's sight line runs straight down the view from just in front of
	// the eye, which stays where the head has it. Shots leave from the eye along the view, so they go
	// where the sights point.
	const bool bAimed = GetWeapon() != nullptr && GetWeapon()->IsAimed();
	const float WantedFieldOfView = BaseFieldOfView * (bAimed ? GetWeapon()->GetStats().AimZoom : 1.0f);
	FirstPersonCameraComponent->SetFieldOfView(FMath::FInterpTo(FirstPersonCameraComponent->FieldOfView, WantedFieldOfView, DeltaSeconds, 12.0f));
	AimFraction = FMath::FInterpTo(AimFraction, bAimed ? 1.0f : 0.0f, DeltaSeconds, 12.0f);
	PlaceView();
	// Sliding, its own player's view leans into it.
	const UCheeseStrikeMovementComponent* Moving = GetCheeseStrikeMovement();
	const bool bSliding = Moving != nullptr && Moving->IsSliding() && IsAlive();
	ViewRoll = FMath::FInterpTo(ViewRoll, bSliding ? CheeseStrikeArms::SlideTilt * CheeseStrikeArms::SlideDepth(GetVelocity().Size2D()) : 0.0f, DeltaSeconds, CheeseStrikeArms::HoldBlendRate);
	// Through a scope, the arms and the weapon are out of the view, and the HUD draws the scope over it.
	const bool bScopeView = IsLocallyControlled() && IsAlive() && OutsideCamera == nullptr && bAimed && GetWeapon() != nullptr && GetWeapon()->GetStats().bScoped && AimFraction >= ScopeFrom;
	if (bScopeView != bLookingThroughScope)
	{
		bLookingThroughScope = bScopeView;
		FirstPersonMesh->SetVisibility(!bScopeView, true);
	}
	// Traced with the console variable, or with -TraceState on the command line.
	static const bool bTraceSwitch = FParse::Param(FCommandLine::Get(), TEXT("TraceState"));
	if ((bTraceSwitch || CVarTraceState.GetValueOnGameThread() != 0) && GetWorld() != nullptr)
	{
		SinceTrace += DeltaSeconds;
		if (SinceTrace >= 0.1f)
		{
			SinceTrace = 0.0f;
			if (IsLocallyControlled() && IsPlayerControlled())
			{
				CheeseStrikeState();
			}
			else if (!IsAlive())
			{
				// Another player's body, as this machine has it.
				const FVector Pelvis = GetMesh()->GetBoneLocation(TEXT("pelvis"));
				UE_LOG(LogCheeseStrike, Display, TEXT("Body of %s: pelvis (%.0f, %.0f, %.0f) fallen %d visible %d"), *DisplayName(), Pelvis.X, Pelvis.Y, Pelvis.Z, GetMesh()->IsSimulatingPhysics() ? 1 : 0, GetMesh()->IsVisible() ? 1 : 0);
			}
		}
	}
	// The eyes settle at their height for standing or crouched; the view
	// itself rides the first-person body's head, which the crouch animation
	// lowers.
	EyeOffset = FMath::FInterpTo(EyeOffset, BaseEyeHeight, DeltaSeconds, CheeseStrikeMovement::CrouchRate);
	// Down, they are where the raised head is.
	const FVector LyingHead = CheeseStrikeDowned::HeadAt() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Eyes->SetRelativeLocation(FMath::Lerp(FVector(0.0f, 0.0f, EyeOffset), LyingHead, FMath::SmoothStep(0.0f, 1.0f, DownBlend)));
}

FRotator ACheeseStrikeCharacter::GetScopeSway() const
{
	const ACheeseStrikeWeapon* Held = GetWeapon();
	if (Held == nullptr || !Held->GetStats().bScoped || !Held->IsAimed() || !IsLocallyControlled() || !IsPlayerControlled() || !IsAlive() || GetWorld() == nullptr)
	{
		return FRotator::ZeroRotator;
	}
	const FVector2D Sway = CheeseStrikeWeapons::ScopeSway(GetWorld()->GetTimeSeconds()) * AimFraction;
	return FRotator(Sway.Y, Sway.X, 0.0f);
}

void ACheeseStrikeCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// The capsule's centre has just dropped; the eyes stay where they were for the moment, then come down.
	EyeOffset += ScaledHalfHeightAdjust;
}

void ACheeseStrikeCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	EyeOffset -= ScaledHalfHeightAdjust;
}

void ACheeseStrikeCharacter::CheeseStrikeState()
{
	const UCheeseStrikeMovementComponent* Movement = GetCheeseStrikeMovement();
	const FVector At = GetActorLocation();
	const FVector Eye = FirstPersonCameraComponent->GetComponentLocation();
	const FVector Body = GetMesh()->GetComponentLocation();
	const ACheeseStrikeWeapon* HeldWeapon = GetWeapon();
	// Which way the weapon in view points, in the view's axes (forward, right, up).
	const UPrimitiveComponent* InView = HeldWeapon != nullptr ? HeldWeapon->GetModel(true) : nullptr;
	const FVector Along = InView != nullptr ? GetControlRotation().UnrotateVector(InView->GetComponentTransform().TransformVectorNoScale(HeldWeapon->GetModelAlong())) : FVector::ZeroVector;
	// And where the hand holding it is (forward, right, up from the eye).
	const FVector HandInView = GetControlRotation().UnrotateVector(FirstPersonMesh->GetBoneLocation(TEXT("hand_r")) - Eye);
	UE_LOG(LogCheeseStrike, Display, TEXT("State: at (%.0f, %.0f, %.0f) speed %.0f eye %.0f above the floor pitch %.1f yaw %.1f crouched %d sprinting %d sliding %d (%.2f s) falling %d jump %d/%d health %.0f alive %d protected %d ghost %.2f frozen %d ammo %d/%d aimed %d fov %.0f shots %d hits %d reloading %d weapon %s drawing %d body (%.0f, %.0f, %.0f) fallen %d camera (%.0f, %.0f, %.0f) hand in view (%.0f, %.0f, %.0f) weapon along (%.2f, %.2f, %.2f) down %.2f body yaw %.1f leg room %.0f grenades %d throwing %d"),
		At.X, At.Y, At.Z, GetVelocity().Size2D(), Eye.Z - (At.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), FRotator::NormalizeAxis(GetControlRotation().Pitch), FRotator::NormalizeAxis(GetControlRotation().Yaw),
		bIsCrouched ? 1 : 0, Movement != nullptr && Movement->IsSprinting() ? 1 : 0, Movement != nullptr && Movement->IsSliding() ? 1 : 0,
		Movement != nullptr ? Movement->GetSlideElapsed() : -1.0f, Movement != nullptr && Movement->IsFalling() ? 1 : 0, bPressedJump ? 1 : 0, JumpCurrentCount,
		Health, IsAlive() ? 1 : 0, IsProtected() ? 1 : 0, GhostShown, IsFrozen() ? 1 : 0, HeldWeapon != nullptr ? HeldWeapon->GetMagazine() : -1, HeldWeapon != nullptr ? HeldWeapon->GetReserve() : -1, HeldWeapon != nullptr && HeldWeapon->IsAimed() ? 1 : 0,
		FirstPersonCameraComponent->FieldOfView, HeldWeapon != nullptr ? HeldWeapon->ShotsFired : 0, HeldWeapon != nullptr ? HeldWeapon->HitsConfirmed : 0, HeldWeapon != nullptr && HeldWeapon->IsReloading() ? 1 : 0,
		HeldWeapon != nullptr ? *HeldWeapon->GetWeaponId().ToString() : TEXT("none"), HeldWeapon != nullptr && HeldWeapon->IsDrawing() ? 1 : 0,
		Body.X, Body.Y, Body.Z, GetMesh()->IsSimulatingPhysics() ? 1 : 0, Eye.X, Eye.Y, Eye.Z, HandInView.X, HandInView.Y, HandInView.Z, Along.X, Along.Y, Along.Z,
		DownBlend, FRotator::NormalizeAxis(GetActorRotation().Yaw), DownLegRoom, Grenades != nullptr ? Grenades->GetCount() : -1, Grenades != nullptr && Grenades->IsThrowing() ? 1 : 0);
	// Down: where the body's bones are in its frame (ahead toward the feet, right, up from the capsule's axis on the floor).
	if (DownBlend > 0.0f)
	{
		FString Bones;
		for (const TCHAR* Bone : {TEXT("pelvis"), TEXT("spine_03"), TEXT("head"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("upperarm_r"), TEXT("hand_r"), TEXT("calf_l"), TEXT("foot_l"), TEXT("calf_r"), TEXT("foot_r")})
		{
			const FVector In = DownBodyFrame(GetMesh()->GetBoneLocation(FName(Bone)));
			Bones += FString::Printf(TEXT(" %s (%.0f, %.0f, %.0f)"), Bone, In.X, In.Y, In.Z);
		}
		const FVector Camera = DownBodyFrame(Eye);
		const FVector Head = DownBodyFrame(Eyes->GetComponentLocation());
		UE_LOG(LogCheeseStrike, Display, TEXT("Down: camera (%.0f, %.0f, %.0f) eyes (%.0f, %.0f, %.0f) aim yaw %.1f bones%s"), Camera.X, Camera.Y, Camera.Z, Head.X, Head.Y, Head.Z, GetDownAimYaw(), *Bones);
	}
	if (HeldWeapon != nullptr)
	{
		// Where the hands and the rifle are, in each body, relative to the eyes: for checking the weapon sits in the hands.
		const FVector FirstHand = FirstPersonMesh->GetBoneLocation(TEXT("hand_r")) - Eye;
		const FVector FirstRifle = HeldWeapon->GetFirstPersonMesh()->GetComponentLocation() - Eye;
		const FVector Hand = GetMesh()->GetBoneLocation(TEXT("hand_r")) - Eye;
		const FVector Rifle = HeldWeapon->GetThirdPersonMesh()->GetComponentLocation() - Eye;
		// The rifle is modelled along its Y axis.
		const FVector FirstBarrel = HeldWeapon->GetFirstPersonMesh()->GetRightVector();
		const FVector Barrel = HeldWeapon->GetThirdPersonMesh()->GetRightVector();
		const FVector View = FirstPersonCameraComponent->GetForwardVector();
		const FVector Head = GetMesh()->GetBoneLocation(TEXT("head")) - GetMesh()->GetComponentLocation();
		const UCheeseStrikeAnimInstance* Animation = Cast<UCheeseStrikeAnimInstance>(GetMesh()->GetAnimInstance());
		// What the player's view is drawn from, which should be this character's camera.
		if (const APlayerController* Player = Cast<APlayerController>(GetController()))
		{
			if (Player->PlayerCameraManager != nullptr)
			{
				const FVector Seen = Player->PlayerCameraManager->GetCameraLocation();
				const FRotator Facing = Player->PlayerCameraManager->GetCameraRotation();
				UE_LOG(LogCheeseStrike, Display, TEXT("View: from (%.0f, %.0f, %.0f) facing (pitch %.0f, yaw %.0f) through %s"), Seen.X, Seen.Y, Seen.Z, Facing.Pitch, Facing.Yaw, *GetNameSafe(Player->GetViewTarget()));
			}
		}
		// Where the first-person rifle is in the body's right hand's space: how a body without weapon_r should hold it.
		const FTransform Wanted = HeldWeapon->GetFirstPersonMesh()->GetComponentTransform().GetRelativeTransform(GetMesh()->GetSocketTransform(TEXT("hand_r")));
		UE_LOG(LogCheeseStrike, Display, TEXT("Grip wanted on the body's hand_r: at (%.1f, %.1f, %.1f) turned (pitch %.1f, yaw %.1f, roll %.1f)"), Wanted.GetLocation().X, Wanted.GetLocation().Y, Wanted.GetLocation().Z, Wanted.Rotator().Pitch, Wanted.Rotator().Yaw, Wanted.Rotator().Roll);
		UE_LOG(LogCheeseStrike, Display, TEXT("Weapon in hand: %s"), *HeldWeapon->DescribeModel(Eye));
		// And where the left hand is on the rifle, in the rifle's own space: it should stay put whatever the aim.
		const FVector LeftOnRifle = LeftHandOnRifle();
		UE_LOG(LogCheeseStrike, Display, TEXT("Hands: first-person hand (%.0f, %.0f, %.0f) rifle (%.0f, %.0f, %.0f) barrel (%.2f, %.2f, %.2f) left hand on the rifle at (%.1f, %.1f, %.1f); body hand (%.0f, %.0f, %.0f) rifle (%.0f, %.0f, %.0f) barrel (%.2f, %.2f, %.2f) head (%.0f, %.0f, %.0f) over the feet; view (%.2f, %.2f, %.2f) aim pitch %.0f"),
			FirstHand.X, FirstHand.Y, FirstHand.Z, FirstRifle.X, FirstRifle.Y, FirstRifle.Z, FirstBarrel.X, FirstBarrel.Y, FirstBarrel.Z, LeftOnRifle.X, LeftOnRifle.Y, LeftOnRifle.Z,
			Hand.X, Hand.Y, Hand.Z, Rifle.X, Rifle.Y, Rifle.Z, Barrel.X, Barrel.Y, Barrel.Z, Head.X, Head.Y, Head.Z, View.X, View.Y, View.Z, Animation != nullptr ? Animation->AimPitch : 0.0f);
	}
	if (!IsAlive())
	{
		const FVector Pelvis = GetMesh()->GetBoneLocation(TEXT("pelvis"));
		const FVector Head = GetMesh()->GetBoneLocation(TEXT("head"));
		UE_LOG(LogCheeseStrike, Display, TEXT("Dead: pelvis (%.0f, %.0f, %.0f) head (%.0f, %.0f, %.0f) visible %d"), Pelvis.X, Pelvis.Y, Pelvis.Z, Head.X, Head.Y, Head.Z, GetMesh()->IsVisible() ? 1 : 0);
	}
}

void ACheeseStrikeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const UCheeseStrikeInputSet* Input = UCheeseStrikeInputSet::Get(this);
	if (EnhancedInputComponent == nullptr || Input == nullptr)
	{
		UE_LOG(LogCheeseStrike, Error, TEXT("'%s' has no Enhanced Input component or no input set to bind to."), *GetNameSafe(this));
		return;
	}
	EnhancedInputComponent->BindAction(Input->Move, ETriggerEvent::Triggered, this, &ACheeseStrikeCharacter::MoveInput);
	EnhancedInputComponent->BindAction(Input->Move, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::MoveEnded);
	EnhancedInputComponent->BindAction(Input->MouseLook, ETriggerEvent::Triggered, this, &ACheeseStrikeCharacter::MouseLookInput);
	EnhancedInputComponent->BindAction(Input->StickLook, ETriggerEvent::Triggered, this, &ACheeseStrikeCharacter::StickLookInput);
	EnhancedInputComponent->BindAction(Input->Jump, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::DoJumpStart);
	EnhancedInputComponent->BindAction(Input->Jump, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::DoJumpEnd);
	EnhancedInputComponent->BindAction(Input->Sprint, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::SprintStarted);
	EnhancedInputComponent->BindAction(Input->Sprint, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::SprintEnded);
	EnhancedInputComponent->BindAction(Input->Crouch, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::CrouchStarted);
	EnhancedInputComponent->BindAction(Input->Crouch, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::CrouchEnded);
	EnhancedInputComponent->BindAction(Input->SprintToggle, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::SprintToggled);
	EnhancedInputComponent->BindAction(Input->CrouchToggle, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::CrouchToggled);
	EnhancedInputComponent->BindAction(Input->Fire, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::FireStarted);
	EnhancedInputComponent->BindAction(Input->Fire, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::FireEnded);
	EnhancedInputComponent->BindAction(Input->Aim, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::AimStarted);
	EnhancedInputComponent->BindAction(Input->Aim, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::AimEnded);
	EnhancedInputComponent->BindAction(Input->Reload, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::ReloadStarted);
	EnhancedInputComponent->BindAction(Input->DrawPrimary, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::PrimaryStarted);
	EnhancedInputComponent->BindAction(Input->DrawSidearm, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::SidearmStarted);
	EnhancedInputComponent->BindAction(Input->DrawKnife, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::KnifeStarted);
	EnhancedInputComponent->BindAction(Input->NextWeapon, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::NextWeaponStarted);
	EnhancedInputComponent->BindAction(Input->PreviousWeapon, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::PreviousWeaponStarted);
	EnhancedInputComponent->BindAction(Input->Streak, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::StreakStarted);
	EnhancedInputComponent->BindAction(Input->Grenade, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::GrenadeStarted);
	EnhancedInputComponent->BindAction(Input->Grenade, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::GrenadeEnded);
	EnhancedInputComponent->BindAction(Input->Interact, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::InteractStarted);
	EnhancedInputComponent->BindAction(Input->Interact, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::InteractEnded);
}

void ACheeseStrikeCharacter::InteractStarted()
{
	if (!TakesInput())
	{
		return;
	}
	// Someone down in reach comes first; else a weapon in reach (on a stand or the floor), swapped for the primary.
	CheeseStrikeModes::EInteract Kind = CheeseStrikeModes::EInteract::None;
	if ((LastStand == nullptr || LastStand->InteractTarget(Kind) == nullptr) && Inventory != nullptr && Inventory->PressForPickup())
	{
		return;
	}
	if (LastStand != nullptr)
	{
		LastStand->SetInteracting(true);
	}
}

void ACheeseStrikeCharacter::InteractEnded()
{
	if (LastStand != nullptr)
	{
		LastStand->SetInteracting(false);
	}
}

void ACheeseStrikeCharacter::GrenadeStarted()
{
	// Marking where a Cheese Strike goes, the hands are busy with the marker. Held, the arc shows; let go, it is thrown.
	const ACheeseStrikePlayerState* Player = GetPlayerState<ACheeseStrikePlayerState>();
	if (Grenades != nullptr && TakesInput() && (Player == nullptr || !Player->IsMarkingStrike()))
	{
		Grenades->BeginHold();
	}
}

void ACheeseStrikeCharacter::GrenadeEnded()
{
	if (Grenades != nullptr && Grenades->IsHolding())
	{
		MoveEnded();
		Grenades->EndHold(GetControlRotation().Vector());
	}
}

void ACheeseStrikeCharacter::DoThrowGrenade()
{
	if (Grenades != nullptr && TakesInput())
	{
		MoveEnded();
		Grenades->Throw(GetControlRotation().Vector());
	}
}

void ACheeseStrikeCharacter::StreakStarted()
{
	if (ACheeseStrikePlayerState* Player = GetPlayerState<ACheeseStrikePlayerState>(); Player != nullptr && TakesInput())
	{
		Player->StreakPressed();
	}
}

void ACheeseStrikeCharacter::MoveInput(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	DoMove(Axis.X, Axis.Y);
}

bool ACheeseStrikeCharacter::MoveStrikeMark(float Yaw, float Pitch)
{
	// Choosing where a Cheese Strike goes, the look moves its mark on the strike map rather than the view, as fast as the
	// player's options would turn it, whichever way they invert it.
	ACheeseStrikePlayerState* Mine = GetPlayerState<ACheeseStrikePlayerState>();
	if (Mine == nullptr || !Mine->IsMarkingStrike())
	{
		return false;
	}
	const ACheeseStrikePlayerController* Player = Cast<ACheeseStrikePlayerController>(GetController());
	if (TakesInput() && (Player == nullptr || !Player->IsPaused()))
	{
		const FVector2D Scale = Player != nullptr ? Player->LookScale() : FVector2D(1.0, 1.0);
		Mine->MoveStrikeTarget(FVector2D(Yaw * FMath::Abs(Scale.X), Pitch * FMath::Abs(Scale.Y)));
	}
	return true;
}

void ACheeseStrikeCharacter::MouseLookInput(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (MoveStrikeMark(Axis.X * CheeseStrikeInput::MouseDegreesPerCount, Axis.Y * CheeseStrikeInput::MouseDegreesPerCount))
	{
		return;
	}
	DoAim(Axis.X * CheeseStrikeInput::MouseDegreesPerCount, Axis.Y * CheeseStrikeInput::MouseDegreesPerCount);
}

void ACheeseStrikeCharacter::StickLookInput(const FInputActionValue& Value)
{
	// A stick turns at a rate, however many frames a second are drawn, on a curve (CheeseStrikeInput::StickTurn).
	const FVector2D Axis = Value.Get<FVector2D>();
	const float Seconds = GetWorld() != nullptr ? GetWorld()->GetDeltaSeconds() : 0.0f;
	const FVector2D Turn = CheeseStrikeInput::StickTurn(Axis, Seconds, StickEdgeSeconds);
	if (MoveStrikeMark(Turn.X, -Turn.Y))
	{
		AimAssist = CheeseStrikeAimAssist::FAssist();
		return;
	}
	// Slower over an enemy, and drawn a little toward one while turning: the stick's alone, never the mouse's or a bot's.
	const ACheeseStrikePlayerController* Player = Cast<ACheeseStrikePlayerController>(GetController());
	AimAssist = TakesInput() && (Player == nullptr || !Player->IsPaused()) ? CheeseStrikeAimAssist::ForCharacter(this, !Turn.IsNearlyZero(), Seconds) : CheeseStrikeAimAssist::FAssist();
	// A stick's up is the other way to the mouse's, which DoAim takes: unturned, pushing up looked down.
	DoAim(Turn.X * AimAssist.TurnScale, -Turn.Y * AimAssist.TurnScale);
	if (AimAssist.PullYaw != 0.0f || AimAssist.PullPitch != 0.0f)
	{
		AddControllerYawInput(AimAssist.PullYaw);
		AddControllerPitchInput(AimAssist.PullPitch);
	}
}

void ACheeseStrikeCharacter::DoAim(float Yaw, float Pitch)
{
	const ACheeseStrikePlayerController* Player = Cast<ACheeseStrikePlayerController>(GetController());
	if (GetController() && (Player == nullptr || !Player->IsPaused()))
	{
		// As fast as the player's options say, and up and down whichever way they say.
		const FVector2D Scale = Player != nullptr ? Player->LookScale() : FVector2D(1.0, 1.0);
		AddControllerYawInput(Yaw * Scale.X);
		AddControllerPitchInput(Pitch * Scale.Y);
	}
}

void ACheeseStrikeCharacter::DoMove(float Right, float Forward)
{
	if (GetController() && TakesInput())
	{
		// Down, it scoots toward its feet or its head, and the sideways keys turn it round slowly (the view, which the body follows).
		if (IsDowned())
		{
			AddMovementInput(GetActorForwardVector(), Forward);
			AddControllerYawInput(Right * CheeseStrikeDowned::TurnRate * (GetWorld() != nullptr ? GetWorld()->GetDeltaSeconds() : 0.0f));
			return;
		}
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ACheeseStrikeCharacter::DoJumpStart()
{
	if (TakesInput() && !IsDowned())
	{
		Jump();
	}
}

void ACheeseStrikeCharacter::DoJumpEnd()
{
	StopJumping();
}

void ACheeseStrikeCharacter::DoSprint(bool bHeld)
{
	if (UCheeseStrikeMovementComponent* Movement = GetCheeseStrikeMovement())
	{
		Movement->SetWantsToSprint(bHeld && TakesInput() && !IsDowned());
	}
}

void ACheeseStrikeCharacter::DoCrouch(bool bHeld)
{
	if (bHeld && TakesInput())
	{
		Crouch();
	}
	else if (!IsDowned())
	{
		// Down, it stays crouched (UCheeseStrikeLastStand).
		UnCrouch();
	}
}

void ACheeseStrikeCharacter::DoFire(bool bHeld)
{
	ACheeseStrikeWeapon* HeldWeapon = GetWeapon();
	if (HeldWeapon == nullptr)
	{
		return;
	}
	// The hands busy with a throw, the trigger waits for it.
	if (bHeld && TakesInput() && (Grenades == nullptr || !Grenades->IsBusy()))
	{
		HeldWeapon->StartFiring();
	}
	else
	{
		HeldWeapon->StopFiring();
	}
}

void ACheeseStrikeCharacter::DoAimDownSights(bool bHeld)
{
	ACheeseStrikeWeapon* HeldWeapon = GetWeapon();
	if (HeldWeapon == nullptr)
	{
		return;
	}
	// Down, from the hip only.
	if (bHeld && TakesInput() && !IsDowned() && (Grenades == nullptr || !Grenades->IsBusy()))
	{
		HeldWeapon->StartAiming();
	}
	else
	{
		HeldWeapon->StopAiming();
	}
}

void ACheeseStrikeCharacter::DoReload()
{
	if (GetWeapon() != nullptr && TakesInput())
	{
		GetWeapon()->StartReload();
	}
}

void ACheeseStrikeCharacter::DoDrawWeapon(EWeaponSlot Slot)
{
	if (TakesInput())
	{
		// The sights stay up through a change while they are held.
		const bool bAiming = GetWeapon() != nullptr && GetWeapon()->IsAimed();
		Inventory->Draw(Slot);
		if (bAiming && GetWeapon() != nullptr)
		{
			GetWeapon()->StartAiming();
		}
	}
}

void ACheeseStrikeCharacter::DoNextWeapon(int32 Step)
{
	if (TakesInput())
	{
		const bool bAiming = GetWeapon() != nullptr && GetWeapon()->IsAimed();
		Inventory->DrawNext(Step);
		if (bAiming && GetWeapon() != nullptr)
		{
			GetWeapon()->StartAiming();
		}
	}
}

void ACheeseStrikeCharacter::PrimaryStarted()
{
	DoDrawWeapon(EWeaponSlot::Primary);
}

void ACheeseStrikeCharacter::SidearmStarted()
{
	DoDrawWeapon(EWeaponSlot::Sidearm);
}

void ACheeseStrikeCharacter::KnifeStarted()
{
	DoDrawWeapon(EWeaponSlot::Knife);
}

void ACheeseStrikeCharacter::NextWeaponStarted()
{
	DoNextWeapon(1);
}

void ACheeseStrikeCharacter::PreviousWeaponStarted()
{
	DoNextWeapon(-1);
}

void ACheeseStrikeCharacter::FireStarted()
{
	MoveEnded();
	// Marking where a Cheese Strike goes, the shot marks it instead.
	if (ACheeseStrikePlayerState* Player = GetPlayerState<ACheeseStrikePlayerState>(); Player != nullptr && Player->IsMarkingStrike() && TakesInput())
	{
		Player->MarkStrike();
		return;
	}
	DoFire(true);
}

void ACheeseStrikeCharacter::FireEnded()
{
	DoFire(false);
}

void ACheeseStrikeCharacter::AimStarted()
{
	// Aiming ends a gamepad's sprint, as firing does.
	MoveEnded();
	DoAimDownSights(true);
}

void ACheeseStrikeCharacter::AimEnded()
{
	DoAimDownSights(false);
}

void ACheeseStrikeCharacter::ReloadStarted()
{
	DoReload();
}

void ACheeseStrikeCharacter::ResumeSprintKey()
{
	const bool bCan = IsLocallyControlled() && IsPlayerControlled() && TakesInput();
	if (bCan && !bCouldAct)
	{
		const APlayerController* Player = Cast<APlayerController>(GetController());
		const ULocalPlayer* Local = Player != nullptr ? Player->GetLocalPlayer() : nullptr;
		const UEnhancedInputLocalPlayerSubsystem* Subsystem = Local != nullptr ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		const UEnhancedPlayerInput* PlayerInput = Subsystem != nullptr ? Subsystem->GetPlayerInput() : nullptr;
		const UCheeseStrikeInputSet* Input = UCheeseStrikeInputSet::Get(this);
		if (PlayerInput != nullptr && Input != nullptr && Input->Sprint != nullptr && PlayerInput->GetActionValue(Input->Sprint).Get<bool>())
		{
			DoSprint(true);
		}
	}
	bCouldAct = bCan;
}

void ACheeseStrikeCharacter::SprintStarted()
{
	DoSprint(true);
}

void ACheeseStrikeCharacter::SprintToggled()
{
	bSprintLatched = !bSprintLatched && TakesInput();
	DoSprint(bSprintLatched);
}

void ACheeseStrikeCharacter::MoveEnded()
{
	// A gamepad's sprint goes on until the stick is let go.
	if (bSprintLatched)
	{
		bSprintLatched = false;
		DoSprint(false);
	}
}

void ACheeseStrikeCharacter::CrouchToggled()
{
	// Crouches, or stands; at a sprint, a crouch slides.
	DoCrouch(!bIsCrouched);
}

void ACheeseStrikeCharacter::SprintEnded()
{
	DoSprint(false);
}

void ACheeseStrikeCharacter::CrouchStarted()
{
	DoCrouch(true);
}

void ACheeseStrikeCharacter::CrouchEnded()
{
	DoCrouch(false);
}
