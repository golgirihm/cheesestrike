// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/CheeseStrikeInput.h"

class UCheeseStrikeInputSet;
class UInputAction;

/**
 * The controls page (SCheeseStrikeMenu, EMenuPage::Controls): every action
 * the game has, with its keys on the keyboard and mouse and its buttons on
 * an Xbox pad and a Nintendo (Switch Pro) one, read from the bindings
 * themselves (UCheeseStrikeInputSet, Input/CheeseStrikeInput.cpp), so that
 * the page says what the game does. An action the page has no row for (a
 * new one) still gets a row, named after itself, at the end.
 */
namespace CheeseStrikeControls
{
	/** Which gamepad's names a button goes by: the same button in the same place is A on an Xbox pad and B on a Nintendo one. */
	enum class EPad : uint8
	{
		Xbox,
		PlayStation,
		Nintendo,
	};
	/** The prompts' glyphs for a pad, and the pad for the prompts' glyphs (Auto as Xbox). */
	CheeseStrikeInput::EGlyphs GlyphsFor(EPad Pad);
	EPad PadFor(CheeseStrikeInput::EGlyphs Glyphs);

	/** A key on the keyboard or the mouse as the page writes it: "W", "SPACE", "LMB", "WHEEL DOWN", "MOUSE". */
	FString KeyboardLabel(const FKey& Key);
	/** A gamepad's button or stick as `Pad` has it: "A", "CROSS" or "B", "RT", "R2" or "ZR", "LEFT STICK", "PRESS LS", "L3". */
	FString PadLabel(const FKey& Key, EPad Pad);
	/** Several keys in a cell, in a sensible order (WASD before the arrows), one way of each written once. */
	FString JoinLabels(TArray<FString> Labels);

	/** A row of the page. */
	struct FRow
	{
		FString Action;
		FString Keyboard;
		FString Xbox;
		FString PlayStation;
		FString Nintendo;
		/** The actions it shows (a held key on the keyboard and a pressed button on a pad can be two). */
		TArray<const UInputAction*> Actions;
	};
	/** The page's rows for `Set`: its actions in the page's order, then any it doesn't know of. */
	TArray<FRow> Rows(const UCheeseStrikeInputSet& Set);
	/** Every action `Set` has (its UInputAction properties), whether it has keys or not. */
	TArray<const UInputAction*> AllActions(const UCheeseStrikeInputSet& Set);
}
