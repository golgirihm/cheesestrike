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
 * blood's card, in a smoky grey with a faint gouda tint, M_SmokeMist from
 * tools/art/smoke_cloud.py) swirling slowly round an invisible sphere,
 * growing to about five metres across in a second, lasting twelve and
 * thinning away over the last three, with a soft hiss while it smokes
 * (CheeseStrikeSynth::Hiss). The sphere blocks the Visibility channel and
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
	/** The cards shown now (for the tests), and the sphere. */
	int32 GetCardCount() const;
	const USphereComponent* GetSight() const { return Sight; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Each card's own place in the swirl: its direction out from the middle (unit), how far out (a share of the radius), its size (a share) and its turn about the up axis a second. */
	struct FCard
	{
		FVector Out = FVector::ForwardVector;
		float Share = 1.0f;
		float Size = 1.0f;
		float Turn = 0.0f;
		float Phase = 0.0f;
	};
	void MakeCards();
	void ShowCards();

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Sight;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UInstancedStaticMeshComponent> Cards;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Hiss;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;

	/** The server's time it began. */
	UPROPERTY(Replicated)
	float BeganAt = 0.0f;
	TArray<FCard> Swirl;
	TArray<FTransform> Shown;
	float CardHalf = 50.0f;
	bool bLooksLoaded = false;
	bool bSightOn = true;
};
