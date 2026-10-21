// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Object.h"
#include "CheeseStrikePlayScript.generated.h"

class ACheeseStrikePlayerController;
class UInputAction;

/**
 * Plays the game from a script instead of a person, for watching a change
 * from outside: the game is started with -PlayScript=<file>, and the local
 * player's controller runs the file's commands, one at a time, injecting
 * input through Enhanced Input, so that no window needs focus and the
 * person at the PC keeps their mouse and keyboard. With -RenderOffscreen
 * there is no window at all; screenshots still come out.
 *
 * One command a line, `#` for a comment:
 *   wait <seconds>              do nothing for so long
 *   hold <action> [x [y]]       hold an input from now on: Move x y, Look x y, or Jump, Sprint, Crouch, Fire, Aim, Reload, Scoreboard,
 *                               Primary, Sidearm, Knife, NextWeapon, PreviousWeapon, LastWeapon, Streak
 *   release <action>            let it go
 *   tap <action> [x [y]]        hold it for a few frames
 *   look <yaw> <pitch>          turn the view by so many degrees, at once
 *   shot <name>                 save a screenshot as Saved/Screenshots/<platform>/<name>.png
 *   state                       write the character's state to the log now
 *   log <text>                  write a line to the log
 *   cmd <console command>       run a console command, such as CheeseStrikeDie
 *   quit                        end the game
 *
 * A test plays one too (ACheeseStrikePlayerController::Play), in the
 * editor, and then looks at what it did.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikePlayScript : public UObject
{
	GENERATED_BODY()

public:
	/** The script named on the command line, or null if there is none. */
	static UCheeseStrikePlayScript* FromCommandLine(ACheeseStrikePlayerController* Controller);
	/** A script from `Text`, owned by `Outer`, for `Controller` to play. */
	static UCheeseStrikePlayScript* FromText(UObject* Outer, ACheeseStrikePlayerController* Controller, const FString& Text);

	/** Reads `Text` as commands; returns how many. */
	int32 Load(const FString& Text);

	/** Runs the script on for `DeltaSeconds`: the next commands, and the inputs still held. */
	void Advance(float DeltaSeconds);

	/** Keeps holding what `Previous` was holding, so that one script can follow another without a key going up. */
	void TakeOver(const UCheeseStrikePlayScript* Previous);

	/** Goes on through `NewController`: the one a player gets after travelling to another match. */
	void Attach(ACheeseStrikePlayerController* NewController) { Controller = NewController; }

	/** Whether every command has run and no wait is left; inputs may still be held. */
	bool IsFinished() const { return Next >= Commands.Num() && Waiting <= 0.0f; }
	int32 NumCommands() const { return Commands.Num(); }

private:
	struct FCommand
	{
		FString Verb;
		TArray<FString> Args;
	};

	/** Runs one command; returns false if it is a wait that is not over. */
	bool Run(const FCommand& Command);
	void InjectHeld();
	const UInputAction* ActionNamed(const FString& Name) const;
	FString Describe(const FCommand& Command) const;

	UPROPERTY()
	TObjectPtr<ACheeseStrikePlayerController> Controller;
	TArray<FCommand> Commands;
	int32 Next = 0;
	float Waiting = 0.0f;
	/** The inputs held, by action name, with their values. */
	TMap<FString, FVector2D> Held;
	/** Taps still to be let go of, by action name, with frames left. */
	TMap<FString, int32> Tapped;
};

/**
 * Keeps the game's play script for as long as the game runs: a player
 * gets a new controller whenever they travel to another match, and the
 * script carries on through it rather than starting over.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikePlayScriptSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The script `Controller` should play from now: the one under way, or the command line's on the first ask; null if there is none. */
	UCheeseStrikePlayScript* ScriptFor(ACheeseStrikePlayerController* Controller);
	/** Makes `Script` the one under way. */
	void SetScript(UCheeseStrikePlayScript* Script) { Current = Script; }

private:
	UPROPERTY()
	TObjectPtr<UCheeseStrikePlayScript> Current;
	bool bAskedCommandLine = false;
};
