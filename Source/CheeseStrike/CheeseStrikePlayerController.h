// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/CheeseStrikeMenu.h"
#include "CheeseStrikePlayerController.generated.h"

class ACameraActor;
class ACheeseStrikeMenuFigure;
class ACheeseStrikePodium;
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

	/**
	 * How much the player's look turns the view, across and up and down, as
	 * the options set it (CheeseStrikeSettings::LookScale): how fast, the
	 * aimed share while the sights are up, and up and down the other way if
	 * inverted. A play script looks as the usual settings would.
	 */
	FVector2D LookScale() const;

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
	/** Whether this player has been told to follow the server to another level (and is waiting to): the lobby says where to. */
	bool IsLeaving() const { return bLeaving; }
	/** The map (CheeseStrikeRules::Maps) this player was told to follow the server to, or INDEX_NONE. */
	int32 GetLeavingFor() const { return LeavingFor; }
	/**
	 * Whether what this player sees is ready to be shown: a page of the menu
	 * over the level, or the match through their character. Right after a
	 * level loads it isn't (a joiner has no controller or character for a
	 * moment, and the engine's view from the world's origin shows), and the
	 * loading cover stays up (UCheeseStrikeCurtainSubsystem). On a level the
	 * lobby sent everyone to it stays up until the match begins there, when
	 * they are all ready (ServerReady), and then the view is the character's.
	 */
	bool IsViewReady() const;
	/**
	 * Server: this player's level is loaded and their view can be shown, so
	 * a match the lobby sent everyone to may begin (ACheeseStrikeGameMode::
	 * ReadyToStartMatch). Each player's machine says so once per level.
	 */
	UFUNCTION(Server, Reliable)
	void ServerReady();
	/** Server: whether this player has said so. */
	bool HasSaidReady() const { return bSaidReady; }
	/** Whether this player's machine is ready to say so: on the level, with the match's state and their own. */
	bool CanSayReady() const;
	/** Whether the lobby shows, with the host's match in it, ready to be played in (SCheeseStrikeMenu::IsLobbyReady). */
	bool IsLobbyReady() const;
	/**
	 * Whether the results held up on the way back to the lobby can go
	 * (UCheeseStrikePostGameSubsystem): the lobby is ready, or the player is
	 * on the front end, out of the match.
	 */
	bool IsDoneWithTheResults() const;
	/**
	 * The podium after the match (ACheeseStrikePodium), made on this machine
	 * once the results have been up a moment (CheeseStrikePodiums::IsShowing),
	 * viewed until the match goes back to the lobby; null while it isn't up.
	 */
	const ACheeseStrikePodium* GetPodium() const { return Podium; }
	/** When it came, by the world's real time. */
	float GetPodiumShownAt() const { return PodiumShownAt; }
	/** Whether this player may skip it: the host (or a player alone). */
	bool CanSkipPodium() const;
	/** The host, over the podium: everyone goes back to the lobby now (ACheeseStrikeGameMode::BackToLobbyNow). */
	void SkipPodium();
	/** The console's way to it, for play scripts: `CheeseStrikeSkipPodium`. */
	UFUNCTION(Exec)
	void CheeseStrikeSkipPodium();
	/** Picks a page of the front end (main, find, credits, options, controls), or the options or the controls from the pause menu. */
	void ChooseMenu(EMenuPage Page);
	/** Back from a page to the one it came from: the front end's main page, the pause menu, or the options (from the controls). */
	void BackFromMenu();
	/** Hosts a match with the usual rules on the first map, through the online service, or the local network without Steam: straight into its lobby. */
	void HostFromMenu();
	/** The host sets the match's rules and map from the lobby (INDEX_NONE keeps the map); everyone in it sees them. */
	void SetRulesFromMenu(int32 KillLimit, float RespawnDelay, int32 Map = INDEX_NONE);
	/** The host sets how well the bots play, and how many there are (-1 for every empty seat), from the lobby. */
	void SetBotSkillFromMenu(int32 Skill);
	/** The host picks the mode (CheeseStrikeModes::Modes), and the rounds to win in a mode of rounds, from the lobby. */
	void SetModeFromMenu(int32 ModeIndex);
	void SetRoundsFromMenu(int32 Rounds);
	void SetBotsFromMenu(int32 Wanted);
	/** In the lobby of a mode of sides: this player goes over to the other side, if it has room (ACheeseStrikeGameMode::SwitchSide). */
	void SwitchSideFromMenu();
	/** The host moves a bot of the lobby (its number) to the other side. */
	void MoveBotFromMenu(int32 Bot);
	UFUNCTION(Server, Reliable)
	void ServerSwitchSide();
	/**
	 * A joiner: the host is leaving the lobby (or, `bInMatch`, the match)
	 * or quitting the game (UCheeseStrikeSessionSubsystem::SayGoodbye). The
	 * joiner goes back to the front end, which says so.
	 */
	UFUNCTION(Client, Reliable)
	void ClientHostLeft(bool bInMatch);
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
	/** From the console or a play script: the host calls off the countdown. */
	UFUNCTION(Exec)
	void CheeseStrikeCancel();
	/** From the console or a play script: shows a page of the front end, as its buttons do (Main, Find, Credits, Options or Controls, which also open from the pause menu), and of the options the tab named (Controls, View, Sound or Graphics). */
	UFUNCTION(Exec)
	void CheeseStrikePage(const FString& Page, const FString& Tab = TEXT(""));
	/** From the console or a play script: opens the lobby's nth dropdown (0 the map, then the mode and on down), for looking at it; -1 shuts it. */
	UFUNCTION(Exec)
	void CheeseStrikeDropdown(int32 Number = 0);
	/** From the console or a play script: goes over to the other side in the lobby, as its button does. */
	UFUNCTION(Exec)
	void CheeseStrikeSwitchSide();
	/** From the console or a play script: the host moves the lobby's nth bot (0 the first added) to the other side. */
	UFUNCTION(Exec)
	void CheeseStrikeMoveBot(int32 Bot = 0);

	/** Matches through the online service (UCheeseStrikeSessionSubsystem), from the console: host with URL options such as ?KillLimit=10, find, join the nth found, leave. */
	/** Logs every bot's state: what it is doing, its health, ammunition and speed (the host's world only). */
	UFUNCTION(Exec)
	void CheeseStrikeBots();
	/** Logs every mouse's state, and how much cheese is left (the host's world only). */
	UFUNCTION(Exec)
	void CheeseStrikeMice();
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
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
	/** Following the server to another level, this player keeps their own `?Nick=`: the server's URL carries the host's. */
	virtual void ClientTravelInternal_Implementation(const FString& URL, ETravelType TravelType, bool bSeamless, FGuid MapPackageGuid) override;


	void ScoreboardStarted();
	void ScoreboardEnded();
	/** Shows `Page`, or none, over the view, with the mouse for it; while a page shows, only the menu takes input. */
	void ShowMenu(EMenuPage Page);
	/** Whether the local player is in a hosted or joined match rather than alone on the level. */
	bool IsInAMatch() const;
	/** Plays the countdown's tones as the match's seconds go by (CheeseStrikeMenu::CountdownTone). */
	void SoundCountdown();
	/** The match is over and on its way back to the lobby: the results are held up until the lobby is there. */
	void HoldTheResults();
	/** The menu's music, as the page wants it (CheeseStrikeMusic::IsWanted). */
	void PlayMusic();
	void ShowMenuCamera();
	void HideMenuCamera();
	/** Puts the podium up when it is time, keeps the view on it, and takes it down. */
	void UpdatePodium();
	void ShowPodium();
	void HidePodium();

	/** The script playing the game in place of a person, if -PlayScript named one or a test gave one. */
	UPROPERTY()
	TObjectPtr<UCheeseStrikePlayScript> PlayScript;
	/** The level seen while in the menu, and the figure in the middle of it. */
	UPROPERTY()
	TObjectPtr<ACameraActor> MenuCamera;
	UPROPERTY()
	TObjectPtr<ACheeseStrikeMenuFigure> MenuFigure;
	UPROPERTY()
	TObjectPtr<ACheeseStrikePodium> Podium;
	/** The host's button over it. */
	TSharedPtr<SWidget> PodiumSkip;
	float PodiumShownAt = 0.0f;
	TSharedPtr<SCheeseStrikeMenu> Menu;
	EMenuPage ShownPage = EMenuPage::None;
	EMenuPage ChosenPage = EMenuPage::Main;
	/** The page the controls were opened from, to go back to. */
	EMenuPage ControlsFrom = EMenuPage::Main;
	bool bPaused = false;
	bool bLeaving = false;
	int32 LeavingFor = INDEX_NONE;
	/** Server: this player is ready for a match the lobby sent everyone to; and on their machine, whether it has said so. */
	bool bSaidReady = false;
	bool bReadySent = false;
	/** The last whole second of the countdown sounded, for its tones (CheeseStrikeMenu::CountdownTone). */
	int32 SecondSounded = INDEX_NONE;
	/** The same for the count before the match and its rounds (ACheeseStrikeGameState::GetGoAt). */
	int32 GoSounded = INDEX_NONE;
	bool bScoreboardHeld = false;
	bool bStartAtTaken = false;
};
