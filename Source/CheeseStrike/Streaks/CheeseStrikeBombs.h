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

/** One wheel: where it lands, and the server's time it does. */
USTRUCT()
struct FCheeseStrikeDrop
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize Impact = FVector::ZeroVector;
	UPROPERTY()
	float LandsAt = 0.0f;
};

/**
 * Gouda wheels falling from the sky: a Cheese Strike's run along a line,
 * or the Gouda Overload's rain on every enemy (CheeseStrikeStreaks). The
 * server plans each wheel (where it lands, the first thing it meets coming
 * straight down, and when) and every machine is told; each shows the wheel
 * falling, tumbling, then where it lands a flash, a thump, a shockwave
 * swelling to exactly where its harm stops, and a scorch, while the
 * server harms whoever the burst can see within its radius (a wall or a
 * roof between them saves them), as the caller's kill. A Cheese Strike is
 * heralded by a falling whistle where it was marked; the Overload by a
 * siren everyone hears, and then a whistle over each wheel. Gone once the
 * last has landed.
 */
UCLASS(NotPlaceable)
class CHEESESTRIKE_API ACheeseStrikeBombs : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeBombs();

	/** Server only: `Caller`'s Cheese Strike, along the line through `Spot` that `Yaw` faces. */
	void StartStrike(ACheeseStrikePlayerState* Caller, const FVector& Spot, float Yaw);
	/** Server only: `Caller`'s Gouda Overload: the warning, then the rain on their enemies. */
	void StartOverload(ACheeseStrikePlayerState* Caller);

	CheeseStrikeStreaks::EStreak GetKind() const { return static_cast<CheeseStrikeStreaks::EStreak>(Kind); }
	const TArray<FCheeseStrikeDrop>& GetDrops() const { return Drops; }
	/** Server only: how many wheels have landed, and how many players they have harmed. */
	int32 GetLanded() const { return Landed; }
	int32 GetHits() const { return Hits; }
	/** The server's time the rain of the Overload begins and ends; 0 for a Cheese Strike. */
	float GetRainFrom() const { return RainFrom; }
	float GetRainUntil() const { return RainUntil; }

	/** Where a wheel dropped over `Over` lands: the first thing it meets coming straight down from high above. False over nothing. */
	static bool Landing(UWorld* World, const FVector& Over, FVector& OutImpact);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Server only: a wheel over `Over` that lands at `LandsAt`. */
	void AddDrop(const FVector& Over, float LandsAt);
	/** Server only: the harm a wheel does where it lands. */
	void Burst(const FCheeseStrikeDrop& Drop);
	/** Every machine: the flash and the thump. */
	void ShowBurst(const FVector& At);
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
	float RainFrom = 0.0f;
	UPROPERTY(Replicated)
	float RainUntil = 0.0f;

	/** Server only: whose it is, and what it has done. */
	TWeakObjectPtr<ACheeseStrikePlayerState> Caller;
	float NextRainAt = 0.0f;
	int32 Landed = 0;
	int32 Hits = 0;
	/** The server's time it is done with, once everything is planned. */
	float DoneAt = 0.0f;

	/** Each machine: how far it has shown each drop (0 not yet, 1 its whistle, 2 falling, 3 landed), and the falling wheels. */
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
