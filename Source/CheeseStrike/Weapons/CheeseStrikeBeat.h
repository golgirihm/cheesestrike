// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

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
	};

	/** Whether events are being kept: a test says so, or the trace is on. */
	extern bool bRecording;
	bool IsKeeping();
	/** Notes that `Event` happened for `Weapon` on the machine whose world is `World` (null for any) now. */
	void Note(const UObject* Weapon, EEvent Event);
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
	double PaceShow(double Arrival, double LastShown, double Interval, double Lead, double Behind);
}
