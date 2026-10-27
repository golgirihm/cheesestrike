// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/QuartzQuantizationUtilities.h"

class UObject;

/**
 * The beat of automatic fire as a machine puts it on: when each shot was
 * fired, shown, heard and flashed, by the real clock, kept only while a test
 * or the trace (CheeseStrike.TraceGunfire 1) asks, so that "the gun is a little
 * irregular" can be measured rather than judged by ear. Also the rule that
 * paces the shots another machine is told of, so that they are shown on the
 * beat they were fired on rather than as the network brought them.
 */
namespace CheeseStrikeBeat
{
	enum class EEvent : uint8
	{
		/** The shooter's machine fired it. */
		Fired,
		/** The server allowed it. */
		Served,
		/** A machine showed it (animation, flash and sound together). */
		Shown,
		/** Its sound started. */
		Sounded,
		/** Its muzzle flash lit. */
		Flashed,
		/** Its sound was scheduled on the sound beat: the time it is to be heard at (see FSoundBeat), not when it was asked for. */
		Scheduled,
	};

	/** Whether events are being kept: a test says so, or the trace is on. */
	extern bool bRecording;
	bool IsKeeping();
	/** Notes that `Event` happened for `Weapon` on the machine whose world is `World` (null for any) now. */
	void Note(const UObject* Weapon, EEvent Event);
	/** Notes that `Event` happened for `Weapon` at `Time` (seconds, real clock). */
	void NoteAt(const UObject* Weapon, EEvent Event, double Time);
	/** Forgets everything. */
	void Reset();
	/** The times (seconds, real clock) of `Event` for `Weapon`, in order. */
	TArray<double> Times(const UObject* Weapon, EEvent Event);

	/** How even a run of times is. */
	struct FEvenness
	{
		int32 Intervals = 0;
		double Mean = 0.0;
		double Shortest = 0.0;
		double Longest = 0.0;
		double Deviation = 0.0;
	};
	FEvenness Evenness(const TArray<double>& Times);

	/**
	 * When to show a shot another machine was told of at `Arrival` (seconds), the one before having been shown at
	 * `LastShown`, when the weapon fires every `Interval`: held a `Lead` back at the start of a run (a gap of more than two
	 * intervals) so that the network's clumps and lulls fall inside it, then one `Interval` after the last, or at its
	 * arrival if that is later. If it would come to be more than `Behind` after its arrival (a server that sends faster
	 * than the beat), the run starts over from the arrival.
	 */
	/**
	 * The beat the shot sounds of automatic fire are heard on, kept apart from the frames that show the shots: a shot
	 * is shown in a frame, but its sound is handed to the audio engine's Quartz clock, which starts it at a tick
	 * (sample-accurately), so the sounds come exactly an interval apart whatever the frames do. The first sound of a
	 * run starts the clock, a Lead after its shot was shown; every later one is queued for the first tick after the
	 * shot was shown, the Lead being the room a late frame has. A shot that comes after its tick goes to the next, or
	 * after any sound already queued for that tick, so no two sounds ever share one.
	 */
	struct FSoundBeat
	{
		/** What to do with a shot's sound. */
		struct FPlan
		{
			/** The sound starts the run: reset the clock and play it, at CallAt. */
			bool bStartsRun = false;
			/** The beat (tick) it is heard on, counted from the run's start. */
			int64 Slot = 0;
			/** When to hand it to the audio engine (seconds, real clock): now, or later if its tick is taken. */
			double CallAt = 0.0;
			/** When it is heard (seconds, real clock; for a run's first, when it is handed over). */
			double BeatTime = 0.0;
		};

		/** The share of an interval the first sound of a run is held back, the room a late frame has before the tick. */
		static constexpr double LeadShare = 0.6;
		/** How near before a tick a sound can still be queued for it (the game thread's hand-over to the audio thread takes this long). */
		static constexpr double Guard = 0.012;
		/** A run ends when its sounds stop for this many intervals. */
		static constexpr double EndsAfterIntervals = 2.5;

		bool bRunning = false;
		/** Whether the run's first sound has been handed over yet (it is held back a lead, and a frame may be late); until then the others wait. */
		bool bStarted = false;
		/** The time (real clock) of the run's tick 0. */
		double Origin = 0.0;
		double Interval = 0.0;
		int64 LastSlot = -1;

		/** What to do with the sound of a shot shown at `Now`, the weapon firing every `NewInterval`. Remembers it. */
		FPlan Plan(double Now, double NewInterval);
		/** The run's first sound was handed over at `At`: that is its tick 0. */
		void Started(double At);
		/** Forgets the run. */
		void Reset();
	};

	/**
	 * The Quartz boundary a shot sound is queued on: the clock's next tick (one tick a shot), counted from now. Quartz's
	 * default counts from the bar, where a multiplier of 1 is the bar's first tick: the next bar line, 32 ticks off.
	 */
	FQuartzQuantizationBoundary NextTick();

	double PaceShow(double Arrival, double LastShown, double Interval, double Lead, double Behind);
}
