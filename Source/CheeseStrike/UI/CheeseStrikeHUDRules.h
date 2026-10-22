// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CheeseStrikeRules.h"

/**
 * The HUD's rules, as plain functions that a test can call without a world
 * (CheeseStrike.HUD.*): how the minimap turns and what of the level it
 * shows, which way a damage arc points, how red the edges are, what the row
 * of players says about each, what the score summary shows, how a name tag
 * shrinks with distance, and how the death screen's cheese runs.
 * ACheeseStrikeHUD draws by them.
 */
namespace CheeseStrikeHUD
{
	// ---------------------------------------------------------------- the minimap

	/**
	 * The way north lies on every map: +Y. The maps are laid out with x
	 * running west to east and y south to north (tools/art/build_creamery.py,
	 * docs/unreal/maps/), so the N mark points where their notes say north is.
	 */
	constexpr float NorthYaw = 90.0f;
	/** How far the minimap reaches from its middle to its edge, in cm. */
	constexpr float MinimapRange = 3000.0f;

	/**
	 * Where a point `Offset` (world cm along X and Y, from the player) lands
	 * on a minimap turned so that the player's facing (`ViewYaw`, degrees)
	 * is up, at `Scale` pixels a cm: pixels to the right of and below its
	 * middle. A point to the player's right is to the right.
	 */
	FVector2D MapPoint(const FVector2D& Offset, float ViewYaw, float Scale);
	/** Where the N mark rides the edge of a square minimap `HalfSize` pixels from its middle to its edge, from its middle. */
	FVector2D NorthMark(float ViewYaw, float HalfSize);
	/** The convex hull of `Points`, in order round it; fewer than three points come back as they are. */
	TArray<FVector2D> ConvexHull(TArray<FVector2D> Points);
	/** A convex `Polygon` cut to the rectangle from `Min` to `Max`; empty if none of it is inside. */
	TArray<FVector2D> ClipToRect(const TArray<FVector2D>& Polygon, const FVector2D& Min, const FVector2D& Max);
	/** How wide a convex `Polygon` is across its narrowest way: a wall's thickness, a slab of ground's width. */
	float NarrowestWidth(const TArray<FVector2D>& Polygon);
	/** Pieces at least this wide across (cm) are ground or platforms, not walls, when they rise over the player's head. */
	constexpr float WideFrom = 800.0f;

	/** How a piece of the level shows on the minimap, by its height against the player's: from not at all to a wall. */
	enum class ELayer : uint8
	{
		/** Over the player's head: a ceiling, a floor above, a lintel over a door. */
		Hidden,
		/** A level below: the yard from a roof, the canal from its bank. */
		Below,
		/** What the player stands on, or could: floors, ramps, low kerbs. */
		Floor,
		/** A level above beside the player: the bank of the canal from down in it, a platform over their head. */
		Raised,
		/** Cover to crouch behind. */
		Low,
		/** Walls and anything a head's height. */
		Wall
	};
	/**
	 * The layer of a piece of the level running from `Bottom` to `Top`
	 * (world cm) for a player whose feet are at `Feet`. A tilted slab (a
	 * ramp, `bRamp`) is floor wherever the player could be on it; a piece at
	 * least WideFrom across (`bWide`) that rises over their head is a level
	 * above, not a wall.
	 */
	ELayer LayerFor(float Bottom, float Top, float Feet, bool bRamp, bool bWide = false);

	/**
	 * Where an Unmanned Aerial Cheese is in its sweeps at `Now`, for one
	 * whose reveal ends at `Ends` after `RevealTime` seconds of sweeps every
	 * `Period`: which sweep (INDEX_NONE when none is up), seconds since it,
	 * and how strongly what it saw shows (full at the sweep, fading towards
	 * the next).
	 */
	struct FSweep
	{
		int32 Index = INDEX_NONE;
		float Since = 0.0f;
		float Fade = 0.0f;
	};
	FSweep SweepAt(float Now, float Ends, float RevealTime, float Period);

	// ---------------------------------------------------------------- damage

	/** Seconds a damage arc shows. */
	constexpr float DamageArcLife = 1.6f;
	/**
	 * Which way, round the middle of the view, the arc for damage from
	 * `From` points for a player at `At` (both world X and Y) facing
	 * `ViewYaw`: degrees clockwise from straight ahead (the top of the
	 * screen), -180 to 180.
	 */
	float DamageArcAngle(float ViewYaw, const FVector2D& At, const FVector2D& From);
	/** How strong an arc is `Age` seconds after its hit: full at first, gone at DamageArcLife. */
	float DamageArcFade(float Age);

	/** The red edges at `Health` of `MaxHealth`, `Seconds` into play: how strong (0 to 1), and how far in they reach (0 the very edge, 1 well in). */
	struct FVignette
	{
		float Alpha = 0.0f;
		float Reach = 0.0f;
	};
	/** Health under which the edges show, and under which they pulse. */
	constexpr float VignetteFrom = 60.0f;
	constexpr float VignettePulseUnder = 25.0f;
	FVignette VignetteFor(float Health, float MaxHealth, float Seconds);

	/**
	 * Health is never shown as a number: a bar of HealthSegments blocks at
	 * the lower left, each a share of the most there is. How full each is
	 * (0 to 1, left to right) at `Health` of `MaxHealth`: the full ones,
	 * then the one being worn down, then the empty; any health at all keeps
	 * a sliver of the first, so that alive never looks empty.
	 */
	constexpr int32 HealthSegments = 5;
	TArray<float> HealthFills(float Health, float MaxHealth, int32 Segments = HealthSegments);
	/** Whether the bar shows as low (red, and the edges pulse): at VignettePulseUnder's share or under. */
	bool IsHealthLow(float Health, float MaxHealth);

	// ---------------------------------------------------------------- the players along the top

	enum class EStatus : uint8
	{
		Alive,
		/** Killed, and coming back. */
		Respawning,
		/** Killed for good, until the round is over (Elimination). */
		Out,
		/** Down in Execution's last stand: alive, bleeding out, until revived or finished. */
		Down
	};
	/** Out, then dead, then down, then alive: a body down that dies, or a player out, is no longer down. */
	EStatus StatusOf(bool bOut, bool bDead, bool bDown = false);
	/** The colour of being down, on the row and the scoreboard: a red-orange, apart from both sides' colours. */
	inline const FLinearColor DownColour(1.0f, 0.12f, 0.03f, 1.0f);
	/** How much of a player's bleeding out is left (1 just down, 0 gone), with `Seconds` of `Total` to go. */
	float BleedLeft(float Seconds, float Total);
	/** Whole seconds until a player killed at `DiedAt` comes back, at `Now` (the server's time as this machine has it); 0 to the whole delay. */
	int32 SecondsToComeBack(float RespawnDelay, float Now, float DiedAt);
	/** A Steam id from a player's unique net id, given as its type and its text; 0 if it isn't Steam's. */
	uint64 SteamIdFrom(const FString& Type, const FString& Id);
	/** What stands for a player without a picture: the first letters of their first two words, or the first two of one word. */
	FString Initials(const FString& Name);

	// ---------------------------------------------------------------- the score summary

	/** One block of the score summary: a player or a side, their score, and whether it is the local player's own. */
	struct FScoreLine
	{
		/** What it is, over the score: "YOU", "LEADER", or a side's name. */
		FString Caption;
		/** Whose it is, under the score; empty for a side. */
		FString Name;
		int32 Score = 0;
		/** The side whose colour it takes, or INDEX_NONE for none. */
		int32 Team = INDEX_NONE;
		bool bMine = false;
	};
	/**
	 * What the bottom of the screen says about the score: a few blocks,
	 * the limit they race to (0 for none, as in a round with no score), and
	 * the goal in words. A free-for-all has the player's own kills and the
	 * best of the others'; a team mode has the sides; a mode with rounds
	 * puts the round in `Goal`.
	 */
	struct FScoreSummary
	{
		TArray<FScoreLine> Lines;
		int32 Limit = 0;
		FString Goal;
	};
	/** A free-for-all's summary: `Me`'s kills, and the leader's (or, if `Me` leads, the next best), racing to `Limit`. `Sorted` is the scoreboard in order. */
	FScoreSummary FreeForAll(const TArray<CheeseStrikeRules::FStanding>& Sorted, const FString& Me, int32 Limit);
	/** Gun Game's summary: `Me`'s step up the ladder and the leader's (or the next best's), out of `Steps`; `SortedBySteps` is everyone with their step (1 the first) as their kills, in order. */
	FScoreSummary GunGame(const TArray<CheeseStrikeRules::FStanding>& SortedBySteps, const FString& Me, int32 Steps);
	/** A team mode's summary: each side's score in its colour, `MyTeam`'s marked, racing to `Limit`, with `Goal` (the round, or what wins) between them. */
	FScoreSummary Teams(const int32 (&Scores)[CheeseStrikeRules::Teams], int32 MyTeam, int32 Limit, const FString& Goal);

	// ---------------------------------------------------------------- name tags

	/** How a teammate's name tag looks `Distance` cm away: its size (1 close up) and how solid it is. */
	struct FTagLook
	{
		float Scale = 1.0f;
		float Alpha = 1.0f;
	};
	FTagLook TagLook(float Distance);

	// ---------------------------------------------------------------- the death screen

	/** Seconds the cheese takes to run down when the player is killed. */
	constexpr float MeltTime = 1.6f;
	/** How many drips run down the screen. */
	constexpr int32 Drips = 26;
	/** One drip of melted cheese: where across the screen (0 left to 1 right), how wide and how far down it has run (shares of the screen's height). */
	struct FDrip
	{
		float X = 0.0f;
		float Width = 0.0f;
		float Length = 0.0f;
	};
	/** Drip `Index` (of Drips) `Seconds` after the player was killed: the same for every frame, running down and slowing. */
	FDrip Drip(int32 Index, float Seconds);
	/** How deep the sheet of cheese at the top is, as a share of the screen's height, `Seconds` after. */
	float SheetDepth(float Seconds);
	/** How far letter `Index` of the death screen's words has sagged `Seconds` after (0 standing, 1 sagged all it will). */
	float LetterSag(int32 Index, float Seconds);
}
