// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CheeseStrikeModes.h"
#include "Weapons/CheeseStrikeGrenadeRules.h"

/**
 * The loadout, as plain rules a test can call without a world: what a
 * player chooses before a match (a primary among the three weakest, a kind
 * of grenade, a hero), what each is to begin with, which modes have one,
 * and when it may be changed. The choices are kept in the settings
 * (UCheeseStrikeSettings) and on the player state (ACheeseStrikePlayerState,
 * sent by ACheeseStrikePlayerController::ServerSetLoadout), chosen on the
 * loadout page (EMenuPage::Loadout) and applied by the server as a character
 * comes in, or at once during a count-in.
 */
namespace CheeseStrikeLoadout
{
	/** The primaries to choose from, in the order the page shows them: the Colby Carbine, the Cheddar Blaster and the Munster Twin SMGs, the three weakest. */
	const TArray<FName>& Primaries();
	/** Whether `Id` is one of them. */
	bool IsPrimaryOffered(FName Id);
	/** What a fresh install carries: the Carbine, the cheese grenade, Lt. Belica (CheeseStrikeBodies::DefaultHero). */
	FName DefaultPrimary();
	constexpr CheeseStrikeGrenades::EGrenadeKind DefaultGrenade = CheeseStrikeGrenades::EGrenadeKind::Cheese;
	int32 DefaultHero();
	/** `Id` if it is offered, else the default. */
	FName PrimaryOrDefault(FName Id);

	/** Whether `Mode` is played with a loadout: every mode but Gun Game, whose ladder gives the weapons. */
	bool HasLoadout(CheeseStrikeModes::EMode Mode);

	/** Where a player is, as far as changing the loadout goes. */
	enum class EWhen : uint8
	{
		/** On the front end, or alone on a level of their own (no match with others). */
		FrontEnd,
		/** In a lobby, the match waiting to start (counting down too). */
		Lobby,
		/** In a match, held by the count before it or before a round. */
		CountIn,
		/** In a match, play on (paused or not). */
		Playing,
		/** The match is over. */
		Over,
	};
	/** Whether the loadout may be changed `When`: on the front end, in the lobby and during a count-in; never otherwise, the pause menu included. */
	bool MayChange(EWhen When);
	/**
	 * Where a player is: not `bInAMatch` (a match with others: a level with a connection of its own) is the front end;
	 * in one, before it starts the lobby, held by a count (`bCountingIn`) a count-in, over (`bMatchEnded`) over, else
	 * playing.
	 */
	EWhen WhenFor(bool bInAMatch, bool bMatchStarted, bool bMatchEnded, bool bCountingIn);

	/**
	 * Whether the loadout page shows of itself: a count-in runs (`bCountingIn`) in a mode with a loadout
	 * (`bHasLoadout`), and the player hasn't put the page away for this count (`DismissedGoAt`: the count's end they
	 * dismissed it for, as ACheeseStrikeGameState::GetGoAt has it; 0 for none).
	 */
	bool OffersPage(bool bHasLoadout, bool bCountingIn, float GoAt, float DismissedGoAt);

	/** What the page says over its choices during a count-in: "THE MATCH BEGINS IN 8", or "THE ROUND BEGINS IN 5" for a later round (`bRound`); `Left` seconds, rounded up. */
	FString CountText(bool bRound, float Left);
}
