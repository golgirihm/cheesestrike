// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCredits.h"

namespace CheeseStrikeCredits
{
	const FCredit Credits[] = {
		{TEXT("CheeseStrike: Gouda Overload"), TEXT("Made by Hamid Golgiri.")},
		{TEXT("Engine"), TEXT("Made with Unreal Engine 5, by Epic Games. Begun from its First Person template.")},
		{TEXT("The soldiers"), TEXT("Lt. Belica, Murdock, Twinblast and Wraith, from Paragon, by Epic Games (Fab).")},
		{TEXT("Animations, the rifle, the pistol and the shotgun and their sounds, and the bursts of the streaks"), TEXT("From Lyra Starter Game, by Epic Games.")},
		{TEXT("The weapons"), TEXT("\"PPSH 41 Low-poly PBR\" and \"DP28 Light Machine Gun Low-poly PBR\" by Max3d (Fab). \"SV-98 Sniper Rifle\" and \"Classic M4\" by SpatialNeglect (Fab), licensed under Creative Commons Attribution 4.0 (CC BY 4.0): creativecommons.org/licenses/by/4.0; made into cheese for this game.")},
		{TEXT("Surfaces, props and decals"), TEXT("Quixel Megascans (Fab): the Warehouse pack, Industrial Concrete Floor, Shuttered Concrete Wall, Asphalt, Painted Corrugated Sheets, Scratched Painted Metal Sheet, Garage Door, Concrete Barrier and Oil Stain.")},
		{TEXT("The shipping container"), TEXT("\"Freight shipping container - Orange\" by Sousinho Games (Fab).")},
		{TEXT("The mice"), TEXT("\"Black Rat\" by Nestaeric (Fab), licensed under Creative Commons Attribution 4.0 (CC BY 4.0): creativecommons.org/licenses/by/4.0")},
		{TEXT("The mice's sounds"), TEXT("From OpenGameArt.org. \"Squeaky Rat\" by Iwan \"qubodup\" Gabovitch, CC0, from sounds by Zabuhailo and moneykube (freesound.org). \"Rat Monster Screams\" by Iwan \"qubodup\" Gabovitch, licensed under Creative Commons Attribution 3.0 (CC BY 3.0): creativecommons.org/licenses/by/3.0. \"Rabbit Eating\" by Voltiment555, CC0. \"7 Eating Crunches\" by StarNinjas, CC0.")},
		{TEXT("Online play"), TEXT("Steamworks, by Valve.")},
	};

	int32 NumCredits()
	{
		return UE_ARRAY_COUNT(Credits);
	}

	FString AllText()
	{
		FString All;
		for (const FCredit& Credit : Credits)
		{
			All += FString(Credit.Heading) + TEXT("\n") + Credit.Text + TEXT("\n");
		}
		return All;
	}
}
