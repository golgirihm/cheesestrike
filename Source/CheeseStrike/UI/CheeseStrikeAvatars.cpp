// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeAvatars.h"
#include "CheeseStrikeHUDDraw.h"
#include "CheeseStrikeHUDRules.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"

#if CHEESESTRIKE_STEAM
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

bool UCheeseStrikeAvatars::UsesSteam(const UWorld* World)
{
	const IOnlineSubsystem* Online = World != nullptr ? Online::GetSubsystem(World) : nullptr;
	return Online != nullptr && Online->GetSubsystemName() == STEAM_SUBSYSTEM;
}

UTexture2D* UCheeseStrikeAvatars::For(const APlayerState* Player)
{
	if (Player == nullptr || Player->IsABot() || !Player->GetUniqueId().IsValid())
	{
		return nullptr;
	}
	return ForUniqueId(Player->GetUniqueId().GetType().ToString(), Player->GetUniqueId().ToString(), Player->GetWorld());
}

UTexture2D* UCheeseStrikeAvatars::ForUniqueId(const FString& Type, const FString& Text, const UWorld* World)
{
	const uint64 Id = CheeseStrikeHUD::SteamIdFrom(Type, Text);
	if (Id == 0)
	{
		return nullptr;
	}
	if (const TObjectPtr<UTexture2D>* Found = Pictures.Find(Id))
	{
		return *Found;
	}
	if (World == nullptr || !UsesSteam(World))
	{
		return nullptr;
	}
	const double Now = World->GetRealTimeSeconds();
	if (const double* Again = AskAgainAt.Find(Id); Again != nullptr && Now < *Again)
	{
		return nullptr;
	}
	AskAgainAt.Add(Id, Now + 2.0);
#if CHEESESTRIKE_STEAM
	ISteamFriends* Friends = SteamFriends();
	ISteamUtils* Utilities = SteamUtils();
	if (Friends == nullptr || Utilities == nullptr)
	{
		return nullptr;
	}
	const CSteamID Steam(static_cast<uint64>(Id));
	const int Image = Friends->GetMediumFriendAvatar(Steam);
	if (Image <= 0)
	{
		// 0: Steam doesn't have them yet (someone not a friend), so ask it to fetch them; -1: on its way.
		if (Image == 0)
		{
			Friends->RequestUserInformation(Steam, false);
		}
		return nullptr;
	}
	uint32 Width = 0;
	uint32 Height = 0;
	if (!Utilities->GetImageSize(Image, &Width, &Height) || Width == 0 || Height == 0 || Width > 1024 || Height > 1024)
	{
		return nullptr;
	}
	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(Width * Height * 4);
	if (!Utilities->GetImageRGBA(Image, Pixels.GetData(), Pixels.Num()))
	{
		return nullptr;
	}
	UTexture2D* Picture = CheeseStrikeHUDDraw::MakeTexture(Width, Height, Pixels, PF_R8G8B8A8);
	if (Picture != nullptr)
	{
		Pictures.Add(Id, Picture);
	}
	return Picture;
#else
	return nullptr;
#endif
}
