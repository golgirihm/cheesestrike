// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The game's credits, as the front end's Credits page shows them: who made
 * what the game is made of. Every pack or file brought in (a row in
 * Content/LICENSES.md) gets a line here in the same change, and every one
 * whose licence asks for credit (Creative Commons Attribution) must be named
 * here as its licence says (CheeseStrike.Project.TheCreditsNameEveryone checks).
 */
namespace CheeseStrikeCredits
{
	struct FCredit
	{
		/** What it is in the game, as a heading. */
		const TCHAR* Heading;
		/** Who made it and under what terms, one or more lines. */
		const TCHAR* Text;
	};
	extern const FCredit Credits[];
	int32 NumCredits();
	/** Every heading and line, for searching. */
	FString AllText();
}
