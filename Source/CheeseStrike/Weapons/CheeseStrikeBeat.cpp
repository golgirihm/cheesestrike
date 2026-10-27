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
