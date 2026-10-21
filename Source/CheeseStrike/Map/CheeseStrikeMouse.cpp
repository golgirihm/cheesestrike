// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMouse.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "Map/CheeseStrikeCheese.h"
#include "Rules/CheeseStrikeMice.h"

namespace
{
	/** Degrees a second a mouse turns. */
	constexpr float TurnRate = 200.0f;
	/** How near a point of its way counts as there (cm). */
	constexpr float PointReached = 40.0f;
	/** How often it looks again for cheese while going to some (seconds). */
	constexpr float ChooseEvery = 1.0f;
	const TCHAR* const Rat = TEXT("/Game/Fab/Nestaeric/BlackRat/");
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

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Model(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat.SK_BlackRat"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat_Anim_Mammals_walk_A1.SK_BlackRat_Anim_Mammals_walk_A1"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Run(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat_Anim_Mammals_run_A1.SK_BlackRat_Anim_Mammals_run_A1"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Sniff(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat_Anim_Mammals_idle_A2.SK_BlackRat_Anim_Mammals_idle_A2"));
	WalkAnimation = Walk.Object;
	RunAnimation = Run.Object;
	EatAnimation = Sniff.Object;
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Body);
	Mesh->SetSkeletalMeshAsset(Model.Object);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	// The rat is modelled along its Y axis, nose forward, feet at its origin: turned to face the actor's X, standing on the ground under the body.
	Mesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -75.0f), FRotator(0.0f, -90.0f, 0.0f));
	Mesh->SetRelativeScale3D(FVector(CheeseStrikeMice::Scale));
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
}

void ACheeseStrikeMouse::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeMouse, Doing);
}

void ACheeseStrikeMouse::BeginPlay()
{
	Super::BeginPlay();
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
	bShownOnce = true;
	Shown = Doing;
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
	if (!HasAuthority() || Doing == EMouseDoing::Entering)
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
	return true;
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
	return FVector::Dist2D(Nearest, Feet) <= CheeseStrikeMice::ReachAcross && Box.Min.Z - Feet.Z <= CheeseStrikeMice::ReachUp && Box.Max.Z > Feet.Z - 50.0f;
}

void ACheeseStrikeMouse::ChooseCheese()
{
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, Body->GetScaledBoxExtent().Z);
	const float Time = Now();
	// What the other mice have.
	TSet<const ACheeseStrikeCheese*> Taken;
	for (TActorIterator<ACheeseStrikeMouse> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->GetTarget() != nullptr)
		{
			Taken.Add(It->GetTarget());
		}
	}
	// Every block's cost, the cheapest first; the first that can be walked to (or reached from here) is the one.
	TArray<TPair<float, ACheeseStrikeCheese*>> Costs;
	for (TActorIterator<ACheeseStrikeCheese> It(GetWorld()); It; ++It)
	{
		ACheeseStrikeCheese* Cheese = *It;
		if (Cheese->GetHealth() <= 0.0f || Cheese->IsFalling() || Cheese->IsActorBeingDestroyed())
		{
			continue;
		}
		const float* Until = GivenUp.Find(Cheese);
		if (Until != nullptr && *Until > Time)
		{
			continue;
		}
		const FBox Box = Cheese->GetComponentsBoundingBox();
		Costs.Add({CheeseStrikeMice::CheeseCost(FVector::Dist2D(Box.GetCenter(), Feet), Box.Min.Z - Feet.Z, Taken.Contains(Cheese)), Cheese});
	}
	Costs.Sort([](const TPair<float, ACheeseStrikeCheese*>& A, const TPair<float, ACheeseStrikeCheese*>& B) { return A.Key < B.Key; });
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	for (int32 Index = 0; Index < FMath::Min(Costs.Num(), 6); ++Index)
	{
		ACheeseStrikeCheese* Cheese = Costs[Index].Value;
		if (Cheese == Target.Get() && Path.Num() > 0)
		{
			return;
		}
		bool bWay = CanReach(Cheese);
		if (!bWay && Navigation != nullptr)
		{
			// To the ground beside it.
			const FBox Box = Cheese->GetComponentsBoundingBox();
			FNavLocation Beside;
			bWay = Navigation->ProjectPointToNavigation(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z), Beside, FVector(CheeseStrikeMice::ReachAcross, CheeseStrikeMice::ReachAcross, CheeseStrikeMice::ReachUp + 100.0f)) && FindWay(Beside.Location);
		}
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
		if (Distance <= FMath::Max(Step, PathIndex == Path.Num() - 1 ? 1.0f : PointReached))
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
	// From a step above its feet: it walks up a ramp's rise, not onto a crate it brushes past.
	if (!GetWorld()->LineTraceSingleByObjectType(Ground, Feet + FVector(0.0f, 0.0f, 60.0f), Feet - FVector(0.0f, 0.0f, 5000.0f), Objects, Params))
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
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Wanted, DeltaSeconds, TurnRate));
}

void ACheeseStrikeMouse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	const float Time = Now();
	Fear = CheeseStrikeMice::FearAfter(Fear, DeltaSeconds);
	switch (Doing)
	{
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
		if (Cheese == nullptr || Cheese->GetHealth() <= 0.0f || Cheese->IsActorBeingDestroyed() || Cheese->IsFalling() || Time >= NextChoiceAt)
		{
			NextChoiceAt = Time + ChooseEvery;
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
