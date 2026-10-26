// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"

/**
 * The podium after a match, as plain rules a test can call without a world:
 * who stands on it and in what order, what score is written under each, how
 * long it shows, and what each hero does on it. The match's results show
 * first for a moment (After), then the podium for Seconds with a countdown,
 * and then everyone goes back to the lobby (CheeseStrikeRules::ResultsTime
 * is the two together). ACheeseStrikePodium stages it on every machine from
 * the replicated player states; UI/CheeseStrikePodiumHUD writes over it.
 */
namespace CheeseStrikePodiums
{
	/** Seconds the match's results show before the podium. */
	constexpr float After = 3.0f;
	/** Seconds the podium shows, counted down, before everyone goes back to the lobby. */
	constexpr float Seconds = 15.0f;
	/** Seconds a skip leaves before the match goes back to the lobby: long enough for every machine to hear of it and keep the podium up. */
	constexpr float SkipSeconds = 0.75f;
	/** How many stand on it at most. */
	constexpr int32 Places = 3;

	/** A player as the podium and the scoreboard order them. */
	struct FContender
	{
		CheeseStrikeRules::FStanding Standing;
		/** Their side (in a team mode); INDEX_NONE for none. */
		int32 Team = INDEX_NONE;
		/** Gun Game's step, from 0. */
		int32 GunLevel = 0;
		/** King of the Hill's points of their own. */
		int32 HillPoints = 0;
		int32 HillSeconds = 0;
	};

	/**
	 * The standing a player is ordered by (CheeseStrikeRules::Sorted), as the
	 * scoreboard orders them: by the kills, but Gun Game by the step reached
	 * first (a step is worth more than any number of kills) and the solo King
	 * of the Hill by the points first.
	 */
	CheeseStrikeRules::FStanding RankedStanding(const FContender& Contender, CheeseStrikeModes::EWinBy WinBy, bool bTeams);

	/**
	 * Who stands on the podium, as indexes into `Contenders`, the first place
	 * first, at most Places of them (fewer with fewer players). In a mode of
	 * sides, the winning side's best (`WinningTeam`): the podium is the
	 * winners'; with no side the winner, everyone's. Otherwise everyone's.
	 */
	TArray<int32> Choose(TConstArrayView<FContender> Contenders, CheeseStrikeModes::EWinBy WinBy, bool bTeams, int32 WinningTeam);

	/** What is scored in the mode, as the podium writes it under a player: their kills, Gun Game's step (from 1), the solo hill's points. */
	int32 ScoreOf(const FContender& Contender, CheeseStrikeModes::EWinBy WinBy, bool bTeams);
	/** That score in words: "12 KILLS", "1 KILL", "STEP 7 OF 18" (`Steps` the ladder's length), "100 POINTS". */
	FString ScoreWords(int32 Score, CheeseStrikeModes::EWinBy WinBy, bool bTeams, int32 Steps = 0);

	/** Whether the podium shows: the match is over (`bEnded`) and the lobby is `Seconds` or less away (`LobbyAt`, by the server's clock `Now`). */
	bool IsShowing(bool bEnded, float LobbyAt, float Now);
	/** The countdown's number: whole seconds until the lobby, rounded up; 0 once it is all but there. */
	int32 SecondsLeft(float LobbyAt, float Now);

	/** Where place `Place` (0 first) stands, counted from the camera's left: the second, the first in the middle, the third. */
	int32 SlotOf(int32 Place);
	/** How high its pedestal is, in centimetres: the first highest. */
	float PedestalHeight(int32 Place);
	/** How many wheels of cheese its pedestal is stacked from. */
	int32 Wheels(int32 Place);

	/**
	 * What hero `Body` (CheeseStrikeBodies::All) does on place `Place`: one of
	 * its own Paragon emotes, as an object path. The first strikes a victory
	 * pose, the second boasts, the third taunts; nothing for a body there are none for.
	 */
	const TCHAR* Emote(int32 Body, int32 Place);
}
