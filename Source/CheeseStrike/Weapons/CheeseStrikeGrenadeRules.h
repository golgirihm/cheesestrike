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

	/**
	 * The kinds of grenade a player may carry (chosen in the loadout, Rules/CheeseStrikeLoadout.h): the cheese grenade,
	 * which bursts and harms, and the Smoked Gouda, which smokes: thrown, flown and bounced alike, its fuse shorter, and
	 * at its end no harm but a cloud of smoke (ACheeseStrikeSmokeCloud) that hides whoever is behind it from bots' eyes
	 * and the HUD's name tags for a while, though not from the Unmanned Aerial Cheese, and stops no shot.
	 */
	enum class EGrenadeKind : uint8
	{
		Cheese,
		Smoke,
	};
	constexpr int32 NumKinds = 2;
	/** What each is called ("Cheese Grenade", "Smoked Gouda"), and the word it is kept and picked by ("cheese", "smoke"). */
	const TCHAR* NameOf(EGrenadeKind Kind);
	const TCHAR* KeyOf(EGrenadeKind Kind);
	/** The kind `Key` names (its word or its name, either case), or none. */
	TOptional<EGrenadeKind> KindFromKey(const FString& Key);
	/** A kind as replicated (a byte): any other number is the cheese grenade. */
	EGrenadeKind KindAt(int32 Index);
	/** Seconds from leaving the hand to going off, for `Kind`. */
	float FuseOf(EGrenadeKind Kind);
	/** Whether `Kind`'s burst harms anyone: only the cheese grenade's does. */
	bool Harms(EGrenadeKind Kind);

	/** The Smoked Gouda's fuse: shorter than the cheese grenade's, so that the smoke comes where it was thrown. */
	constexpr float SmokeFuse = 1.5f;
	/** Its cloud: how long it grows for, how wide it gets (cm across), how long it lasts, and how long it thins for at its end. */
	constexpr float CloudGrowSeconds = 1.0f;
	constexpr float CloudAcross = 1000.0f;
	constexpr float CloudLife = 12.0f;
	constexpr float CloudThinSeconds = 3.0f;
	/** How many mist cards swirl in it (between 80 and 150), and how far up its middle sits over where it burst (cm). */
	constexpr int32 CloudCards = 140;
	constexpr float CloudMiddleUp = 110.0f;
	/**
	 * Its core (wave 16): an ellipsoid about its middle, CloudCoreShare of the cloud's radius across and CloudCoreFlat of
	 * that up and down (the cards lie flattened as smoke rolled out along the ground), in which whoever stands is hidden
	 * whole from outside at any distance: each machine draws a veil of solid cards (CloudVeil) across its own camera's
	 * view of it, CloudVeilAhead before it, so that nothing in it is seen through the smoke, the swirling cards round it
	 * its soft edge.
	 */
	constexpr float CloudCoreShare = 0.74f;
	constexpr float CloudCoreFlat = 0.62f;
	/** Whether someone whose body runs from `Feet` up `Height` cm (`Feet` measured from where the cloud burst) stands wholly inside the core at `Age`. */
	bool CloudCoreHides(const FVector& Feet, float Height, float Age);
	/** How far before the core the veil stands (cm; more than the cards' soft depth fade, so it is whole over anyone in the core), and how near the eye it comes at the nearest (cm: from inside the core, nothing further is seen). */
	constexpr float CloudVeilAhead = 100.0f;
	constexpr float CloudVeilNearest = 80.0f;
	/** How much of a solid card's radius is whole (M_SmokeMist with its custom data 1 at 1): the veil's cards are made this much bigger than what they must hide. */
	constexpr float CloudVeilWhole = 0.75f;
	/**
	 * The veil for an eye at `Eye` (from the cloud's middle) at `Age`: a card turned to the eye, its middle at `Middle`
	 * (from the cloud's middle) and its axes `Right` and `Up`, which must be whole over `HalfRight` by `HalfUp` (cm) about
	 * its middle to hide everything of the core behind it: the core's outline as the eye sees it, at the veil's distance.
	 * From inside the core (or nearer it than CloudVeilAhead), a veil CloudVeilNearest before the eye, wide enough for any
	 * view. None (HalfRight 0) while the cloud is too thin to block sight.
	 */
	struct FVeil
	{
		FVector Middle = FVector::ZeroVector;
		FVector Right = FVector::RightVector;
		FVector Up = FVector::UpVector;
		float HalfRight = 0.0f;
		float HalfUp = 0.0f;
		bool bAtTheEye = false;
	};
	FVeil CloudVeil(const FVector& Eye, float Age);
	/** The sphere inside it that blocks sight (bots' eyes, name tags): a little inside the cards, which thin at the edge. */
	constexpr float CloudSightRadius = 380.0f;
	/** How wide the cloud is `Age` seconds after it burst (its radius, cm): growing to its full size over CloudGrowSeconds, easing, and nothing past its life. */
	float CloudRadiusAt(float Age);
	/** How thick it is then (0 to 1): whole until its last CloudThinSeconds, then thinning to nothing. */
	float CloudOpacityAt(float Age);
	/** Whether it still blocks sight at `Age`: while it is at least half as thick as it was. */
	bool CloudBlocksSightAt(float Age);

	/** Grenades a player comes in with, and the most carried (the design's two). */
	constexpr int32 SpawnCount = 2;
	constexpr int32 MostCarried = 2;

	/** Seconds from leaving the hand to the burst. */
	constexpr float Fuse = 2.5f;
	/** How fast it leaves the hand (about half the range it had at 1500), and how far above the aim (degrees), as a throw goes a little up from where one looks. */
	constexpr float ThrowSpeed = 1060.0f;
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

	/**
	 * Taking a grenade in hand and putting it back, on every machine: the
	 * weapon in hand goes down out of view (as a weapon drawn comes up), then
	 * the grenade comes up in the right hand, the left let go; after the throw,
	 * or a throw put away, the reverse. One number runs it, the switch: from 0
	 * (the weapon up) through 1 (the weapon gone, the hand still down) to 2
	 * (the grenade up).
	 */
	constexpr float PutDownSeconds = 0.15f;
	constexpr float TakeUpSeconds = 0.15f;
	/** The switch `DeltaSeconds` on from `Switch`: toward 2 while the hands are busy with a grenade, else toward 0. */
	float SwitchStep(float Switch, bool bBusy, float DeltaSeconds);
	/** At `Switch`, how far the weapon in hand is down (0 up, 1 out of view) and the grenade hand up (0 to 1). */
	float WeaponDown(float Switch);
	float GrenadeUp(float Switch);
	/** Whether the weapon in hand is put out of sight: from the switch, and for as long as a throw goes on. */
	bool IsWeaponAway(float Switch, bool bThrowing);

	/** Where a grenade leaves the hand, from the eyes along the aim (cm: ahead, right, down). */
	constexpr float ReleaseAhead = 35.0f;
	constexpr float ReleaseRight = 14.0f;
	constexpr float ReleaseDown = 10.0f;
	/** The share of a throw at which the grenade leaves the hand. */
	constexpr float ReleasePhase = ReleaseDelay / ThrowTime;
	/** The right hand with the grenade in it: where it is from the eyes (cm: ahead, right, up, in the frame of the aim), which way its fingers point and its palm faces (in the same axes). */
	struct FHandAt
	{
		FVector At = FVector::ZeroVector;
		FVector Fingers = FVector::UpVector;
		FVector Palm = FVector::ForwardVector;
	};
	/** Held up ready, low on the right ahead of the eyes, the palm turned in so that the grenade in it shows. */
	FHandAt HeldHand();
	/**
	 * At `Phase` (0 to 1, over ThrowTime) of a throw: wound back by the ear,
	 * brought over and forward to let go at ReleasePhase where the grenade
	 * leaves the hand (ReleaseAhead, ReleaseRight, ReleaseDown: the hand a
	 * little short of it, the grenade in its palm), then on down across the
	 * body and out of sight.
	 */
	FHandAt ThrowHand(float Phase);
	/** How much a throw has the arm `Phase` into it: all of it in its first tenth, whether the hand was up yet or not. */
	float ThrowWeight(float Phase);

	/** The arc shown while a throw is held: a dot every PreviewSpacing cm of its flight, none in its first PreviewSkip (too near the eyes), at most PreviewDots. */
	constexpr float PreviewSpacing = 30.0f;
	constexpr float PreviewSkip = 20.0f;
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
