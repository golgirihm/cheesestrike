// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/CheeseStrikeMenu.h"
#include "CheeseStrikePlayerController.generated.h"

class ACameraActor;
class UCheeseStrikePlayScript;
class UInputMappingContext;

/**
 * The player's controller: puts the game's input bindings on for a local
 * player (see UCheeseStrikeInputSet), shows the scoreboard while its key is
 * held, limits how far the view looks up and down through its camera
 * manager, and plays a script in place of a person when given one
 * (UCheeseStrikePlayScript).
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACheeseStrikePlayerController();

	/** Whether the scoreboard's key is held. */
	bool IsScoreboardShown() const { return bScoreboardHeld; }

	/** Plays `Text` as a script from now on, in place of any other; see UCheeseStrikePlayScript for the commands. */
	UCheeseStrikePlayScript* Play(const FString& Text);
	UCheeseStrikePlayScript* GetPlayScript() const { return PlayScript; }

	/** True the first time only: -StartAt is for this player's first character. */
	bool TakeStartAt();

	/**
	 * The menu over the level (SCheeseStrikeMenu): which page shows follows
	 * where the match is (CheeseStrikeMenu::PageFor), and the menu's buttons
	 * come back here. The level is seen from a slowly turning menu camera
	 * until the match starts.
	 */
	EMenuPage GetMenuPage() const { return ShownPage; }
	/** Picks a page of the front end (main, host, find). */
	void ChooseMenu(EMenuPage Page);
	/** Starts the match here and now, alone (or as the host, without the countdown). */
	void PlayAlone();
	/** Hosts a match with these rules, through the online service, or the local network without Steam. */
	void HostFromMenu(int32 KillLimit, float RespawnDelay);
	/** Looks for matches, through the service or the local network without Steam. */
	void FindFromMenu();
	/** The host starts the match, after the countdown. */
	void StartMatchFromMenu();
	/** Leaves the match, or the lobby, back to the level alone and the front end. */
	void LeaveFromMenu();
	void QuitFromMenu();
	/** Opens or closes the pause menu during a match. */
	void TogglePause();
	bool IsPaused() const { return bPaused; }

	/** From the console or a play script: the host starts the match (at once with any argument). */
	UFUNCTION(Exec)
	void CheeseStrikeStart(const FString& AtOnce = TEXT(""));

	/** Matches through the online service (UCheeseStrikeSessionSubsystem), from the console: host with URL options such as ?KillLimit=10, find, join the nth found, leave. */
	UFUNCTION(Exec)
	void CheeseStrikeHost(const FString& Options = TEXT(""));
	UFUNCTION(Exec)
	void CheeseStrikeHostLan(const FString& Options = TEXT(""));
	UFUNCTION(Exec)
	void CheeseStrikeFind();
	UFUNCTION(Exec)
	void CheeseStrikeFindLan();
	UFUNCTION(Exec)
	void CheeseStrikeJoin(int32 Number = 1);
	UFUNCTION(Exec)
	void CheeseStrikeLeave();

protected:
	/** Mapping contexts a Blueprint of this controller adds, besides the game's own. */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void ScoreboardStarted();
	void ScoreboardEnded();
	void MenuPressed();
	/** Shows `Page`, or none, over the view, with the mouse for it. */
	void ShowMenu(EMenuPage Page);
	/** Whether the local player is in a hosted or joined match rather than alone on the level. */
	bool IsInAMatch() const;
	void ShowMenuCamera();
	void HideMenuCamera();

	/** The script playing the game in place of a person, if -PlayScript named one or a test gave one. */
	UPROPERTY()
	TObjectPtr<UCheeseStrikePlayScript> PlayScript;
	/** The level seen while in the menu. */
	UPROPERTY()
	TObjectPtr<ACameraActor> MenuCamera;
	TSharedPtr<SCheeseStrikeMenu> Menu;
	EMenuPage ShownPage = EMenuPage::None;
	EMenuPage ChosenPage = EMenuPage::Main;
	bool bPaused = false;
	float SinceMenuRefresh = 0.0f;
	bool bScoreboardHeld = false;
	bool bStartAtTaken = false;
};
