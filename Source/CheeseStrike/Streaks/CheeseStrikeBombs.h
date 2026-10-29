// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "CheeseStrikeBombs.generated.h"

class ACheeseStrikePlayerState;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/** One wheel: where it lands, the server's time it does, and how far over that the roof is that it bursts through (0 under open sky). */
USTRUCT()
struct FCheeseStrikeDrop
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize Impact = FVector::ZeroVector;
	UPROPERTY()
	float LandsAt = 0.0f;
	UPROPERTY()
	float Roof = 0.0f;

	/** How far it falls, and how fast it starts: from the sky, or from under the roof it has come through (CheeseStrikeStreaks::RoofGap). */
	float FallHeight() const;
	float FallSpeed() const;
	float FallTime() const;
};

/**
 * The Cheese Strike and the Gouda Overload (CheeseStrikeStreaks).
 *
 * A Cheese Strike is gouda wheels falling from the sky over an area. The
 * server plans each wheel (where it lands, the first thing it meets coming
 * straight down, and when; indoors, its wheels come through the roof onto
 * the floor under it) and every machine is told; each shows the wheel
 * falling, tumbling, then where it lands a flash, a thump, a shockwave
 * swelling to exactly where its harm stops, and a scorch, while the
 * server harms whoever the burst can see within its radius (a wall or a
 * roof between them saves them), as the caller's kill. It is heralded by
 * a falling whistle where it was marked. Gone once the last has landed.
 *
 * The Gouda Overload is heralded by a siren everyone hears, and as its
 * warning ends the server splats every living enemy of its caller with
 * melted gouda, wherever they are (ACheeseStrikeCharacter::Splat: slowed,
 * their view smeared). The caller's haste and Gatling are the game mode's,
 * at the call.
 */
UCLASS(NotPlaceable)
class CHEESESTRIKE_API ACheeseStrikeBombs : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeBombs();

	/** Server only: `Caller`'s Cheese Strike, over the area round `Spot` (CheeseStrikeStreaks::StrikeArea), its ring of wheels turned to `Yaw`. */
	void StartStrike(ACheeseStrikePlayerState* Caller, const FVector& Spot, float Yaw);
	/** Server only: `Caller`'s Gouda Overload: the warning, then the gouda on their enemies. */
	void StartOverload(ACheeseStrikePlayerState* Caller);

	CheeseStrikeStreaks::EStreak GetKind() const { return static_cast<CheeseStrikeStreaks::EStreak>(Kind); }
	const TArray<FCheeseStrikeDrop>& GetDrops() const { return Drops; }
	/** Server only: how many wheels have landed, and how many players they have harmed (or the Overload splatted). */
	int32 GetLanded() const { return Landed; }
	int32 GetHits() const { return Hits; }
	/** The server's time the Overload's gouda splats; 0 for a Cheese Strike. */
	float GetSplatAt() const { return SplatAt; }

	/** Where a wheel dropped over `Over` lands: the first thing it meets coming straight down from high above. False over nothing. */
	static bool Landing(UWorld* World, const FVector& Over, FVector& OutImpact);
	/**
	 * Where a Cheese Strike's wheel for `Over` (a point of its area, at the
	 * mark's height) lands: under a roof (FCheeseStrikeOverhead), on the floor
	 * under that point, coming through the roof (OutRoof, its height over
	 * where it lands); else as Landing has it, OutRoof 0.
	 */
	static bool StrikeLanding(UWorld* World, const class FCheeseStrikeOverhead& Overhead, const FVector& Over, FVector& OutImpact, float& OutRoof);
	/**
	 * The ground at `Where` (world X and Y) for feet at `Height`: the first
	 * floor down from a head's height over them (the one they would stand
	 * on there, under any roof or deck), or, where that starts inside
	 * something or meets nothing, the first thing under the sky (Landing);
	 * players left out; false over nothing. What a Cheese
	 * Strike chosen on the minimap is marked on, at the chooser's feet's
	 * height (the floor they see the map for).
	 */
	static bool GroundNear(UWorld* World, const FVector2D& Where, float Height, FVector& OutGround);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Server only: the Overload's gouda on every living enemy of its caller, now. */
	void SplatEnemies();
public:
	/** How long one of the Overload's effects lasts: `Usual` (CheeseStrikeStreaks' GooSeconds, HasteSeconds, GatlingSeconds), or every one of them CheeseStrike.OverloadSeconds when that is set (the play tests). */
	static float OverloadTime(float Usual);
protected:
	/** Every machine: a wheel bursting through the roof at `At`: a puff, a crack, and a scorch left on the roof's underside. */
	void ShowThroughRoof(const FVector& At);
	/** Server only: the harm a wheel does where it lands. */
	void Burst(const FCheeseStrikeDrop& Drop);
	/** Every machine: the flash and the thump; `bIndoors` under a roof, where the eye has adjusted to the dark and a flash as bright as one in the sun would white the view out. */
	void ShowBurst(const FVector& At, bool bIndoors = false);
	/** Every machine: the warning, once, when its time comes. */
	void ShowWarning();
	float Now() const;

	/** CheeseStrikeStreaks::EStreak. */
	UPROPERTY(Replicated)
	uint8 Kind = 0;
	UPROPERTY(Replicated)
	TArray<FCheeseStrikeDrop> Drops;
	/** The server's time the warning begins, and where (the strike's mark); 0 before. */
	UPROPERTY(Replicated)
	float WarnAt = 0.0f;
	UPROPERTY(Replicated)
	FVector_NetQuantize WarnWhere = FVector::ZeroVector;
	UPROPERTY(Replicated)
	float SplatAt = 0.0f;

	/** Server only: whose it is, and what it has done. */
	TWeakObjectPtr<ACheeseStrikePlayerState> Caller;
	bool bSplatted = false;
	int32 Landed = 0;
	int32 Hits = 0;
	/** The server's time it is done with, once everything is planned. */
	float DoneAt = 0.0f;

	/** Each machine: how far it has shown each drop (0 not yet, 1 its whistle, 2 falling, 3 landed), whether it has come through its roof, and the falling wheels. */
	TArray<bool> Through;
	TArray<uint8> Shown;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Falling;
	bool bWarned = false;
	/** The bursts still glowing. */
	struct FFlash
	{
		TWeakObjectPtr<UPointLightComponent> Light;
		TWeakObjectPtr<UStaticMeshComponent> Ball;
		float At = 0.0f;
		/** How far its shockwave swells (cm), and how bright it starts. */
		float Reach = 0.0f;
		float Brightness = 0.0f;
	};
	TArray<FFlash> Flashes;

	UPROPERTY()
	TObjectPtr<UStaticMesh> WheelMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> BallMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BallMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WheelMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ScorchMaterial;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> Thumps;
	UPROPERTY()
	TObjectPtr<USoundBase> Rumble;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;
};
