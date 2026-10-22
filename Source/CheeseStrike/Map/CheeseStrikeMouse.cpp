// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMouse.h"
#include "Animation/AnimSequence.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "Map/CheeseStrikeCheese.h"
#include "Rules/CheeseStrikeFalls.h"
#include "Rules/CheeseStrikeMice.h"

namespace
{
	/** Degrees a second a mouse turns. */
	constexpr float MouseTurnRate = 200.0f;
	/** How near a point of its way counts as there (cm). */
	constexpr float MousePointReached = 40.0f;
	/** How often it looks again for cheese while going to some (seconds). */
	constexpr float MouseChooseEvery = 1.0f;
	/** A way is found afresh when it is this old (seconds), or the mouse this far below it (cm): it fell through eaten cheese. */
	constexpr float MouseFreshWay = 4.0f;
	constexpr float MouseFallenBelow = 150.0f;
	/** How far off a block it looks for somewhere to stand to eat it (cm), and how many ways there it tries. */
	constexpr float MouseStandOff = 100.0f;
	constexpr int32 MouseWaysTried = 4;
	/** A mouse's sounds are a rat's, pitched down by about this for its size, and heard this far (cm), at full loudness within the first. */
	constexpr float MousePitch = 0.7f;
	constexpr float MouseHeardWithin = 600.0f;
	constexpr float MouseHeardTo = 3500.0f;
	/** Seconds between its squeaks, from and to; and between its crunches while it eats. */
	constexpr float MouseSqueakFrom = 4.0f;
	constexpr float MouseSqueakTo = 9.0f;
	constexpr float MouseCrunchFrom = 0.4f;
	constexpr float MouseCrunchTo = 0.9f;
	/** How far below its feet it looks for ground before a step (cm): a step down a stair or a ramp, not off an edge. */
	constexpr float MouseStepDown = 100.0f;
	/** Seconds after falling in that it is out of sight, under the fondue. */
	constexpr float MouseSunkAfter = 1.5f;
	/** What it feels the ground with, under its middle (cm): a ball, which a crack between two blocks doesn't let through. */
	constexpr float MouseFootRadius = 8.0f;
	/** It stands on a block whose top its feet are on, or that is within this of them (cm) across: eating it, it would fall. */
	constexpr float MouseStandsWithin = 30.0f;

	UAudioComponent* MakeSpeaker(AActor* Mouse, USceneComponent* Body, const TCHAR* Name)
	{
		UAudioComponent* Audio = Mouse->CreateDefaultSubobject<UAudioComponent>(Name);
		Audio->SetupAttachment(Body);
		Audio->bAutoActivate = false;
		Audio->bOverrideAttenuation = true;
		Audio->AttenuationOverrides.bAttenuate = true;
		Audio->AttenuationOverrides.bSpatialize = true;
		Audio->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		Audio->AttenuationOverrides.AttenuationShapeExtents = FVector(MouseHeardWithin, 0.0f, 0.0f);
		Audio->AttenuationOverrides.FalloffDistance = MouseHeardTo - MouseHeardWithin;
		return Audio;
	}

	USoundBase* FindMouseSound(const TCHAR* Name)
	{
		ConstructorHelpers::FObjectFinder<USoundBase> Sound(*FString::Printf(TEXT("/Game/OpenGameArt/Mice/SW_%s.SW_%s"), Name, Name));
		return Sound.Object;
	}
}

ACheeseStrikeMouse::ACheeseStrikeMouse()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(30.0f);

	// Its body as shots see it; it blocks nothing that moves.
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->InitBoxExtent(FVector(170.0f, 80.0f, 75.0f));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetCanEverAffectNavigation(false);

	Model = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat.SK_BlackRat")));
	Walk = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat_Anim_Mammals_walk_A1.SK_BlackRat_Anim_Mammals_walk_A1")));
	Run = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat_Anim_Mammals_run_A1.SK_BlackRat_Anim_Mammals_run_A1")));
	Sniff = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat_Anim_Mammals_idle_A2.SK_BlackRat_Anim_Mammals_idle_A2")));
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Body);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	// The rat is modelled along its Y axis, nose forward, feet at its origin: turned to face the actor's X, standing on the ground under the body.
	Mesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -75.0f), FRotator(0.0f, -90.0f, 0.0f));
	Mesh->SetRelativeScale3D(FVector(CheeseStrikeMice::Scale));
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Gnawing = MakeSpeaker(this, Body, TEXT("Gnawing"));
	Mouth = MakeSpeaker(this, Body, TEXT("Mouth"));
	Voice = MakeSpeaker(this, Body, TEXT("Voice"));
	Gnawing->SetSound(FindMouseSound(TEXT("Gnawing")));
	Squeaks = {FindMouseSound(TEXT("RatSqueak")), FindMouseSound(TEXT("RatSqueakPain"))};
	Crunches = {FindMouseSound(TEXT("Crunch1")), FindMouseSound(TEXT("Crunch2")), FindMouseSound(TEXT("Crunch3")), FindMouseSound(TEXT("Crunch4"))};
	Scream = FindMouseSound(TEXT("RatScream"));
}

bool ACheeseStrikeMouse::HasBody() const
{
	if (Mesh->GetSkeletalMeshAsset() == nullptr || WalkAnimation == nullptr || RunAnimation == nullptr || EatAnimation == nullptr)
	{
		return false;
	}
	for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
	{
		if (Mesh->GetMaterial(Index) == nullptr)
		{
			return false;
		}
	}
	return Mesh->GetNumMaterials() > 0;
}

bool ACheeseStrikeMouse::HasSounds() const
{
	return Gnawing->GetSound() != nullptr && Scream != nullptr && !Squeaks.Contains(nullptr) && !Crunches.Contains(nullptr);
}

void ACheeseStrikeMouse::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeMouse, Doing);
}

void ACheeseStrikeMouse::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetSkeletalMeshAsset(Model.LoadSynchronous());
	WalkAnimation = Walk.LoadSynchronous();
	RunAnimation = Run.LoadSynchronous();
	EatAnimation = Sniff.LoadSynchronous();
	NextSqueakAt = Now() + FMath::FRandRange(MouseSqueakFrom, MouseSqueakTo);
	ShowDoing();
}

float ACheeseStrikeMouse::Now() const
{
	return GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void ACheeseStrikeMouse::Enter(const FVector& From, const FVector& To)
{
	EnterFrom = From;
	EnterTo = To;
	EnteredFor = 0.0f;
	Doing = EMouseDoing::Entering;
	SetActorLocation(From + FVector(0.0f, 0.0f, Body->GetScaledBoxExtent().Z));
	SetActorRotation(FRotator(0.0f, (To - From).Rotation().Yaw, 0.0f));
	ShowDoing();
}

void ACheeseStrikeMouse::OnRep_Doing()
{
	ShowDoing();
}

void ACheeseStrikeMouse::ShowDoing()
{
	if (bShownOnce && Shown == Doing)
	{
		return;
	}
	const bool bWasEating = bShownOnce && Shown == EMouseDoing::Eating;
	const bool bFirst = !bShownOnce;
	bShownOnce = true;
	Shown = Doing;
	if (GetNetMode() != NM_DedicatedServer && GetWorld() != nullptr && GetWorld()->IsGameWorld())
	{
		// Gnawing while it eats; a scream as it runs.
		if (Doing == EMouseDoing::Eating)
		{
			Gnawing->SetPitchMultiplier(MousePitch * FMath::FRandRange(0.95f, 1.1f));
			Gnawing->SetVolumeMultiplier(0.6f);
			Gnawing->Play(FMath::FRandRange(0.0f, 10.0f));
			NextCrunchAt = Now() + FMath::FRandRange(0.0f, MouseCrunchFrom);
		}
		else if (bWasEating)
		{
			Gnawing->FadeOut(0.3f, 0.0f);
		}
		if ((Doing == EMouseDoing::Fleeing || Doing == EMouseDoing::Sinking) && !bFirst)
		{
			Say(Voice, Scream, 1.0f, MousePitch);
		}
	}
	UAnimSequence* Animation = Doing == EMouseDoing::Eating ? EatAnimation.Get() : (Doing == EMouseDoing::Walking ? WalkAnimation.Get() : RunAnimation.Get());
	if (Animation != nullptr && Mesh != nullptr)
	{
		Mesh->PlayAnimation(Animation, true);
		// A rat's stride at a mouse's size: slower than a rat's, its legs being longer.
		Mesh->SetPlayRate(Doing == EMouseDoing::Fleeing ? 0.8f : (Doing == EMouseDoing::Walking ? 0.7f : 1.0f));
	}
}

float ACheeseStrikeMouse::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Taken = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || Doing == EMouseDoing::Entering || Doing == EMouseDoing::Sinking)
	{
		return Taken;
	}
	Fear += Damage;
	if (Fear >= CheeseStrikeMice::FearAt && Doing != EMouseDoing::Fleeing)
	{
		const AActor* Shooter = EventInstigator != nullptr && EventInstigator->GetPawn() != nullptr ? static_cast<AActor*>(EventInstigator->GetPawn()) : DamageCauser;
		RunFrom(Shooter != nullptr ? Shooter->GetActorLocation() : GetActorLocation() - GetActorForwardVector() * 1000.0f);
	}
	return Damage;
}

void ACheeseStrikeMouse::RunFrom(const FVector& Danger)
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (Navigation == nullptr)
	{
		return;
	}
	// Away: of a few places in reach, the furthest from where the shots came from.
	const FVector Here = GetActorLocation();
	FVector Best = Here;
	float BestDistance = -1.0f;
	for (int32 Try = 0; Try < 8; ++Try)
	{
		FNavLocation Spot;
		if (Navigation->GetRandomReachablePointInRadius(Here, 3000.0f, Spot) && FVector::Dist2D(Spot.Location, Danger) > BestDistance)
		{
			BestDistance = FVector::Dist2D(Spot.Location, Danger);
			Best = Spot.Location;
		}
	}
	if (BestDistance < 0.0f || !FindWay(Best))
	{
		return;
	}
	Target = nullptr;
	Doing = EMouseDoing::Fleeing;
	FleeUntil = Now() + CheeseStrikeMice::FleeFor;
	ShowDoing();
}

bool ACheeseStrikeMouse::FindWay(const FVector& To)
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (Navigation == nullptr)
	{
		return false;
	}
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, Body->GetScaledBoxExtent().Z);
	UNavigationPath* Way = Navigation->FindPathToLocationSynchronously(GetWorld(), Feet, To, this);
	if (Way == nullptr || !Way->IsValid() || Way->IsPartial() || Way->PathPoints.Num() == 0)
	{
		return false;
	}
	Path = Way->PathPoints;
	PathIndex = 0;
	PathFoundAt = Now();
	return true;
}

bool ACheeseStrikeMouse::FindWayBeside(const ACheeseStrikeCheese* Cheese)
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (Navigation == nullptr || Cheese == nullptr)
	{
		return false;
	}
	// Places to stand beside it, off its sides and corners, the nearest first: not on top of it, nor higher (the
	// navigation mesh lies on cheese too, and the top of a stack is an island), nor too far below it to reach it.
	const FBox Box = Cheese->GetComponentsBoundingBox();
	const FVector Middle = Box.GetCenter();
	const FVector Half = Box.GetExtent();
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, Body->GetScaledBoxExtent().Z);
	// At its foot; or, for one taller than a mouse reaches (a tile of The Fondue Floor, standing in the vat), looking
	// from a little under its top.
	const float StandAt = Box.Max.Z - Box.Min.Z > CheeseStrikeMice::ReachUp ? Box.Max.Z - CheeseStrikeMice::ReachUp + 50.0f : Box.Min.Z;
	TArray<FVector> Spots;
	for (int32 X = -1; X <= 1; ++X)
	{
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			if (X != 0 || Y != 0)
			{
				Spots.Add(FVector(Middle.X + X * (Half.X + MouseStandOff), Middle.Y + Y * (Half.Y + MouseStandOff), StandAt));
			}
		}
	}
	Spots.Sort([&Feet](const FVector& A, const FVector& B) { return FVector::DistSquared2D(A, Feet) < FVector::DistSquared2D(B, Feet); });
	int32 Tries = 0;
	for (const FVector& Spot : Spots)
	{
		FNavLocation Beside;
		if (!Navigation->ProjectPointToNavigation(Spot, Beside, FVector(MouseStandOff, MouseStandOff, CheeseStrikeMice::ReachUp)))
		{
			continue;
		}
		// Over it, or level with its top so near its edge that it would stand on it (StandsOn).
		const bool bOnIt = Box.ExpandBy(FVector(-10.0f, -10.0f, 0.0f)).IsInsideXY(Beside.Location)
			|| (Box.ExpandBy(FVector(MouseStandsWithin, MouseStandsWithin, 0.0f)).IsInsideXY(Beside.Location) && FMath::Abs(Beside.Location.Z - Box.Max.Z) < 30.0f);
		if (bOnIt || Beside.Location.Z > Box.Max.Z + 10.0f || Box.Min.Z - Beside.Location.Z > CheeseStrikeMice::ReachUp)
		{
			continue;
		}
		if (FindWay(Beside.Location))
		{
			return true;
		}
		if (++Tries >= MouseWaysTried)
		{
			break;
		}
	}
	return false;
}

bool ACheeseStrikeMouse::CanReach(const ACheeseStrikeCheese* Cheese) const
{
	if (Cheese == nullptr)
	{
		return false;
	}
	const FBox Box = Cheese->GetComponentsBoundingBox();
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, Body->GetScaledBoxExtent().Z);
	const FVector Nearest = Box.GetClosestPointTo(Feet);
	// Never from on top of it: it would eat the floor from under its own feet.
	return FVector::Dist2D(Nearest, Feet) <= CheeseStrikeMice::ReachAcross && Box.Min.Z - Feet.Z <= CheeseStrikeMice::ReachUp && Box.Max.Z > Feet.Z - 50.0f && !StandsOn(Cheese);
}

bool ACheeseStrikeMouse::StandsOn(const ACheeseStrikeCheese* Cheese) const
{
	if (Cheese == nullptr)
	{
		return false;
	}
	const FBox Box = Cheese->GetComponentsBoundingBox();
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, Body->GetScaledBoxExtent().Z);
	return Box.ExpandBy(FVector(MouseStandsWithin, MouseStandsWithin, 0.0f)).IsInsideXY(Feet) && FMath::Abs(Feet.Z - Box.Max.Z) < 30.0f;
}

bool ACheeseStrikeMouse::HasGroundAt(const FVector& Feet) const
{
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MouseStep), false, this);
	for (TActorIterator<ACheeseStrikeMouse> It(GetWorld()); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	FHitResult Ground;
	return GetWorld()->SweepSingleByObjectType(Ground, Feet + FVector(0.0f, 0.0f, 60.0f), Feet - FVector(0.0f, 0.0f, MouseStepDown), FQuat::Identity, Objects, FCollisionShape::MakeSphere(MouseFootRadius), Params);
}

bool ACheeseStrikeMouse::ComeBack()
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (Navigation == nullptr)
	{
		return false;
	}
	// Over the edge as the mice first came (ACheeseStrikeGameMode::SpawnMice), at whichever way in still has ground near inside it.
	TArray<AActor*> Entries;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("MouseEntry"))))
		{
			Entries.Add(*It);
		}
	}
	for (int32 Index = Entries.Num() - 1; Index > 0; --Index)
	{
		Entries.Swap(Index, FMath::RandHelper(Index + 1));
	}
	for (const AActor* Entry : Entries)
	{
		const FVector Top = Entry->GetActorLocation();
		FNavLocation Ground;
		if (Navigation->ProjectPointToNavigation(Top + Entry->GetActorForwardVector() * 500.0f, Ground, FVector(600.0f, 600.0f, 2000.0f)) && !CheeseStrikeFalls::IsBelowKillHeight(GetWorld(), Ground.Location.Z))
		{
			SetActorHiddenInGame(false);
			SetActorEnableCollision(true);
			FallSpeed = 0.0f;
			Fear = 0.0f;
			Target = nullptr;
			Path.Reset();
			GivenUp.Reset();
			UE_LOG(LogCheeseStrike, Display, TEXT("Mouse %s comes back out of the fondue, over the edge at %s"), *GetName(), *Top.ToCompactString());
			Enter(Top, Ground.Location);
			return true;
		}
	}
	return false;
}

void ACheeseStrikeMouse::ChooseCheese()
{
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, Body->GetScaledBoxExtent().Z);
	const float Time = Now();
	// What the other mice have.
	TSet<const ACheeseStrikeCheese*> Taken;
	TArray<const ACheeseStrikeMouse*> Others;
	for (TActorIterator<ACheeseStrikeMouse> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->GetTarget() != nullptr)
		{
			Taken.Add(It->GetTarget());
		}
		if (*It != this && It->GetDoing() != EMouseDoing::Sinking)
		{
			Others.Add(*It);
		}
	}
	// Every block's cost, the cheapest first; the first that can be walked to (or reached from here) is the one.
	TArray<TPair<float, ACheeseStrikeCheese*>> Costs;
	for (TActorIterator<ACheeseStrikeCheese> It(GetWorld()); It; ++It)
	{
		ACheeseStrikeCheese* Cheese = *It;
		// Never what the map keeps: its way through, and what its starts stand on.
		if (!CheeseStrikeMice::MayEat(Cheese->IsEdible(), Cheese->GetHealth(), Cheese->IsFalling()) || Cheese->IsActorBeingDestroyed())
		{
			continue;
		}
		const float* Until = GivenUp.Find(Cheese);
		if (Until != nullptr && *Until > Time)
		{
			continue;
		}
		const FBox Box = Cheese->GetComponentsBoundingBox();
		// Sunk into the fondue: out of reach for good.
		if (CheeseStrikeFalls::IsBelowKillHeight(GetWorld(), Box.Max.Z))
		{
			continue;
		}
		// What another mouse stands on is as good as theirs: eaten, it would drop them in.
		const bool bTheirs = Taken.Contains(Cheese) || Others.ContainsByPredicate([Cheese](const ACheeseStrikeMouse* Other) { return Other->StandsOn(Cheese); });
		Costs.Add({CheeseStrikeMice::CheeseCost(FVector::Dist2D(Box.GetCenter(), Feet), Box.Min.Z - Feet.Z, bTheirs), Cheese});
	}
	Costs.Sort([](const TPair<float, ACheeseStrikeCheese*>& A, const TPair<float, ACheeseStrikeCheese*>& B) { return A.Key < B.Key; });
	for (int32 Index = 0; Index < FMath::Min(Costs.Num(), 6); ++Index)
	{
		ACheeseStrikeCheese* Cheese = Costs[Index].Value;
		if (Cheese == Target.Get() && Path.Num() > 0)
		{
			return;
		}
		const bool bWay = CanReach(Cheese) || FindWayBeside(Cheese);
		if (bWay)
		{
			if (Cheese != Target.Get())
			{
				Target = Cheese;
				ProgressAt = Time;
				Closest = FVector::Dist2D(Cheese->GetComponentsBoundingBox().GetClosestPointTo(Feet), Feet);
			}
			return;
		}
		// Nowhere to stand within reach of it: not for a while.
		GivenUp.Add(Cheese, Time + CheeseStrikeMice::GiveUpFor);
	}
	Target = nullptr;
	Path.Reset();
}

bool ACheeseStrikeMouse::FollowPath(float Speed, float DeltaSeconds)
{
	if (!Path.IsValidIndex(PathIndex))
	{
		return true;
	}
	const float Lift = Body->GetScaledBoxExtent().Z;
	float Step = Speed * DeltaSeconds;
	FVector Here = GetActorLocation() - FVector(0.0f, 0.0f, Lift);
	FVector Heading = GetActorForwardVector();
	while (Step > 0.0f && Path.IsValidIndex(PathIndex))
	{
		const FVector ToPoint = Path[PathIndex] - Here;
		const float Distance = ToPoint.Size();
		if (Distance <= FMath::Max(Step, PathIndex == Path.Num() - 1 ? 1.0f : MousePointReached))
		{
			if (Distance <= Step)
			{
				Here = Path[PathIndex];
				Step -= Distance;
			}
			++PathIndex;
			continue;
		}
		Heading = ToPoint.GetSafeNormal();
		Here += Heading * Step;
		Step = 0.0f;
	}
	// Never off an edge: a way found before the cheese ahead was eaten leads into a hole. It stops, and finds its way afresh.
	if (!HasGroundAt(Here))
	{
		Path.Reset();
		TurnToward(Heading, DeltaSeconds);
		return true;
	}
	SetActorLocation(Here + FVector(0.0f, 0.0f, Lift));
	TurnToward(Heading, DeltaSeconds);
	return !Path.IsValidIndex(PathIndex);
}

void ACheeseStrikeMouse::KeepOnGround(float DeltaSeconds)
{
	const float Lift = Body->GetScaledBoxExtent().Z;
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, Lift);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MouseGround), false, this);
	for (TActorIterator<ACheeseStrikeMouse> It(GetWorld()); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	FHitResult Ground;
	// From a step above its feet: it walks up a ramp's rise, not onto a crate it brushes past. Felt with a small ball, so
	// that it doesn't drop through the crack between two blocks it stands across.
	if (!GetWorld()->SweepSingleByObjectType(Ground, Feet + FVector(0.0f, 0.0f, 60.0f), Feet - FVector(0.0f, 0.0f, 5000.0f), FQuat::Identity, Objects, FCollisionShape::MakeSphere(MouseFootRadius), Params))
	{
		return;
	}
	float Z = Ground.ImpactPoint.Z;
	if (Z < Feet.Z - 2.0f)
	{
		// Nothing under it any more: it falls.
		FallSpeed += 980.0f * DeltaSeconds;
		Z = FMath::Max(Z, Feet.Z - FallSpeed * DeltaSeconds);
	}
	else
	{
		FallSpeed = 0.0f;
	}
	SetActorLocation(FVector(Feet.X, Feet.Y, Z + Lift));
}

void ACheeseStrikeMouse::TurnToward(const FVector& Direction, float DeltaSeconds)
{
	if (Direction.SizeSquared2D() < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FRotator Wanted(0.0f, Direction.Rotation().Yaw, 0.0f);
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Wanted, DeltaSeconds, MouseTurnRate));
}

void ACheeseStrikeMouse::Say(UAudioComponent* Speaker, USoundBase* Sound, float Volume, float Around)
{
	if (Speaker == nullptr || Sound == nullptr)
	{
		return;
	}
	Speaker->SetSound(Sound);
	Speaker->SetVolumeMultiplier(Volume);
	Speaker->SetPitchMultiplier(Around * FMath::FRandRange(0.9f, 1.15f));
	Speaker->Play();
}

void ACheeseStrikeMouse::Sound(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const float Time = Now();
	if (Shown == EMouseDoing::Sinking)
	{
		return;
	}
	if (Time >= NextSqueakAt)
	{
		NextSqueakAt = Time + FMath::FRandRange(MouseSqueakFrom, MouseSqueakTo);
		// Not over its scream.
		if (!Voice->IsPlaying() && Squeaks.Num() > 0)
		{
			Say(Voice, Squeaks[FMath::RandRange(0, Squeaks.Num() - 1)], 0.8f, MousePitch);
		}
	}
	if (Shown == EMouseDoing::Eating && Time >= NextCrunchAt && Crunches.Num() > 0)
	{
		NextCrunchAt = Time + FMath::FRandRange(MouseCrunchFrom, MouseCrunchTo);
		Say(Mouth, Crunches[FMath::RandRange(0, Crunches.Num() - 1)], 0.7f, MousePitch + 0.1f);
	}
}

void ACheeseStrikeMouse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Sound(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	const float Time = Now();
	Fear = CheeseStrikeMice::FearAfter(Fear, DeltaSeconds);
	// Below the map's kill height, it has fallen into the fondue: it screams and goes under, and comes back over the edge a while later.
	if (Doing != EMouseDoing::Entering && Doing != EMouseDoing::Sinking && CheeseStrikeFalls::IsBelowKillHeight(GetWorld(), GetActorLocation().Z - Body->GetScaledBoxExtent().Z))
	{
		Doing = EMouseDoing::Sinking;
		BackAt = Time + CheeseStrikeFalls::MouseGoneFor;
		++FallenIn;
		Target = nullptr;
		Path.Reset();
		SetActorEnableCollision(false);
		UE_LOG(LogCheeseStrike, Display, TEXT("Mouse %s fell in at %s"), *GetName(), *GetActorLocation().ToCompactString());
	}
	switch (Doing)
	{
	case EMouseDoing::Sinking:
		if (!IsHidden() && Time >= BackAt - CheeseStrikeFalls::MouseGoneFor + MouseSunkAfter)
		{
			SetActorHiddenInGame(true);
		}
		if (Time >= BackAt && !ComeBack())
		{
			BackAt = Time + 2.0f;
		}
		break;
	case EMouseDoing::Entering:
	{
		// Over the top of the wall and down to the ground, in an arc.
		EnteredFor += DeltaSeconds;
		const float Through = FMath::Clamp(EnteredFor / CheeseStrikeMice::EnterTime, 0.0f, 1.0f);
		const FVector Flat = FMath::Lerp(EnterFrom, EnterTo, Through);
		const float Height = FMath::Lerp(EnterFrom.Z, EnterTo.Z, Through * Through) + 150.0f * FMath::Sin(Through * PI);
		SetActorLocation(FVector(Flat.X, Flat.Y, Height + Body->GetScaledBoxExtent().Z));
		if (Through >= 1.0f)
		{
			Doing = EMouseDoing::Walking;
			NextChoiceAt = 0.0f;
		}
		break;
	}
	case EMouseDoing::Fleeing:
		FollowPath(CheeseStrikeMice::FleeSpeed, DeltaSeconds);
		if (Time >= FleeUntil)
		{
			Doing = EMouseDoing::Walking;
			Fear = 0.0f;
			NextChoiceAt = 0.0f;
		}
		break;
	case EMouseDoing::Walking:
	case EMouseDoing::Eating:
	{
		ACheeseStrikeCheese* Cheese = Target.Get();
		// Its way is stale when it has fallen below it (what it walked on was eaten) or is old (the mesh changes as the cheese goes).
		if (Path.IsValidIndex(PathIndex) && (GetActorLocation().Z - Body->GetScaledBoxExtent().Z < Path[PathIndex].Z - MouseFallenBelow || Time - PathFoundAt > MouseFreshWay))
		{
			Path.Reset();
			NextChoiceAt = FMath::Min(NextChoiceAt, Time);
		}
		if (Cheese == nullptr || Cheese->GetHealth() <= 0.0f || Cheese->IsActorBeingDestroyed() || Cheese->IsFalling() || Time >= NextChoiceAt)
		{
			NextChoiceAt = Time + MouseChooseEvery;
			ChooseCheese();
			Cheese = Target.Get();
		}
		if (Cheese == nullptr)
		{
			Doing = EMouseDoing::Walking;
			break;
		}
		const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, Body->GetScaledBoxExtent().Z);
		const float Distance = FVector::Dist2D(Cheese->GetComponentsBoundingBox().GetClosestPointTo(Feet), Feet);
		if (Distance < Closest - 25.0f)
		{
			Closest = Distance;
			ProgressAt = Time;
		}
		if (CanReach(Cheese))
		{
			// Eats, facing it: a bite every half second.
			Doing = EMouseDoing::Eating;
			TurnToward(Cheese->GetComponentsBoundingBox().GetCenter() - GetActorLocation(), DeltaSeconds);
			if (Time >= NextBiteAt)
			{
				NextBiteAt = Time + CheeseStrikeMice::BiteEvery;
				ProgressAt = Time;
				Cheese->Bite(CheeseStrikeMice::Bite);
				if (Cheese->GetHealth() <= 0.0f)
				{
					++Eaten;
					Target = nullptr;
					NextChoiceAt = 0.0f;
				}
			}
		}
		else
		{
			Doing = EMouseDoing::Walking;
			if (FollowPath(CheeseStrikeMice::WalkSpeed, DeltaSeconds))
			{
				// At the end of the way and still out of reach: a new way next time it chooses.
				NextChoiceAt = FMath::Min(NextChoiceAt, Time + 0.2f);
				Path.Reset();
			}
		}
		// No progress for a while: it leaves this one alone.
		if (Target.IsValid() && Time - ProgressAt > CheeseStrikeMice::GiveUpAfter)
		{
			GivenUp.Add(Target, Time + CheeseStrikeMice::GiveUpFor);
			UE_LOG(LogCheeseStrike, Verbose, TEXT("Mouse %s gives up on %s"), *GetName(), *Target->GetName());
			Target = nullptr;
			Path.Reset();
			NextChoiceAt = 0.0f;
		}
		break;
	}
	}
	if (Doing != EMouseDoing::Entering)
	{
		KeepOnGround(DeltaSeconds);
	}
	ShowDoing();
}
