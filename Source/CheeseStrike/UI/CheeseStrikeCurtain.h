// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CheeseStrikeCurtain.generated.h"

class SWidget;
struct FWorldContext;

/**
 * The loading cover: while a level loads (the game's thread busy, in the
 * game but not the editor), the engine's loading screen shows it, its
 * spinner turning; when a level has loaded, a cover in the lobby's looks
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
	/** A level begins to load: the engine's loading screen shows the cover, turning, on its own thread while it does. */
	void OnLevelLoading(const FWorldContext& Context, const FString& Level);
	/** What the cover says going to `Level` from the level `From`: the lobby when hosting or joining, back to the lobby from a match, the map's name, or "Loading". */
	FString WhereTo(const FString& Level, const UWorld* From) const;
	bool Watch(float DeltaSeconds);
	void Lift();

	TSharedPtr<SWidget> Curtain;
	TWeakObjectPtr<UWorld> CoveredWorld;
	FTSTicker::FDelegateHandle Ticker;
	FDelegateHandle LoadedHandle;
	FDelegateHandle LoadingHandle;
	/** What the loading screen said, for the cover after it to say the same. */
	FString LoadingTo;
	double LoadingSince = 0.0;
	double UpSince = 0.0;
	/** When the last level finished loading, and which: a second load of it soon after is logged with its stack, to find who asked. */
	double LoadedAt = 0.0;
	FString LoadedLevel;
	/** The DPI scale the viewport had when the loading screen went up, kept for the cover that follows (see OnLevelLoaded). */
	float CoverScale = 0.0f;
	/** Where the covered level is, as the cover says it. */
	FString GoingTo;
};
