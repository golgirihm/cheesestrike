// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/CheeseStrikeMenu.h"
#include "CheeseStrikePlayerController.generated.h"

class ACameraActor;
class ACheeseStrikeMenuFigure;
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
	/** Picks a page of the front end (main, find). */
	void ChooseMenu(EMenuPage Page);
	/** Hosts a match with the usual rules on the first map, through the online service, or the local network without Steam: straight into its lobby. */
	void HostFromMenu();
	/** The host sets the match's rules and map from the lobby (INDEX_NONE keeps the map); everyone in it sees them. */
	void SetRulesFromMenu(int32 KillLimit, float RespawnDelay, int32 Map = INDEX_NONE);
	/** The host sets how well the bots play, from the lobby. */
	void SetBotSkillFromMenu(int32 Skill);
	/** Looks for matches, through the service or the local network without Steam. */
	void FindFromMenu();
	/** The host starts the match, after the countdown. */
	void StartMatchFromMenu();
	/** The host thinks better of it during the countdown. */
	void CancelStartFromMenu();
	/** Leaves the match, or the lobby, back to the level alone and the front end. */
	void LeaveFromMenu();
	void QuitFromMenu();
	/** Opens or closes the pause menu during a match: while it is open, the character takes no input. */
	void TogglePause();
	bool IsPaused() const { return bPaused; }
	/** Escape, or the gamepad's menu button: pauses or resumes a match, or goes back to the front end's main page. */
	void MenuPressed();

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void ScoreboardStarted();
	void ScoreboardEnded();
	/** Shows `Page`, or none, over the view, with the mouse for it; while a page shows, only the menu takes input. */
	void ShowMenu(EMenuPage Page);
	/** Whether the local player is in a hosted or joined match rather than alone on the level. */
	bool IsInAMatch() const;
	void ShowMenuCamera();
	void HideMenuCamera();

	/** The script playing the game in place of a person, if -PlayScript named one or a test gave one. */
	UPROPERTY()
	TObjectPtr<UCheeseStrikePlayScript> PlayScript;
	/** The level seen while in the menu, and the figure in the middle of it. */
	UPROPERTY()
	TObjectPtr<ACameraActor> MenuCamera;
	UPROPERTY()
	TObjectPtr<ACheeseStrikeMenuFigure> MenuFigure;
	TSharedPtr<SCheeseStrikeMenu> Menu;
	EMenuPage ShownPage = EMenuPage::None;
	EMenuPage ChosenPage = EMenuPage::Main;
	bool bPaused = false;
	float SinceMenuRefresh = 0.0f;
	bool bScoreboardHeld = false;
	bool bStartAtTaken = false;
};
