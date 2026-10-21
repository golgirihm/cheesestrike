// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CheeseStrikeCurtain.generated.h"

class SWidget;

/**
 * The loading cover: when a level has loaded, a cover in the lobby's looks
 * (its dark tile, a spinner, and where the player is going) stays over the
 * view until what the player sees is ready, a page of the menu or the match
 * through their character (ACheeseStrikePlayerController::IsViewReady), or
 * for CurtainFor seconds at the most. Without it a joiner sees, for a
 * moment, the engine's view from the world's origin, before their
 * controller and character have come from the host. On a level the lobby
 * sent everyone to, it stays up on every player's screen until the match
 * begins, when all are ready, saying how many are ("Waiting for players
 * (1/2)").
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeCurtainSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The longest the cover stays up, in seconds: longer than a match waits for its players (CheeseStrikeRules::WaitForPlayers). */
	static constexpr float CurtainFor = 30.0f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool IsUp() const { return Curtain.IsValid(); }
	/** What the cover says now, or nothing when it is down. */
	FString GetSaying() const;

private:
	/** What the cover over `World` says: how many are ready while its match waits for them, or else `Otherwise`. */
	static FString SayingFor(const UWorld* World, const FString& Otherwise);
	void OnLevelLoaded(UWorld* World);
	bool Watch(float DeltaSeconds);
	void Lift();

	TSharedPtr<SWidget> Curtain;
	TWeakObjectPtr<UWorld> CoveredWorld;
	FTSTicker::FDelegateHandle Ticker;
	FDelegateHandle LoadedHandle;
	double UpSince = 0.0;
	/** Where the covered level is, as the cover says it. */
	FString GoingTo;
};
