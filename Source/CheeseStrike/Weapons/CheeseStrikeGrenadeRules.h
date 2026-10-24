// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The cheese grenade as plain numbers and arithmetic, apart from the engine
 * so that it can be tested alone: how many a player carries, how it is
 * thrown, its fuse, and the harm its burst does with distance. The grenade
 * itself is ACheeseStrikeGrenade, thrown by its carrier's
 * UCheeseStrikeGrenades. Lengths are centimetres at life size, times
 * seconds, speeds centimetres a second.
 *
 * Its harm follows the match's rules for who may hurt whom
 * (CheeseStrikeModes::CanHurt), as the streaks' do: never a teammate in a
 * mode with sides (friendly fire is off), nobody between rounds. In
 * Execution it puts an enemy down as any hit would, and, like a streak,
 * never finishes someone already down: that takes a hand up close. Its
 * thrower it hurts as it would an enemy, a death by it counted as a fall
 * is (ACheeseStrikeCharacter::TakeOwnBlast): nobody's kill, unless someone
 * hurt them just before.
 */
namespace CheeseStrikeGrenades
{
	/** What it is called, in the kill feed. */
	extern const TCHAR* const Name;
	/** How the feed and the death screen say someone died by their own: "blew themself up". */
	extern const TCHAR* const OwnWords;

	/** Grenades a player comes in with, and the most carried (the design's two). */
	constexpr int32 SpawnCount = 2;
	constexpr int32 MostCarried = 2;

	/** Seconds from leaving the hand to the burst. */
	constexpr float Fuse = 2.5f;
	/** How fast it leaves the hand, and how far above the aim (degrees), as a throw goes a little up from where one looks. */
	constexpr float ThrowSpeed = 1500.0f;
	constexpr float ThrowLift = 7.0f;
	/** The share of the thrower's own speed it carries. */
	constexpr float CarriedSpeedShare = 0.5f;
	/** Gravity on it, as a share of the world's. */
	constexpr float GravityScale = 1.0f;
	/** How much of its speed a bounce keeps (into the surface), and how much along the surface friction takes. */
	constexpr float Bounciness = 0.4f;
	constexpr float Friction = 0.55f;
	/** How fast it slows rolling along the ground (cm/s each second): the bounces' friction alone would let it roll on and on. */
	constexpr float RollDrag = 900.0f;
	/** Its size for colliding (cm): a round of cheese about 9 cm across. */
	constexpr float CollisionRadius = 5.0f;

	/** Seconds from the press to the grenade leaving the hand (the throw's wind-up), and to the hands being free again. */
	constexpr float ReleaseDelay = 0.35f;
	constexpr float ThrowTime = 0.8f;

	/**
	 * A player's throw is held (G or the right bumper held down): the
	 * grenade is taken up in the free hand and its arc shown, and the throw
	 * is made as the key is let go (the whole throw, as a bot's).
	 */
	/** What becomes of a held throw: it goes on being held, is thrown (the key let go), or is put away, the grenade kept. */
	enum class EHeld : uint8
	{
		Holding,
		Throw,
		PutAway,
	};
	/**
	 * A held throw, each frame: put away if the holder can't throw now (dead,
	 * down, held by a count, none left) or has changed weapons since taking
	 * it in hand; else thrown once the key is let go.
	 */
	EHeld HeldThrow(bool bKeyHeld, bool bCanThrow, bool bChangedWeapon);

	/** The arc shown while a throw is held: a dot every PreviewSpacing cm of its flight, none in its first PreviewSkip (too near the eyes), at most PreviewDots. */
	constexpr float PreviewSpacing = 30.0f;
	constexpr float PreviewSkip = 70.0f;
	constexpr int32 PreviewDots = 80;
	/** The dots along `Path` (its points in order of flight): every `Spacing` cm along it from `Skip` on, at most `MostDots`. */
	TArray<FVector> PreviewPoints(TConstArrayView<FVector> Path, float Spacing = PreviewSpacing, float Skip = PreviewSkip, int32 MostDots = PreviewDots);

	/** The burst: harm up to Radius from it, Most at its middle and falling off in a straight line to Least at the edge. */
	constexpr float Radius = 450.0f;
	constexpr float Most = 130.0f;
	constexpr float Least = 15.0f;
	/** The harm the burst does `Distance` from where it goes off; none past Radius. */
	float DamageAt(float Distance);

	/** How it leaves the hand, thrown along `Aim` (normalized) by someone moving at `CarrierVelocity`. */
	FVector LaunchVelocity(const FVector& Aim, const FVector& CarrierVelocity);

	/**
	 * The pitch (degrees, up positive) to throw at, from the hand, so as to
	 * land `Across` cm away and `Rise` cm higher (below if negative), at
	 * `Speed` under gravity `Gravity` (cm/s², positive): the lower of the
	 * two arcs. False if it can't reach.
	 */
	bool PitchToReach(float Across, float Rise, float Speed, float Gravity, float& OutPitch);

	/** For a bot: what it knows when it thinks about throwing one. */
	struct FBotThrow
	{
		/** Grenades it carries, and seconds since it last threw one. */
		int32 Carried = 0;
		float SinceLastThrow = 1000.0f;
		/** Seconds since it last saw its enemy, and how far away (cm) they were then, along the ground. */
		float SinceSeen = 1000.0f;
		float Distance = 0.0f;
		/** Whether it sees them now. */
		bool bSeesThem = false;
		/** A number from 0 to 1, drawn at random for this thought. */
		float Roll = 1.0f;
	};
	/** Seconds a bot waits between its throws, its range for throwing (cm), and how long after losing sight of someone it still throws at where they were. */
	constexpr float BotInterval = 9.0f;
	constexpr float BotNearest = 650.0f;
	constexpr float BotFarthest = 2200.0f;
	constexpr float BotMemory = 3.0f;
	/** The chance, each second it could, that a bot throws one: at someone just gone behind cover, and at someone in sight. */
	constexpr float BotChanceHidden = 0.6f;
	constexpr float BotChanceInSight = 0.12f;
	/**
	 * Whether a bot throws one now, thinking once a second: it carries one,
	 * hasn't thrown one lately, and its enemy is within its range, neither
	 * close enough for the throw to waste a moment better spent shooting nor
	 * beyond a good throw; most likely at someone it has just lost behind
	 * cover, to flush them out, now and then at someone in sight.
	 */
	bool BotShouldThrow(const FBotThrow& Thought);
	/** How near (cm) to where a bot means it to land a throw it works out must come down, or it doesn't throw. */
	constexpr float BotLandsWithin = 220.0f;
	/** How far from itself (cm) a bot keeps where its grenade comes down: its own burst hurts it. */
	constexpr float BotOwnMargin = Radius + 150.0f;
	/** How far (cm) a bot keeps from a grenade about to burst that can hurt it (its own, or an enemy's): out of its reach, and a little more. */
	constexpr float BotKeepsAway = Radius + 150.0f;
	/**
	 * Whether a bot `Distance` cm from a grenade whose burst can hurt it (`bCanHurt`) gets away from it: one that
	 * hasn't burst, within BotKeepsAway, whoever threw it. A bot that goes after an enemy where its own grenade
	 * landed would otherwise run into it.
	 */
	bool BotGetsAway(float Distance, bool bCanHurt, bool bBurst);
	/** Where a bot means a grenade to come down, to throw at someone `Distance` cm away: short of them, as it bounces and rolls on, but never within BotOwnMargin of itself. */
	float BotLandingShort(float Distance);
}
