// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBeat.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

namespace CheeseStrikeBeat
{
	bool bRecording = false;

	namespace
	{
		FCriticalSection Lock;
		TMap<TPair<const UObject*, uint8>, TArray<double>> Kept;
	}

	bool IsKeeping()
	{
		return bRecording;
	}

	void Note(const UObject* Weapon, EEvent Event)
	{
		if (!bRecording)
		{
			return;
		}
		FScopeLock Guard(&Lock);
		Kept.FindOrAdd(TPair<const UObject*, uint8>(Weapon, static_cast<uint8>(Event))).Add(FPlatformTime::Seconds());
	}

	void NoteAt(const UObject* Weapon, EEvent Event, double Time)
	{
		if (!bRecording)
		{
			return;
		}
		FScopeLock Guard(&Lock);
		Kept.FindOrAdd(TPair<const UObject*, uint8>(Weapon, static_cast<uint8>(Event))).Add(Time);
	}

	void Reset()
	{
		FScopeLock Guard(&Lock);
		Kept.Reset();
	}

	TArray<double> Times(const UObject* Weapon, EEvent Event)
	{
		FScopeLock Guard(&Lock);
		const TArray<double>* Found = Kept.Find(TPair<const UObject*, uint8>(Weapon, static_cast<uint8>(Event)));
		return Found != nullptr ? *Found : TArray<double>();
	}

	FEvenness Evenness(const TArray<double>& Times)
	{
		FEvenness Result;
		if (Times.Num() < 2)
		{
			return Result;
		}
		Result.Intervals = Times.Num() - 1;
		Result.Shortest = BIG_NUMBER;
		for (int32 Index = 1; Index < Times.Num(); ++Index)
		{
			const double Gap = Times[Index] - Times[Index - 1];
			Result.Mean += Gap;
			Result.Shortest = FMath::Min(Result.Shortest, Gap);
			Result.Longest = FMath::Max(Result.Longest, Gap);
		}
		Result.Mean /= Result.Intervals;
		for (int32 Index = 1; Index < Times.Num(); ++Index)
		{
			Result.Deviation += FMath::Square(Times[Index] - Times[Index - 1] - Result.Mean);
		}
		Result.Deviation = FMath::Sqrt(Result.Deviation / Result.Intervals);
		return Result;
	}

	FSoundBeat::FPlan FSoundBeat::Plan(double Now, double NewInterval)
	{
		FPlan Result;
		const double Last = bRunning ? Origin + LastSlot * Interval : 0.0;
		if (!bRunning || NewInterval != Interval || Now - Last > EndsAfterIntervals * Interval)
		{
			Interval = NewInterval;
			bRunning = true;
			LastSlot = 0;
			bStarted = false;
			Origin = Now + LeadShare * Interval;
			Result.bStartsRun = true;
			Result.Slot = 0;
			Result.CallAt = Origin;
			Result.BeatTime = Origin;
			return Result;
		}
		// The first tick that can still be had for a sound handed over now.
		int64 Slot = static_cast<int64>(FMath::FloorToDouble((Now + Guard - Origin) / Interval)) + 1;
		Slot = FMath::Max<int64>(Slot, 1);
		Result.CallAt = Now;
		if (Slot <= LastSlot)
		{
			// That tick has a sound: this one takes the next, handed over in the middle of the tick before it.
			Slot = LastSlot + 1;
			Result.CallAt = FMath::Max(Now, Origin + (Slot - 1) * Interval + 0.5 * Interval);
		}
		if (!bStarted)
		{
			Result.CallAt = FMath::Max(Result.CallAt, Origin + 0.001);
		}
		LastSlot = Slot;
		Result.Slot = Slot;
		Result.BeatTime = Origin + Slot * Interval;
		return Result;
	}

	void FSoundBeat::Started(double At)
	{
		Origin = At;
		bStarted = true;
	}

	void FSoundBeat::Reset()
	{
		bRunning = false;
		LastSlot = -1;
	}

	FQuartzQuantizationBoundary NextTick()
	{
		return FQuartzQuantizationBoundary(EQuartzCommandQuantization::Tick, 1.0f, EQuarztQuantizationReference::CurrentTimeRelative);
	}

	double PaceShow(double Arrival, double LastShown, double Interval, double Lead, double Behind)
	{
		if (Arrival - LastShown > 2.0 * Interval)
		{
			return Arrival + Lead;
		}
		const double At = FMath::Max(Arrival, LastShown + Interval);
		return At - Arrival > Behind ? Arrival + Lead : At;
	}
}
