// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * TEMPORARY: a sampler of five looks for the menus, for picking one from
 * (wave 15, group 9). A dev page of the menu (EMenuPage::Themes, the front
 * end's THEMES button outside shipping builds, `CheeseStrikeThemes` on the
 * console) shows each theme's sample: a title, a primary, a secondary and a
 * disabled button, a dropdown, a slider, a toggle, a text field, a row of
 * tabs, a card with a tooltip and a chip, each theme with its own shape,
 * type, motion and sounds (CheeseStrikeSynth::ThemeHover, ThemeClick). To be
 * reverted, or trimmed to the theme picked, once the user has chosen.
 */
namespace CheeseStrikeThemes
{
	constexpr int32 NumThemes = 5;

	/** How a theme's corners are cut. */
	enum class EShape : uint8
	{
		/** Rounded by Rounding. */
		Rounded,
		/** Square. */
		Square,
		/** Chamfered: cut straight across by Rounding, drawn as polygons. */
		Chamfer,
	};

	/** What a theme does under the mouse. */
	enum class EMotion : uint8
	{
		/** Lifts a little, its shadow growing (the Cheese Board). */
		Lift,
		/** A thick underline slides in from the left (the Deli Counter). */
		Underline,
		/** A glow comes up round it (Cave Aged). */
		Glow,
		/** A bouncy scale, overshooting and settling (Cartoon Cheese). */
		Bounce,
		/** A diagonal hatch comes over it (Tactical). */
		Hatch,
	};

	struct FTheme
	{
		const TCHAR* Name;
		/** One line on what it is. */
		const TCHAR* Blurb;
		EShape Shape;
		/** The corner's radius, or the chamfer's cut, in Slate units. */
		float Rounding;
		/** The outline's width on buttons, cards and fields; 0 for none. */
		float Outline;
		EMotion Motion;
		/** A drop shadow under buttons and cards. */
		bool bShadow;
		/** Type: the faces of the engine's default font (Regular, Bold, Light, Black, BoldCondensed) and their tracking (letter spacing, in thousandths of an em). */
		FName TitleFace;
		int32 TitleTracking;
		FName BodyFace;
		FName ButtonFace;
		int32 ButtonTracking;
		/** Whether buttons and titles are set in capitals. */
		bool bUpper;
		/** A dark outline round the titles' and buttons' letters, in pixels; 0 for none. */
		int32 TextOutline;
		/** Colours: the sample's own panel, surfaces (plain buttons, cards), the accent (primary buttons, chips, the chosen), lines, text. */
		FLinearColor Panel;
		FLinearColor Surface;
		FLinearColor SurfaceLit;
		FLinearColor Ink;
		FLinearColor Muted;
		FLinearColor Accent;
		FLinearColor AccentLit;
		FLinearColor AccentInk;
		FLinearColor Line;
		FLinearColor Shadow;
		FLinearColor Glow;
		FLinearColor Disabled;
		FLinearColor DisabledInk;
		FLinearColor Field;
	};

	/** The theme at `Index` (0 to NumThemes - 1; out of range, the first). */
	const FTheme& Theme(int32 Index);
	/** `Text` as the theme sets its buttons and titles: in capitals if it does. */
	FString Label(int32 Index, const FString& Text);
}
