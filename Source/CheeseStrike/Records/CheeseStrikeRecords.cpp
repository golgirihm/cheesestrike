// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "Records/CheeseStrikeRecords.h"
#include "CheeseStrike.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

FString UCheeseStrikeRecords::SlotName()
{
	return GIsAutomationTesting ? TEXT("RecordsTest") : TEXT("Records");
}

void UCheeseStrikeRecordsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Records = Cast<UCheeseStrikeRecords>(UGameplayStatics::LoadGameFromSlot(UCheeseStrikeRecords::SlotName(), 0));
	if (Records == nullptr)
	{
		Records = Cast<UCheeseStrikeRecords>(UGameplayStatics::CreateSaveGameObject(UCheeseStrikeRecords::StaticClass()));
	}
}

void UCheeseStrikeRecordsSubsystem::Deinitialize()
{
	Save();
	Super::Deinitialize();
}

void UCheeseStrikeRecordsSubsystem::Record(CheeseStrikeAchievements::ERecord Record, uint8 Detail)
{
	for (const FString& Stat : CheeseStrikeAchievements::StatsFor(Record, Detail))
	{
		Add(Stat, 1);
	}
	if (Record == CheeseStrikeAchievements::ERecord::MatchPlayed)
	{
		Save();
	}
}

void UCheeseStrikeRecordsSubsystem::Add(const FString& Stat, int32 Amount)
{
	if (Records == nullptr || Stat.IsEmpty() || Amount == 0)
	{
		return;
	}
	const TMap<FString, int32> Before = Records->Stats;
	int32& Count = Records->Stats.FindOrAdd(Stat);
	Count = FMath::Max(0, Count + Amount);
	UE_LOG(LogCheeseStrike, Verbose, TEXT("Records: %s %d"), *Stat, Count);
	for (const FString& Id : CheeseStrikeAchievements::NewlyUnlocked(Before, Records->Stats))
	{
		if (!Records->Unlocked.Contains(Id))
		{
			if (const CheeseStrikeAchievements::FAchievement* Achievement = CheeseStrikeAchievements::Find(Id))
			{
				OnUnlocked(*Achievement);
			}
		}
	}
}

void UCheeseStrikeRecordsSubsystem::Save()
{
	if (Records != nullptr)
	{
		UGameplayStatics::SaveGameToSlot(Records, UCheeseStrikeRecords::SlotName(), 0);
	}
}

int32 UCheeseStrikeRecordsSubsystem::GetStat(const FString& Stat) const
{
	return Records != nullptr ? CheeseStrikeAchievements::Value(Records->Stats, Stat) : 0;
}

bool UCheeseStrikeRecordsSubsystem::IsUnlocked(const FString& Id) const
{
	return Records != nullptr && Records->Unlocked.Contains(Id);
}

const CheeseStrikeAchievements::FAchievement* UCheeseStrikeRecordsSubsystem::GetToast(double Now, float& OutSince) const
{
	OutSince = static_cast<float>(Now - ToastAt);
	if (ToastId.IsEmpty() || OutSince < 0.0f || OutSince > CheeseStrikeAchievements::ToastSeconds)
	{
		return nullptr;
	}
	return CheeseStrikeAchievements::Find(ToastId);
}

TArray<FString> UCheeseStrikeRecordsSubsystem::Describe() const
{
	TArray<FString> Lines;
	for (const CheeseStrikeAchievements::FAchievement& Each : CheeseStrikeAchievements::All())
	{
		const int32 Value = GetStat(Each.Stat);
		Lines.Add(FString::Printf(TEXT("%s %-24s %-30s %d/%d  %s"), IsUnlocked(Each.Id) ? TEXT("[x]") : TEXT("[ ]"), *Each.Id, *Each.Name, FMath::Min(Value, Each.Threshold), Each.Threshold, *Each.Description));
	}
	return Lines;
}

void UCheeseStrikeRecordsSubsystem::OnUnlocked(const CheeseStrikeAchievements::FAchievement& Achievement)
{
	Records->Unlocked.Add(Achievement.Id);
	UE_LOG(LogCheeseStrike, Display, TEXT("Achievement unlocked: %s (%s): %s"), *Achievement.Name, *Achievement.Id, *Achievement.Description);
	ToastId = Achievement.Id;
	ToastAt = FPlatformTime::Seconds();
	SetOnSteam(Achievement);
}

void UCheeseStrikeRecordsSubsystem::SetOnSteam(const CheeseStrikeAchievements::FAchievement& Achievement)
{
	// To fill in once the game has its App ID and these achievements exist on Steamworks (their API names are the ids).
	UE_LOG(LogCheeseStrike, Display, TEXT("Achievement %s: not sent to Steam (no App ID yet)"), *Achievement.Id);
}
