// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHill.h"
#include "Rules/CheeseStrikeCredit.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Map/CheeseStrikePlace.h"
#include "Rules/CheeseStrikeModes.h"

namespace
{
	/** How high the column of light rises (cm), how often the server looks at who is in the hill (s), and its light's strength (candelas). */
	constexpr float HillColumnHeight = 420.0f;
	constexpr float HillScoreStep = 0.1f;
	constexpr float HillLightStrength = 30.0f;

	/** A place's name for announcements: "the Press", or "the next spot" where there is none. */
	FString HillPlaceName(const UWorld* World, const FVector& At)
	{
		const FString Name = ACheeseStrikePlace::NameAt(World, At);
		return Name.IsEmpty() ? FString(TEXT("a new spot")) : Name;
	}
}

// Nobody's: a cool grey, apart from every side's colour and the solo mode's green and red: darker than the Mozza
// Radicals' white, which is no colour either.
const FLinearColor ACheeseStrikeHill::Neutral(0.38f, 0.4f, 0.44f, 1.0f);
const FLinearColor ACheeseStrikeHill::Own(0.3f, 0.92f, 0.42f, 1.0f);
const FLinearColor ACheeseStrikeHill::Other(0.93f, 0.2f, 0.15f, 1.0f);

ACheeseStrikeHillSpot::ACheeseStrikeHillSpot()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetHidden(true);
	SetCanBeDamaged(false);
}

ACheeseStrikeHill::ACheeseStrikeHill()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetCanBeDamaged(false);
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const auto MakeMesh = [this](const TCHAR* Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Root);
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetGenerateOverlapEvents(false);
		Part->SetUsingAbsoluteLocation(true);
		Part->SetUsingAbsoluteRotation(true);
		Part->SetUsingAbsoluteScale(true);
		Part->SetHiddenInGame(true);
		return Part;
	};
	Ring = MakeMesh(TEXT("Ring"), Plane.Object);
	Column = MakeMesh(TEXT("Column"), Cylinder.Object);
	NextRing = MakeMesh(TEXT("NextRing"), Plane.Object);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetUsingAbsoluteLocation(true);
	Light->SetCastShadows(false);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.0f);
	Light->SetVisibility(false);
}

ACheeseStrikeHill* ACheeseStrikeHill::Find(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<ACheeseStrikeHill> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ACheeseStrikeHill::GatherSpots(UWorld* World, TArray<FVector>& OutSpots, TArray<float>& OutRadii)
{
	OutSpots.Reset();
	OutRadii.Reset();
	if (World == nullptr)
	{
		return;
	}
	TArray<const ACheeseStrikeHillSpot*> Found;
	for (TActorIterator<ACheeseStrikeHillSpot> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	Found.Sort([](const ACheeseStrikeHillSpot& A, const ACheeseStrikeHillSpot& B) { return A.GetOrder() < B.GetOrder(); });
	for (const ACheeseStrikeHillSpot* Each : Found)
	{
		OutSpots.Add(Each->GetActorLocation());
		OutRadii.Add(Each->GetRadius() > 50.0f ? Each->GetRadius() : CheeseStrikeHills::Radius);
	}
	if (OutSpots.Num() == 0)
	{
		// A level made before the hill: one spot, by the start nearest the middle of them all, on the floor under it.
		TArray<FVector> Starts;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Starts.Add(It->GetActorLocation());
		}
		if (Starts.Num() > 0)
		{
			FVector Middle = FVector::ZeroVector;
			for (const FVector& Each : Starts)
			{
				Middle += Each / Starts.Num();
			}
			Starts.Sort([&Middle](const FVector& A, const FVector& B) { return FVector::DistSquared(A, Middle) < FVector::DistSquared(B, Middle); });
			FVector Spot = Starts[0];
			FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(CheeseStrikeHillFloor), false);
			if (World->LineTraceSingleByChannel(Hit, Spot, Spot - FVector(0.0f, 0.0f, 400.0f), ECC_WorldStatic, Query))
			{
				Spot.Z = Hit.ImpactPoint.Z;
			}
			OutSpots.Add(Spot);
			OutRadii.Add(CheeseStrikeHills::Radius);
		}
	}
}

void ACheeseStrikeHill::Begin(const TArray<FVector>& Spots, const TArray<float>& InRadii, float FirstMovesAt)
{
	Centres = Spots;
	Radii = InRadii;
	Radii.SetNum(Centres.Num());
	for (float& Each : Radii)
	{
		Each = Each > 50.0f ? Each : CheeseStrikeHills::Radius;
	}
	Spot = 0;
	Next = CheeseStrikeHills::NextSpot(Spot, Centres.Num());
	MovesAt = FirstMovesAt;
	Moves = 0;
	Clock = CheeseStrikeHills::FClock();
	Announced = CheeseStrikeHills::FHold();
	bWarned = false;
	SetActorLocation(GetCentre());
	UE_LOG(LogCheeseStrike, Display, TEXT("Hill: %d spots, the first at %s (%s)"), Centres.Num(), *GetCentre().ToCompactString(), *HillPlaceName(GetWorld(), GetCentre()));
	ForceNetUpdate();
}

void ACheeseStrikeHill::MoveNow()
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	MoveOn(State != nullptr ? State->GetServerWorldTimeSeconds() : 0.0f);
}

float ACheeseStrikeHill::SecondsToMove() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return State != nullptr ? FMath::Max(0.0f, MovesAt - static_cast<float>(State->GetServerWorldTimeSeconds())) : 0.0f;
}

CheeseStrikeHills::FHold ACheeseStrikeHill::GetHold() const
{
	CheeseStrikeHills::FHold Hold;
	Hold.State = static_cast<CheeseStrikeHills::EHold>(HoldState);
	Hold.Holder = Holder;
	return Hold;
}

bool ACheeseStrikeHill::IsHeldBy(const APlayerState* Viewer) const
{
	const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Viewer);
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Ours == nullptr || Match == nullptr || GetHold().State != CheeseStrikeHills::EHold::Held)
	{
		return false;
	}
	return Match->HasTeams() ? Ours->GetTeam() == Holder : Ours->GetPlayerId() == Holder;
}

bool ACheeseStrikeHill::ContainsActor(const AActor* Actor) const
{
	const ACharacter* Character = Cast<ACharacter>(Actor);
	if (Character == nullptr)
	{
		return false;
	}
	return Contains(Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
}

FLinearColor ACheeseStrikeHill::ColourFor(const APlayerState* Viewer, float Time) const
{
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	switch (GetHold().State)
	{
	case CheeseStrikeHills::EHold::Held:
		if (Match != nullptr && Match->HasTeams())
		{
			return CheeseStrikeModes::TeamColour(Holder);
		}
		return IsHeldBy(Viewer) ? Own : Other;
	case CheeseStrikeHills::EHold::Contested:
	{
		// Flickering between white and the danger red: nobody scores.
		const float Beat = 0.5f + 0.5f * FMath::Sin(Time * 9.0f);
		return FMath::Lerp(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), Other, Beat * 0.7f);
	}
	default:
		return Neutral;
	}
}

TArray<CheeseStrikeRules::FStanding> ACheeseStrikeHill::SoloStandings(const AGameStateBase* Match)
{
	TArray<CheeseStrikeRules::FStanding> Lines;
	if (Match != nullptr)
	{
		for (const APlayerState* Each : Match->PlayerArray)
		{
			if (const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Each))
			{
				CheeseStrikeRules::FStanding Line = Ours->Standing();
				Line.Kills = Ours->GetHillPoints();
				Lines.Add(Line);
			}
		}
	}
	return CheeseStrikeRules::Sorted(Lines);
}

void ACheeseStrikeHill::BeginPlay()
{
	Super::BeginPlay();
	// By path, when it comes into play: the material is the scripts' to rebuild (a constructor's would be held for good).
	if (UMaterialInterface* Look = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CheeseStrike/Effects/M_Hill.M_Hill")))
	{
		RingLook = UMaterialInstanceDynamic::Create(Look, this);
		ColumnLook = UMaterialInstanceDynamic::Create(Look, this);
		NextLook = UMaterialInstanceDynamic::Create(Look, this);
		ColumnLook->SetScalarParameterValue(TEXT("Column"), 1.0f);
		NextLook->SetScalarParameterValue(TEXT("Strength"), 0.45f);
		Ring->SetMaterial(0, RingLook);
		Column->SetMaterial(0, ColumnLook);
		NextRing->SetMaterial(0, NextLook);
	}
	else
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("Hill: no M_Hill (tools/art/hill_material.py); the hill shows only on the HUD"));
	}
}

void ACheeseStrikeHill::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && HasBegun())
	{
		Pending += DeltaSeconds;
		if (Pending >= HillScoreStep)
		{
			Score(Pending);
			Pending = 0.0f;
		}
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateLook();
	}
}

void ACheeseStrikeHill::Score(float Seconds)
{
	UWorld* World = GetWorld();
	ACheeseStrikeGameState* Match = World != nullptr ? World->GetGameState<ACheeseStrikeGameState>() : nullptr;
	ACheeseStrikeGameMode* Mode = World != nullptr ? World->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	if (Match == nullptr || Mode == nullptr || !Mode->IsMatchInProgress() || Match->IsFrozen())
	{
		return;
	}
	const float Now = Match->GetServerWorldTimeSeconds();
	// Moving: a warning first, then on to the next spot.
	if (NumSpots() > 1)
	{
		if (!bWarned && CheeseStrikeHills::IsWarning(MovesAt - Now))
		{
			bWarned = true;
			Match->Announce(FString::Printf(TEXT("Hill moving in %.0f  ·  next: %s"), FMath::CeilToFloat(MovesAt - Now), *HillPlaceName(World, GetNextCentre())));
		}
		if (Now >= MovesAt)
		{
			MoveOn(Now);
			Match->Announce(FString::Printf(TEXT("The hill has moved  ·  %s"), *HillPlaceName(World, GetCentre())));
		}
	}
	else if (Now >= MovesAt)
	{
		MovesAt = Now + CheeseStrikeHills::HillTime;
	}

	// Who stands in it: the living, by their feet.
	const bool bTeams = Match->HasTeams();
	TArray<CheeseStrikeHills::FInHill> Inside;
	TArray<ACheeseStrikePlayerState*> InsideStates;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		ACheeseStrikePlayerState* Theirs = It->GetPlayerState<ACheeseStrikePlayerState>();
		if (!It->IsAlive() || Theirs == nullptr || !ContainsActor(*It))
		{
			continue;
		}
		Inside.Add({Theirs->GetTeam(), Theirs->GetPlayerId()});
		InsideStates.Add(Theirs);
	}
	const CheeseStrikeHills::FHold Hold = CheeseStrikeHills::HoldOf(bTeams, Inside);
	// The time captured goes to one of those holding the hill, in turn: the holder's side in a team mode, and nobody while it is contested.
	if (Hold.State == CheeseStrikeHills::EHold::Held)
	{
		TArray<ACheeseStrikePlayerState*> Holders;
		for (ACheeseStrikePlayerState* Each : InsideStates)
		{
			if (!bTeams || Each->GetTeam() == Hold.Holder)
			{
				Holders.Add(Each);
			}
		}
		if (Holders.Num() > 0)
		{
			Holders[CheeseStrikeCredit::CreditIndex(CreditedSeconds, Holders.Num())]->AddHillTime(Seconds);
			CreditedSeconds += Seconds;
		}
	}
	ACheeseStrikePlayerState* HolderState = nullptr;
	if (Hold.State == CheeseStrikeHills::EHold::Held && !bTeams)
	{
		HolderState = InsideStates.Num() > 0 ? InsideStates[0] : nullptr;
	}
	if (Hold != GetHold())
	{
		HoldState = static_cast<uint8>(Hold.State);
		Holder = Hold.Holder;
		HolderName = Hold.State != CheeseStrikeHills::EHold::Held ? FString() : (bTeams ? FString(CheeseStrikeRules::TeamNames[FMath::Clamp(Hold.Holder, 0, CheeseStrikeRules::Teams - 1)]) : (HolderState != nullptr ? HolderState->GetPlayerName() : FString()));
		ForceNetUpdate();
	}
	// Captured: a new holder (not the same one back after a contest).
	if (Hold.State == CheeseStrikeHills::EHold::Held && Hold.Holder != Announced.Holder)
	{
		Announced = Hold;
		Match->Announce(FString::Printf(TEXT("%s captured the hill"), *HolderName), CheeseStrikeStreaks::EStreak::None, bTeams ? Hold.Holder : CheeseStrikeModes::NoTeam, bTeams ? FString() : HolderName);
		UE_LOG(LogCheeseStrike, Display, TEXT("Hill: %s captured it at %s"), *HolderName, *HillPlaceName(World, GetCentre()));
	}

	const int32 Points = Clock.Advance(Seconds, Hold);
	if (Points <= 0)
	{
		return;
	}
	const int32 Target = Match->GetKillLimit();
	if (bTeams)
	{
		Match->AddTeamScore(Hold.Holder, Points);
		if (Match->GetTeamScore(Hold.Holder) >= Target)
		{
			Mode->HillWon(Hold.Holder, nullptr);
		}
	}
	else if (HolderState != nullptr)
	{
		HolderState->AddHillPoints(Points);
		if (HolderState->GetHillPoints() >= Target)
		{
			Mode->HillWon(CheeseStrikeModes::NoTeam, HolderState);
		}
	}
}

void ACheeseStrikeHill::MoveOn(float MovesFrom)
{
	CreditedSeconds = 0.0f;
	if (!HasBegun())
	{
		return;
	}
	Spot = Next;
	Next = CheeseStrikeHills::NextSpot(Spot, NumSpots());
	MovesAt = MovesFrom + CheeseStrikeHills::HillTime;
	++Moves;
	bWarned = false;
	Clock = CheeseStrikeHills::FClock();
	Announced = CheeseStrikeHills::FHold();
	HoldState = static_cast<uint8>(CheeseStrikeHills::EHold::Neutral);
	Holder = INDEX_NONE;
	HolderName.Reset();
	SetActorLocation(GetCentre());
	UE_LOG(LogCheeseStrike, Display, TEXT("Hill: moved to spot %d of %d, %s"), Spot + 1, NumSpots(), *HillPlaceName(GetWorld(), GetCentre()));
	ForceNetUpdate();
}

void ACheeseStrikeHill::UpdateLook()
{
	const bool bShown = HasBegun();
	Ring->SetHiddenInGame(!bShown || RingLook == nullptr);
	Column->SetHiddenInGame(!bShown || ColumnLook == nullptr);
	Light->SetVisibility(bShown);
	const bool bNext = bShown && IsWarning() && Next != Spot;
	NextRing->SetHiddenInGame(!bNext || NextLook == nullptr);
	if (!bShown)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const APlayerController* Viewer = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	const float Time = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	const FLinearColor Colour = ColourFor(Viewer != nullptr ? Viewer->PlayerState.Get() : nullptr, Time);
	const FVector Centre = GetCentre();
	const float Across = GetRadius() * 2.0f / 100.0f;
	Ring->SetWorldLocationAndRotation(Centre + FVector(0.0f, 0.0f, 3.0f), FRotator::ZeroRotator);
	Ring->SetWorldScale3D(FVector(Across, Across, 1.0f));
	Column->SetWorldLocationAndRotation(Centre + FVector(0.0f, 0.0f, HillColumnHeight / 2.0f + 2.0f), FRotator::ZeroRotator);
	Column->SetWorldScale3D(FVector(Across, Across, HillColumnHeight / 100.0f));
	Light->SetWorldLocation(Centre + FVector(0.0f, 0.0f, 160.0f));
	Light->SetLightColor(Colour);
	Light->SetIntensity(HillLightStrength);
	Light->SetAttenuationRadius(GetRadius() * 1.8f);
	for (UMaterialInstanceDynamic* Look : {RingLook.Get(), ColumnLook.Get()})
	{
		if (Look != nullptr)
		{
			Look->SetVectorParameterValue(TEXT("Colour"), Colour);
		}
	}
	if (bNext && NextLook != nullptr)
	{
		const float NextAcross = GetNextRadius() * 2.0f / 100.0f;
		NextRing->SetWorldLocationAndRotation(GetNextCentre() + FVector(0.0f, 0.0f, 3.0f), FRotator::ZeroRotator);
		NextRing->SetWorldScale3D(FVector(NextAcross, NextAcross, 1.0f));
		// Blinking, as the warning does.
		NextLook->SetVectorParameterValue(TEXT("Colour"), FMath::Fmod(Time, 0.8f) < 0.5f ? Neutral : Neutral * 0.4f);
	}
}

void ACheeseStrikeHill::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeHill, Centres);
	DOREPLIFETIME(ACheeseStrikeHill, Radii);
	DOREPLIFETIME(ACheeseStrikeHill, Spot);
	DOREPLIFETIME(ACheeseStrikeHill, Next);
	DOREPLIFETIME(ACheeseStrikeHill, MovesAt);
	DOREPLIFETIME(ACheeseStrikeHill, Moves);
	DOREPLIFETIME(ACheeseStrikeHill, HoldState);
	DOREPLIFETIME(ACheeseStrikeHill, Holder);
	DOREPLIFETIME(ACheeseStrikeHill, HolderName);
}
