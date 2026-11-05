// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// TEMPORARY: the sampler of UI themes (CheeseStrikeThemes.h). Everything the sampler draws is in this file, so that it can
// be deleted whole, or cut down to the theme picked, once the user has chosen. The menu's own looks are untouched: the
// page is drawn in the pages' panel as any page is, each theme's sample on a panel of its own colour.

#include "CheeseStrikeThemes.h"
#include "CheeseStrikeMenu.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "CheeseStrikePlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Sound/SoundWaveProcedural.h"
#include "Streaks/CheeseStrikeSynth.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace CheeseStrikeThemes
{
	const FTheme& Theme(int32 Index)
	{
		// Field by field: name, blurb, shape, rounding, outline, motion, shadow; the title's face and tracking, the body's
		// face, the buttons' face and tracking, capitals, the letters' outline; then the colours: panel, surface, surface
		// lit, ink, muted, accent, accent lit, accent's ink, line, shadow, glow, disabled, disabled's ink, field.
		static const FTheme Themes[NumThemes] = {
			{
				TEXT("Cheese Board"), TEXT("Warm cream and cheese gold on dark walnut: generous rounding, soft shadows, a lift under the mouse; a wooden tick and knock."),
				EShape::Rounded, 12.0f, 0.0f, EMotion::Lift, true,
				FName("Bold"), 20, FName("Regular"), FName("Bold"), 10, false, 0,
				FLinearColor(0.17f, 0.105f, 0.06f), FLinearColor(0.95f, 0.89f, 0.76f), FLinearColor(1.0f, 0.97f, 0.88f), FLinearColor(0.26f, 0.16f, 0.08f), FLinearColor(0.72f, 0.6f, 0.46f),
				FLinearColor(0.95f, 0.7f, 0.2f), FLinearColor(1.0f, 0.8f, 0.34f), FLinearColor(0.3f, 0.17f, 0.05f), FLinearColor(0.95f, 0.7f, 0.2f), FLinearColor(0.0f, 0.0f, 0.0f, 0.4f),
				FLinearColor(1.0f, 0.8f, 0.34f), FLinearColor(0.36f, 0.28f, 0.2f), FLinearColor(0.62f, 0.52f, 0.42f), FLinearColor(0.99f, 0.96f, 0.9f),
			},
			{
				TEXT("Deli Counter"), TEXT("Flat white and butcher red with black lettering, square corners, a thick underline sliding in under the mouse; a paper tick and a ticket punch."),
				EShape::Square, 0.0f, 2.0f, EMotion::Underline, false,
				FName("Black"), 0, FName("Regular"), FName("BoldCondensed"), 40, true, 0,
				FLinearColor(0.97f, 0.96f, 0.94f), FLinearColor(1.0f, 1.0f, 1.0f), FLinearColor(0.96f, 0.95f, 0.93f), FLinearColor(0.04f, 0.04f, 0.04f), FLinearColor(0.42f, 0.4f, 0.38f),
				FLinearColor(0.76f, 0.08f, 0.1f), FLinearColor(0.86f, 0.14f, 0.15f), FLinearColor(1.0f, 1.0f, 1.0f), FLinearColor(0.04f, 0.04f, 0.04f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f),
				FLinearColor(0.76f, 0.08f, 0.1f), FLinearColor(0.88f, 0.87f, 0.85f), FLinearColor(0.6f, 0.58f, 0.56f), FLinearColor(1.0f, 1.0f, 1.0f),
			},
			{
				TEXT("Cave Aged"), TEXT("Near-black slate with thin gold hairlines, light wide-set type, a gold glow under the mouse; a glass ping and a stone tap."),
				EShape::Rounded, 4.0f, 1.0f, EMotion::Glow, false,
				FName("Light"), 160, FName("Regular"), FName("Regular"), 110, true, 0,
				FLinearColor(0.065f, 0.07f, 0.08f), FLinearColor(0.1f, 0.105f, 0.12f), FLinearColor(0.15f, 0.155f, 0.17f), FLinearColor(0.9f, 0.87f, 0.8f), FLinearColor(0.55f, 0.53f, 0.48f),
				FLinearColor(0.8f, 0.63f, 0.28f), FLinearColor(0.92f, 0.75f, 0.36f), FLinearColor(0.1f, 0.08f, 0.05f), FLinearColor(0.8f, 0.63f, 0.28f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f),
				FLinearColor(0.95f, 0.75f, 0.3f), FLinearColor(0.085f, 0.09f, 0.1f), FLinearColor(0.38f, 0.37f, 0.34f), FLinearColor(0.05f, 0.055f, 0.065f),
			},
			{
				TEXT("Cartoon Cheese"), TEXT("Bright yellow and orange in chunky dark outlines, big rounding, a bouncy scale under the mouse; a pop and a boing."),
				EShape::Rounded, 20.0f, 3.0f, EMotion::Bounce, true,
				FName("Black"), 0, FName("Bold"), FName("Black"), 0, true, 2,
				FLinearColor(1.0f, 0.92f, 0.58f), FLinearColor(1.0f, 0.84f, 0.2f), FLinearColor(1.0f, 0.9f, 0.4f), FLinearColor(0.22f, 0.12f, 0.04f), FLinearColor(0.5f, 0.36f, 0.16f),
				FLinearColor(1.0f, 0.52f, 0.1f), FLinearColor(1.0f, 0.62f, 0.22f), FLinearColor(1.0f, 0.98f, 0.9f), FLinearColor(0.22f, 0.12f, 0.04f), FLinearColor(0.22f, 0.12f, 0.04f, 1.0f),
				FLinearColor(1.0f, 0.62f, 0.22f), FLinearColor(0.88f, 0.82f, 0.66f), FLinearColor(0.55f, 0.48f, 0.36f), FLinearColor(1.0f, 0.99f, 0.95f),
			},
			{
				TEXT("Tactical"), TEXT("Olive and charcoal with chamfered corners cut as polygons, stencil-like capitals, a hatch under the mouse; a mechanical click and a radio blip."),
				EShape::Chamfer, 8.0f, 2.0f, EMotion::Hatch, false,
				FName("BoldCondensed"), 220, FName("Regular"), FName("BoldCondensed"), 160, true, 0,
				FLinearColor(0.1f, 0.11f, 0.1f), FLinearColor(0.19f, 0.22f, 0.16f), FLinearColor(0.25f, 0.29f, 0.21f), FLinearColor(0.88f, 0.9f, 0.8f), FLinearColor(0.55f, 0.6f, 0.45f),
				FLinearColor(0.43f, 0.5f, 0.27f), FLinearColor(0.5f, 0.58f, 0.32f), FLinearColor(0.96f, 0.97f, 0.88f), FLinearColor(0.6f, 0.68f, 0.42f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f),
				FLinearColor(0.6f, 0.68f, 0.42f), FLinearColor(0.14f, 0.15f, 0.13f), FLinearColor(0.4f, 0.44f, 0.35f), FLinearColor(0.12f, 0.14f, 0.11f),
			},
		};
		return Themes[Index >= 0 && Index < NumThemes ? Index : 0];
	}

	FString Label(int32 Index, const FString& Text)
	{
		return Theme(Index).bUpper ? FText::FromString(Text).ToUpper().ToString() : Text;
	}
}

namespace
{
	using namespace CheeseStrikeThemes;

	/** What a themed widget is drawn as. */
	enum class EFace : uint8
	{
		/** A secondary button. */
		Plain,
		/** The primary button. */
		Primary,
		/** A button that can't be pressed. */
		Disabled,
		/** One of a row of tabs, the chosen one marked. */
		Tab,
		/** A toggle's track with its knob, on or off. */
		Toggle,
		/** A dropdown's own button, with a chevron; open or shut. */
		Dropdown,
		/** An item of a dropdown's list, the chosen one marked. */
		Item,
		/** A card, with a title and lines. */
		Card,
		/** A text field, edged in the accent while it has the focus. */
		Field,
		/** A chip: a small tag. */
		Chip,
		/** A dropdown's open list. */
		List,
		/** The sample's own panel. */
		Panel,
		/** The card's tooltip. */
		Tooltip,
	};

	/** The sampler's fonts: the engine's default (Roboto) in the theme's face, tracked as it says, with its outline on titles and buttons. */
	FSlateFontInfo ThemeFont(const FTheme& T, const FName& Face, int32 Size, int32 Tracking, bool bOutlined)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Face, Size);
		Font.LetterSpacing = Tracking;
		if (bOutlined && T.TextOutline > 0)
		{
			Font.OutlineSettings = FFontOutlineSettings(T.TextOutline, T.Line);
		}
		return Font;
	}

	FSlateFontInfo TitleFont(const FTheme& T, int32 Size = 30)
	{
		return ThemeFont(T, T.TitleFace, Size, T.TitleTracking, true);
	}

	FSlateFontInfo ButtonFont(const FTheme& T, int32 Size = 17)
	{
		return ThemeFont(T, T.ButtonFace, Size, T.ButtonTracking, true);
	}

	FSlateFontInfo BodyFont(const FTheme& T, int32 Size = 15)
	{
		return ThemeFont(T, T.BodyFace, Size, 0, false);
	}

	/** The colour of words written straight on the sample's panel (a value, a state): the ink, unless both it and the panel are dark (the Cheese Board's walnut), when the surface's cream is. */
	FLinearColor PanelInk(const FTheme& T)
	{
		return T.Panel.GetLuminance() < 0.3f && T.Ink.GetLuminance() < 0.3f ? T.Surface : T.Ink;
	}

	/** A rounded box of `Radius` in white, tinted as it is drawn (the plain white brush for none); made once a radius. */
	const FSlateBrush* RoundBrush(float Radius)
	{
		if (Radius <= 0.01f)
		{
			return FCoreStyle::Get().GetBrush("WhiteBrush");
		}
		static TMap<int32, TUniquePtr<FSlateRoundedBoxBrush>> Made;
		const int32 Key = FMath::RoundToInt(Radius * 4.0f);
		if (!Made.Contains(Key))
		{
			Made.Add(Key, MakeUnique<FSlateRoundedBoxBrush>(FLinearColor::White, Radius));
		}
		return Made[Key].Get();
	}

	/** A button's style that draws nothing of its own: the themed widgets paint their faces themselves. */
	const FButtonStyle& BareStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource())
			.SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f))
			.SetNormalForeground(FLinearColor::White).SetHoveredForeground(FLinearColor::White).SetPressedForeground(FLinearColor::White).SetDisabledForeground(FLinearColor::White);
		return Style;
	}

	/** The ring round the item with the focus, in each theme: its own shape, in a colour that shows on its panel. */
	const FSlateBrush* FocusRing(int32 Index)
	{
		static TUniquePtr<FSlateRoundedBoxBrush> Made[NumThemes];
		const int32 At = FMath::Clamp(Index, 0, NumThemes - 1);
		if (!Made[At])
		{
			const FTheme& T = Theme(At);
			const bool bLightPanel = T.Panel.GetLuminance() > 0.5f;
			const FLinearColor Ring = bLightPanel ? T.Line : (At == 0 ? T.SurfaceLit : T.AccentLit);
			Made[At] = MakeUnique<FSlateRoundedBoxBrush>(FLinearColor::Transparent, T.Shape == EShape::Rounded ? T.Rounding : (T.Shape == EShape::Chamfer ? 3.0f : 0.0f), Ring, 3.0f);
		}
		return Made[At].Get();
	}

	/** A slider in each theme's looks: its bar, and a thumb of the theme's shape in its accent. */
	const FSliderStyle& SliderStyle(int32 Index)
	{
		static TUniquePtr<FSliderStyle> Made[NumThemes];
		const int32 At = FMath::Clamp(Index, 0, NumThemes - 1);
		if (!Made[At])
		{
			const FTheme& T = Theme(At);
			TUniquePtr<FSliderStyle> Style = MakeUnique<FSliderStyle>();
			const float BarRound = T.Shape == EShape::Square ? 0.0f : 3.0f;
			// The bar: the panel's colour pulled a little toward the words written on it, so that it shows on light and dark panels alike.
			const FLinearColor Bar = FMath::Lerp(T.Panel, PanelInk(T), 0.22f);
			const FLinearColor BarLit = FMath::Lerp(T.Panel, PanelInk(T), 0.32f);
			Style->SetNormalBarImage(FSlateRoundedBoxBrush(Bar, BarRound));
			Style->SetHoveredBarImage(FSlateRoundedBoxBrush(BarLit, BarRound));
			Style->SetDisabledBarImage(FSlateRoundedBoxBrush(Bar, BarRound));
			switch (At)
			{
			case 1:
				Style->SetNormalThumbImage(FSlateRoundedBoxBrush(T.Accent, 0.0f, FVector2f(14.0f, 22.0f)));
				Style->SetHoveredThumbImage(FSlateRoundedBoxBrush(T.AccentLit, 0.0f, FVector2f(14.0f, 22.0f)));
				break;
			case 2:
				Style->SetNormalThumbImage(FSlateRoundedBoxBrush(T.Accent, 4.0f, FVector2f(12.0f, 20.0f)));
				Style->SetHoveredThumbImage(FSlateRoundedBoxBrush(T.AccentLit, 4.0f, FVector2f(12.0f, 20.0f)));
				break;
			case 3:
				Style->SetNormalThumbImage(FSlateRoundedBoxBrush(T.Accent, 12.0f, T.Line, 3.0f, FVector2f(24.0f, 24.0f)));
				Style->SetHoveredThumbImage(FSlateRoundedBoxBrush(T.AccentLit, 12.0f, T.Line, 3.0f, FVector2f(24.0f, 24.0f)));
				break;
			case 4:
				Style->SetNormalThumbImage(FSlateRoundedBoxBrush(T.Accent, 0.0f, T.Line, 2.0f, FVector2f(16.0f, 18.0f)));
				Style->SetHoveredThumbImage(FSlateRoundedBoxBrush(T.AccentLit, 0.0f, T.Line, 2.0f, FVector2f(16.0f, 18.0f)));
				break;
			default:
				Style->SetNormalThumbImage(FSlateRoundedBoxBrush(T.Accent, 9.0f, FVector2f(18.0f, 18.0f)));
				Style->SetHoveredThumbImage(FSlateRoundedBoxBrush(T.AccentLit, 9.0f, FVector2f(18.0f, 18.0f)));
				break;
			}
			Style->SetDisabledThumbImage(Style->NormalThumbImage);
			Style->SetBarThickness(At == 3 ? 10.0f : 6.0f);
			Made[At] = MoveTemp(Style);
		}
		return *Made[At];
	}

	/** The corners of a box at `At` of `Size` with its corners cut straight across by `Cut`, clockwise from the top left. */
	TArray<FVector2f> ChamferCorners(const FVector2f& At, const FVector2f& Size, float Cut)
	{
		const float C = FMath::Clamp(Cut, 0.0f, FMath::Min(Size.X, Size.Y) * 0.5f);
		return {At + FVector2f(C, 0.0f), At + FVector2f(Size.X - C, 0.0f), At + FVector2f(Size.X, C), At + FVector2f(Size.X, Size.Y - C),
			At + FVector2f(Size.X - C, Size.Y), At + FVector2f(C, Size.Y), At + FVector2f(0.0f, Size.Y - C), At + FVector2f(0.0f, C)};
	}

	/** Draws a box at `At` (local) of `Size` in `Colour`, with the theme's corners: rounded or square as a brush, chamfered as a polygon of its own vertices. */
	void DrawShape(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f& At, const FVector2f& Size, EShape Shape, float Cut, const FLinearColor& Colour)
	{
		if (Colour.A <= 0.002f || Size.X <= 0.0f || Size.Y <= 0.0f)
		{
			return;
		}
		const float Fit = FMath::Min(Cut, FMath::Min(Size.X, Size.Y) * 0.5f);
		if (Shape != EShape::Chamfer)
		{
			FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(Size, FSlateLayoutTransform(At)), RoundBrush(Shape == EShape::Rounded ? Fit : 0.0f), ESlateDrawEffect::None, Colour);
			return;
		}
		const FSlateRenderTransform& Transform = G.GetAccumulatedRenderTransform();
		const FColor Vertex = Colour.ToFColorSRGB();
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Indexes;
		for (const FVector2f& Corner : ChamferCorners(At, Size, Fit))
		{
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Corner, FVector2f(0.0f, 0.0f), Vertex));
		}
		for (int32 Index = 1; Index < 7; ++Index)
		{
			Indexes.Add(0);
			Indexes.Add(Index);
			Indexes.Add(Index + 1);
		}
		FSlateDrawElement::MakeCustomVerts(Out, Layer, FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush")), Verts, Indexes, nullptr, 0, 0);
	}

	/** Draws a diagonal hatch over the chamfered box at `At` of `Size`, each line clipped to the polygon (it is convex). */
	void DrawHatch(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f& At, const FVector2f& Size, float Cut, const FLinearColor& Colour)
	{
		const TArray<FVector2f> Poly = ChamferCorners(At, Size, Cut);
		const FVector2f Centre = At + Size * 0.5f;
		const FVector2f Along(1.0f, 1.0f);
		for (float K = -Size.Y; K < Size.X; K += 6.0f)
		{
			// The line x - y = K from the box's top, running down and right.
			const FVector2f Start(At.X + K, At.Y);
			float In = 0.0f;
			float Exit = Size.Y;
			bool bOutside = false;
			for (int32 Edge = 0; Edge < Poly.Num() && !bOutside; ++Edge)
			{
				const FVector2f E0 = Poly[Edge];
				const FVector2f E1 = Poly[(Edge + 1) % Poly.Num()];
				FVector2f Normal(-(E1.Y - E0.Y), E1.X - E0.X);
				if (FVector2f::DotProduct(Normal, Centre - E0) < 0.0f)
				{
					Normal = -Normal;
				}
				const float Denominator = FVector2f::DotProduct(Normal, Along);
				const float Numerator = FVector2f::DotProduct(Normal, E0 - Start);
				if (FMath::IsNearlyZero(Denominator))
				{
					bOutside = Numerator > 0.0f;
				}
				else if (Denominator > 0.0f)
				{
					In = FMath::Max(In, Numerator / Denominator);
				}
				else
				{
					Exit = FMath::Min(Exit, Numerator / Denominator);
				}
			}
			if (!bOutside && In < Exit)
			{
				FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), TArray<FVector2f>{Start + Along * In, Start + Along * Exit}, ESlateDrawEffect::None, Colour, true, 1.2f);
			}
		}
	}

	/** What a themed widget is drawn with: how far under the mouse it is (0 to 1), pressed, its knob along (a toggle), chosen (a tab, an item; a dropdown open), and a field with the focus. */
	struct FPaintState
	{
		float Hover = 0.0f;
		bool bPressed = false;
		float Knob = 0.0f;
		bool bChosen = false;
		bool bFocused = false;
	};

	/** Eased out: quick at first, settling. */
	float EaseOut(float Along)
	{
		return 1.0f - FMath::Square(1.0f - FMath::Clamp(Along, 0.0f, 1.0f));
	}

	/**
	 * Paints `Face` in theme `T`'s looks over the widget's whole geometry: its shadow, glow, outline and fill, with what the
	 * theme does under the mouse (an underline sliding in, a glow, a hatch; the lift and the bounce are render transforms,
	 * set by the widget's tick), the dropdown's chevron and the toggle's knob. Returns the layer the content goes on.
	 */
	int32 PaintFace(const FTheme& T, EFace Face, const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, const FPaintState& S)
	{
		const float Tint = Style.GetColorAndOpacityTint().A;
		const auto Dim = [Tint](FLinearColor Colour, float Alpha = 1.0f)
		{
			Colour.A *= Alpha * Tint;
			return Colour;
		};
		const FVector2f Size = FVector2f(G.GetLocalSize());
		const FVector2f At(0.0f, 0.0f);
		const bool bChamfer = T.Shape == EShape::Chamfer;
		// The fill, the edge and the corners for the face.
		FLinearColor Fill = T.Surface;
		FLinearColor Edge = T.Line;
		float EdgeWidth = T.Outline;
		float Cut = T.Rounding;
		bool bShadow = T.bShadow;
		bool bMotion = true;
		const bool bChosenPlain = S.bChosen && (T.Motion == EMotion::Underline || T.Motion == EMotion::Glow);
		switch (Face)
		{
		case EFace::Plain:
		case EFace::Dropdown:
			Fill = FMath::Lerp(T.Surface, T.SurfaceLit, S.Hover);
			break;
		case EFace::Primary:
			Fill = FMath::Lerp(T.Accent, T.AccentLit, S.Hover);
			break;
		case EFace::Disabled:
			Fill = T.Disabled;
			Edge = Dim(T.DisabledInk, 0.5f);
			bMotion = false;
			bShadow = false;
			break;
		case EFace::Tab:
			if (S.bChosen)
			{
				Fill = bChosenPlain ? T.Surface : FMath::Lerp(T.Accent, T.AccentLit, S.Hover);
			}
			else
			{
				Fill = Dim(T.SurfaceLit, S.Hover * 0.9f);
				EdgeWidth = 0.0f;
				bShadow = false;
			}
			break;
		case EFace::Toggle:
			Fill = FMath::Lerp(FMath::Lerp(T.Surface, T.SurfaceLit, S.Hover), FMath::Lerp(T.Accent, T.AccentLit, S.Hover), S.Knob);
			Cut = bChamfer ? FMath::Min(T.Rounding, 6.0f) : FMath::Min(T.Rounding, Size.Y * 0.5f);
			bMotion = false;
			bShadow = false;
			break;
		case EFace::Item:
			Fill = S.bChosen ? T.Accent : Dim(T.SurfaceLit, S.Hover);
			EdgeWidth = 0.0f;
			Cut = bChamfer ? FMath::Min(T.Rounding, 5.0f) : FMath::Max(0.0f, T.Rounding - 4.0f);
			bShadow = false;
			bMotion = T.Motion == EMotion::Underline || T.Motion == EMotion::Hatch;
			break;
		case EFace::Card:
		case EFace::Tooltip:
		case EFace::List:
			Fill = T.Surface;
			bMotion = Face == EFace::Card;
			break;
		case EFace::Field:
			Fill = T.Field;
			Edge = S.bFocused ? T.Accent : T.Line;
			EdgeWidth = S.bFocused ? FMath::Max(2.0f, T.Outline) : T.Outline;
			bShadow = false;
			bMotion = false;
			break;
		case EFace::Chip:
			Fill = T.Accent;
			EdgeWidth = 0.0f;
			Cut = bChamfer ? FMath::Min(T.Rounding, 5.0f) : FMath::Min(T.Rounding, Size.Y * 0.5f);
			bShadow = false;
			bMotion = false;
			break;
		case EFace::Panel:
			Fill = T.Panel;
			Edge = Dim(T.Line, 0.5f);
			EdgeWidth = FMath::Min(T.Outline, 1.0f);
			Cut = bChamfer ? 12.0f : T.Rounding;
			bShadow = false;
			bMotion = false;
			break;
		}
		if (S.bPressed && bMotion)
		{
			Fill = FMath::Lerp(Fill, FLinearColor::Black, 0.12f);
		}
		const float Hover = bMotion ? S.Hover : 0.0f;
		// The shadow: a hard one offset (a sticker's), or a soft one of three layers that grows as the widget lifts.
		if (bShadow && Tint > 0.0f)
		{
			if (T.Shadow.A >= 0.99f)
			{
				DrawShape(Out, Layer, G, At + FVector2f(3.0f, 4.0f), Size, T.Shape, Cut, Dim(T.Shadow));
			}
			else
			{
				const float Lift = Hover;
				DrawShape(Out, Layer, G, At + FVector2f(-2.0f, 6.0f + 4.0f * Lift), Size + FVector2f(4.0f, 2.0f), T.Shape, Cut + 2.0f, Dim(T.Shadow, 0.2f));
				DrawShape(Out, Layer, G, At + FVector2f(-1.0f, 4.0f + 3.0f * Lift), Size + FVector2f(2.0f, 1.0f), T.Shape, Cut + 1.0f, Dim(T.Shadow, 0.4f));
				DrawShape(Out, Layer, G, At + FVector2f(0.0f, 2.0f + 2.0f * Lift), Size, T.Shape, Cut, Dim(T.Shadow, 0.6f));
			}
		}
		++Layer;
		// The glow: rings of the glow colour widening out from the edge, under the fill.
		if (bMotion && T.Motion == EMotion::Glow && Hover > 0.0f)
		{
			const float Grow[] = {3.0f, 6.0f, 10.0f};
			const float Alphas[] = {0.45f, 0.25f, 0.1f};
			for (int32 Ring = 0; Ring < 3; ++Ring)
			{
				DrawShape(Out, Layer, G, At - FVector2f(Grow[Ring], Grow[Ring]), Size + FVector2f(Grow[Ring], Grow[Ring]) * 2.0f, T.Shape, Cut + Grow[Ring], Dim(T.Glow, Alphas[Ring] * Hover));
			}
		}
		++Layer;
		// The outline is the box in the edge's colour with the fill inset by its width.
		if (EdgeWidth > 0.0f)
		{
			DrawShape(Out, Layer, G, At, Size, T.Shape, Cut, Dim(Edge));
			++Layer;
		}
		const float Inset = EdgeWidth;
		const float InnerCut = bChamfer ? FMath::Max(0.0f, Cut - Inset * 0.414f) : FMath::Max(0.0f, Cut - Inset);
		DrawShape(Out, Layer, G, At + FVector2f(Inset, Inset), Size - FVector2f(Inset, Inset) * 2.0f, T.Shape, InnerCut, Dim(Fill));
		++Layer;
		// The underline sliding in from the left (a chosen tab's full), and the hatch coming over.
		if (bMotion && T.Motion == EMotion::Underline)
		{
			const float Share = (Face == EFace::Tab && S.bChosen) ? 1.0f : EaseOut(Hover);
			if (Share > 0.0f)
			{
				const FLinearColor Under = Face == EFace::Primary || (Face == EFace::Item && S.bChosen) ? T.AccentInk : (Face == EFace::Tab && S.bChosen ? T.Accent : T.Line);
				const float Thick = 3.0f;
				FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2f((Size.X - 2.0f * Inset) * Share, Thick), FSlateLayoutTransform(FVector2f(Inset, Size.Y - Inset - Thick))), RoundBrush(0.0f), ESlateDrawEffect::None, Dim(Under));
			}
		}
		if (bMotion && T.Motion == EMotion::Hatch && Hover > 0.0f)
		{
			DrawHatch(Out, Layer, G, At + FVector2f(Inset, Inset), Size - FVector2f(Inset, Inset) * 2.0f, InnerCut, Dim(Face == EFace::Primary ? T.AccentInk : T.Line, 0.35f * EaseOut(Hover)));
		}
		++Layer;
		// A dropdown's chevron at the right: down, or up while its list is open.
		if (Face == EFace::Dropdown)
		{
			const float Middle = Size.Y * 0.5f;
			const float Dip = S.bChosen ? -4.0f : 4.0f;
			const float Right = Size.X - 14.0f;
			FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), TArray<FVector2f>{FVector2f(Right - 12.0f, Middle - Dip * 0.5f), FVector2f(Right - 6.0f, Middle + Dip * 0.5f), FVector2f(Right, Middle - Dip * 0.5f)}, ESlateDrawEffect::None, Dim(T.Ink), true, 2.0f);
		}
		// A toggle's knob, slid along its track.
		if (Face == EFace::Toggle)
		{
			const float Gap = 3.0f + Inset;
			const float Knob = Size.Y - 2.0f * Gap;
			const float X = Gap + S.Knob * (Size.X - 2.0f * Gap - Knob);
			const FLinearColor KnobColour = FMath::Lerp(T.Muted, T.AccentInk, S.Knob);
			DrawShape(Out, Layer, G, FVector2f(X, Gap), FVector2f(Knob, Knob), T.Shape, bChamfer ? 4.0f : (T.Shape == EShape::Square ? 0.0f : Knob * 0.5f), Dim(KnobColour));
		}
		return Layer + 1;
	}

	/** Plays a theme's hover or click sound on this machine (CheeseStrikeSynth::ThemeHover, ThemeClick). */
	void PlayThemeSound(const TWeakObjectPtr<ACheeseStrikePlayerController>& Player, int32 Theme, bool bClick)
	{
		if (Player.IsValid())
		{
			UGameplayStatics::PlaySound2D(Player.Get(), CheeseStrikeSynth::MakeSound(Player.Get(), bClick ? CheeseStrikeSynth::ThemeClick(Theme) : CheeseStrikeSynth::ThemeHover(Theme)), 1.0f);
		}
	}

	/**
	 * How far a themed widget is under the mouse, moved a little each frame (0.15 s there and back; the glow 0.2 s), and
	 * the render transform the theme's motion asks for: the lift (up a few pixels, eased) and the bounce (a spring to 1.06
	 * of its size, underdamped, so that it overshoots and settles).
	 */
	struct FHoverMotion
	{
		float Along = 0.0f;
		float Scale = 1.0f;
		float Velocity = 0.0f;

		void Advance(const FTheme& T, bool bHover, float DeltaTime, SWidget& Widget)
		{
			const float Step = FMath::Min(DeltaTime, 0.1f) / (T.Motion == EMotion::Glow ? 0.2f : 0.15f);
			Along = FMath::Clamp(Along + (bHover ? Step : -Step), 0.0f, 1.0f);
			if (T.Motion == EMotion::Bounce)
			{
				const float Target = bHover ? 1.06f : 1.0f;
				const float Dt = FMath::Min(DeltaTime, 0.05f);
				Velocity += (Target - Scale) * 500.0f * Dt;
				Velocity *= FMath::Exp(-Dt * 9.0f);
				Scale = FMath::Clamp(Scale + Velocity * Dt, 0.9f, 1.2f);
				Widget.SetRenderTransform(FMath::Abs(Scale - 1.0f) > 0.0005f ? TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2D(Scale))) : TOptional<FSlateRenderTransform>());
			}
			else if (T.Motion == EMotion::Lift)
			{
				const float Up = 3.0f * EaseOut(Along);
				Widget.SetRenderTransform(Up > 0.01f ? TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(0.0, -Up))) : TOptional<FSlateRenderTransform>());
			}
		}
	};

	/** How many themed buttons the page has made so far (their ordinals, for CheeseStrike.ThemeHover). */
	int32 ThemedButtonsMade = 0;

	TAutoConsoleVariable<int32> CVarThemeHover(TEXT("CheeseStrike.ThemeHover"), 0,
		TEXT("n: the theme sampler's n-th themed button, in the order they are made on the page (a sample's three tabs, its three buttons, its dropdown, its toggle; then the next sample's), is drawn as if hovered, for looking at it; 0: none."));

	/**
	 * A button in a theme's looks: Slate's button with no looks of its own, its face painted by PaintFace, moved by its
	 * theme's motion under the mouse, its theme's sounds on hover and press, and ringed in its theme's focus ring.
	 */
	class SThemedButton : public SButton
	{
	public:
		SLATE_BEGIN_ARGS(SThemedButton) : _Theme(0), _Face(EFace::Plain), _Chosen(false), _Padding(FMargin(16.0f, 9.0f)), _HAlign(HAlign_Center), _Enabled(true) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
			SLATE_ARGUMENT(int32, Theme)
			SLATE_ARGUMENT(EFace, Face)
			SLATE_ATTRIBUTE(bool, Chosen)
			SLATE_ARGUMENT(FMargin, Padding)
			SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
			SLATE_ARGUMENT(bool, Enabled)
			SLATE_EVENT(FOnClicked, OnClicked)
			SLATE_ARGUMENT(TWeakObjectPtr<ACheeseStrikePlayerController>, Player)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			ThemeIndex = InArgs._Theme;
			Face = InArgs._Face;
			Chosen = InArgs._Chosen;
			Player = InArgs._Player;
			Ordinal = ++ThemedButtonsMade;
			Knob = Chosen.Get(false) ? 1.0f : 0.0f;
			TWeakPtr<SThemedButton> Weak = SharedThis(this);
			SButton::Construct(SButton::FArguments()
				.ButtonStyle(&BareStyle())
				.ContentPadding(InArgs._Padding)
				.HAlign(InArgs._HAlign)
				.VAlign(VAlign_Center)
				.OnClicked(InArgs._OnClicked)
				.OnHovered_Lambda([Weak]()
				{
					const TSharedPtr<SThemedButton> Button = Weak.Pin();
					if (Button.IsValid() && Button->IsEnabled())
					{
						PlayThemeSound(Button->Player, Button->ThemeIndex, false);
					}
				})
				.OnPressed_Lambda([Weak]()
				{
					const TSharedPtr<SThemedButton> Button = Weak.Pin();
					if (Button.IsValid() && Button->IsEnabled())
					{
						PlayThemeSound(Button->Player, Button->ThemeIndex, true);
					}
				})
				[
					InArgs._Content.Widget
				]);
			SetEnabled(InArgs._Enabled);
			SetRenderTransformPivot(FVector2D(0.5, 0.5));
		}

		/** Whether it is under the mouse now (or held so for looking at it): what its text colour and its motion follow. */
		bool IsLit() const
		{
			return IsEnabled() && Face != EFace::Disabled && (IsHovered() || CVarThemeHover.GetValueOnGameThread() == Ordinal);
		}

		virtual const FSlateBrush* GetFocusBrush() const override
		{
			return SCheeseStrikeMenu::AreFocusRingsShown() ? FocusRing(ThemeIndex) : nullptr;
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SButton::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
			Motion.Advance(Theme(ThemeIndex), IsLit(), InDeltaTime, *this);
			const float Step = FMath::Min(InDeltaTime, 0.1f) / 0.15f;
			Knob = FMath::Clamp(Knob + (Chosen.Get(false) ? Step : -Step), 0.0f, 1.0f);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			FPaintState State;
			State.Hover = Motion.Along;
			State.bPressed = IsPressed();
			State.Knob = Knob;
			State.bChosen = Chosen.Get(false);
			const int32 Over = PaintFace(Theme(ThemeIndex), Face, AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, State);
			return SButton::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, Over, InWidgetStyle, bParentEnabled);
		}

	private:
		int32 ThemeIndex = 0;
		EFace Face = EFace::Plain;
		TAttribute<bool> Chosen;
		TWeakObjectPtr<ACheeseStrikePlayerController> Player;
		int32 Ordinal = 0;
		FHoverMotion Motion;
		float Knob = 0.0f;
	};

	/** A box in a theme's looks (a card, a field, a chip, a list, the sample's panel, a tooltip), painted by PaintFace; a card moves under the mouse as the theme's buttons do. */
	class SThemedBox : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SThemedBox) : _Theme(0), _Face(EFace::Card), _Padding(FMargin(16.0f, 12.0f)) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
			SLATE_ARGUMENT(int32, Theme)
			SLATE_ARGUMENT(EFace, Face)
			SLATE_ARGUMENT(FMargin, Padding)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			ThemeIndex = InArgs._Theme;
			Face = InArgs._Face;
			ChildSlot.Padding(InArgs._Padding) [ InArgs._Content.Widget ];
			SetRenderTransformPivot(FVector2D(0.5, 0.5));
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
			if (Face == EFace::Card)
			{
				Motion.Advance(Theme(ThemeIndex), IsHovered(), InDeltaTime, *this);
			}
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			FPaintState State;
			State.Hover = Motion.Along;
			State.bFocused = Face == EFace::Field && HasFocusedDescendants();
			const int32 Over = PaintFace(Theme(ThemeIndex), Face, AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, State);
			return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, Over, InWidgetStyle, bParentEnabled);
		}

	private:
		int32 ThemeIndex = 0;
		EFace Face = EFace::Card;
		FHoverMotion Motion;
	};

	/** The sampler's slider: Slate's, in the theme's style, ringed in the theme's focus ring. */
	class SThemedSlider : public SSlider
	{
	public:
		void SetTheme(int32 Index) { ThemeIndex = Index; }
		virtual const FSlateBrush* GetFocusBrush() const override
		{
			return SCheeseStrikeMenu::AreFocusRingsShown() ? FocusRing(ThemeIndex) : nullptr;
		}

	private:
		int32 ThemeIndex = 0;
	};

	/** A themed dropdown's open list: up and down move along its items and stop at the ends, nothing leaves it sideways, B or Escape shut it (as the menu's own, SCheeseStrikeDropList). */
	class SThemedDropList : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SThemedDropList) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
			SLATE_ARGUMENT(TArray<TSharedPtr<SWidget>>, Items)
			SLATE_EVENT(FSimpleDelegate, OnClose)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Items = InArgs._Items;
			OnClose = InArgs._OnClose;
			ChildSlot [ InArgs._Content.Widget ];
		}

		virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent) override
		{
			const EUINavigation Way = InNavigationEvent.GetNavigationType();
			if (Way == EUINavigation::Up || Way == EUINavigation::Down)
			{
				int32 At = INDEX_NONE;
				for (int32 Index = 0; Index < Items.Num(); ++Index)
				{
					At = Items[Index].IsValid() && Items[Index]->HasUserFocus(InNavigationEvent.GetUserIndex()).IsSet() ? Index : At;
				}
				const int32 To = FMath::Clamp(At + (Way == EUINavigation::Down ? 1 : -1), 0, Items.Num() - 1);
				if (Items.IsValidIndex(To) && To != At)
				{
					return FNavigationReply::Explicit(Items[To]);
				}
			}
			return FNavigationReply::Stop();
		}

		virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
		{
			return InKeyEvent.GetKey() == EKeys::SpaceBar ? FReply::Handled() : FReply::Unhandled();
		}

		virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
		{
			return InKeyEvent.GetKey() == EKeys::SpaceBar ? FReply::Handled() : SCompoundWidget::OnKeyUp(MyGeometry, InKeyEvent);
		}

		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
		{
			if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
			{
				OnClose.ExecuteIfBound();
				return FReply::Handled();
			}
			return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
		}

	private:
		TArray<TSharedPtr<SWidget>> Items;
		FSimpleDelegate OnClose;
	};

	/** The colour of a button's words in theme `T`: by its face, chosen, and under the mouse. */
	FLinearColor TextColour(const FTheme& T, EFace Face, bool bChosen, bool bLit)
	{
		switch (Face)
		{
		case EFace::Primary: return T.AccentInk;
		case EFace::Disabled: return T.DisabledInk;
		case EFace::Tab:
			if (bChosen)
			{
				return T.Motion == EMotion::Underline || T.Motion == EMotion::Glow ? T.Accent : T.AccentInk;
			}
			return bLit ? T.Ink : T.Muted;
		case EFace::Item: return bChosen ? T.AccentInk : T.Ink;
		default: return T.Ink;
		}
	}

	/** A line of text in the theme's looks. */
	TSharedRef<STextBlock> ThemedText(const FString& Text, const FSlateFontInfo& Font, const TAttribute<FSlateColor>& Colour)
	{
		return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font).ColorAndOpacity(Colour);
	}

	/**
	 * A button in theme `Index`'s looks with `Text` (a live attribute) in the theme's button type, coloured by its face and
	 * its state; `Chosen` for a tab or an item (or a dropdown's open state); `Player` for the sounds.
	 */
	TSharedRef<SThemedButton> MakeThemedButton(int32 Index, EFace Face, const TAttribute<FText>& Text, const TAttribute<bool>& Chosen, FOnClicked OnClicked, const TWeakObjectPtr<ACheeseStrikePlayerController>& Player,
		const FMargin& Padding = FMargin(16.0f, 9.0f), EHorizontalAlignment HAlign = HAlign_Center, int32 Size = 17)
	{
		const FTheme& T = Theme(Index);
		// The words read the button's state as they are drawn: the button is made round them, so they reach it through this.
		const TSharedRef<TWeakPtr<SThemedButton>> Holder = MakeShared<TWeakPtr<SThemedButton>>();
		const TAttribute<FSlateColor> Colour = TAttribute<FSlateColor>::CreateLambda([Holder, Index, Face, Chosen]()
		{
			const TSharedPtr<SThemedButton> Button = Holder->Pin();
			return FSlateColor(TextColour(Theme(Index), Face, Chosen.Get(false), Button.IsValid() && Button->IsLit()));
		});
		TSharedRef<SThemedButton> Button = SNew(SThemedButton)
			.Theme(Index)
			.Face(Face)
			.Chosen(Chosen)
			.Padding(Padding)
			.HAlign(HAlign)
			.Enabled(Face != EFace::Disabled)
			.OnClicked(OnClicked)
			.Player(Player)
			[
				SNew(STextBlock).Text(Text).Font(ButtonFont(T, Size)).ColorAndOpacity(Colour)
			];
		*Holder = Button;
		return Button;
	}

	/** The card's tooltip in the theme's looks: a title in the accent over the lines, on a card of the theme's shape. */
	TSharedRef<SWidget> MakeThemedTooltip(int32 Index, const FString& Title, const FString& Text)
	{
		const FTheme& T = Theme(Index);
		return SNew(SBox).Visibility(EVisibility::HitTestInvisible)
			[
				SNew(SThemedBox).Theme(Index).Face(EFace::Tooltip).Padding(FMargin(14.0f, 9.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f)) [ ThemedText(Label(Index, Title), ButtonFont(T, 13), T.Shape == EShape::Chamfer ? T.Line : T.Accent) ]
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(FText::FromString(Text)).Font(BodyFont(T, 14)).ColorAndOpacity(T.Ink).WrapTextAt(CheeseStrikeMenu::TooltipWrap) ]
				]
			];
	}

	/** A row's small label, in the theme's body type, muted. */
	TSharedRef<SWidget> RowLabel(int32 Index, const FString& Text)
	{
		const FTheme& T = Theme(Index);
		return SNew(SBox).VAlign(VAlign_Center).Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f)) [ ThemedText(Text, BodyFont(T, 15), T.Muted) ];
	}
}

using namespace CheeseStrikeThemes;

TSharedRef<SWidget> SCheeseStrikeMenu::MakeThemedDropdown(int32 Index, const TArray<FString>& Choices, TFunction<int32()> GetChosen, TFunction<void(int32)> Pick)
{
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	const TAttribute<FText> Shown = TAttribute<FText>::CreateLambda([Index, Choices, GetChosen]()
	{
		const int32 Chosen = GetChosen();
		return FText::FromString(Choices.IsValidIndex(Chosen) ? Label(Index, Choices[Chosen]) : FString(TEXT("-")));
	});
	// As MakeDropdown: the anchor is made after its list's buttons, which close it, and reach it through this.
	const TSharedRef<TWeakPtr<SMenuAnchor>> Holder = MakeShared<TWeakPtr<SMenuAnchor>>();
	const TSharedRef<TWeakPtr<SWidget>> ChosenItem = MakeShared<TWeakPtr<SWidget>>();
	const TSharedRef<TWeakPtr<SWidget>> Opener = MakeShared<TWeakPtr<SWidget>>();
	const auto List = [this, Index, Holder, ChosenItem, Opener, Choices, GetChosen, Pick, Player]() -> TSharedRef<SWidget>
	{
		const int32 Chosen = GetChosen();
		const TSharedPtr<SMenuAnchor> Anchor = Holder->Pin();
		const float Width = Anchor.IsValid() ? static_cast<float>(Anchor->GetCachedGeometry().GetLocalSize().X) : 0.0f;
		TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
		TArray<TSharedPtr<SWidget>> Buttons;
		for (int32 Each = 0; Each < Choices.Num(); ++Each)
		{
			TSharedRef<SThemedButton> Item = MakeThemedButton(Index, EFace::Item, FText::FromString(Label(Index, Choices[Each])), Each == Chosen, FOnClicked::CreateLambda([Holder, Pick, Each]()
			{
				if (const TSharedPtr<SMenuAnchor> Open = Holder->Pin())
				{
					Open->SetIsOpen(false);
				}
				Pick(Each);
				return FReply::Handled();
			}), Player, FMargin(12.0f, 7.0f), HAlign_Left);
			Buttons.Add(Item);
			if (Each == Chosen || (Each == 0 && !Choices.IsValidIndex(Chosen)))
			{
				*ChosenItem = Item;
			}
			Items->AddSlot().AutoHeight().Padding(FMargin(0.0f, 1.0f)) [ Item ];
		}
		return SNew(SThemedDropList)
			.Items(Buttons)
			.OnClose_Lambda([this, Holder, Opener]()
			{
				if (const TSharedPtr<SMenuAnchor> Open = Holder->Pin())
				{
					Open->SetIsOpen(false);
				}
				DropFocus.Reset();
				GiveFocus(Opener->Pin());
			})
			[
				SNew(SBox).MinDesiredWidth(Width)
				[
					SNew(SThemedBox).Theme(Index).Face(EFace::List).Padding(FMargin(4.0f)) [ Items ]
				]
			];
	};
	TSharedRef<SMenuAnchor> Anchor = SNew(SMenuAnchor)
		.Placement(MenuPlacement_ComboBox)
		.Method(EPopupMethod::UseCurrentWindow)
		.ShowMenuBackground(false)
		.OnGetMenuContent_Lambda(List);
	*Holder = Anchor;
	Dropdowns.Add(Anchor);
	TWeakPtr<SMenuAnchor> Weak = Anchor;
	const TAttribute<bool> IsOpen = TAttribute<bool>::CreateLambda([Weak]() { const TSharedPtr<SMenuAnchor> Open = Weak.Pin(); return Open.IsValid() && Open->IsOpen(); });
	TSharedRef<SThemedButton> Button = MakeThemedButton(Index, EFace::Dropdown, Shown, IsOpen, FOnClicked::CreateLambda([this, Weak, ChosenItem]()
	{
		if (const TSharedPtr<SMenuAnchor> Open = Weak.Pin())
		{
			Open->SetIsOpen(!Open->IsOpen());
			DropFocus = Open->IsOpen() ? *ChosenItem : TWeakPtr<SWidget>();
			KeepFocus();
		}
		return FReply::Handled();
	}), Player, FMargin(14.0f, 9.0f, 40.0f, 9.0f), HAlign_Left);
	*Opener = Button;
	DropdownButtons.Add(Button);
	Anchor->SetContent(Focusable(FString::Printf(TEXT("Dropdown %d"), Dropdowns.Num()), Button));
	return Anchor;
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeThemeSample(int32 Index)
{
	const FTheme& T = Theme(Index);
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	if (!ThemeSamples.IsValidIndex(Index))
	{
		ThemeSamples.SetNum(NumThemes);
	}
	const auto Press = [](TFunction<void()> Do) { return FOnClicked::CreateLambda([Do]() { Do(); return FReply::Handled(); }); };
	const auto Nothing = []() { return FReply::Handled(); };
	// The sample's tabs: the one chosen is kept per sample and read live.
	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
	const TCHAR* const TabNames[] = {TEXT("Overview"), TEXT("Rules"), TEXT("Seats")};
	for (int32 Tab = 0; Tab < UE_ARRAY_COUNT(TabNames); ++Tab)
	{
		const FString Name = Label(Index, TabNames[Tab]);
		Tabs->AddSlot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
		[
			Focusable(Name, MakeThemedButton(Index, EFace::Tab, FText::FromString(Name), TAttribute<bool>::CreateLambda([this, Index, Tab]() { return ThemeSamples.IsValidIndex(Index) && ThemeSamples[Index].Tab == Tab; }),
				Press([this, Index, Tab]() { if (ThemeSamples.IsValidIndex(Index)) ThemeSamples[Index].Tab = Tab; }), Player, FMargin(14.0f, 7.0f), HAlign_Center, 15))
		];
	}
	// The buttons: primary, secondary, disabled; and a chip at the row's end.
	const FString Primary = Label(Index, TEXT("Start the match"));
	const FString Secondary = Label(Index, TEXT("Leave"));
	const FString Disabled = Label(Index, TEXT("Waiting for players"));
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f)) [ Focusable(Primary, MakeThemedButton(Index, EFace::Primary, FText::FromString(Primary), false, FOnClicked::CreateLambda(Nothing), Player)) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f)) [ Focusable(Secondary, MakeThemedButton(Index, EFace::Plain, FText::FromString(Secondary), false, FOnClicked::CreateLambda(Nothing), Player)) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f)) [ Focusable(Disabled, MakeThemedButton(Index, EFace::Disabled, FText::FromString(Disabled), false, FOnClicked::CreateLambda(Nothing), Player)) ]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SThemedBox).Theme(Index).Face(EFace::Chip).Padding(FMargin(11.0f, 3.0f)) [ ThemedText(Label(Index, TEXT("8 / 16 in")), ButtonFont(T, 13), T.AccentInk) ]
		];
	// The dropdown and the toggle.
	const TArray<FString> Maps = {TEXT("Gruyère Grotto"), TEXT("The Creamery"), TEXT("The Market"), TEXT("The Proving Ground")};
	TSharedRef<SThemedButton> Toggle = SNew(SThemedButton)
		.Theme(Index)
		.Face(EFace::Toggle)
		.Padding(FMargin(0.0f))
		.Chosen(TAttribute<bool>::CreateLambda([this, Index]() { return ThemeSamples.IsValidIndex(Index) && ThemeSamples[Index].bOn; }))
		.OnClicked(Press([this, Index]() { if (ThemeSamples.IsValidIndex(Index)) ThemeSamples[Index].bOn = !ThemeSamples[Index].bOn; }))
		.Player(Player)
		[
			SNew(SBox).WidthOverride(48.0f).HeightOverride(24.0f)
		];
	TSharedRef<SHorizontalBox> Pickers = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth() [ RowLabel(Index, TEXT("Map")) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(300.0f)
			[
				MakeThemedDropdown(Index, Maps, [this, Index]() { return ThemeSamples.IsValidIndex(Index) ? ThemeSamples[Index].Choice : 0; }, [this, Index](int32 Choice) { if (ThemeSamples.IsValidIndex(Index)) ThemeSamples[Index].Choice = Choice; })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(28.0f, 0.0f, 0.0f, 0.0f)) [ RowLabel(Index, TEXT("Aim assist")) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Focusable(TEXT("Toggle"), Toggle) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock).Font(ButtonFont(T, 14)).ColorAndOpacity(PanelInk(T)).Text_Lambda([this, Index]() { return FText::FromString(Label(Index, ThemeSamples.IsValidIndex(Index) && ThemeSamples[Index].bOn ? TEXT("On") : TEXT("Off"))); })
		];
	// The slider, its value written beside it.
	TSharedRef<SThemedSlider> Slider = SNew(SThemedSlider)
		.Style(&SliderStyle(Index))
		.MinValue(0.0f)
		.MaxValue(1.0f)
		.StepSize(0.01f)
		.MouseUsesStep(true)
		.RequiresControllerLock(false)
		.Value_Lambda([this, Index]() { return ThemeSamples.IsValidIndex(Index) ? ThemeSamples[Index].Value : 0.7f; })
		.OnValueChanged_Lambda([this, Index](float Value) { if (ThemeSamples.IsValidIndex(Index)) ThemeSamples[Index].Value = Value; });
	Slider->SetTheme(Index);
	TSharedRef<SHorizontalBox> SliderRow = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth() [ RowLabel(Index, TEXT("Music volume")) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ SNew(SBox).WidthOverride(300.0f) [ Focusable(TEXT("Slider"), Slider) ] ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock).Font(ButtonFont(T, 14)).ColorAndOpacity(PanelInk(T)).Text_Lambda([this, Index]() { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt((ThemeSamples.IsValidIndex(Index) ? ThemeSamples[Index].Value : 0.7f) * 100.0f))); })
		];
	// The text field, and the card with its tooltip.
	TSharedRef<SEditableText> Field = SNew(SEditableText)
		.HintText(FText::FromString(TEXT("Your name")))
		.Font(BodyFont(T, 16))
		.ColorAndOpacity(T.Ink)
		.RevertTextOnEscape(true)
		.ClearKeyboardFocusOnCommit(true);
	const FString CardTitle = TEXT("Deathmatch");
	TSharedRef<SWidget> Card = WithTooltipMade(
		SNew(SThemedBox).Theme(Index).Face(EFace::Card).Padding(FMargin(16.0f, 10.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 3.0f)) [ ThemedText(Label(Index, CardTitle), ButtonFont(T, 15), T.Shape == EShape::Chamfer ? T.Line : T.Accent) ]
			+ SVerticalBox::Slot().AutoHeight() [ ThemedText(TEXT("Gruyère Grotto, to 50 kills"), BodyFont(T, 14), T.Ink) ]
			+ SVerticalBox::Slot().AutoHeight() [ ThemedText(TEXT("7 bots of normal skill, back 5 s after a death"), BodyFont(T, 14), T.Muted) ]
		],
		[Index, CardTitle]() { return MakeThemedTooltip(Index, TEXT("The lobby's rules"), TEXT("What the host set, as a joiner sees it: the mode, the map, the kill limit, the bots and the time back after a death.")); });
	TSharedRef<SHorizontalBox> Lower = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
		[
			SNew(SBox).WidthOverride(300.0f)
			[
				SNew(SThemedBox).Theme(Index).Face(EFace::Field).Padding(FMargin(12.0f, 9.0f)) [ Focusable(TEXT("Text field"), Field) ]
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(16.0f, 0.0f, 0.0f, 0.0f)) [ Card ];
	// The sample on its own panel: the title and the blurb, then the rows.
	return SNew(SThemedBox).Theme(Index).Face(EFace::Panel).Padding(FMargin(24.0f, 18.0f, 24.0f, 22.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight() [ ThemedText(Label(Index, T.Name), TitleFont(T), T.Shape == EShape::Chamfer ? T.Line : T.Accent) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 2.0f, 0.0f, 12.0f)) [ SNew(STextBlock).Text(FText::FromString(T.Blurb)).Font(BodyFont(T, 14)).ColorAndOpacity(T.Muted).WrapTextAt(820.0f) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f)) [ Tabs ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f)) [ Buttons ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f)) [ Pickers ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f)) [ SliderRow ]
			+ SVerticalBox::Slot().AutoHeight() [ Lower ]
		];
}

TSharedRef<SWidget> SCheeseStrikeMenu::MakeThemes()
{
	ThemedButtonsMade = 0;
	// The page's dropdowns are kept as the lobby's are (CheeseStrikeDropdown n opens the n-th): those of the page last drawn are gone.
	Dropdowns.Reset();
	DropdownButtons.Reset();
	if (ThemeSamples.Num() != NumThemes)
	{
		ThemeSamples.SetNum(NumThemes);
	}
	TWeakObjectPtr<ACheeseStrikePlayerController> Player = Controller;
	TArray<FString> Tabs;
	for (int32 Index = 0; Index < NumThemes; ++Index)
	{
		Tabs.Add(FText::FromString(Theme(Index).Name).ToUpper().ToString());
	}
	Tabs.Add(TEXT("SHOW ALL"));
	// The tabs are made before the samples, so that they come first among the page's items (the focus starts on them).
	TSharedRef<SWidget> TabRow = MakeChoiceButtons(Tabs, bAllThemes ? NumThemes : ThemeShown, true, [this](int32 Index)
	{
		bAllThemes = Index >= NumThemes;
		ThemeShown = bAllThemes ? ThemeShown : Index;
		Refresh();
	});
	TSharedRef<SVerticalBox> Samples = SNew(SVerticalBox);
	if (bAllThemes)
	{
		for (int32 Index = 0; Index < NumThemes; ++Index)
		{
			Samples->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 16.0f)) [ MakeThemeSample(Index) ];
		}
	}
	else
	{
		Samples->AddSlot().AutoHeight() [ MakeThemeSample(FMath::Clamp(ThemeShown, 0, NumThemes - 1)) ];
	}
	TSharedPtr<SScrollBox> Scroll;
	TSharedRef<SWidget> ThemesPage = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ MakeTitle(TEXT("THEMES")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 14.0f)) [ TabRow ]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SAssignNew(Scroll, SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
			+ SScrollBox::Slot().Padding(FMargin(0.0f, 4.0f, 14.0f, 4.0f)) [ Samples ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
		[
			MakeLine(TEXT("A temporary sampler to pick a look from: hover and click for each theme's own sounds. Not in shipping builds."), true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f)).HAlign(HAlign_Left)
		[
			SNew(SBox).WidthOverride(304.0f) [ MakeButton(TEXT("BACK"), [Player]() { if (Player.IsValid()) Player->BackFromMenu(); }) ]
		];
	CreditsScroll = Scroll;
	return ThemesPage;
}

void SCheeseStrikeMenu::ShowTheme(int32 Index)
{
	bAllThemes = Index >= NumThemes;
	ThemeShown = bAllThemes ? ThemeShown : FMath::Max(0, Index);
}

FString SCheeseStrikeMenu::ThemesKey() const
{
	return FString::Printf(TEXT("theme %d all %d"), ThemeShown, bAllThemes ? 1 : 0);
}
