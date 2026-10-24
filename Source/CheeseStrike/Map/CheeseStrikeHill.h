// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rules/CheeseStrikeHills.h"
#include "Rules/CheeseStrikeRules.h"
#include "CheeseStrikeHill.generated.h"

class ACheeseStrikePlayerState;
class AGameStateBase;
class APlayerState;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Where King of the Hill's hill may stand: a spot on the floor that a map's
 * script places (tools/art/map_kit.py, hill_spot), in the order the hill
 * goes round them, with the hill's radius there. It draws nothing; the hill
 * (ACheeseStrikeHill) does.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeHillSpot : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeHillSpot();

	int32 GetOrder() const { return Order; }
	float GetRadius() const { return Radius; }

protected:
	/** Its place in the round the hill goes, from 0. */
	UPROPERTY(EditAnywhere, Category="Hill")
	int32 Order = 0;
	/** The hill's radius here (cm). */
	UPROPERTY(EditAnywhere, Category="Hill")
	float Radius = CheeseStrikeHills::Radius;
};

/**
 * King of the Hill's hill, one a match: spawned by the game mode as the
 * match begins, it goes round the map's spots (ACheeseStrikeHillSpot), a
 * minute at each (CheeseStrikeHills::HillTime), warning before it moves,
 * and on the server scores for whoever holds it (CheeseStrikeHill: a side
 * whose players alone stand in it, or in the solo mode one player alone),
 * counts each player's time in it, announces it being captured and moving,
 * and ends the match when the points are reached
 * (ACheeseStrikeGameMode::HillWon). Where it is, where it goes next, when,
 * and who holds it replicate to everyone.
 *
 * On every machine it is a glowing ring on the floor with a column of light
 * rising from it (M_Hill, tools/art/hill_material.py), coloured for the
 * player looking: the cheese's pale yellow while nobody holds it, the side's
 * colour that holds it (in the solo mode green for the player's own hold, red
 * for anyone else's), flickering white while contested; and, while it warns
 * that it is moving, a fainter ring where it goes. The HUD marks it on the
 * screen and the minimap (UI/CheeseStrikeHillHUD).
 */
UCLASS(NotPlaceable)
class CHEESESTRIKE_API ACheeseStrikeHill : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeHill();

	/** The match's hill, if it has one. */
	static ACheeseStrikeHill* Find(const UWorld* World);
	/**
	 * The level's hill spots in their order, with their radii (each on the
	 * floor, as its map's script puts it); for a level with none, one on the
	 * floor by the start nearest the middle of them all.
	 */
	static void GatherSpots(UWorld* World, TArray<FVector>& OutSpots, TArray<float>& OutRadii);

	/** Server only: the spots to go round, in order, the first standing from now and moving on at `FirstMovesAt` (the server's time). */
	void Begin(const TArray<FVector>& Spots, const TArray<float>& Radii, float FirstMovesAt);
	/** Server only: moves on to the next spot now (the tests', and the console's CheeseStrikeMoveHill). */
	void MoveNow();

	bool HasBegun() const { return Centres.Num() > 0; }
	int32 NumSpots() const { return Centres.Num(); }
	int32 GetSpotIndex() const { return Spot; }
	int32 GetNextIndex() const { return Next; }
	/** Where the hill is (its middle, on the floor) and how big; and where it goes next. */
	FVector GetCentre() const { return Centres.IsValidIndex(Spot) ? Centres[Spot] : GetActorLocation(); }
	float GetRadius() const { return Radii.IsValidIndex(Spot) ? Radii[Spot] : CheeseStrikeHills::Radius; }
	FVector GetNextCentre() const { return Centres.IsValidIndex(Next) ? Centres[Next] : GetCentre(); }
	float GetNextRadius() const { return Radii.IsValidIndex(Next) ? Radii[Next] : GetRadius(); }
	/** Seconds until it moves, and whether it is warning that it will (CheeseStrikeHills::IsWarning). */
	float SecondsToMove() const;
	bool IsWarning() const { return NumSpots() > 1 && CheeseStrikeHills::IsWarning(SecondsToMove()); }
	/** How many times it has moved. */
	int32 GetMoves() const { return Moves; }

	/** Who holds it: by side in a team mode, by player id in the solo mode. */
	CheeseStrikeHills::FHold GetHold() const;
	/** Its holder in words: a side's name or a player's; empty unless held. */
	const FString& GetHolderName() const { return HolderName; }
	/** Whether `Viewer` (or their side) is the holder. */
	bool IsHeldBy(const APlayerState* Viewer) const;
	/** Whether someone whose feet are at `Feet` stands in it. */
	bool Contains(const FVector& Feet) const { return HasBegun() && CheeseStrikeHills::IsInside(Feet, GetCentre(), GetRadius()); }
	/** Whether `Actor` (a character) stands in it. */
	bool ContainsActor(const AActor* Actor) const;

	/** Its colour for `Viewer` at the world's time `Time`: neutral, a side's (or the viewer's own, or another's), or contested. */
	FLinearColor ColourFor(const APlayerState* Viewer, float Time) const;
	/** The colour nobody holding it shows: a grey, unlike any side's (CheeseStrikeModes::TeamColour). */
	static const FLinearColor Neutral;
	/** The solo mode's colour for the viewer's own hold, and for anyone else's. */
	static const FLinearColor Own;
	static const FLinearColor Other;

	/** Solo King of the Hill's scoreboard: everyone's hill points as their kills, in order. */
	static TArray<CheeseStrikeRules::FStanding> SoloStandings(const AGameStateBase* Match);

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Server: who stands in it, the points their holding earns, their time in it, the announcements, and moving on. */
	void Score(float Seconds);
	/** Server: on to the next spot, starting its minute at `MovesFrom`. */
	void MoveOn(float MovesFrom);
	/** Every machine: the ring, the column and the light where the hill is, in the viewer's colour, and the next ring while it warns. */
	void UpdateLook();

	UPROPERTY(VisibleAnywhere, Category="Hill")
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category="Hill")
	TObjectPtr<UStaticMeshComponent> Ring;
	UPROPERTY(VisibleAnywhere, Category="Hill")
	TObjectPtr<UStaticMeshComponent> Column;
	UPROPERTY(VisibleAnywhere, Category="Hill")
	TObjectPtr<UStaticMeshComponent> NextRing;
	UPROPERTY(VisibleAnywhere, Category="Hill")
	TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingLook;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ColumnLook;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NextLook;

	UPROPERTY(Replicated)
	TArray<FVector> Centres;
	UPROPERTY(Replicated)
	TArray<float> Radii;
	UPROPERTY(Replicated)
	int32 Spot = 0;
	UPROPERTY(Replicated)
	int32 Next = 0;
	/** The server's time it moves on. */
	UPROPERTY(Replicated)
	float MovesAt = 0.0f;
	UPROPERTY(Replicated)
	int32 Moves = 0;
	/** CheeseStrikeHills::EHold, and the holder (a side, or a player's id). */
	UPROPERTY(Replicated)
	uint8 HoldState = 0;
	UPROPERTY(Replicated)
	int32 Holder = INDEX_NONE;
	UPROPERTY(Replicated)
	FString HolderName;

	/** Server: the points' clock, the time since it last looked at who is in it, the holder last announced as capturing it, and whether this spot's warning has been given. */
	CheeseStrikeHills::FClock Clock;
	float Pending = 0.0f;
	CheeseStrikeHills::FHold Announced;
	bool bWarned = false;
};
