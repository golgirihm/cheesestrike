// Copyright Epic Games, Inc. All Rights Reserved.


#include "CheeseStrikeCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "CheeseStrikeCharacter.h"
#include "Settings/CheeseStrikeSettings.h"

ACheeseStrikeCameraManager::ACheeseStrikeCameraManager()
{
	// set the min/max pitch
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;
}

void ACheeseStrikeCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);
	if (Cast<ACheeseStrikeCharacter>(OutVT.Target) == nullptr)
	{
		return;
	}
	// The character's camera keeps the engine's usual field of view across (narrowing it to aim); the settings' is top to
	// bottom, so it is turned into one across for this view's shape, and the character's taken as a share of the usual.
	FVector2D Size(16.0, 9.0);
	const ULocalPlayer* Local = PCOwner != nullptr ? PCOwner->GetLocalPlayer() : nullptr;
	if (Local != nullptr && Local->ViewportClient != nullptr)
	{
		Local->ViewportClient->GetViewportSize(Size);
	}
	const float Aspect = Size.Y > 0.0 ? static_cast<float>(Size.X / Size.Y) : 16.0f / 9.0f;
	const float Usual = GetDefault<UCameraComponent>()->FieldOfView;
	const float Wanted = CheeseStrikeSettings::HorizontalFieldOfView(UCheeseStrikeSettings::FieldOfViewNow(), Aspect);
	OutVT.POV.FOV = FMath::Clamp(OutVT.POV.FOV * Wanted / FMath::Max(1.0f, Usual), 5.0f, 170.0f);
}
