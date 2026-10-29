// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Rules/CheeseStrikeAchievements.h"
#include "CheeseStrikeRecords.generated.h"

/**
 * A player's records on their own machine: the counts the achievements watch
 * (Rules/CheeseStrikeAchievements.h) and the achievements already unlocked,
 * kept in the save game slot "Records".
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeRecords : public USaveGame
{
	GENERATED_BODY()

public:
	/** The save game slot (the automation tests use a slot of their own, so that they never touch a player's). */
	static FString SlotName();

	/** Each stat's count, by its name (CheeseStrikeAchievements::Stat, ModeStat, StreakStat). */
	UPROPERTY(SaveGame)
	TMap<FString, int32> Stats;
	/** The ids of the achievements unlocked, in the order they were. */
	UPROPERTY(SaveGame)
	TArray<FString> Unlocked;
};

/**
 * Keeps this machine's player's records (UCheeseStrikeRecords): loaded as the
 * game starts, counted as the server tells of what the player did
 * (ACheeseStrikePlayerController::ClientRecord), saved at each match's end and
 * as the game closes. An achievement whose stat reaches its threshold is
 * unlocked (OnUnlocked): logged, shown on the HUD for a few seconds
 * ("ACHIEVEMENT: HEAD CHEESE", GetToast) and handed to Steam (SetOnSteam, a
 * stub until the game has an App ID). `CheeseStrikeStats` on the console lists
 * them all.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeRecordsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** What the server said the player did: adds one to each of its stats (CheeseStrikeAchievements::StatsFor), unlocks what that earns, and at a match's end saves. */
	void Record(CheeseStrikeAchievements::ERecord Record, uint8 Detail);
	/** Adds `Amount` to `Stat` and unlocks what that earns (Record's own step; development's CheeseStrikeRecord). */
	void Add(const FString& Stat, int32 Amount);
	/** Writes the records to their slot. */
	void Save();

	int32 GetStat(const FString& Stat) const;
	bool IsUnlocked(const FString& Id) const;
	const UCheeseStrikeRecords* GetRecords() const { return Records; }

	/** The latest achievement unlocked within its toast's time of `Now` (platform seconds) and how long ago, or null. */
	const CheeseStrikeAchievements::FAchievement* GetToast(double Now, float& OutSince) const;
	/** Every achievement with its progress, a line each, for the console (CheeseStrikeStats). */
	TArray<FString> Describe() const;

protected:
	/** An achievement unlocked: logged, kept, shown and handed to Steam. */
	void OnUnlocked(const CheeseStrikeAchievements::FAchievement& Achievement);
	/**
	 * Steam's side of an unlock: a stub until the game has an App ID and its
	 * achievements are made on Steamworks with the same ids
	 * (docs/unreal/design.md, "Achievements"). Then, on Steam
	 * (UCheeseStrikeAvatars::UsesSteam): SteamUserStats()->SetAchievement(Id)
	 * and StoreStats(), after RequestCurrentStats has answered.
	 */
	void SetOnSteam(const CheeseStrikeAchievements::FAchievement& Achievement);

	UPROPERTY(Transient)
	TObjectPtr<UCheeseStrikeRecords> Records;
	/** The toast's achievement and when it was unlocked (platform seconds). */
	FString ToastId;
	double ToastAt = -1000.0;
};
