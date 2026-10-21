// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "CheeseStrikeCameraManager.generated.h"

/**
 *  Basic First Person camera manager.
 *  Limits min/max look pitch, and widens or narrows the first-person view
 *  to the player's field of view (UCheeseStrikeSettings).
 */
UCLASS()
class ACheeseStrikeCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	ACheeseStrikeCameraManager();

protected:
	/** Through a character's eyes, the field of view the settings ask for, in the same proportion as the character's own (narrower while aiming). */
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
};
