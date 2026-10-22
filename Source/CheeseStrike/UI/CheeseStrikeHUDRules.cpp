// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHUDRules.h"
#include "Math/RandomStream.h"

namespace CheeseStrikeHUD
{
	namespace
	{
		float Ease(float Fraction)
		{
			const float P = FMath::Clamp(Fraction, 0.0f, 1.0f);
			return P * P * (3.0f - 2.0f * P);
		}

		/** How far inside edge `Edge` (left, right, top, bottom) of the rectangle a point is: negative outside. */
		float Inside(const FVector2D& Point, int32 Edge, const FVector2D& Min, const FVector2D& Max)
		{
			switch (Edge)
			{
			case 0: return Point.X - Min.X;
			case 1: return Max.X - Point.X;
			case 2: return Point.Y - Min.Y;
			default: return Max.Y - Point.Y;
			}
		}
	}

	FVector2D MapPoint(const FVector2D& Offset, float ViewYaw, float Scale)
	{
		const float Radians = FMath::DegreesToRadians(ViewYaw);
		const FVector2D Forward(FMath::Cos(Radians), FMath::Sin(Radians));
		const FVector2D Right(-FMath::Sin(Radians), FMath::Cos(Radians));
		return FVector2D(FVector2D::DotProduct(Offset, Right), -FVector2D::DotProduct(Offset, Forward)) * Scale;
	}

	FVector2D NorthMark(float ViewYaw, float HalfSize)
	{
		const float North = FMath::DegreesToRadians(NorthYaw);
		const FVector2D Way = MapPoint(FVector2D(FMath::Cos(North), FMath::Sin(North)), ViewYaw, 1.0f);
		const float Longest = FMath::Max(FMath::Abs(Way.X), FMath::Abs(Way.Y));
		return Longest > UE_SMALL_NUMBER ? Way * (HalfSize / Longest) : FVector2D(0.0f, -HalfSize);
	}

	TArray<FVector2D> ConvexHull(TArray<FVector2D> Points)
	{
		if (Points.Num() < 3)
		{
			return Points;
		}
		Points.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X || (A.X == B.X && A.Y < B.Y); });
		const auto Turn = [](const FVector2D& O, const FVector2D& A, const FVector2D& B) { return FVector2D::CrossProduct(A - O, B - O); };
		TArray<FVector2D> Hull;
		Hull.Reserve(Points.Num() + 1);
		// Andrew's monotone chain: the lower side, then the upper.
		for (const FVector2D& Point : Points)
		{
			while (Hull.Num() >= 2 && Turn(Hull[Hull.Num() - 2], Hull.Last(), Point) <= 0.0f)
			{
				Hull.Pop(EAllowShrinking::No);
			}
			Hull.Add(Point);
		}
		const int32 Lower = Hull.Num() + 1;
		for (int32 Index = Points.Num() - 2; Index >= 0; --Index)
		{
			while (Hull.Num() >= Lower && Turn(Hull[Hull.Num() - 2], Hull.Last(), Points[Index]) <= 0.0f)
			{
				Hull.Pop(EAllowShrinking::No);
			}
			Hull.Add(Points[Index]);
		}
		Hull.Pop(EAllowShrinking::No);
		return Hull;
	}

	TArray<FVector2D> ClipToRect(const TArray<FVector2D>& Polygon, const FVector2D& Min, const FVector2D& Max)
	{
		// Sutherland and Hodgman: cut by each of the four edges in turn.
		TArray<FVector2D> Out = Polygon;
		for (int32 Edge = 0; Edge < 4 && Out.Num() > 0; ++Edge)
		{
			const TArray<FVector2D> In = MoveTemp(Out);
			Out.Reset();
			for (int32 Index = 0; Index < In.Num(); ++Index)
			{
				const FVector2D& From = In[Index];
				const FVector2D& To = In[(Index + 1) % In.Num()];
				const float A = Inside(From, Edge, Min, Max);
				const float B = Inside(To, Edge, Min, Max);
				if (A >= 0.0f)
				{
					Out.Add(From);
				}
				if ((A >= 0.0f) != (B >= 0.0f))
				{
					Out.Add(From + (To - From) * (A / (A - B)));
				}
			}
		}
		return Out.Num() >= 3 ? Out : TArray<FVector2D>();
	}

	float NarrowestWidth(const TArray<FVector2D>& Polygon)
	{
		if (Polygon.Num() < 3)
		{
			return 0.0f;
		}
		// Across from each edge, the furthest corner; the least of those.
		float Narrowest = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const FVector2D& From = Polygon[Index];
			const FVector2D Along = (Polygon[(Index + 1) % Polygon.Num()] - From).GetSafeNormal();
			if (Along.IsNearlyZero())
			{
				continue;
			}
			float Furthest = 0.0f;
			for (const FVector2D& Corner : Polygon)
			{
				Furthest = FMath::Max(Furthest, FMath::Abs(FVector2D::CrossProduct(Along, Corner - From)));
			}
			Narrowest = FMath::Min(Narrowest, Furthest);
		}
		return Narrowest == TNumericLimits<float>::Max() ? 0.0f : Narrowest;
	}

	ELayer LayerFor(float Bottom, float Top, float Feet, bool bRamp, bool bWide)
	{
		// Over the head (a little over: a lintel 2.5 m up leaves a door open), or a level below.
		if (Bottom > Feet + 230.0f)
		{
			return ELayer::Hidden;
		}
		if (Top < Feet - 150.0f)
		{
			return ELayer::Below;
		}
		if (bRamp || Top <= Feet + 50.0f)
		{
			return ELayer::Floor;
		}
		if (Top <= Feet + 140.0f)
		{
			return ELayer::Low;
		}
		return bWide && Top > Feet + 230.0f ? ELayer::Raised : ELayer::Wall;
	}

	FSweep SweepAt(float Now, float Ends, float RevealTime, float Period)
	{
		FSweep Sweep;
		const float Since = Now - (Ends - RevealTime);
		if (Ends <= 0.0f || Now >= Ends || Since < 0.0f || Period <= 0.0f)
		{
			return Sweep;
		}
		Sweep.Index = FMath::FloorToInt(Since / Period);
		Sweep.Since = Since - Sweep.Index * Period;
		Sweep.Fade = 1.0f - 0.65f * Sweep.Since / Period;
		return Sweep;
	}

	float DamageArcAngle(float ViewYaw, const FVector2D& At, const FVector2D& From)
	{
		const FVector2D Way = From - At;
		if (Way.IsNearlyZero())
		{
			return 0.0f;
		}
		return FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(Way.Y, Way.X)) - ViewYaw);
	}

	float DamageArcFade(float Age)
	{
		constexpr float Hold = 0.35f;
		if (Age < Hold)
		{
			return 1.0f;
		}
		return 1.0f - Ease((Age - Hold) / (DamageArcLife - Hold));
	}

	TArray<float> HealthFills(float Health, float MaxHealth, int32 Segments)
	{
		TArray<float> Fills;
		Fills.Init(0.0f, FMath::Max(Segments, 0));
		if (Segments <= 0 || MaxHealth <= 0.0f || Health <= 0.0f)
		{
			return Fills;
		}
		const float Blocks = FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) * Segments;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			Fills[Index] = FMath::Clamp(Blocks - Index, 0.0f, 1.0f);
		}
		Fills[0] = FMath::Max(Fills[0], 0.08f);
		return Fills;
	}

	bool IsHealthLow(float Health, float MaxHealth)
	{
		return MaxHealth > 0.0f && Health > 0.0f && Health <= VignettePulseUnder * MaxHealth / 100.0f;
	}

	FVignette VignetteFor(float Health, float MaxHealth, float Seconds)
	{
		FVignette Look;
		if (Health <= 0.0f || MaxHealth <= 0.0f || Health >= VignetteFrom * MaxHealth / 100.0f)
		{
			return Look;
		}
		const float Strength = 1.0f - Health / (VignetteFrom * MaxHealth / 100.0f);
		Look.Alpha = 0.4f + 0.4f * Strength;
		Look.Reach = Strength;
		if (Health < VignettePulseUnder * MaxHealth / 100.0f)
		{
			// A slow breath, a little over once a second.
			const float Breath = 0.5f + 0.5f * FMath::Sin(Seconds * 2.0f * UE_PI * 1.2f);
			Look.Alpha *= 0.8f + 0.2f * Breath;
			Look.Reach = FMath::Min(1.0f, Look.Reach + 0.08f * Breath);
		}
		return Look;
	}

	EStatus StatusOf(bool bOut, bool bDead, bool bDown)
	{
		return bOut ? EStatus::Out : bDead ? EStatus::Respawning : bDown ? EStatus::Down : EStatus::Alive;
	}

	int32 SecondsToComeBack(float RespawnDelay, float Now, float DiedAt)
	{
		// A joiner's idea of the server's time can lag the death it was told of: never more than the whole delay.
		return FMath::Clamp(FMath::CeilToInt(RespawnDelay - (Now - DiedAt)), 0, FMath::CeilToInt(RespawnDelay));
	}

	uint64 SteamIdFrom(const FString& Type, const FString& Id)
	{
		if (!Type.Equals(TEXT("STEAM"), ESearchCase::IgnoreCase) || Id.IsEmpty() || Id.Len() > 20)
		{
			return 0;
		}
		for (const TCHAR Character : Id)
		{
			if (!FChar::IsDigit(Character))
			{
				return 0;
			}
		}
		return FCString::Strtoui64(*Id, nullptr, 10);
	}

	FString Initials(const FString& Name)
	{
		TArray<FString> Words;
		Name.TrimStartAndEnd().ParseIntoArrayWS(Words);
		if (Words.Num() == 0)
		{
			return TEXT("?");
		}
		if (Words.Num() == 1)
		{
			return Words[0].Left(2).ToUpper();
		}
		return (Words[0].Left(1) + Words[1].Left(1)).ToUpper();
	}

	FScoreSummary FreeForAll(const TArray<CheeseStrikeRules::FStanding>& Sorted, const FString& Me, int32 Limit)
	{
		FScoreSummary Summary;
		Summary.Limit = Limit;
		Summary.Goal = FString::Printf(TEXT("FIRST TO %d"), Limit);
		const int32 Mine = Sorted.IndexOfByPredicate([&Me](const CheeseStrikeRules::FStanding& Line) { return Line.Name == Me; });
		if (Mine != INDEX_NONE)
		{
			Summary.Lines.Add({TEXT("YOU"), Sorted[Mine].Name, Sorted[Mine].Kills, INDEX_NONE, true});
		}
		const int32 Best = Mine == 0 ? 1 : 0;
		if (Sorted.IsValidIndex(Best))
		{
			Summary.Lines.Add({Mine == 0 ? TEXT("2ND") : TEXT("LEADER"), Sorted[Best].Name, Sorted[Best].Kills, INDEX_NONE, false});
		}
		return Summary;
	}

	FScoreSummary GunGame(const TArray<CheeseStrikeRules::FStanding>& SortedBySteps, const FString& Me, int32 Steps)
	{
		FScoreSummary Summary = FreeForAll(SortedBySteps, Me, Steps);
		Summary.Goal = FString::Printf(TEXT("THROUGH ALL %d WEAPONS"), Steps);
		return Summary;
	}

	FScoreSummary Teams(const int32 (&Scores)[CheeseStrikeRules::Teams], int32 MyTeam, int32 Limit, const FString& Goal)
	{
		FScoreSummary Summary;
		Summary.Limit = Limit;
		Summary.Goal = Goal;
		for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
		{
			Summary.Lines.Add({FString(CheeseStrikeRules::TeamNames[Team]), FString(), Scores[Team], Team, Team == MyTeam});
		}
		return Summary;
	}

	FTagLook TagLook(float Distance)
	{
		FTagLook Look;
		Look.Scale = FMath::Lerp(1.0f, 0.6f, FMath::Clamp((Distance - 1000.0f) / 4000.0f, 0.0f, 1.0f));
		Look.Alpha = FMath::Lerp(1.0f, 0.45f, FMath::Clamp((Distance - 2500.0f) / 4500.0f, 0.0f, 1.0f));
		return Look;
	}

	FDrip Drip(int32 Index, float Seconds)
	{
		FRandomStream Random(7919 * (Index + 1));
		FDrip Drip;
		Drip.X = (Index + 0.5f + Random.FRandRange(-0.35f, 0.35f)) / Drips;
		Drip.Width = 0.013f + 0.037f * FMath::Square(Random.FRand());
		// Most run a little way; a few run most of the way down.
		const float Reach = 0.05f + 0.8f * FMath::Pow(Random.FRand(), 2.6f);
		const float Delay = Random.FRandRange(0.05f, 0.45f);
		const float Run = MeltTime - Delay + Random.FRandRange(-0.2f, 0.2f);
		if (Seconds > Delay)
		{
			const float Fraction = (Seconds - Delay) / Run;
			Drip.Length = Reach * Ease(Fraction);
			// Done running, it still creeps.
			if (Fraction > 1.0f)
			{
				Drip.Length += Reach * 0.04f * (Seconds - Delay - Run);
			}
		}
		return Drip;
	}

	float SheetDepth(float Seconds)
	{
		const float Fraction = FMath::Clamp(Seconds / 0.35f, 0.0f, 1.0f);
		return 0.1f * (1.0f - FMath::Square(1.0f - Fraction));
	}

	float LetterSag(int32 Index, float Seconds)
	{
		FRandomStream Random(104729 * (Index + 3));
		const float Amount = Random.FRandRange(0.3f, 1.0f);
		const float Start = 0.45f + Random.FRandRange(0.0f, 0.3f);
		return Amount * Ease((Seconds - Start) / 1.1f);
	}
}
