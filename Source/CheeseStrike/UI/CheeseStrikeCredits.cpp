// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCredits.h"

namespace CheeseStrikeCredits
{
	const FCredit Credits[] = {
		{TEXT("CheeseStrike: Gouda Overload"), TEXT("Made by Hamid Golgiri.")},
		{TEXT("Engine"), TEXT("Made with Unreal Engine 5, by Epic Games.")},
		{TEXT("The soldiers"), TEXT("Lt. Belica, Murdock, Twinblast and Wraith, from Paragon, by Epic Games (Fab).")},
		{TEXT("Animations, the rifle, the pistol and the shotgun and their sounds, and the bursts of the streaks and the grenades"), TEXT("From Lyra Starter Game, by Epic Games.")},
		{TEXT("The weapons"), TEXT("\"PPSH 41 Low-poly PBR\" and \"DP28 Light Machine Gun Low-poly PBR\" by Max3d (Fab). \"M1911 (FREE)\" by Xeradev (Fab). \"SV-98 Sniper Rifle\" and \"Classic M4\" by SpatialNeglect (Fab), licensed under Creative Commons Attribution 4.0 (CC BY 4.0): creativecommons.org/licenses/by/4.0; made into cheese for this game.")},
		{TEXT("Surfaces, props and decals"), TEXT("Quixel Megascans (Fab): the Warehouse pack, Industrial Concrete Floor, Shuttered Concrete Wall, Asphalt, Painted Corrugated Sheets, Scratched Painted Metal Sheet, Garage Door, Concrete Barrier and Oil Stain.")},
		{TEXT("The Labyrinth's stone, brick and wood"), TEXT("From Poly Haven, CC0: \"Medieval Blocks 03\", \"Medieval Red Brick\", \"Old Planks 02\" and \"Medieval Wood\" by Rob Tuytel, and \"Monastery Stone Floor\" by Amal Kumar.")},
		{TEXT("Gruyère Grotto's rock and mine"), TEXT("Quixel Megascans (Fab): the Old Mine pack and two Quarry Cliffs.")},
		{TEXT("Crumb Kitchen's things and surfaces"), TEXT("From Poly Haven (polyhaven.com), CC0.")},
		{TEXT("The shipping container"), TEXT("\"Freight shipping container - Orange\" by Sousinho Games (Fab).")},
		{TEXT("The vermin"), TEXT("\"Black Rat\" by Nestaeric (Fab), licensed under Creative Commons Attribution 4.0 (CC BY 4.0): creativecommons.org/licenses/by/4.0")},
		{TEXT("The vermin's sounds"), TEXT("From OpenGameArt.org. \"Squeaky Rat\" by Iwan \"qubodup\" Gabovitch, CC0, from sounds by Zabuhailo and moneykube (freesound.org). \"Rat Monster Screams\" by Iwan \"qubodup\" Gabovitch, licensed under Creative Commons Attribution 3.0 (CC BY 3.0): creativecommons.org/licenses/by/3.0. \"Rabbit Eating\" by Voltiment555, CC0. \"7 Eating Crunches\" by StarNinjas, CC0.")},
		{TEXT("Rind Market's surfaces and props"), TEXT("From Poly Haven (polyhaven.com), CC0: textures by Rob Tuytel, Amal Kumar, Dario Barresi and Dimitrios Savva, colormass and Rico Cilliers; lamps, a barrel, a crate, a basket and a bench by Josh Dean, James Ray Cock, Kuutti Siitonen and Kirill Sannikov.")},
		{TEXT("The menu's music"), TEXT("\"Bravery Beats\" by MintoDog, from OpenGameArt.org, CC0.")},
		{TEXT("The knife's stab"), TEXT("From \"8 Wet Squish, Slurp Impacts\" by Independent.nu, from OpenGameArt.org, CC0.")},
		{TEXT("The Swiss Shotgun's shots"), TEXT("From \"The Free Firearm Sound Library\" by Ben Jaszczak, Brian Nelson, Kevin Heras and Matthew Nanney, from OpenGameArt.org, CC0.")},
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
