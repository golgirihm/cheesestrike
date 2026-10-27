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
	 * The beat the shot sounds of automatic fire are heard on, kept apart from the frames that show the shots: a shot
	 * is shown in a frame, but its sound is handed to the audio engine's Quartz clock, which starts it at a tick
	 * (sample-accurately), so the sounds come exactly an interval apart whatever the frames do. The weapon's clock
	 * ticks once an interval from when it was last set (tick 0), and each shot's sound takes a tick of its own, counted
	 * from there: the first tick far enough ahead for the audio thread to have the sound by then (Lead), or the one
	 * after the last sound's, whichever is later, so that no two sounds ever share a tick and none is lost.
	 *
	 * The tick is named by its number since the clock was set (Quartz's transport-relative count), not as the next tick
	 * from whenever the audio thread takes the sound: that comes 20 to 65 ms after it is handed over (the end of the
	 * game's frame, the audio thread, then an audio buffer), so two sounds handed over either side of a tick were put on
	 * the same next tick, heard as one shot.
	 */
	struct FSoundBeat
	{
		/** The time (seconds, real clock) of tick 0 and the seconds between ticks. */
		double Origin = 0.0;
		double Interval = 0.0;
		/** The last tick taken by a sound (-1: none since the grid was set). */
		int64 LastSlot = -1;

		/** Ticks every `NewInterval` from `NewOrigin` (an estimate of the clock's; it moves a little). A new interval is a new count. */
		void SetGrid(double NewOrigin, double NewInterval);
		/** How far ahead a sound handed over at a frame of `FrameSeconds` must be queued for the audio thread to have it in time. */
		static double LeadFor(double FrameSeconds);
		/** The tick for a shot's sound handed over at `Now`, `Lead` ahead at least and after the last; taken. */
		int64 Take(double Now, double Lead);
		/** The time (real clock) of tick `Slot`. */
		double TickTime(int64 Slot) const { return Origin + Slot * Interval; }
	};

	/**
	 * The Quartz boundary a shot sound is queued on: tick `Slot` counted from when the clock was set (Quartz's next
	 * multiple of Slot + 1 ticks from the transport's start, which is tick Slot itself while it is ahead). Not the bar's
	 * first tick (Quartz's default, the next bar line, 32 ticks off), nor the next tick from whenever the audio thread
	 * takes it.
	 */
	FQuartzQuantizationBoundary AtTick(int64 Slot);
	/** The boundary the clock's transport is reset on when it is set: at once (the handle's own ResetTransport waits for the next bar). */
	FQuartzQuantizationBoundary AtOnce();

	/**
	 * When to show a shot another machine was told of at `Arrival` (seconds), the one before having been shown at
	 * `LastShown`, when the weapon fires every `Interval`: held a `Lead` back at the start of a run (a gap of more than two
	 * intervals) so that the network's clumps and lulls fall inside it, then one `Interval` after the last, or at its
	 * arrival if that is later. If it would come to be more than `Behind` after its arrival (a server that sends faster
	 * than the beat), the run starts over from the arrival.
	 */
	double PaceShow(double Arrival, double LastShown, double Interval, double Lead, double Behind);
}
