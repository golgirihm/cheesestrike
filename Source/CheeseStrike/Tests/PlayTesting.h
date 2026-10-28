// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

class ACheeseStrikeCharacter;
class ACheeseStrikeGameState;
class ACheeseStrikePlayerController;
class FAutomationTestBase;
class UWorld;

/**
 * Tests that play the game: in the editor, on the shooter level, alone or
 * as a host with others joining in the same process, each driven by a
 * play script (UCheeseStrikePlayScript) and then looked at.
 *
 * A test queues the steps from RunTest, in order, and they run over the
 * frames that follow:
 *
 *   PlayTesting::StartPlaying({2, TEXT("?KillLimit=1")});
 *   PlayTesting::Play(1, TEXT("hold Fire\nwait 1\nrelease Fire"));
 *   PlayTesting::Then([this]() { TestFalse(TEXT("the host is dead"), PlayTesting::Character(0)->IsAlive()); });
 *   PlayTesting::StopPlaying();
 *
 * Player 0 is the host, or the only player; 1 is the first to join. The
 * host's world is the server's, so what its characters say is the truth;
 * a joiner's world is what that player sees.
 */
namespace PlayTesting
{
	/** The map tests time some walks (forward for so many seconds) at the old walking speed of 450 cm/s; walking is slower (a fifth since 26 Oct 2026, and a fifth again since 27 Oct), so they walk this much longer. */
	constexpr float WalkTimeScale = 450.0f / 288.0f;

	/** A session: how many play, the host's options for the map's URL, and whether they start apart (each alone, to find one another through the online service) rather than already in one match. */
	struct FSession
	{
		int32 Players = 1;
		FString Options;
		bool bApart = false;
		/** Whether the match waits in the lobby, as a hosted one does; otherwise it starts at once (`?StartNow`). */
		bool bLobby = false;
		/** The level's package, or the shooter level if empty. */
		FString Level;
		/** The primary everyone comes in with (CheeseStrike.SpawnPrimary): the Cheddar Blaster the tests were written for, or empty for the game's own, the Colby Carbine. */
		FString SpawnPrimary = TEXT("blaster");
	};

	/** Queues: start playing as `Session` says, and wait until every player is in, with a character that has its weapon. */
	void StartPlaying(const FSession& Session);
	/** Queues: stop playing. A test that started must queue this last. */
	void StopPlaying();
	/** Queues: wait until `Condition` holds, or fail the test as `What` not having happened in `Timeout` seconds. */
	void WaitUntil(const FString& What, float Timeout, TFunction<bool()> Condition);
	/** Queues: wait `Seconds` of play. */
	void Wait(float Seconds);
	/** Queues: `Do`, once. */
	void Then(TFunction<void()> Do);
	/** Queues: `Player` plays `Script` (see UCheeseStrikePlayScript) through to its end. */
	void Play(int32 Player, const FString& Script, float Timeout = 30.0f);
	/**
	 * Queues: `Shooter` holds the trigger until `What` has happened, or `Timeout` seconds of play have passed, then lets go.
	 * For a kill, rather than a burst of fixed length, which lands fewer shots on a busy PC: a weapon fires at most once a
	 * frame, and a slow frame is several of its shots long.
	 */
	void FireUntil(int32 Shooter, const FString& What, float Timeout, TFunction<bool()> Done);
	/** Queues: put `Player`'s character at `At`, facing `Yaw`, as the server and the player both see it. */
	void PutAt(int32 Player, const FVector& At, float Yaw);

	/** The worlds and what is in them while playing; null before and after. */
	UWorld* World(int32 Player);
	ACheeseStrikePlayerController* Controller(int32 Player);
	ACheeseStrikeCharacter* Character(int32 Player);
	ACheeseStrikeGameState* GameState(int32 Player);
	/** `Of`'s character as `Seen` has it in their world: the live one if there is one, else a body; null until it has arrived. */
	ACheeseStrikeCharacter* CharacterOf(int32 Of, int32 Seen);
	/** The server's clock as `Player` has it. */
	float Now(int32 Player);
	/** The test running, for its checks from inside a step. */
	FAutomationTestBase* Test();
}

#endif
