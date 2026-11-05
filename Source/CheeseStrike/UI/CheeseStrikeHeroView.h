// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeHeroView.generated.h"

class ACheeseStrikeMenuFigure;
class UPointLightComponent;
class USceneCaptureComponent2D;
class USpotLightComponent;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/** The loadout page's picture of the hero (ACheeseStrikeHeroView), as plain numbers. */
namespace CheeseStrikeHeroView
{
	/** The picture's size in pixels: upright, a little over half as wide as it is tall, sharp at 1440p. */
	constexpr int32 PictureWidth = 520;
	constexpr int32 PictureHeight = 900;
	/** How much of the picture's height the hero (feet to the top of the head, about 1.8 m) takes: the whole of them, with a little fog round. */
	constexpr float HeroShare = 0.8f;
	/** How tall a hero stands (cm), and how far the camera is from them. */
	constexpr float HeroHeight = 185.0f;
	constexpr float CameraDistance = 600.0f;
	/** The figure's yaw as the view begins (facing the camera, turned a little), and how fast it turns, as a share of the front end's figure's rate. */
	constexpr float FacingYaw = -150.0f;
	constexpr float TurnShare = 0.4f;
	/** The vertical field of view that fits a hero `HeroHeight` tall at `HeroShare` of the picture from `Distance`, in degrees; and the horizontal one for a picture `Aspect` wide to its height (what the capture takes). */
	float VerticalFieldOfView(float Distance);
	float HorizontalFieldOfView(float Vertical, float Aspect);
}

/**
 * The hero view of the loadout page (SCheeseStrikeMenu::MakeLoadout), wherever it is opened (the front end, over the
 * lobby, a count-in): the loadout's hero holding its primary, whole from head to foot and turning slowly, against a plain
 * soft fog of white and grey (M_HeroBackdrop, tools/art/hero_backdrop.py) and nothing else of any level. A figure of its
 * own (ACheeseStrikeMenuFigure, its pedestal left out) stands high over the level (StageAt), lit by lights of its own,
 * inside a sphere of the fog, and a scene capture draws only those into a picture (GetPicture) that the page shows. Made
 * on the player's machine for its own menu, never replicated; it captures only while shown (SetShown).
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeHeroView : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeHeroView();

	/** Where it stands, far over any level. */
	static const FVector StageAt;
	/** The fog sphere's look, loaded by path as the view begins (so that the script can rebuild it). */
	static constexpr const TCHAR* BackdropPath = TEXT("/Game/CheeseStrike/UI/M_HeroBackdrop.M_HeroBackdrop");

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Wears hero `Hero` and holds primary `Id`, as the front end's figure does. */
	void Show(int32 Hero, FName Id);
	/** Captures (and its figure shows) while the loadout page is up; neither while it isn't. */
	void SetShown(bool bShown);
	bool IsShown() const { return bShown; }
	/** The picture it captures into, which the page draws. */
	UTextureRenderTarget2D* GetPicture() const { return Picture; }
	const ACheeseStrikeMenuFigure* GetFigure() const { return Figure; }

protected:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Stage;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneCaptureComponent2D> Capture;
	/** A sphere round the figure and the camera, the fog drawn on its inside. */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Backdrop;
	/** A soft key from the front left, a fill from the right, a cool rim from behind. */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USpotLightComponent> Key;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Fill;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USpotLightComponent> Rim;
	UPROPERTY()
	TObjectPtr<ACheeseStrikeMenuFigure> Figure;
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> Picture;
	bool bShown = false;
};
