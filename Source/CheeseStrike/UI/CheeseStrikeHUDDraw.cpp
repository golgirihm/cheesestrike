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
#include "UI/CheeseStrikeMenu.h"
#include "TextureResource.h"
#include "UObject/Package.h"

namespace CheeseStrikeHUDDraw
{
	FLinearColor SideColour(int32 Team)
	{
		return CheeseStrikeMenu::TeamColour(Team);
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

	void FShapes::Polygon(TConstArrayView<FVector2D> Corners, const FLinearColor& Colour)
	{
		for (int32 Index = 2; Index < Corners.Num(); ++Index)
		{
			Triangle(Corners[0], Corners[Index - 1], Corners[Index], Colour);
		}
	}

	void FShapes::Disc(const FVector2D& Middle, float Radius, const FLinearColor& Colour, int32 Sides)
	{
		FVector2D Last = Middle + FVector2D(Radius, 0.0f);
		for (int32 Side = 1; Side <= Sides; ++Side)
		{
			const float Angle = 2.0f * UE_PI * Side / Sides;
			const FVector2D Next = Middle + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
			Triangle(Middle, Last, Next, Colour);
			Last = Next;
		}
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
		const int32 Steps = FMath::Max(2, FMath::CeilToInt(FMath::Abs(To - From) / 3.0f));
		const auto Point = [&Middle](float Degrees, float AtRadius)
		{
			const float Radians = FMath::DegreesToRadians(Degrees);
			return Middle + FVector2D(FMath::Sin(Radians), -FMath::Cos(Radians)) * AtRadius;
		};
		const auto Shade = [&Colour, bFade](float Along)
		{
			FLinearColor Shaded = Colour;
			if (bFade)
			{
				const float Edge = FMath::Clamp(FMath::Min(Along, 1.0f - Along) / 0.3f, 0.0f, 1.0f);
				Shaded.A *= Edge * Edge * (3.0f - 2.0f * Edge);
			}
			return Shaded;
		};
		const float Inner = Radius - Thickness / 2.0f;
		const float Outer = Radius + Thickness / 2.0f;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float A = static_cast<float>(Step) / Steps;
			const float B = static_cast<float>(Step + 1) / Steps;
			const float DegreesA = FMath::Lerp(From, To, A);
			const float DegreesB = FMath::Lerp(From, To, B);
			Quad(Point(DegreesA, Inner), Point(DegreesB, Inner), Point(DegreesB, Outer), Point(DegreesA, Outer), Shade(A), Shade(B), Shade(B), Shade(A));
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
