// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStreaks.h"

/**
 * The HUD's rules, as plain functions that a test can call without a world
 * (CheeseStrike.HUD.*): how the minimap turns and what of the level it
 * shows, which way a damage arc points, how red the edges are, what the row
 * of players and the scoreboard say about each, what the score summary shows, how a name tag
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
	/** The other way to MapPoint: the world's offset (cm along X and Y) for a point `Screen` pixels right of and below a minimap's middle. */
	FVector2D MapOffset(const FVector2D& Screen, float ViewYaw, float Scale);
	/**
	 * The strike map's middle (world X and Y) as it follows the strike's
	 * `Mark`: kept where it is while the mark is within `Reach` cm of it
	 * across and up and down the map (turned by `ViewYaw`), and moved just
	 * far enough to bring it back within that when it strays.
	 */
	FVector2D FollowMark(const FVector2D& Middle, const FVector2D& Mark, float ViewYaw, float Reach);
	/** How far the strike map reaches from its middle to its edge (cm), and the share of that the mark may go before the map follows it. */
	constexpr float StrikeMapRange = 3500.0f;
	constexpr float StrikeMapFollow = 0.6f;
	/** Where the N mark rides the edge of a square minimap `HalfSize` pixels from its middle to its edge, from its middle. */
	FVector2D NorthMark(float ViewYaw, float HalfSize);
	/** The convex hull of `Points`, in order round it; fewer than three points come back as they are. */
	TArray<FVector2D> ConvexHull(TArray<FVector2D> Points);
	/** A convex `Polygon` cut to the rectangle from `Min` to `Max`; empty if none of it is inside. */
	TArray<FVector2D> ClipToRect(const TArray<FVector2D>& Polygon, const FVector2D& Min, const FVector2D& Max);
	/** A convex `Polygon` cut to the convex polygon `Clip` (its corners in order round, either way); empty if none of it is inside. */
	TArray<FVector2D> ClipToConvex(const TArray<FVector2D>& Polygon, const TArray<FVector2D>& Clip);
	/** How round the minimap's corners are, in design pixels (ACheeseStrikeHUD::Unit). */
	constexpr float MinimapCorner = 18.0f;
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

	// ---------------------------------------------------------------- rounded shapes

	/**
	 * The corners of the rectangle from `Min` to `Max` with its corners
	 * rounded to `Radius` (held to half its shorter side; 0 for square
	 * corners), each corner an arc of `Steps` segments: in order round,
	 * clockwise on the screen, from the left end of the top left corner's arc.
	 * The canvas has no rounded shapes; the HUD fills this as a polygon. By
	 * default (`Steps` -1) each arc has as many segments as its radius needs
	 * (CornerSteps).
	 */
	TArray<FVector2D> RoundedRect(const FVector2D& Min, const FVector2D& Max, float Radius, int32 Steps = -1);
	/** Segments enough for a quarter circle `Radius` pixels round to look round: its chords stray from the true arc by under CornerError of a pixel. */
	constexpr float CornerError = 0.08f;
	int32 CornerSteps(float Radius);
	/**
	 * How wide the soft edge of the HUD's shapes is, in the screen's pixels:
	 * a shape is solid to half of it inside its outline and fades to clear
	 * half of it outside, as a pixel the edge crosses is covered, so that a
	 * rounded corner or a disc is smooth rather than stepped
	 * (CheeseStrikeHUDDraw::FShapes).
	 */
	constexpr float SoftEdge = 1.0f;
	/**
	 * For each corner of a convex `Outline` (in order round, either way),
	 * the way out from it that moves both its edges out by one: the sum of
	 * their outward normals over one plus their dot product, held to
	 * MostMiter long at a sharp corner. Corners that coincide (an arc of no
	 * radius) take their way from the nearest corners apart from them.
	 */
	constexpr float MostMiter = 3.0f;
	TArray<FVector2D> OutwardMiters(TConstArrayView<FVector2D> Outline);
	/** How far `Point` is inside that rounded rectangle's edge: negative outside. */
	float InsideRoundedRect(const FVector2D& Point, const FVector2D& Min, const FVector2D& Max, float Radius);
	/** `Point` held inside it: itself if it is inside, else the nearest point of its edge. */
	FVector2D HoldInsideRoundedRect(const FVector2D& Point, const FVector2D& Min, const FVector2D& Max, float Radius);

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
		/** Down in Execution's last stand: alive, until revived or finished. */
		Down
	};
	/** Out, then dead, then down, then alive: a body down that dies, or a player out, is no longer down. */
	EStatus StatusOf(bool bOut, bool bDead, bool bDown = false);
	/** The colour of being down, on the row and the scoreboard: a red-orange, apart from both sides' colours. */
	inline const FLinearColor DownColour(1.0f, 0.12f, 0.03f, 1.0f);
	/** Whole seconds until a player killed at `DiedAt` comes back, at `Now` (the server's time as this machine has it); 0 to the whole delay. */
	int32 SecondsToComeBack(float RespawnDelay, float Now, float DiedAt);
	/** A Steam id from a player's unique net id, given as its type and its text; 0 if it isn't Steam's. */
	uint64 SteamIdFrom(const FString& Type, const FString& Id);
	/** What stands for a player without a picture: the first letters of their first two words, or the first two of one word. */
	FString Initials(const FString& Name);

	// ---------------------------------------------------------------- the scoreboard

	/** Who plays a row of the scoreboard, shown by a small figure at its left: a person or a robot. */
	enum class EPlayerKind : uint8
	{
		Person,
		Bot
	};
	/** A bot (APlayerState::IsABot) shows as one; everyone else is a person. */
	EPlayerKind KindOf(bool bIsABot);
	/**
	 * What the scoreboard's status column says for `Status`: nothing while
	 * alive, "DOWN" in Execution's last stand, "OUT" out of the round, and
	 * "DEAD" while coming back.
	 */
	FString StatusWord(EStatus Status);
	/** How solid a row is for `Status`: a player out of the round is faded, one coming back a little. */
	float RowAlpha(EStatus Status);
	/** One row of the scoreboard, as drawn. */
	struct FBoardRow
	{
		FString Name;
		int32 Kills = 0;
		int32 Deaths = 0;
		EStatus Status = EStatus::Alive;
		EPlayerKind Kind = EPlayerKind::Person;
		/** The local player's own row. */
		bool bMine = false;
	};
	FBoardRow BoardRow(const CheeseStrikeRules::FStanding& Line, EStatus Status, bool bIsABot, bool bMine);
	/** `Name` as it fits `Width`: whole if it does, else cut and ended with "...", by `Measure` (its width as drawn). */
	FString FitName(const FString& Name, float Width, TFunctionRef<float(const FString&)> Measure);
	/**
	 * `Line` with the name `Name` in it cut to fit `Width` (FitName), the
	 * rest of the line kept whole: an announcement or a death screen's line
	 * naming a player, whose name may be as long as Steam's (32 letters,
	 * with no spaces to break at).
	 */
	FString FitNameIn(const FString& Line, const FString& Name, float Width, TFunctionRef<float(const FString&)> Measure);

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
	/**
	 * A block of the score summary laid out in `Width` pixels with `Pad` at
	 * either side and `Gap` between its words and its score: the score at
	 * the right, shrunk (ScoreScale) if it would take more than
	 * ScoreMostShare of the room, and the caption over the name at the left,
	 * each cut with dots (FitName) to what the score leaves, so that a side's
	 * long name or a player's never runs into a wide score, at any size of
	 * the window. A caption that would be cut has the score shrunk for it
	 * first, as far as ScoreLeastScale (a side's name over a hill's
	 * three-figure points stays whole). The widths are each text's as drawn.
	 */
	constexpr float ScoreMostShare = 0.45f;
	constexpr float ScoreLeastScale = 0.7f;
	struct FScoreBlockText
	{
		FString Caption;
		FString Name;
		float ScoreScale = 1.0f;
	};
	FScoreBlockText FitScoreBlock(const FScoreLine& Line, float Width, float Pad, float Gap,
		TFunctionRef<float(const FString&)> CaptionWidth, TFunctionRef<float(const FString&)> NameWidth, TFunctionRef<float(const FString&)> ScoreWidth);

	// ---------------------------------------------------------------- the scoreboard's sides

	/** What a side's score on the scoreboard counts, said beside it: "ROUNDS WON" in a mode of rounds, "POINTS" on the hill, else "KILLS". */
	FString TeamScoreLabel(CheeseStrikeModes::EWinBy WinBy, int32 Score);

	// ---------------------------------------------------------------- the kill streak

	/** One pip of the kill-streak panel's track: whether its kill has been made, and the streak that kill earns (None for most). */
	struct FStreakPip
	{
		bool bMade = false;
		CheeseStrikeStreaks::EStreak Earns = CheeseStrikeStreaks::EStreak::None;
	};
	/** The track for a run of `Kills`: a pip for each kill up to the last streak's (the Gouda Overload's ten), the streaks' kills marked; a longer run has them all made. */
	TArray<FStreakPip> StreakPips(int32 Kills);
	/** What the panel says is next: "CHEESE STRIKE IN 2", one kill off "CHEESE STRIKE NEXT KILL"; past the last streak "EVERY STREAK EARNED". `bShort` names it as under its pip ("DRONE IN 3"). */
	FString StreakNext(int32 Kills);
	/** Seconds the count's pop takes after a kill. */
	constexpr float StreakPopTime = 0.45f;
	/** How big the count is drawn `Seconds` after a kill (1 at rest, and before 0): it springs up past 1.4 and settles back by StreakPopTime. */
	float StreakPop(float Seconds);
	/** Seconds a whole track of kills ended by a death takes to drain out of the panel. */
	constexpr float StreakDrainTime = 0.6f;
	/** A streak's name as the panel says it: "UAC", "CHEESE STRIKE", "GOUDA OVERLOAD". */
	FString StreakShortName(CheeseStrikeStreaks::EStreak Kind);
	/** The pips shown filled for a run of `Kills`, from `Shown` (what was shown the frame before), `DeltaSeconds` on: rising StreakFillRate pips a second, and draining a run that has ended (a whole track in StreakDrainTime). */
	constexpr float StreakFillRate = 9.0f;
	float StreakFillToward(float Shown, int32 Kills, float DeltaSeconds);

	// ---------------------------------------------------------------- name tags

	/** How a teammate's name tag looks `Distance` cm away: its size (1 close up) and how solid it is. */
	struct FTagLook
	{
		float Scale = 1.0f;
		float Alpha = 1.0f;
	};
	FTagLook TagLook(float Distance);

	// ---------------------------------------------------------------- the crosshair

	/** What the crosshair shows: the usual lines, or a cancel sign where a shot would do nothing. */
	enum class ECrosshair : uint8
	{
		Usual,
		NoEffect
	};
	/**
	 * The crosshair over a player in the way of the aim: in a mode with a
	 * last stand (`bLastStand`), an enemy down (`bEnemyDown`) whom a shot
	 * from `Distance` cm would not hurt (CheeseStrikeModes::LastStandHit:
	 * only one within FinishRange finishes them) gets the cancel sign;
	 * anything else, the usual lines.
	 */
	ECrosshair CrosshairFor(bool bLastStand, bool bEnemyDown, float Distance);

	// ---------------------------------------------------------------- the death screen

	/**
	 * Whether the death screen shows `Seconds` after the local player was
	 * killed: always while they wait to come back; out of the round
	 * (Elimination, or finished in Execution) for OutDeathScreen seconds,
	 * after which the mode's notice of being out has the screen.
	 */
	constexpr float OutDeathScreen = 4.0f;
	bool ShowsDeathScreen(bool bOut, float Seconds);
	/** How solid the death screen is then: whole, but fading over its last half second when out of the round. */
	float DeathScreenAlpha(bool bOut, float Seconds);

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

	/**
	 * What the HUD says of the mice (the vermin, to the players), `ToGo`
	 * seconds before they come (less than 0 once they have): "VERMIN
	 * INVASION IN 0:59" while the countdown shows (CheeseStrikeMice::Warning),
	 * "THE VERMIN HAVE COME" for VerminCameFor seconds after, else nothing.
	 */
	constexpr float VerminCameFor = 4.0f;
	FString VerminWords(float ToGo);
}
