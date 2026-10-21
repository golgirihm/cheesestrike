// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeWatchCamera.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Input/CheeseStrikeInput.h"

ACheeseStrikeWatchCamera::ACheeseStrikeWatchCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the characters have moved, so that the view keeps up with the one watched.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;
	Camera->SetFieldOfView(90.0f);
	SetReplicates(false);
}

void ACheeseStrikeWatchCamera::Watch(APlayerController* InPlayer)
{
	Player = InPlayer;
	ShowAt = (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f) + Delay;
	if (InPlayer == nullptr)
	{
		return;
	}
	if (InPlayer->PlayerCameraManager != nullptr)
	{
		SetActorLocationAndRotation(InPlayer->PlayerCameraManager->GetCameraLocation(), InPlayer->PlayerCameraManager->GetCameraRotation());
	}
	// Fire goes on to the next teammate.
	EnableInput(InPlayer);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	const UCheeseStrikeInputSet* Set = UCheeseStrikeInputSet::Get(this);
	if (Input != nullptr && Set != nullptr)
	{
		Input->BindAction(Set->Fire, ETriggerEvent::Started, this, &ACheeseStrikeWatchCamera::NextTeammate);
	}
}

void ACheeseStrikeWatchCamera::Stop()
{
	if (APlayerController* Viewer = Player.Get())
	{
		if (Viewer->GetViewTarget() == this)
		{
			Viewer->SetViewTarget(Viewer->GetPawn() != nullptr ? static_cast<AActor*>(Viewer->GetPawn()) : static_cast<AActor*>(Viewer));
		}
		DisableInput(Viewer);
	}
	Destroy();
}

TArray<ACheeseStrikeCharacter*> ACheeseStrikeWatchCamera::Teammates() const
{
	TArray<ACheeseStrikeCharacter*> Found;
	const APlayerController* Viewer = Player.Get();
	const ACheeseStrikePlayerState* Mine = Viewer != nullptr ? Viewer->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Mine == nullptr || Match == nullptr)
	{
		return Found;
	}
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		const APlayerState* Theirs = It->GetPlayerState();
		if (It->IsAlive() && Theirs != nullptr && Theirs != Mine && Match->AreTeammates(Mine, Theirs))
		{
			Found.Add(*It);
		}
	}
	Found.Sort([](const ACheeseStrikeCharacter& A, const ACheeseStrikeCharacter& B) { return A.DisplayName() < B.DisplayName(); });
	return Found;
}

void ACheeseStrikeWatchCamera::NextTeammate()
{
	const TArray<ACheeseStrikeCharacter*> Living = Teammates();
	if (Living.Num() == 0)
	{
		return;
	}
	const int32 Now = Living.IndexOfByKey(Watched.Get());
	Watched = Living[(Now + 1) % Living.Num()];
}

void ACheeseStrikeWatchCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* Viewer = Player.Get();
	if (Viewer == nullptr)
	{
		Destroy();
		return;
	}
	// Back in: a character of their own again.
	const ACheeseStrikeCharacter* Own = Cast<ACheeseStrikeCharacter>(Viewer->GetPawn());
	if (Own != nullptr && Own->IsAlive())
	{
		Stop();
		return;
	}
	if (GetWorld()->GetTimeSeconds() < ShowAt)
	{
		return;
	}
	if (!Watched.IsValid() || !Watched->IsAlive())
	{
		Watched = nullptr;
		NextTeammate();
	}
	if (const ACheeseStrikeCharacter* Them = Watched.Get())
	{
		const FVector Eyes = Them->GetEyes()->GetComponentLocation();
		FRotator Aim = Them->GetBaseAimRotation();
		Aim.Pitch = FMath::ClampAngle(Aim.Pitch, -50.0f, 50.0f);
		FVector Wanted = Eyes - Aim.Vector() * Behind + FVector(0.0f, 0.0f, Above);
		// Not through a wall: as far back as there is room for.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeWatch), false);
		for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, Eyes, Wanted, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.0f), Params))
		{
			Wanted = Hit.Location;
		}
		const bool bFirst = !bShowing;
		SetActorLocationAndRotation(bFirst ? Wanted : FMath::VInterpTo(GetActorLocation(), Wanted, DeltaSeconds, 12.0f), bFirst ? Aim : FMath::RInterpTo(GetActorRotation(), Aim, DeltaSeconds, 10.0f));
	}
	if (!bShowing)
	{
		bShowing = true;
		Viewer->SetViewTargetWithBlend(this, 0.4f);
	}
}
