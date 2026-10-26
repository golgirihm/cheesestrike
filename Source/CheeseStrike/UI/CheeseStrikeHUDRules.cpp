// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHUDRules.h"
#include "Math/RandomStream.h"
#include "Rules/CheeseStrikeMice.h"
#include "Rules/CheeseStrikeModes.h"

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

	FVector2D MapOffset(const FVector2D& Screen, float ViewYaw, float Scale)
	{
		const float Radians = FMath::DegreesToRadians(ViewYaw);
		const FVector2D Forward(FMath::Cos(Radians), FMath::Sin(Radians));
		const FVector2D Right(-FMath::Sin(Radians), FMath::Cos(Radians));
		return Scale > 0.0f ? (Right * Screen.X - Forward * Screen.Y) / Scale : FVector2D::ZeroVector;
	}

	FVector2D FollowMark(const FVector2D& Middle, const FVector2D& Mark, float ViewYaw, float Reach)
	{
		// On the map as it is turned, a centimetre a pixel: hold the mark's offset within the reach each way.
		const FVector2D Off = MapPoint(Mark - Middle, ViewYaw, 1.0f);
		const FVector2D Held(FMath::Clamp(Off.X, -Reach, Reach), FMath::Clamp(Off.Y, -Reach, Reach));
		return Mark - MapOffset(Held, ViewYaw, 1.0f);
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

	TArray<FVector2D> ClipToConvex(const TArray<FVector2D>& Polygon, const TArray<FVector2D>& Clip)
	{
		if (Clip.Num() < 3)
		{
			return TArray<FVector2D>();
		}
		// Which side of each edge is in: the side the polygon winds towards.
		float Area = 0.0f;
		for (int32 Index = 0; Index < Clip.Num(); ++Index)
		{
			Area += FVector2D::CrossProduct(Clip[Index], Clip[(Index + 1) % Clip.Num()]);
		}
		const float Winding = Area >= 0.0f ? 1.0f : -1.0f;
		// Sutherland and Hodgman, by each of the clip's edges in turn.
		TArray<FVector2D> Out = Polygon;
		for (int32 Edge = 0; Edge < Clip.Num() && Out.Num() > 0; ++Edge)
		{
			const FVector2D& EdgeFrom = Clip[Edge];
			const FVector2D Along = Clip[(Edge + 1) % Clip.Num()] - EdgeFrom;
			if (Along.IsNearlyZero())
			{
				continue;
			}
			const auto Inside = [&](const FVector2D& Point) { return Winding * FVector2D::CrossProduct(Along, Point - EdgeFrom); };
			const TArray<FVector2D> In = MoveTemp(Out);
			Out.Reset();
			for (int32 Index = 0; Index < In.Num(); ++Index)
			{
				const FVector2D& From = In[Index];
				const FVector2D& To = In[(Index + 1) % In.Num()];
				const float A = Inside(From);
				const float B = Inside(To);
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

	TArray<FVector2D> RoundedRect(const FVector2D& Min, const FVector2D& Max, float Radius, int32 Steps)
	{
		const float R = FMath::Clamp(Radius, 0.0f, FMath::Min(Max.X - Min.X, Max.Y - Min.Y) / 2.0f);
		if (Steps < 0)
		{
			Steps = CornerSteps(R);
		}
		TArray<FVector2D> Corners;
		if (R <= KINDA_SMALL_NUMBER || Steps < 1)
		{
			Corners = {Min, FVector2D(Max.X, Min.Y), Max, FVector2D(Min.X, Max.Y)};
			return Corners;
		}
		// Each corner's arc about its middle, a quarter turn each, going round clockwise on the screen (y down): the top
		// left from pointing left to pointing up, then the top right, the bottom right and the bottom left.
		const FVector2D Middles[4] = {FVector2D(Min.X + R, Min.Y + R), FVector2D(Max.X - R, Min.Y + R), FVector2D(Max.X - R, Max.Y - R), FVector2D(Min.X + R, Max.Y - R)};
		Corners.Reserve(4 * (Steps + 1));
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			const float Start = UE_PI + Corner * UE_HALF_PI;
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const float Angle = Start + UE_HALF_PI * Step / Steps;
				Corners.Add(Middles[Corner] + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
			}
		}
		return Corners;
	}

	int32 CornerSteps(float Radius)
	{
		if (Radius <= CornerError)
		{
			return 1;
		}
		// A chord across `Angle` of the circle strays from it by Radius (1 - cos(Angle / 2)) at its middle.
		const float Widest = 2.0f * FMath::Acos(FMath::Max(-1.0f, 1.0f - CornerError / Radius));
		return FMath::Clamp(FMath::CeilToInt(UE_HALF_PI / FMath::Max(Widest, UE_KINDA_SMALL_NUMBER)), 1, 32);
	}

	TArray<FVector2D> OutwardMiters(TConstArrayView<FVector2D> Outline)
	{
		const int32 Count = Outline.Num();
		TArray<FVector2D> Miters;
		Miters.Init(FVector2D::ZeroVector, Count);
		if (Count < 3)
		{
			return Miters;
		}
		// Which way round it goes: the outward normal of an edge is to its right one way round, its left the other.
		float Area = 0.0f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D& A = Outline[Index];
			const FVector2D& B = Outline[(Index + 1) % Count];
			Area += A.X * B.Y - B.X * A.Y;
		}
		const float Way = Area >= 0.0f ? 1.0f : -1.0f;
		const auto Outward = [Way](const FVector2D& From, const FVector2D& To)
		{
			const FVector2D Along = (To - From).GetSafeNormal();
			return FVector2D(Along.Y, -Along.X) * Way;
		};
		constexpr float Apart = 1e-3f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D& Here = Outline[Index];
			int32 Before = INDEX_NONE;
			int32 After = INDEX_NONE;
			for (int32 Step = 1; Step < Count && Before == INDEX_NONE; ++Step)
			{
				const int32 At = (Index - Step + Count) % Count;
				Before = FVector2D::Distance(Outline[At], Here) > Apart ? At : INDEX_NONE;
			}
			for (int32 Step = 1; Step < Count && After == INDEX_NONE; ++Step)
			{
				const int32 At = (Index + Step) % Count;
				After = FVector2D::Distance(Outline[At], Here) > Apart ? At : INDEX_NONE;
			}
			if (Before == INDEX_NONE || After == INDEX_NONE)
			{
				continue;
			}
			const FVector2D In = Outward(Outline[Before], Here);
			const FVector2D Out = Outward(Here, Outline[After]);
			const float Meet = 1.0f + FVector2D::DotProduct(In, Out);
			FVector2D Miter = Meet > UE_KINDA_SMALL_NUMBER ? (In + Out) / Meet : In * MostMiter;
			if (Miter.Size() > MostMiter)
			{
				Miter = Miter.GetSafeNormal() * MostMiter;
			}
			Miters[Index] = Miter;
		}
		return Miters;
	}

	float InsideRoundedRect(const FVector2D& Point, const FVector2D& Min, const FVector2D& Max, float Radius)
	{
		// From the rectangle shrunk by the radius, then the radius off: a rounded box's distance.
		const float R = FMath::Clamp(Radius, 0.0f, FMath::Min(Max.X - Min.X, Max.Y - Min.Y) / 2.0f);
		const FVector2D Middle = (Min + Max) / 2.0f;
		const FVector2D Half = (Max - Min) / 2.0f - FVector2D(R, R);
		const FVector2D Off(FMath::Abs(Point.X - Middle.X) - Half.X, FMath::Abs(Point.Y - Middle.Y) - Half.Y);
		const float Outside = FVector2D(FMath::Max(Off.X, 0.0f), FMath::Max(Off.Y, 0.0f)).Size() + FMath::Min(FMath::Max(Off.X, Off.Y), 0.0f);
		return R - Outside;
	}

	FVector2D HoldInsideRoundedRect(const FVector2D& Point, const FVector2D& Min, const FVector2D& Max, float Radius)
	{
		if (InsideRoundedRect(Point, Min, Max, Radius) >= 0.0f)
		{
			return Point;
		}
		const float R = FMath::Clamp(Radius, 0.0f, FMath::Min(Max.X - Min.X, Max.Y - Min.Y) / 2.0f);
		// The nearest point of the inner rectangle, then out from it by the radius towards the point.
		const FVector2D Inner(FMath::Clamp(Point.X, Min.X + R, Max.X - R), FMath::Clamp(Point.Y, Min.Y + R, Max.Y - R));
		const FVector2D Out = Point - Inner;
		return Out.IsNearlyZero() ? Inner : Inner + Out.GetSafeNormal() * R;
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

	EPlayerKind KindOf(bool bIsABot)
	{
		return bIsABot ? EPlayerKind::Bot : EPlayerKind::Person;
	}

	FString StatusWord(EStatus Status)
	{
		switch (Status)
		{
		case EStatus::Down:
			return TEXT("DOWN");
		case EStatus::Out:
			return TEXT("OUT");
		case EStatus::Respawning:
			return TEXT("DEAD");
		default:
			return FString();
		}
	}

	float RowAlpha(EStatus Status)
	{
		return Status == EStatus::Out ? 0.45f : Status == EStatus::Respawning ? 0.7f : 1.0f;
	}

	FBoardRow BoardRow(const CheeseStrikeRules::FStanding& Line, EStatus Status, bool bIsABot, bool bMine)
	{
		FBoardRow Row;
		Row.Name = Line.Name;
		Row.Kills = Line.Kills;
		Row.Deaths = Line.Deaths;
		Row.Status = Status;
		Row.Kind = KindOf(bIsABot);
		Row.bMine = bMine;
		return Row;
	}

	FString FitName(const FString& Name, float Width, TFunctionRef<float(const FString&)> Measure)
	{
		if (Measure(Name) <= Width)
		{
			return Name;
		}
		// The longest start of the name that fits with the dots after it; at least its first letter.
		int32 Keep = Name.Len() - 1;
		while (Keep > 1 && Measure(Name.Left(Keep).TrimEnd() + TEXT("...")) > Width)
		{
			--Keep;
		}
		return Name.Left(FMath::Max(1, Keep)).TrimEnd() + TEXT("...");
	}

	FString FitNameIn(const FString& Line, const FString& Name, float Width, TFunctionRef<float(const FString&)> Measure)
	{
		const int32 At = Name.IsEmpty() ? INDEX_NONE : Line.Find(Name, ESearchCase::IgnoreCase);
		if (At == INDEX_NONE)
		{
			return Line;
		}
		// Cut as it is written in the line (an announcement is drawn in capitals).
		const FString Written = Line.Mid(At, Name.Len());
		return Line.Left(At) + FitName(Written, Width, Measure) + Line.Mid(At + Name.Len());
	}

	ECrosshair CrosshairFor(bool bLastStand, bool bEnemyDown, float Distance)
	{
		if (!bEnemyDown)
		{
			return ECrosshair::Usual;
		}
		// A shot at them as the server takes it: only from within reach does it finish them.
		const CheeseStrikeModes::ELastStandHit Hit = CheeseStrikeModes::LastStandHit(bLastStand, true, 1.0f, 1.0f, Distance <= CheeseStrikeModes::FinishRange);
		return Hit == CheeseStrikeModes::ELastStandHit::Absorbed ? ECrosshair::NoEffect : ECrosshair::Usual;
	}

	bool ShowsDeathScreen(bool bOut, float Seconds)
	{
		return !bOut || Seconds < OutDeathScreen;
	}

	float DeathScreenAlpha(bool bOut, float Seconds)
	{
		return bOut ? FMath::Clamp((OutDeathScreen - Seconds) / 0.5f, 0.0f, 1.0f) : 1.0f;
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

	FScoreBlockText FitScoreBlock(const FScoreLine& Line, float Width, float Pad, float Gap,
		TFunctionRef<float(const FString&)> CaptionWidth, TFunctionRef<float(const FString&)> NameWidth, TFunctionRef<float(const FString&)> ScoreWidth)
	{
		FScoreBlockText Text;
		const float Inside = FMath::Max(0.0f, Width - 2.0f * Pad);
		const float Score = ScoreWidth(FString::FromInt(Line.Score));
		const float Most = Inside * ScoreMostShare;
		Text.ScoreScale = Score > Most && Score > 0.0f ? Most / Score : 1.0f;
		// A caption (a side's name) that would be cut has the score made smaller for it first, down to ScoreLeastScale.
		const FString Caption = Line.Caption.ToUpper();
		const float Wanted = Caption.IsEmpty() ? 0.0f : CaptionWidth(Caption);
		if (Score > 0.0f && Inside - Score * Text.ScoreScale - Gap < Wanted)
		{
			Text.ScoreScale = FMath::Max(FMath::Min(Text.ScoreScale, ScoreLeastScale), (Inside - Gap - Wanted) / Score);
		}
		const float Room = FMath::Max(0.0f, Inside - Score * Text.ScoreScale - Gap);
		Text.Caption = Caption.IsEmpty() ? FString() : FitName(Caption, Room, CaptionWidth);
		Text.Name = Line.Name.IsEmpty() ? FString() : FitName(Line.Name.ToUpper(), Room, NameWidth);
		return Text;
	}

	FString TeamScoreLabel(CheeseStrikeModes::EWinBy WinBy, int32 Score)
	{
		switch (WinBy)
		{
		case CheeseStrikeModes::EWinBy::Rounds:
			return Score == 1 ? TEXT("ROUND WON") : TEXT("ROUNDS WON");
		case CheeseStrikeModes::EWinBy::Hill:
			return TEXT("POINTS");
		default:
			return TEXT("KILLS");
		}
	}

	TArray<FStreakPip> StreakPips(int32 Kills)
	{
		using namespace CheeseStrikeStreaks;
		const int32 Last = Streaks[NumStreaks - 1].Kills;
		TArray<FStreakPip> Pips;
		for (int32 Kill = 1; Kill <= Last; ++Kill)
		{
			FStreakPip& Pip = Pips.AddDefaulted_GetRef();
			Pip.bMade = Kill <= Kills;
			Pip.Earns = EarnedAt(Kill);
		}
		return Pips;
	}

	FString StreakNext(int32 Kills)
	{
		using namespace CheeseStrikeStreaks;
		const EStreak Next = NextAfter(FMath::Max(0, Kills));
		if (Next == EStreak::None)
		{
			return TEXT("EVERY STREAK EARNED");
		}
		const FString Name = StreakShortName(Next);
		const int32 ToGo = Info(Next).Kills - FMath::Max(0, Kills);
		return ToGo <= 1 ? FString::Printf(TEXT("%s next kill"), *Name) : FString::Printf(TEXT("%s in %d"), *Name, ToGo);
	}

	FString StreakShortName(CheeseStrikeStreaks::EStreak Kind)
	{
		switch (Kind)
		{
		case CheeseStrikeStreaks::EStreak::AerialCheese:
			return TEXT("UAC");
		case CheeseStrikeStreaks::EStreak::CheeseStrike:
			return TEXT("Cheese Strike");
		case CheeseStrikeStreaks::EStreak::GoudaOverload:
			return TEXT("Gouda Overload");
		default:
			return FString();
		}
	}

	float StreakFillToward(float Shown, int32 Kills, float DeltaSeconds)
	{
		const float Target = static_cast<float>(FMath::Max(0, Kills));
		if (Shown < Target)
		{
			return FMath::Min(Target, Shown + StreakFillRate * DeltaSeconds);
		}
		// Down (a death): a whole track drains in StreakDrainTime.
		const float Rate = CheeseStrikeStreaks::Streaks[CheeseStrikeStreaks::NumStreaks - 1].Kills / StreakDrainTime;
		return FMath::Max(Target, Shown - Rate * DeltaSeconds);
	}

	float StreakPop(float Seconds)
	{
		if (Seconds < 0.0f || Seconds >= StreakPopTime)
		{
			return 1.0f;
		}
		// Punched up to 1.8 at once, back through 1 a third of the way, a little under, and settled.
		const float Along = Seconds / StreakPopTime;
		return 1.0f + 0.8f * FMath::Square(1.0f - Along) * FMath::Cos(Along * UE_PI * 1.5f);
	}

	float FallFlashAlpha(float Seconds)
	{
		if (Seconds < 0.0f || Seconds >= FallFlashTime)
		{
			return 0.0f;
		}
		return FallFlashPeak * FMath::Square(1.0f - Seconds / FallFlashTime);
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

	FVerminLook VerminLook(float Age, float SinceTick)
	{
		FVerminLook Look;
		const float Along = FMath::Clamp(Age / VerminEntrance, 0.0f, 1.0f);
		// Down fast from 2.5 to just under 1 (70% of the time), then back up to 1.
		Look.Scale = Along < 0.7f ? FMath::Lerp(2.5f, 0.9f, 1.0f - FMath::Square(1.0f - Along / 0.7f)) : FMath::Lerp(0.9f, 1.0f, (Along - 0.7f) / 0.3f);
		Look.Alpha = FMath::Clamp(Along * 2.5f, 0.0f, 1.0f);
		if (Age >= VerminEntrance && SinceTick >= 0.0f && SinceTick < VerminPulse)
		{
			Look.Scale += 0.15f * FMath::Square(1.0f - SinceTick / VerminPulse);
		}
		return Look;
	}

	FString VerminWords(float ToGo)
	{
		if (ToGo > 0.0f && ToGo <= CheeseStrikeMice::Warning)
		{
			const int32 Seconds = FMath::CeilToInt(ToGo);
			return FString::Printf(TEXT("VERMIN INVASION IN %d:%02d"), Seconds / 60, Seconds % 60);
		}
		if (ToGo <= 0.0f && ToGo > -VerminCameFor)
		{
			return TEXT("THE VERMIN HAVE COME");
		}
		return FString();
	}
}
