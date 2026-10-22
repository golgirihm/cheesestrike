// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/CheeseStrikeShot.h"

/**
 * How a bot plays, as plain functions over plain values, so that they can be
 * tested without a world (docs/unreal/design.md, "Bots"): how quickly it
 * notices an enemy, how well it aims and how that steadies, how fast it
 * turns, how it fires in bursts, what it can see, and how far it keeps from
 * whoever it fights. ACheeseStrikeBotController plays by them.
 */
namespace CheeseStrikeBots
{
	/** A skill, as the host picks it in the lobby. */
	struct FSkill
	{
		const TCHAR* Name;
		/** Seconds from first seeing an enemy to the first shot at it. */
		float Reaction;
		/** Degrees off the aim at first, and the share of that it steadies to over SteadyTime of tracking. */
		float AimError;
		float Steadies;
		/** Shots in a burst, and seconds of rest between bursts. */
		int32 Burst;
		float Rest;
		/** How fast it turns, in radians a second. */
		float TurnRate;
	};
	constexpr int32 NumSkills = 3;
	/** Easy, Normal, Hard. */
	extern const FSkill Skills[NumSkills];
	constexpr int32 DefaultSkill = 1;
	/** Skill `Index`, kept within the table. */
	const FSkill& Skill(int32 Index);
	/** The skill a map URL's `?BotSkill=` picks, or `Otherwise`. */
	int32 SkillFrom(const FString& Options, int32 Otherwise = DefaultSkill);

	/** Seconds of tracking over which a bot's aim steadies. */
	constexpr float SteadyTime = 1.5f;
	/** Degrees off a bot's aim at a target it has tracked for `Tracking` seconds. */
	float AimError(const FSkill& Skill, float Tracking);

	/** Turns `Current` toward `Wanted`, no faster than `RadiansPerSecond`, over `DeltaSeconds`; the shorter way round. */
	FRotator TurnToward(const FRotator& Current, const FRotator& Wanted, float RadiansPerSecond, float DeltaSeconds);

	/** How far a bot sees (cm), how wide (half the cone, in degrees), and how close it notices anyone, wherever they are. */
	constexpr float SightRange = 6000.0f;
	constexpr float SightHalfAngle = 60.0f;
	constexpr float NoticeAnywhere = 400.0f;
	/** How far a shot is heard (cm): a bot with nobody in sight turns to it. */
	constexpr float HearingRange = 3000.0f;
	/** Whether something `ToTarget` from a bot facing `Facing` is where the bot could see it (line of sight aside). */
	bool InSight(const FVector& Facing, const FVector& ToTarget);

	/** A bot fights from this share of its weapon's range, backs off from anyone closer than BackOff (cm) (or half its fighting distance, for a weapon that fights closer), and goes for a wedge when its health is under LowHealth. */
	constexpr float FightRangeShare = 0.45f;
	constexpr float BackOff = 600.0f;
	constexpr float LowHealth = 45.0f;
	/** What a bot in a fight does with its feet. */
	enum class EMove : uint8
	{
		/** Too far: closes in. */
		Approach,
		/** In its range: holds there, stepping side to side. */
		Hold,
		/** Too close: steps back. */
		BackOff,
		/** Hurt, with a wedge to be had: gets away to it, firing as it goes. */
		Retreat,
	};
	/** `bWedges`: whether the map has wedges (health) to go for; a hurt bot with none to go for fights on. */
	EMove MoveFor(float Distance, float WeaponRange, float Health, bool bWedges);

	/**
	 * Whether a bot opens fire on someone `Distance` away with a weapon reaching `WeaponRange`:
	 * within LongShotShare of its reach (where the Cheddar Blaster's damage is down to its floor),
	 * or further if they are shooting it. No spraying at specks across the map.
	 */
	constexpr float LongShotShare = 0.6f;
	bool WouldFireAt(float Distance, float WeaponRange, bool bTheyShootMe);

	/**
	 * Its weapons. A bot comes in with a primary suited to its skill, picked
	 * at random from those it plays well (an easy bot takes what forgives a
	 * poor aim, a hard one the rifles that reward a good one).
	 */
	FName PrimaryFor(int32 SkillIndex, FRandomStream& Random);
	/** The weights of each primary for a skill, in CheeseStrikeArsenal::InSlot(Primary)'s order. */
	TArray<TPair<FName, float>> PrimaryWeights(int32 SkillIndex);

	/** Draws the knife on anyone closer than this (cm, from middle to middle), and keeps it out until they are further than KnifeLetGo. */
	constexpr float KnifeFrom = 250.0f;
	constexpr float KnifeLetGo = 350.0f;
	/** With its primary's magazine empty in a fight, it draws the sidearm rather than reload with an enemy closer than this (cm). */
	constexpr float SidearmWithin = 2000.0f;
	/** What a bot has to choose from: what it carries and may draw, and what is left in its primary. */
	struct FArmed
	{
		bool bPrimary = false;
		bool bSidearm = false;
		bool bKnife = false;
		/** Rounds left in the primary's magazine, and whether there are any more to reload it with. */
		int32 PrimaryMagazine = 0;
		bool bPrimaryReserve = false;
	};
	/**
	 * Which weapon a bot wants in hand: the knife for someone close; the
	 * sidearm when the primary is spent, or its magazine empty with an enemy
	 * near (quicker than reloading), until the fight is over; otherwise the
	 * primary. `Distance` is to the enemy it fights (`bFighting`), `Current`
	 * the slot in hand. Only what it may draw is chosen.
	 */
	EWeaponSlot ChooseWeapon(const FArmed& Armed, bool bFighting, float Distance, EWeaponSlot Current);
	/** Whether a bot strikes with a knife reaching `Reach` at someone `Distance` from its eyes. */
	bool WouldStrikeAt(float Distance, float Reach);

	/** Seconds without progress toward where it is going before a bot finds another way. */
	constexpr float StuckTime = 1.2f;
	/** Seconds a bot keeps looking for an enemy it lost sight of, at where it saw them last. */
	constexpr float SearchTime = 6.0f;
	/** Seconds between side steps in a fight, at least and at most. */
	constexpr float StepMin = 0.5f;
	constexpr float StepMax = 1.2f;

	/** Bursts: a bot holds the trigger for a burst's shots, then rests. */
	struct FBursts
	{
		bool bFiring = false;
		/** When the current burst or rest ends. */
		float Until = 0.0f;
	};
	/**
	 * Whether a bot with `Skill` holds the trigger at time `Now`, given whether it
	 * `bWantsToFire` (on target, its reaction over), with a weapon firing every
	 * `FireInterval` s; advances `Bursts`. A bot that stops wanting to fire stops at once, and
	 * starts a fresh burst when it wants to again, unless it is resting.
	 */
	bool HoldsTrigger(FBursts& Bursts, const FSkill& Skill, bool bWantsToFire, float FireInterval, float Now);
	/**
	 * Execution: going to finish an enemy down, who can aim only within
	 * CheeseStrikeDowned::ConeYaw of the way their body faces. A bot keeps
	 * out of that aim: outside it (by half FlankMargin more), it goes straight in
	 * to stand FlankStandOff beside them, on its own side of them; in it, it
	 * makes for the nearer edge, FlankMargin past it, circling no further out
	 * than FlankRadius, a step at a time (ZigzagStep ahead) swerving
	 * ZigzagWidth from side to side across their line of fire every ZigzagPeriod, never straight down
	 * the barrel.
	 */
	constexpr float FlankMargin = 25.0f;
	constexpr float FlankStandOff = 150.0f;
	constexpr float FlankRadius = 600.0f;
	constexpr float ZigzagStep = 300.0f;
	constexpr float ZigzagWidth = 130.0f;
	constexpr float ZigzagPeriod = 1.2f;
	/** How far round from the way someone down at `Downed` faces (`Yaw`) `At` is, in degrees either side (-180 to 180), on the flat. */
	float BearingFromDowned(const FVector& At, const FVector& Downed, float Yaw);
	/** Whether `At` is where someone down at `Downed` facing `Yaw` can aim, give or take `Margin` degrees. */
	bool InDownedAim(const FVector& At, const FVector& Downed, float Yaw, float Margin = 0.0f);
	/**
	 * Where a bot at `At` makes for next, to finish someone down at `Downed`
	 * facing `Yaw`, at time `Now` (for the swerving). In their aim it goes
	 * round by the nearer edge, or with `Way` -1 (having got stuck that
	 * way) by the further.
	 */
	FVector FinishApproach(const FVector& At, const FVector& Downed, float Yaw, float Now, float Way = 1.0f);
}
