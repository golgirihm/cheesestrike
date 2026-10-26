// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CheeseStrikeAvatars.generated.h"

class APlayerState;
class UTexture2D;

/**
 * Players' Steam profile pictures, for the row of players at the top of
 * the HUD: asked of Steam (SteamFriends, then SteamUtils::GetImageRGBA) by
 * the Steam id in each player's unique net id, and kept as textures made
 * in code. Only when the game is on Steam: the editor, the tests and
 * tools/play.ps1 use the Null service, and bots have no Steam id, so they
 * get none and the HUD draws their initials instead.
 */
UCLASS(Transient)
class CHEESESTRIKE_API UCheeseStrikeAvatars : public UObject
{
	GENERATED_BODY()

public:
	/** `Player`'s picture, or null: without Steam, for a bot, or until Steam has it (it is asked again now and then). */
	UTexture2D* For(const APlayerState* Player);
	/** The picture of the Steam user with this unique id (its type and text), or null: as For, for a friend who isn't in the game. */
	UTexture2D* ForUniqueId(const FString& Type, const FString& Text, const UWorld* World);

	/** Whether this game is on the Steam online service. */
	static bool UsesSteam(const UWorld* World);

private:
	UPROPERTY(Transient)
	TMap<uint64, TObjectPtr<UTexture2D>> Pictures;
	/** When to ask Steam again for a picture it didn't have, by Steam id (the world's time). */
	TMap<uint64, double> AskAgainAt;
};
