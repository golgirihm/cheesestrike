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

	void FSoundBeat::SetGrid(double NewOrigin, double NewInterval)
	{
		if (NewInterval != Interval)
		{
			LastSlot = -1;
		}
		Origin = NewOrigin;
		Interval = NewInterval;
	}

	double FSoundBeat::LeadFor(double FrameSeconds)
	{
		// Measured: 22 to 54 ms at 60 frames a second, 35 to 65 ms at 30; the end of the frame, the audio thread's
		// update, then the next audio buffer (21 ms).
		return FMath::Clamp(0.036 + 1.5 * FrameSeconds, 0.05, 0.12);
	}

	int64 FSoundBeat::Take(double Now, double Lead)
	{
		const int64 Ahead = static_cast<int64>(FMath::CeilToDouble((Now + Lead - Origin) / Interval));
		LastSlot = FMath::Max<int64>(FMath::Max<int64>(LastSlot + 1, Ahead), 0);
		return LastSlot;
	}

	FQuartzQuantizationBoundary AtTick(int64 Slot)
	{
		FQuartzQuantizationBoundary Boundary(EQuartzCommandQuantization::Tick, static_cast<float>(Slot + 1), EQuarztQuantizationReference::TransportRelative);
		Boundary.bFireOnClockStart = false;
		return Boundary;
	}

	FQuartzQuantizationBoundary AtOnce()
	{
		return FQuartzQuantizationBoundary(EQuartzCommandQuantization::None);
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
