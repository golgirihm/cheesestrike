// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikePodium.generated.h"

class ACheeseStrikeGameState;
class APlayerController;
class APlayerState;
class UAnimSequenceBase;
class UCameraComponent;
class UMaterialInterface;
class USkeletalMeshComponent;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UCanvas;
class USceneCaptureComponent2D;
class UTexture2D;
class UTextureRenderTarget2D;

/** One who stands on the podium, as every machine works it out from the replicated player states. */
struct FPodiumPlace
{
	FString Name;
	/** 0 for the first. */
	int32 Place = 0;
	/** CheeseStrikeBodies::All's index of their hero, and the side whose colours it wears (INDEX_NONE for its own). */
	int32 Body = 0;
	int32 Team = INDEX_NONE;
	/** Their score in the mode's words: "12 KILLS", "STEP 18", "100 POINTS". */
	FString Score;
	bool bBot = false;
	/** Whether it is the player on this machine. */
	bool bMine = false;
};

/**
 * The podium after a match: a small stage of its own, high over the level
 * (out of its fog and its light, behind walls of its own), the top three
 * standing on pedestals of stacked wheels of cheese, the first in the middle
 * and highest, the second to its left and the third to its right, each its
 * own hero in its side's colours doing one of its own emotes
 * (CheeseStrikePodiums::Emote), lit by a spotlight each and seen by a camera
 * of the stage's. Made on each machine for its own player when the podium
 * comes (ACheeseStrikePlayerController::ShowPodium), never replicated; the
 * HUD writes the names, places and scores over it (UI/CheeseStrikePodiumHUD).
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikePodium : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikePodium();

	/** Where the stage is: far over any level. */
	static const FVector StageAt;

	/** Who stands on the podium as `Match` stands now, first first, at most three; `Mine` is this machine's player. */
	static TArray<FPodiumPlace> PlacesFor(const ACheeseStrikeGameState* Match, const APlayerState* Mine);

	/** Puts `Who` on the podium (and nobody on the places past them). */
	void Stage(const TArray<FPodiumPlace>& Who);
	const TArray<FPodiumPlace>& GetPlaces() const { return Places; }

	/** The middle of the front of place `Place`'s pedestal, under its hero's feet, and the top of the hero's head, in the world. */
	FVector PedestalFront(int32 Place) const;
	FVector HeadOf(int32 Place) const;
	/** Whether place `Place`'s hero is playing its emote. */
	bool IsEmoting(int32 Place) const;

	/**
	 * The picture held on the way back to the lobby (UCheeseStrikePostGameSubsystem::Hold): the stage seen by its
	 * camera, `Size` big, into a render target made in `Outer` (which keeps it once the level is gone), taken every
	 * frame from StartPicture until FinishPicture, which draws `DrawWords` (the HUD's words over the stage) on the last.
	 * The view's own pixels aren't to be had in every game (a viewport drawn straight to the screen, or offscreen).
	 */
	void StartPicture(UObject* Outer, const FIntPoint& Size);
	UTextureRenderTarget2D* FinishPicture(TFunctionRef<void(UCanvas*, const FIntPoint&)> DrawWords);
	/** Where `World` is in a picture `Size` big by the stage's camera; false if behind it. */
	bool ProjectInPicture(const FVector& World, const FIntPoint& Size, FVector2D& Out) const;

	/** The cheese glyph of a number, 1 to 9 (UI/Countdown), for the places. */
	UTexture2D* Glyph(int32 Number) const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** The camera's field of view across for a view `Aspect` wide to its height: the stage always fits. */
	static float FieldOfViewFor(float Aspect);

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> Picture;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TArray<TObjectPtr<USkeletalMeshComponent>> Heroes;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Pedestals;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TArray<TObjectPtr<USpotLightComponent>> Spots;

	/** Every hero's emotes, CheeseStrikePodiums::Places to a body, in CheeseStrikeBodies::All's order. */
	UPROPERTY()
	TArray<TObjectPtr<UAnimSequenceBase>> Emotes;
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMesh>> BodyMeshes;
	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> Glyphs;

private:
	TArray<FPodiumPlace> Places;
	/** Where each place's pedestal stands and how high its top is, in the actor's space. */
	FVector SlotAt(int32 Place) const;
};
