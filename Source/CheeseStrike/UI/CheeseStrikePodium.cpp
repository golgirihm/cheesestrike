// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePodium.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Animation/CheeseStrikeEmote.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Rules/CheeseStrikePodiums.h"
#include "Weapons/CheeseStrikeArsenal.h"

// High over every level: out of its height fog, its playing space and its kill height. The stage's own walls keep the
// level's sun and sky off it.
const FVector ACheeseStrikePodium::StageAt(0.0f, 0.0f, 60000.0f);

namespace
{
	/** Across the stage between the places' middles, and each pedestal's wheels across (the first's a little wider). */
	constexpr float PodiumSpacing = 168.0f;
	constexpr float PodiumWheelWidth = 150.0f;
	constexpr float PodiumFirstWheelWidth = 164.0f;
	/** The camera: how far in front of the pedestals, how high, and the point it looks at. */
	const FVector PodiumCameraAt(870.0f, 0.0f, 190.0f);
	const FVector PodiumLookAt(0.0f, 0.0f, 128.0f);
	/** Its field of view from top to bottom; and across, at least what holds the stage's width at the pedestals. */
	constexpr float PodiumTallView = 31.0f;
	constexpr float PodiumWideHalf = 330.0f;
	/** How tall the heroes stand, near enough, for the names over them. */
	constexpr float PodiumHeroHeight = 190.0f;
	/** The room the stage is in: from its back wall to behind the camera, side to side, and up to its ceiling. */
	constexpr float PodiumRoomBack = -420.0f;
	constexpr float PodiumRoomFront = 1200.0f;
	constexpr float PodiumRoomHalfWidth = 900.0f;
	constexpr float PodiumRoomHeight = 760.0f;
	constexpr float PodiumWall = 40.0f;
	/** A hero's emote starts a little after the one before, so that they don't move as one. */
	constexpr float PodiumStagger = 0.35f;

	const TCHAR* const PodiumCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const PodiumWheelPath = TEXT("/Game/CheeseStrike/Meshes/SM_CheeseWheel.SM_CheeseWheel");
	const TCHAR* const PodiumCheesePath = TEXT("/Game/CheeseStrike/Materials/M_Cheese.M_Cheese");
	const TCHAR* const PodiumWaxPath = TEXT("/Game/CheeseStrike/Materials/M_CheeseWax.M_CheeseWax");
	const TCHAR* const PodiumFloorPath = TEXT("/Game/CheeseStrike/Materials/MI_OldPlanks.MI_OldPlanks");
	const TCHAR* const PodiumWallPath = TEXT("/Game/CheeseStrike/Materials/MI_OldBrick.MI_OldBrick");
	const TCHAR* const PodiumCeilingPath = TEXT("/Game/CheeseStrike/Materials/MI_Painted_Dark.MI_Painted_Dark");
}

ACheeseStrikePodium::ACheeseStrikePodium()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Stage"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PodiumCube(PodiumCubePath);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PodiumWheel(PodiumWheelPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PodiumCheese(PodiumCheesePath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PodiumWax(PodiumWaxPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PodiumPlanks(PodiumFloorPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PodiumBrick(PodiumWallPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PodiumDark(PodiumCeilingPath);

	// The room: a floor of old planks, walls of old brick, a dark ceiling, closed all round.
	int32 Pieces = 0;
	const auto Block = [this, &Pieces](const FVector& Low, const FVector& High, UMaterialInterface* Look)
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Room%d"), Pieces++));
		Piece->SetupAttachment(RootComponent);
		Piece->SetStaticMesh(PodiumCube.Object);
		// The engine's cube is a metre each way about its middle.
		Piece->SetRelativeLocation((Low + High) / 2.0f);
		Piece->SetRelativeScale3D((High - Low) / 100.0f);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Look != nullptr)
		{
			Piece->SetMaterial(0, Look);
		}
		return Piece;
	};
	const float B = PodiumRoomBack, F = PodiumRoomFront, W = PodiumRoomHalfWidth, H = PodiumRoomHeight, T = PodiumWall;
	Block(FVector(B - T, -W - T, -T), FVector(F + T, W + T, 0.0f), PodiumPlanks.Object);
	Block(FVector(B - T, -W - T, 0.0f), FVector(B, W + T, H), PodiumBrick.Object);
	Block(FVector(F, -W - T, 0.0f), FVector(F + T, W + T, H), PodiumBrick.Object);
	Block(FVector(B, -W - T, 0.0f), FVector(F, -W, H), PodiumBrick.Object);
	Block(FVector(B, W, 0.0f), FVector(F, W + T, H), PodiumBrick.Object);
	Block(FVector(B - T, -W - T, H), FVector(F + T, W + T, H + T), PodiumDark.Object);

	// Stacks of wheels in the cellar's back corners, half in the dark, so that it reads as a cheese store.
	int32 Stacked = 0;
	const auto Store = [this, &Stacked](const FVector& Foot, float Width, float Height, int32 Count, bool bWaxed)
	{
		for (int32 Layer = 0; Layer < Count; ++Layer)
		{
			UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Store%d"), Stacked++));
			Piece->SetupAttachment(RootComponent);
			Piece->SetStaticMesh(PodiumWheel.Object);
			Piece->SetRelativeLocation(Foot + FVector(0.0f, 0.0f, Height * (Layer + 0.5f)));
			Piece->SetRelativeRotation(FRotator(0.0f, Layer * 37.0f, 0.0f));
			Piece->SetRelativeScale3D(FVector(Width / 100.0f, Width / 100.0f, Height / 100.0f));
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Piece->SetMaterial(0, bWaxed ? PodiumWax.Object : PodiumCheese.Object);
		}
	};
	Store(FVector(-300.0f, 620.0f, 0.0f), 120.0f, 46.0f, 5, true);
	Store(FVector(-260.0f, 470.0f, 0.0f), 100.0f, 40.0f, 3, false);
	Store(FVector(-300.0f, -620.0f, 0.0f), 120.0f, 46.0f, 4, false);
	Store(FVector(-250.0f, -480.0f, 0.0f), 100.0f, 40.0f, 2, true);

	// The pedestals: stacks of wheels of cheese, the first's three high, the second's two, the third's one.
	for (int32 Place = 0; Place < CheeseStrikePodiums::Places; ++Place)
	{
		const float Width = Place == 0 ? PodiumFirstWheelWidth : PodiumWheelWidth;
		const float Height = CheeseStrikePodiums::PedestalHeight(Place) / CheeseStrikePodiums::Wheels(Place);
		const FVector Foot = SlotAt(Place) * FVector(1.0f, 1.0f, 0.0f);
		for (int32 Layer = 0; Layer < CheeseStrikePodiums::Wheels(Place); ++Layer)
		{
			UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Pedestal%d_%d"), Place, Layer));
			Piece->SetupAttachment(RootComponent);
			Piece->SetStaticMesh(PodiumWheel.Object);
			Piece->SetRelativeLocation(Foot + FVector(0.0f, 0.0f, Height * (Layer + 0.5f)));
			Piece->SetRelativeRotation(FRotator(0.0f, Layer * 53.0f + Place * 20.0f, 0.0f));
			Piece->SetRelativeScale3D(FVector(Width / 100.0f, Width / 100.0f, Height / 100.0f));
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Piece->SetMaterial(0, PodiumCheese.Object);
			Piece->ComponentTags.Add(*FString::Printf(TEXT("Place%d"), Place));
			Pedestals.Add(Piece);
		}
	}

	// The heroes, one on each pedestal's top, facing the camera; put on by Stage.
	for (const CheeseStrikeBodies::FBody& Each : CheeseStrikeBodies::All())
	{
		ConstructorHelpers::FObjectFinder<USkeletalMesh> Model(Each.Mesh);
		BodyMeshes.Add(Model.Object);
	}
	for (int32 Place = 0; Place < CheeseStrikePodiums::Places; ++Place)
	{
		USkeletalMeshComponent* Hero = CreateDefaultSubobject<USkeletalMeshComponent>(*FString::Printf(TEXT("Hero%d"), Place));
		Hero->SetAllowClothActors(false);
		Hero->SetupAttachment(RootComponent);
		Hero->SetRelativeLocation(SlotAt(Place));
		// The heroes are modelled facing along +Y; the stage faces +X, the camera's way.
		Hero->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		Hero->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hero->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Hero->SetVisibility(false);
		Heroes.Add(Hero);

		// A spotlight from high in front, warm, on each.
		USpotLightComponent* Spot = CreateDefaultSubobject<USpotLightComponent>(*FString::Printf(TEXT("Spot%d"), Place));
		Spot->SetupAttachment(RootComponent);
		const FVector Target = SlotAt(Place) + FVector(0.0f, 0.0f, 95.0f);
		const FVector From = FVector(430.0f, SlotAt(Place).Y * 0.6f, 640.0f);
		Spot->SetRelativeLocation(From);
		Spot->SetRelativeRotation((Target - From).Rotation());
		Spot->SetIntensityUnits(ELightUnits::Candelas);
		Spot->SetIntensity(Place == 0 ? 2600.0f : 2000.0f);
		Spot->SetAttenuationRadius(1400.0f);
		Spot->SetInnerConeAngle(13.0f);
		Spot->SetOuterConeAngle(21.0f);
		Spot->SetLightColor(FLinearColor(1.0f, 0.88f, 0.7f));
		Spot->SetSourceRadius(12.0f);
		Spots.Add(Spot);
	}
	// From behind and above, cool, along the heroes' edges; a soft fill from the camera's side; a glow on the back wall.
	USpotLightComponent* Rim = CreateDefaultSubobject<USpotLightComponent>(TEXT("Rim"));
	Rim->SetupAttachment(RootComponent);
	Rim->SetRelativeLocation(FVector(-330.0f, 0.0f, 620.0f));
	Rim->SetRelativeRotation((FVector(60.0f, 0.0f, 120.0f) - FVector(-330.0f, 0.0f, 620.0f)).Rotation());
	Rim->SetIntensityUnits(ELightUnits::Candelas);
	Rim->SetIntensity(1400.0f);
	Rim->SetAttenuationRadius(1400.0f);
	Rim->SetInnerConeAngle(30.0f);
	Rim->SetOuterConeAngle(46.0f);
	Rim->SetLightColor(FLinearColor(0.7f, 0.82f, 1.0f));
	UPointLightComponent* Fill = CreateDefaultSubobject<UPointLightComponent>(TEXT("Fill"));
	Fill->SetupAttachment(RootComponent);
	Fill->SetRelativeLocation(FVector(700.0f, 0.0f, 260.0f));
	Fill->SetIntensityUnits(ELightUnits::Candelas);
	Fill->SetIntensity(90.0f);
	Fill->SetAttenuationRadius(1600.0f);
	Fill->SetCastShadows(false);
	Fill->SetLightColor(FLinearColor(1.0f, 0.9f, 0.78f));
	UPointLightComponent* Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(PodiumRoomBack + 120.0f, 0.0f, 420.0f));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(60.0f);
	Glow->SetAttenuationRadius(900.0f);
	Glow->SetCastShadows(false);
	Glow->SetLightColor(FLinearColor(1.0f, 0.62f, 0.3f));

	// The camera: a little over head height in front, looking down at the pedestals, its exposure its own (the level's
	// adapts to its own light, and would take seconds to come to this room's).
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetRelativeLocation(PodiumCameraAt);
	Camera->SetRelativeRotation((PodiumLookAt - PodiumCameraAt).Rotation());
	Camera->SetFieldOfView(FieldOfViewFor(16.0f / 9.0f));
	Camera->PostProcessBlendWeight = 1.0f;
	FPostProcessSettings& Look = Camera->PostProcessSettings;
	Look.bOverride_AutoExposureMethod = true;
	Look.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	Look.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Look.AutoExposureApplyPhysicalCameraExposure = false;
	Look.bOverride_AutoExposureBias = true;
	Look.AutoExposureBias = -1.5f;
	Look.bOverride_VignetteIntensity = true;
	Look.VignetteIntensity = 0.55f;
	Look.bOverride_BloomIntensity = true;
	Look.BloomIntensity = 0.45f;
	Look.bOverride_MotionBlurAmount = true;
	Look.MotionBlurAmount = 0.0f;

	// The places' numbers, in cheese lettering like the countdown's but straight on (T_Podium_*, tools/art/countdown_glyphs.py).
	for (int32 Number = 1; Number <= CheeseStrikePodiums::Places; ++Number)
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> Lettering(*FString::Printf(TEXT("/Game/CheeseStrike/UI/Countdown/T_Podium_%d.T_Podium_%d"), Number, Number));
		Glyphs.Add(Lettering.Object);
	}
	// Every hero's emotes, loaded with the class so that they are cooked with it.
	for (int32 Body = 0; Body < CheeseStrikeBodies::All().Num(); ++Body)
	{
		for (int32 Place = 0; Place < CheeseStrikePodiums::Places; ++Place)
		{
			const TCHAR* Path = CheeseStrikePodiums::Emote(Body, Place);
			UAnimSequenceBase* Found = nullptr;
			if (Path != nullptr)
			{
				ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Emote(Path);
				Found = Emote.Object;
			}
			Emotes.Add(Found);
		}
	}
}

float ACheeseStrikePodium::FieldOfViewFor(float Aspect)
{
	const float Across = 2.0f * FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(PodiumTallView / 2.0f)) * Aspect));
	const float Fits = 2.0f * FMath::RadiansToDegrees(FMath::Atan(PodiumWideHalf / (PodiumCameraAt - PodiumLookAt).Size()));
	return FMath::Max(Across, Fits);
}

FVector ACheeseStrikePodium::SlotAt(int32 Place) const
{
	// From the camera (looking along -X) its left is +Y: the second there, the first in the middle, the third on the right.
	const int32 Slot = CheeseStrikePodiums::SlotOf(Place);
	return FVector(Place == 0 ? -20.0f : 0.0f, (1 - Slot) * PodiumSpacing, CheeseStrikePodiums::PedestalHeight(Place));
}

void ACheeseStrikePodium::BeginPlay()
{
	Super::BeginPlay();
	// The heroes' own guns show: their emotes are made with them in hand.
}

TArray<FPodiumPlace> ACheeseStrikePodium::PlacesFor(const ACheeseStrikeGameState* Match, const APlayerState* Mine)
{
	TArray<FPodiumPlace> Out;
	if (Match == nullptr)
	{
		return Out;
	}
	const CheeseStrikeModes::EWinBy WinBy = Match->GetModeInfo().WinBy;
	const bool bTeams = Match->HasTeams();
	TArray<CheeseStrikePodiums::FContender> Contenders;
	TArray<const ACheeseStrikePlayerState*> Players;
	for (const APlayerState* Each : Match->PlayerArray)
	{
		if (const ACheeseStrikePlayerState* Player = Cast<ACheeseStrikePlayerState>(Each))
		{
			CheeseStrikePodiums::FContender& Contender = Contenders.AddDefaulted_GetRef();
			Contender.Standing = Player->Standing();
			Contender.Team = Match->TeamOf(Player);
			Contender.GunLevel = Player->GetGunLevel();
			Contender.HillPoints = Player->GetHillPoints();
			Contender.HillSeconds = Player->GetHillSeconds();
			Players.Add(Player);
		}
	}
	const TArray<int32> Chosen = CheeseStrikePodiums::Choose(Contenders, WinBy, bTeams, Match->GetWinningTeam());
	for (int32 Place = 0; Place < Chosen.Num(); ++Place)
	{
		const ACheeseStrikePlayerState* Player = Players[Chosen[Place]];
		FPodiumPlace& Placed = Out.AddDefaulted_GetRef();
		Placed.Name = Player->GetPlayerName();
		Placed.Place = Place;
		Placed.Body = FMath::Max(0, ACheeseStrikeCharacter::BodyOf(Match, Player, Placed.Team));
		Placed.Score = CheeseStrikePodiums::ScoreWords(CheeseStrikePodiums::ScoreOf(Contenders[Chosen[Place]], WinBy, bTeams), WinBy, bTeams, CheeseStrikeArsenal::GunGameLadder().Num());
		Placed.bBot = Player->IsABot();
		Placed.bMine = Player == Mine;
	}
	return Out;
}

void ACheeseStrikePodium::Stage(const TArray<FPodiumPlace>& Who)
{
	Places = Who;
	for (int32 Place = 0; Place < CheeseStrikePodiums::Places; ++Place)
	{
		const FPodiumPlace* Placed = Who.IsValidIndex(Place) ? &Who[Place] : nullptr;
		USkeletalMeshComponent* Hero = Heroes[Place];
		// An empty place has neither its hero nor its pedestal nor its light.
		for (UStaticMeshComponent* Piece : Pedestals)
		{
			if (Piece->ComponentHasTag(*FString::Printf(TEXT("Place%d"), Place)))
			{
				Piece->SetVisibility(Placed != nullptr);
			}
		}
		Spots[Place]->SetVisibility(Placed != nullptr);
		if (Placed == nullptr || !BodyMeshes.IsValidIndex(Placed->Body) || BodyMeshes[Placed->Body] == nullptr)
		{
			Hero->SetVisibility(false);
			continue;
		}
		const CheeseStrikeBodies::FBody& Look = CheeseStrikeBodies::All()[Placed->Body];
		Hero->SetSkeletalMeshAsset(BodyMeshes[Placed->Body]);
		Hero->EmptyOverrideMaterials();
		// Dressed as their character was: their side's colours, or the hero's own without sides.
		for (int32 Slot = 0; Placed->Team >= 0 && Slot < Look.NumTeamSlots; ++Slot)
		{
			if (UMaterialInterface* Dress = LoadObject<UMaterialInterface>(nullptr, *CheeseStrikeBodies::TeamLook(Placed->Body, Placed->Team, Look.TeamSlots[Slot])))
			{
				Hero->SetMaterialByName(FName(Look.TeamSlots[Slot]), Dress);
			}
		}
		Hero->SetVisibility(true);
		UAnimSequenceBase* Emote = Emotes.IsValidIndex(Placed->Body * CheeseStrikePodiums::Places + Place) ? Emotes[Placed->Body * CheeseStrikePodiums::Places + Place].Get() : nullptr;
		if (Emote != nullptr)
		{
			// Looped without a seam: the end of each pass blended into the start of the next (UCheeseStrikeEmoteAnimInstance).
			if (Hero->GetAnimInstance() == nullptr || !Hero->GetAnimInstance()->IsA<UCheeseStrikeEmoteAnimInstance>())
			{
				Hero->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				Hero->SetAnimInstanceClass(UCheeseStrikeEmoteAnimInstance::StaticClass());
			}
			if (UCheeseStrikeEmoteAnimInstance* Playing = Cast<UCheeseStrikeEmoteAnimInstance>(Hero->GetAnimInstance()))
			{
				Playing->SetEmote(Emote, FMath::Fmod(Place * PodiumStagger, FMath::Max(0.1f, Emote->GetPlayLength())));
			}
		}
	}
}

bool ACheeseStrikePodium::IsEmoting(int32 Place) const
{
	const USkeletalMeshComponent* Hero = Heroes.IsValidIndex(Place) ? Heroes[Place].Get() : nullptr;
	if (Hero == nullptr || !Hero->IsVisible() || !Places.IsValidIndex(Place))
	{
		return false;
	}
	const UCheeseStrikeEmoteAnimInstance* Playing = Cast<UCheeseStrikeEmoteAnimInstance>(Hero->GetAnimInstance());
	const int32 Index = Places[Place].Body * CheeseStrikePodiums::Places + Place;
	return Playing != nullptr && Playing->IsEmoting() && Emotes.IsValidIndex(Index) && Emotes[Index] != nullptr && Playing->GetEmote() == Emotes[Index];
}

FVector ACheeseStrikePodium::PedestalFront(int32 Place) const
{
	const float Width = Place == 0 ? PodiumFirstWheelWidth : PodiumWheelWidth;
	return GetActorTransform().TransformPosition(SlotAt(Place) * FVector(1.0f, 1.0f, 0.0f) + FVector(Width / 2.0f, 0.0f, 0.0f));
}

FVector ACheeseStrikePodium::HeadOf(int32 Place) const
{
	return GetActorTransform().TransformPosition(SlotAt(Place) + FVector(0.0f, 0.0f, PodiumHeroHeight));
}

FVector ACheeseStrikePodium::BoneOf(int32 Place, FName Bone) const
{
	const USkeletalMeshComponent* Hero = Heroes.IsValidIndex(Place) ? Heroes[Place].Get() : nullptr;
	const int32 Index = Hero != nullptr ? Hero->GetBoneIndex(Bone) : INDEX_NONE;
	return Index != INDEX_NONE ? Hero->GetBoneTransform(Index, FTransform::Identity).GetLocation() : FVector::ZeroVector;
}

UTexture2D* ACheeseStrikePodium::Glyph(int32 Number) const
{
	return Glyphs.IsValidIndex(Number - 1) ? Glyphs[Number - 1].Get() : nullptr;
}

void ACheeseStrikePodium::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Framed for the view's shape: a wider one sees more of the cellar, a narrower one still the whole podium.
	const APlayerController* Viewer = Cast<APlayerController>(GetOwner());
	const ULocalPlayer* Local = Viewer != nullptr ? Viewer->GetLocalPlayer() : nullptr;
	FVector2D Size(16.0, 9.0);
	if (Local != nullptr && Local->ViewportClient != nullptr)
	{
		Local->ViewportClient->GetViewportSize(Size);
	}
	if (Size.X > 0.0 && Size.Y > 0.0)
	{
		Camera->SetFieldOfView(FieldOfViewFor(static_cast<float>(Size.X / Size.Y)));
	}
}
