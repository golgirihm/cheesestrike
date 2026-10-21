// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMice.h"
#include "Kismet/GameplayStatics.h"

namespace CheeseStrikeMice
{
	int32 MiceFor(int32 Blocks)
	{
		if (Blocks <= 0)
		{
			return 0;
		}
		return FMath::Clamp(FMath::RoundToInt(static_cast<float>(Blocks) / BlocksPerMouse), MinMice, MaxMice);
	}

	bool HasMice(const FString& Options)
	{
		return UGameplayStatics::ParseOption(Options, TEXT("Mice")) != TEXT("0");
	}

	float ArrivalFrom(const FString& Options)
	{
		const FString Value = UGameplayStatics::ParseOption(Options, TEXT("MiceAt"));
		return Value.IsNumeric() ? FMath::Max(0.0f, FCString::Atof(*Value)) : ArriveAfter;
	}

	float CheeseCost(float Distance, float Height, bool bClaimedByAnother)
	{
		return Distance + HeightCost * FMath::Max(0.0f, Height) + (bClaimedByAnother ? Claimed : 0.0f);
	}

	int32 Stage(float Health, float Full)
	{
		if (Full <= 0.0f || Health >= Full)
		{
			return 0;
		}
		// Bitten into at all: the first stage; two thirds gone, the third.
		const float Eaten = 1.0f - FMath::Max(0.0f, Health) / Full;
		return FMath::Clamp(1 + FMath::FloorToInt(Eaten * 3.0f), 1, 3);
	}

	float FearAfter(float Fear, float Seconds)
	{
		return FMath::Max(0.0f, Fear - FearFades * Seconds);
	}

	bool MayEat(bool bEdible, float Health, bool bFalling)
	{
		return bEdible && Health > 0.0f && !bFalling;
	}

	bool IsHeld(const FBox2D& Footprint, TConstArrayView<FBox2D> Contacts)
	{
		// The corners of every contact, kept to the footprint.
		TArray<FVector2D> Points;
		for (const FBox2D& Contact : Contacts)
		{
			const FVector2D Low(FMath::Max(Contact.Min.X, Footprint.Min.X), FMath::Max(Contact.Min.Y, Footprint.Min.Y));
			const FVector2D High(FMath::Min(Contact.Max.X, Footprint.Max.X), FMath::Min(Contact.Max.Y, Footprint.Max.Y));
			if (High.X - Low.X < HeldThinnest || High.Y - Low.Y < HeldThinnest)
			{
				continue;
			}
			Points.Append({Low, FVector2D(High.X, Low.Y), High, FVector2D(Low.X, High.Y)});
		}
		if (Points.Num() < 3)
		{
			return false;
		}
		// Their convex hull (Andrew's monotone chain), anticlockwise.
		Points.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X || (A.X == B.X && A.Y < B.Y); });
		auto Turn = [](const FVector2D& O, const FVector2D& A, const FVector2D& B) { return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X); };
		TArray<FVector2D> Hull;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const int32 Start = Hull.Num();
			for (int32 Index = 0; Index < Points.Num(); ++Index)
			{
				const FVector2D& Point = Points[Pass == 0 ? Index : Points.Num() - 1 - Index];
				while (Hull.Num() >= Start + 2 && Turn(Hull[Hull.Num() - 2], Hull.Last(), Point) <= 0.0)
				{
					Hull.Pop();
				}
				Hull.Add(Point);
			}
			Hull.Pop();
		}
		if (Hull.Num() < 3)
		{
			return false;
		}
		// The middle inside every edge, by the margin.
		const FVector2D Middle = Footprint.GetCenter();
		const FVector2D Size = Footprint.GetSize();
		const double Margin = HeldMargin * FMath::Min(Size.X, Size.Y);
		for (int32 Index = 0; Index < Hull.Num(); ++Index)
		{
			const FVector2D& A = Hull[Index];
			const FVector2D& B = Hull[(Index + 1) % Hull.Num()];
			const double Length = FVector2D::Distance(A, B);
			if (Length <= UE_KINDA_SMALL_NUMBER)
			{
				continue;
			}
			if (Turn(A, B, Middle) / Length < Margin)
			{
				return false;
			}
		}
		return true;
	}
}
