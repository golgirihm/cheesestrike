// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeSmokeCloud.generated.h"

class UAudioComponent;
class UInstancedStaticMeshComponent;
class USoundAttenuation;
class USphereComponent;

/**
 * The Smoked Gouda's cloud (CheeseStrikeGrenades: EGrenadeKind::Smoke, the
 * Cloud* numbers): spawned by the server where the round went off, and
 * shown on every machine that draws: a hundred or so mist cards (the
 * blood's card in M_SmokeMist from tools/art/smoke_cloud.py: since wave 17
 * each one of eight soft, billowing puffs of smoke drawn from noise, lit
 * from the sky through their billows, each rolling slowly over on the view)
 * swirling slowly round an invisible sphere,
 * growing to about ten metres across in a second, lasting twelve and
 * thinning away over the last three, with a soft hiss while it smokes
 * (CheeseStrikeSynth::Hiss); and before its core, between it and this
 * machine's camera, a veil of solid cards (the same look, made solid by
 * their custom data 1: CheeseStrikeGrenades::CloudVeil), so that whoever
 * stands in its core is hidden whole from outside at any distance (wave
 * 16; from inside the core, the veil is just before the eye). The sphere blocks the Visibility channel and
 * nothing else: bots' sight (ACheeseStrikeBotController::CanSeeInto) and the
 * HUD's name tags are stopped by it, shots and movement are not (the shots'
 * traces leave every cloud out, IgnoreClouds), and the Unmanned Aerial
 * Cheese still reveals through it. Tested by
 * CheeseStrike.Play.ASmokeGrenadeHidesWhatIsBehindIt; the tests shorten it
 * with CheeseStrike.SmokeSeconds.
 */
UCLASS(NotPlaceable)
class CHEESESTRIKE_API ACheeseStrikeSmokeCloud : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeSmokeCloud();

	/** Server only, as it is spawned: it begins at the server's time `At`. */
	void Begin(float At);
	/** Seconds since it began, by the server's clock as this machine has it. */
	float Age() const;
	/** Whether it still blocks sight (CheeseStrikeGrenades::CloudBlocksSightAt). */
	bool BlocksSight() const;
	/** How long it lasts: CheeseStrikeGrenades::CloudLife, or CheeseStrike.SmokeSeconds for the tests. */
	static float Life();
	/** Leaves every cloud in `World` out of a trace, so that a shot or a burst goes through the smoke. */
	static void IgnoreClouds(FCollisionQueryParams& Params, const UWorld* World);
	/** The cards shown now (for the tests), the veil's, and the sphere. */
	int32 GetCardCount() const;
	int32 GetVeilCount() const;
	const USphereComponent* GetSight() const { return Sight; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Each card's own place in the swirl: its direction out from the middle (unit), how far out (a share of the radius), its size (a share), its turn about the up axis a second, and its roll on the view a second (radians: its billows turning slowly over). */
	struct FCard
	{
		FVector Out = FVector::ForwardVector;
		float Share = 1.0f;
		float Size = 1.0f;
		float Turn = 0.0f;
		float Phase = 0.0f;
		float Spin = 0.0f;
		float Shade = 1.0f;
	};
	void MakeCards();
	void ShowCards();
	void ShowVeil(const FVector& Camera, float At, float Opacity);

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Sight;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> Cards;
	/** The veil: a solid card over the core's outline as this machine's camera sees it, and lobes round its edge. */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> Veil;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Hiss;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;

	/** The server's time it began. */
	UPROPERTY(Replicated)
	float BeganAt = 0.0f;
	TArray<FCard> Swirl;
	TArray<FTransform> Shown;
	TArray<FTransform> VeilShown;
	float CardHalf = 50.0f;
	bool bLooksLoaded = false;
	bool bSightOn = true;
};
