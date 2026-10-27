// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHUDDraw.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GlobalRenderResources.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Rules/CheeseStrikeModes.h"
#include "CheeseStrikeHUDRules.h"
#include "TextureResource.h"
#include "UObject/Package.h"

namespace CheeseStrikeHUDDraw
{
	FLinearColor SideColour(int32 Team)
	{
		return CheeseStrikeModes::TeamColour(Team);
	}

	float Unit(const UCanvas* Canvas)
	{
		return Canvas != nullptr ? FMath::Max(0.5f, Canvas->SizeY / 1080.0f) : 1.0f;
	}

	FLinearColor Faded(const FLinearColor& Colour, float Alpha)
	{
		return FLinearColor(Colour.R, Colour.G, Colour.B, Colour.A * Alpha);
	}

	FSlateFontInfo Font(float Size)
	{
		// The canvas draws a Slate font only through a UFont that caches at runtime: one made once, holding Slate's own
		// Roboto (whose faces are files under Engine/Content/Slate/Fonts, staged with every game).
		static TWeakObjectPtr<UFont> Made;
		if (!Made.IsValid())
		{
			UFont* Roboto = NewObject<UFont>(GetTransientPackage(), TEXT("CheeseStrikeHUDFont"));
			Roboto->FontCacheType = EFontCacheType::Runtime;
			Roboto->GetMutableInternalCompositeFont() = *FCoreStyle::GetDefaultFont();
			Roboto->AddToRoot();
			Made = Roboto;
		}
		return FSlateFontInfo(Made.Get(), FMath::Max(1, FMath::RoundToInt(Size)), TEXT("BoldCondensed"));
	}

	FVector2D Measure(const FString& Text, const FSlateFontInfo& InFont)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D::ZeroVector;
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, InFont, 1.0f);
	}

	void Text(UCanvas* Canvas, const FString& InText, const FVector2D& At, const FSlateFontInfo& InFont, const FLinearColor& Colour, float Align, bool bShadow, const FVector2D& Stretch)
	{
		if (Canvas == nullptr || InText.IsEmpty())
		{
			return;
		}
		FVector2D Position = At;
		if (Align != 0.0f)
		{
			Position.X -= Measure(InText, InFont).X * Stretch.X * Align;
		}
		FCanvasTextItem Item(Position, FText::FromString(InText), InFont, Colour);
		Item.Scale = Stretch;
		if (bShadow)
		{
			const float Offset = FMath::Max(1.0f, InFont.Size / 14.0f);
			Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f * Colour.A), FVector2D(Offset, Offset));
			// The Mozza Radicals' white is edged in dark too, so that it reads over a pale ground.
			if (FLinearColor(Colour.R, Colour.G, Colour.B, 1.0f).Equals(CheeseStrikeModes::TeamColour(1), 0.02f))
			{
				Item.bOutlined = true;
				Item.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f * Colour.A);
			}
		}
		Canvas->DrawItem(Item);
	}

	void Picture(UCanvas* Canvas, UTexture2D* Texture, const FVector2D& At, const FVector2D& Size, const FLinearColor& Colour)
	{
		if (Canvas == nullptr || Texture == nullptr || Texture->GetResource() == nullptr)
		{
			return;
		}
		FCanvasTileItem Tile(At, Texture->GetResource(), Size, Colour);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}

	void RoundedPicture(UCanvas* Canvas, UTexture2D* Texture, const FVector2D& At, const FVector2D& Size, float Radius, const FLinearColor& Colour)
	{
		if (Canvas == nullptr || Texture == nullptr || Texture->GetResource() == nullptr || Size.X <= 0.0f || Size.Y <= 0.0f)
		{
			return;
		}
		// A fan of triangles from the middle, each corner's place in the picture its share of the way across, inside a soft
		// edge (CheeseStrikeHUD::SoftEdge) that fades the picture out over the pixels its outline crosses.
		const TArray<FVector2D> Corners = CheeseStrikeHUD::RoundedRect(At, At + Size, Radius);
		const TArray<FVector2D> Miters = CheeseStrikeHUD::OutwardMiters(Corners);
		const float Half = CheeseStrikeHUD::SoftEdge / 2.0f;
		const FVector2D Middle = At + Size / 2.0f;
		const auto Place = [&At, &Size](const FVector2D& Point) { return FVector2D((Point.X - At.X) / Size.X, (Point.Y - At.Y) / Size.Y); };
		const FLinearColor Clear = Faded(Colour, 0.0f);
		TArray<FCanvasUVTri> Triangles;
		const auto Add = [&Triangles, &Place](const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& ColourA, const FLinearColor& ColourB, const FLinearColor& ColourC)
		{
			FCanvasUVTri& Tri = Triangles.AddDefaulted_GetRef();
			Tri.V0_Pos = A;
			Tri.V1_Pos = B;
			Tri.V2_Pos = C;
			Tri.V0_UV = Place(A);
			Tri.V1_UV = Place(B);
			Tri.V2_UV = Place(C);
			Tri.V0_Color = ColourA;
			Tri.V1_Color = ColourB;
			Tri.V2_Color = ColourC;
		};
		for (int32 Index = 0; Index < Corners.Num(); ++Index)
		{
			const int32 Next = (Index + 1) % Corners.Num();
			const FVector2D InA = Corners[Index] - Miters[Index] * Half;
			const FVector2D InB = Corners[Next] - Miters[Next] * Half;
			const FVector2D OutA = Corners[Index] + Miters[Index] * Half;
			const FVector2D OutB = Corners[Next] + Miters[Next] * Half;
			Add(Middle, InA, InB, Colour, Colour, Colour);
			Add(InA, InB, OutB, Colour, Colour, Clear);
			Add(InA, OutB, OutA, Colour, Clear, Clear);
		}
		FCanvasTriangleItem Item(Triangles[0], Texture->GetResource());
		Item.TriangleList = MoveTemp(Triangles);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}

	UTexture2D* MakeTexture(int32 Width, int32 Height, const TArray<uint8>& Pixels, EPixelFormat Format)
	{
		if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height * 4)
		{
			return nullptr;
		}
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, Format);
		if (Texture == nullptr || Texture->GetPlatformData() == nullptr || Texture->GetPlatformData()->Mips.Num() == 0)
		{
			return nullptr;
		}
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num());
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}

	void FShapes::Triangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Colour)
	{
		Triangle(A, B, C, Colour, Colour, Colour);
	}

	void FShapes::Triangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& ColourA, const FLinearColor& ColourB, const FLinearColor& ColourC)
	{
		FCanvasUVTri& Tri = Triangles.AddDefaulted_GetRef();
		Tri.V0_Pos = A;
		Tri.V1_Pos = B;
		Tri.V2_Pos = C;
		Tri.V0_Color = ColourA;
		Tri.V1_Color = ColourB;
		Tri.V2_Color = ColourC;
	}

	void FShapes::Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& Colour)
	{
		Quad(A, B, C, D, Colour, Colour, Colour, Colour);
	}

	void FShapes::Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& ColourA, const FLinearColor& ColourB, const FLinearColor& ColourC, const FLinearColor& ColourD)
	{
		Triangle(A, B, C, ColourA, ColourB, ColourC);
		Triangle(A, C, D, ColourA, ColourC, ColourD);
	}

	void FShapes::Rect(float X, float Y, float Width, float Height, const FLinearColor& Colour)
	{
		Quad(FVector2D(X, Y), FVector2D(X + Width, Y), FVector2D(X + Width, Y + Height), FVector2D(X, Y + Height), Colour);
	}

	void FShapes::RectAcross(float X, float Y, float Width, float Height, const FLinearColor& Left, const FLinearColor& Right)
	{
		Quad(FVector2D(X, Y), FVector2D(X + Width, Y), FVector2D(X + Width, Y + Height), FVector2D(X, Y + Height), Left, Right, Right, Left);
	}

	void FShapes::Frame(float X, float Y, float Width, float Height, float Thickness, const FLinearColor& Colour)
	{
		Rect(X, Y, Width, Thickness, Colour);
		Rect(X, Y + Height - Thickness, Width, Thickness, Colour);
		Rect(X, Y + Thickness, Thickness, Height - 2.0f * Thickness, Colour);
		Rect(X + Width - Thickness, Y + Thickness, Thickness, Height - 2.0f * Thickness, Colour);
	}

	void FShapes::RoundedRect(float X, float Y, float Width, float Height, float Radius, const FLinearColor& Colour)
	{
		SmoothPolygon(CheeseStrikeHUD::RoundedRect(FVector2D(X, Y), FVector2D(X + Width, Y + Height), Radius), Colour);
	}

	void FShapes::RoundedRectDown(float X, float Y, float Width, float Height, float Radius, const FLinearColor& Top, const FLinearColor& Bottom)
	{
		const TArray<FVector2D> Corners = CheeseStrikeHUD::RoundedRect(FVector2D(X, Y), FVector2D(X + Width, Y + Height), Radius);
		SmoothFill(Corners, [&](const FVector2D& Point) { return FMath::Lerp(Top, Bottom, Height > 0.0f ? FMath::Clamp((Point.Y - Y) / Height, 0.0f, 1.0f) : 0.0f); });
	}

	void FShapes::RoundedFrame(float X, float Y, float Width, float Height, float Radius, float Thickness, const FLinearColor& Colour)
	{
		// The outer outline and one inset by the thickness, its corners as much less round, with as many segments to its
		// arcs, joined corner to corner; thinner than the soft edge, as wide as it and fainter.
		const float Drawn = FMath::Max(Thickness, CheeseStrikeHUD::SoftEdge);
		const float Grow = (Drawn - Thickness) / 2.0f;
		const FLinearColor Shown = Faded(Colour, Thickness / Drawn);
		const FVector2D Min(X - Grow, Y - Grow);
		const FVector2D Max(X + Width + Grow, Y + Height + Grow);
		const float Outside = Radius + Grow;
		const int32 Steps = CheeseStrikeHUD::CornerSteps(FMath::Clamp(Outside, 0.0f, FMath::Min(Max.X - Min.X, Max.Y - Min.Y) / 2.0f));
		const TArray<FVector2D> Outer = CheeseStrikeHUD::RoundedRect(Min, Max, FMath::Max(0.001f, Outside), Steps);
		const TArray<FVector2D> Inner = CheeseStrikeHUD::RoundedRect(Min + FVector2D(Drawn, Drawn), Max - FVector2D(Drawn, Drawn), FMath::Max(0.001f, Outside - Drawn), Steps);
		if (Outer.Num() != Inner.Num())
		{
			Frame(X, Y, Width, Height, Thickness, Colour);
			return;
		}
		SmoothBand(Outer, Inner, Shown);
	}

	void FShapes::Polygon(TConstArrayView<FVector2D> Corners, const FLinearColor& Colour)
	{
		for (int32 Index = 2; Index < Corners.Num(); ++Index)
		{
			Triangle(Corners[0], Corners[Index - 1], Corners[Index], Colour);
		}
	}

	void FShapes::SmoothPolygon(TConstArrayView<FVector2D> Corners, const FLinearColor& Colour)
	{
		SmoothFill(Corners, [&Colour](const FVector2D&) { return Colour; });
	}

	void FShapes::SmoothFill(TConstArrayView<FVector2D> Corners, TFunctionRef<FLinearColor(const FVector2D&)> ColourAt)
	{
		const int32 Count = Corners.Num();
		if (Count < 3)
		{
			return;
		}
		// Solid to half the soft edge inside the outline, then fading to clear half of it outside.
		const TArray<FVector2D> Miters = CheeseStrikeHUD::OutwardMiters(Corners);
		const float Half = CheeseStrikeHUD::SoftEdge / 2.0f;
		TArray<FVector2D, TInlineAllocator<64>> Inner;
		TArray<FLinearColor, TInlineAllocator<64>> Colours;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Inner.Add(Corners[Index] - Miters[Index] * Half);
			Colours.Add(ColourAt(Corners[Index]));
		}
		for (int32 Index = 2; Index < Count; ++Index)
		{
			Triangle(Inner[0], Inner[Index - 1], Inner[Index], Colours[0], Colours[Index - 1], Colours[Index]);
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Next = (Index + 1) % Count;
			Quad(Inner[Index], Inner[Next], Corners[Next] + Miters[Next] * Half, Corners[Index] + Miters[Index] * Half,
				Colours[Index], Colours[Next], Faded(Colours[Next], 0.0f), Faded(Colours[Index], 0.0f));
		}
	}

	void FShapes::SmoothBand(TConstArrayView<FVector2D> Outer, TConstArrayView<FVector2D> Inner, const FLinearColor& Colour)
	{
		const int32 Count = Outer.Num();
		if (Count < 3 || Inner.Num() != Count)
		{
			return;
		}
		// Solid between the two outlines each moved half the soft edge into the band, fading to clear half of it out of it.
		const TArray<FVector2D> OuterWay = CheeseStrikeHUD::OutwardMiters(Outer);
		const TArray<FVector2D> InnerWay = CheeseStrikeHUD::OutwardMiters(Inner);
		const float Half = CheeseStrikeHUD::SoftEdge / 2.0f;
		const FLinearColor Clear = Faded(Colour, 0.0f);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Next = (Index + 1) % Count;
			const FVector2D OuterInA = Outer[Index] - OuterWay[Index] * Half;
			const FVector2D OuterInB = Outer[Next] - OuterWay[Next] * Half;
			const FVector2D InnerOutA = Inner[Index] + InnerWay[Index] * Half;
			const FVector2D InnerOutB = Inner[Next] + InnerWay[Next] * Half;
			Quad(OuterInA, OuterInB, InnerOutB, InnerOutA, Colour);
			Quad(OuterInA, OuterInB, Outer[Next] + OuterWay[Next] * Half, Outer[Index] + OuterWay[Index] * Half, Colour, Colour, Clear, Clear);
			Quad(InnerOutA, InnerOutB, Inner[Next] - InnerWay[Next] * Half, Inner[Index] - InnerWay[Index] * Half, Colour, Colour, Clear, Clear);
		}
	}

	void FShapes::Fringe(const FVector2D& A, const FVector2D& B, const FVector2D& Out, const FLinearColor& Colour)
	{
		Quad(A, B, B + Out, A + Out, Colour, Colour, Faded(Colour, 0.0f), Faded(Colour, 0.0f));
	}

	void FShapes::SoftRim(TConstArrayView<FVector2D> Outline, TConstArrayView<FLinearColor> Colours, bool bOpen)
	{
		const int32 Count = Outline.Num();
		if (Count < 3 || Colours.Num() != Count)
		{
			return;
		}
		const TArray<FVector2D> Miters = CheeseStrikeHUD::OutwardMiters(Outline);
		const float Edge = CheeseStrikeHUD::SoftEdge;
		for (int32 Index = 0; Index < (bOpen ? Count - 1 : Count); ++Index)
		{
			const int32 Next = (Index + 1) % Count;
			Quad(Outline[Index], Outline[Next], Outline[Next] + Miters[Next] * Edge, Outline[Index] + Miters[Index] * Edge,
				Colours[Index], Colours[Next], Faded(Colours[Next], 0.0f), Faded(Colours[Index], 0.0f));
		}
	}

	void FShapes::Disc(const FVector2D& Middle, float Radius, const FLinearColor& Colour, int32 Sides)
	{
		// At least as many sides as a circle that big needs to look round (four of its quarter arcs).
		const int32 Round = FMath::Max(Sides, 4 * CheeseStrikeHUD::CornerSteps(Radius));
		TArray<FVector2D, TInlineAllocator<64>> Rim;
		for (int32 Side = 0; Side < Round; ++Side)
		{
			const float Angle = 2.0f * UE_PI * Side / Round;
			Rim.Add(Middle + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		SmoothPolygon(Rim, Colour);
	}

	void FShapes::Bulb(const FVector2D& Middle, float Radius, float Depth, const FLinearColor& Inner, const FLinearColor& Outer, int32 Sides)
	{
		FVector2D Last = Middle + FVector2D(Radius, 0.0f);
		for (int32 Side = 1; Side <= Sides; ++Side)
		{
			const float Angle = UE_PI * Side / Sides;
			const FVector2D Next = Middle + FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Depth);
			Triangle(Middle, Last, Next, Inner, Outer, Outer);
			Last = Next;
		}
	}

	void FShapes::Line(const FVector2D& From, const FVector2D& To, float Thickness, const FLinearColor& Colour)
	{
		const FVector2D Along = To - From;
		if (Along.IsNearlyZero())
		{
			return;
		}
		const FVector2D Across = FVector2D(-Along.Y, Along.X).GetSafeNormal() * (Thickness / 2.0f);
		Quad(From - Across, To - Across, To + Across, From + Across, Colour);
	}

	void FShapes::Arc(const FVector2D& Middle, float Radius, float Thickness, float From, float To, const FLinearColor& Colour, bool bFade)
	{
		// Short enough steps for the circle to look round, and a soft edge either side; thinner than the soft edge, as wide
		// as it and fainter.
		const float Drawn = FMath::Max(Thickness, CheeseStrikeHUD::SoftEdge);
		const float Outer = Radius + Drawn / 2.0f;
		const float Inner = FMath::Max(0.0f, Radius - Drawn / 2.0f);
		const int32 Steps = FMath::Max3(2, FMath::CeilToInt(FMath::Abs(To - From) / 90.0f * CheeseStrikeHUD::CornerSteps(Outer + 0.5f)), FMath::CeilToInt(FMath::Abs(To - From) / 3.0f));
		const float Half = CheeseStrikeHUD::SoftEdge / 2.0f;
		const FLinearColor Base = Faded(Colour, Thickness / Drawn);
		const auto Point = [&Middle](float Degrees, float AtRadius)
		{
			const float Radians = FMath::DegreesToRadians(Degrees);
			return Middle + FVector2D(FMath::Sin(Radians), -FMath::Cos(Radians)) * FMath::Max(0.0f, AtRadius);
		};
		const auto Shade = [&Base, bFade](float Along)
		{
			FLinearColor Shaded = Base;
			if (bFade)
			{
				const float Edge = FMath::Clamp(FMath::Min(Along, 1.0f - Along) / 0.3f, 0.0f, 1.0f);
				Shaded.A *= Edge * Edge * (3.0f - 2.0f * Edge);
			}
			return Shaded;
		};
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float A = static_cast<float>(Step) / Steps;
			const float B = static_cast<float>(Step + 1) / Steps;
			const float DegreesA = FMath::Lerp(From, To, A);
			const float DegreesB = FMath::Lerp(From, To, B);
			const FLinearColor ShadeA = Shade(A);
			const FLinearColor ShadeB = Shade(B);
			const FLinearColor ClearA = Faded(ShadeA, 0.0f);
			const FLinearColor ClearB = Faded(ShadeB, 0.0f);
			Quad(Point(DegreesA, Inner + Half), Point(DegreesB, Inner + Half), Point(DegreesB, Outer - Half), Point(DegreesA, Outer - Half), ShadeA, ShadeB, ShadeB, ShadeA);
			Quad(Point(DegreesA, Outer - Half), Point(DegreesB, Outer - Half), Point(DegreesB, Outer + Half), Point(DegreesA, Outer + Half), ShadeA, ShadeB, ClearB, ClearA);
			if (Inner > 0.0f)
			{
				Quad(Point(DegreesA, Inner + Half), Point(DegreesB, Inner + Half), Point(DegreesB, Inner - Half), Point(DegreesA, Inner - Half), ShadeA, ShadeB, ClearB, ClearA);
			}
		}
	}

	void FShapes::Fade(float Alpha)
	{
		if (Alpha >= 1.0f)
		{
			return;
		}
		for (FCanvasUVTri& Tri : Triangles)
		{
			Tri.V0_Color.A *= Alpha;
			Tri.V1_Color.A *= Alpha;
			Tri.V2_Color.A *= Alpha;
		}
	}

	void FShapes::Draw(UCanvas* Canvas)
	{
		if (Canvas != nullptr && Triangles.Num() > 0)
		{
			FCanvasTriangleItem Item(Triangles[0], GWhiteTexture);
			Item.TriangleList = MoveTemp(Triangles);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
		Triangles.Reset();
	}
}
