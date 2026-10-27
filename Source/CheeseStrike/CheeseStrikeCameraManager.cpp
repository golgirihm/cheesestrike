// Copyright Epic Games, Inc. All Rights Reserved.


#include "CheeseStrikeCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "CheeseStrikeCharacter.h"
#include "Settings/CheeseStrikeSettings.h"
#include "Weapons/CheeseStrikeWeapon.h"
#include "CheeseStrike.h"

static TAutoConsoleVariable<int32> CVarTraceView(TEXT("CheeseStrike.TraceView"), 0, TEXT("Log, every frame, where the weapon in hand is in its player's view (2: and where the arms' bones are)."));

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
	// Leaning into a slide.
	ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(OutVT.Target);
	OutVT.POV.Rotation.Roll += Character->GetViewRoll();
	// A scoped rifle's breathing.
	const FRotator Sway = Character->GetScopeSway();
	OutVT.POV.Rotation.Pitch += Sway.Pitch;
	OutVT.POV.Rotation.Yaw += Sway.Yaw;
	// Where the weapon in hand is in the view, for how steadily it is held (and for the trace).
	const UPrimitiveComponent* Model = Character->GetWeapon() != nullptr ? Character->GetWeapon()->GetModel(true) : nullptr;
	if (Model == nullptr)
	{
		return;
	}
	const FTransform View(OutVT.POV.Rotation, OutVT.POV.Location);
	const FTransform InView = Model->GetComponentTransform().GetRelativeTransform(View);
	Character->NoteWeaponInView(InView);
	if (CVarTraceView.GetValueOnGameThread() != 0)
	{
		const FVector Body = View.InverseTransformPosition(Character->GetFirstPersonMesh()->GetComponentLocation());
		UE_LOG(LogCheeseStrike, Display, TEXT("View: weapon at (%.3f, %.3f, %.3f) turned (%.3f, %.3f, %.3f), body at (%.3f, %.3f, %.3f), eye (%.2f, %.2f, %.2f) dt %.4f"),
			InView.GetLocation().X, InView.GetLocation().Y, InView.GetLocation().Z, InView.Rotator().Pitch, InView.Rotator().Yaw, InView.Rotator().Roll,
			Body.X, Body.Y, Body.Z, OutVT.POV.Location.X, OutVT.POV.Location.Y, OutVT.POV.Location.Z, DeltaTime);
		if (CVarTraceView.GetValueOnGameThread() > 1)
		{
			FString Bones;
			for (const TCHAR* Name : {TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), TEXT("spine_03"), TEXT("neck_01"), TEXT("head")})
			{
				const FVector At = View.InverseTransformPosition(Character->GetFirstPersonMesh()->GetBoneLocation(Name));
				Bones += FString::Printf(TEXT(" %s (%.1f, %.1f, %.1f)"), Name, At.X, At.Y, At.Z);
			}
			UE_LOG(LogCheeseStrike, Display, TEXT("View bones:%s"), *Bones);
		}
	}
}
