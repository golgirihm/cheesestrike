// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBodies.h"

namespace CheeseStrikeBodies
{
	namespace
	{
		/**
		 * How the rifle sits on each hero's hand_r: turned as the mannequin holds it in its weapon_r, the two playing
		 * the same frame of their rifle idles (tools/art/measure_bodies.py; Belica's was first measured in the game,
		 * as the first-person mannequin's rifle in her hand_r's space, and agrees), and placed as on Belica's.
		 */
		const FVector GripPlace(-1.2f, 1.3f, -0.5f);
		const FTransform BelicaGrip(FRotator(-0.6f, 87.3f, 7.7f), GripPlace);
		const FTransform MurdockGrip(FRotator(-5.9f, 94.5f, -7.4f), GripPlace);
		const FTransform WraithGrip(FRotator(-2.6f, 89.8f, -4.9f), GripPlace);
		const FTransform TwinblastGrip(FRotator(-2.0f, 87.2f, 8.3f), GripPlace);

		/** Belica's own guns are skinned to bones the weapon hides (ACheeseStrikeWeapon::HideBuiltInWeapons); the others' have slots of their own. */
		const TCHAR* const MurdockWeapons[] = {TEXT("M04_Weapons")};
		const TCHAR* const WraithWeapons[] = {TEXT("M03_weapon")};
		const TCHAR* const TwinblastWeapons[] = {TEXT("M02_Pistol")};

		/** Murdock's helmet and chest are one slot, apart from his arms. */
		const TCHAR* const MurdockFirstPersonHidden[] = {TEXT("M00_Body")};

		/** The slots each hero wears its side's colours on (tools/art/team_looks.py): its armour, metal or coat. */
		const TCHAR* const BelicaTeamSlots[] = {TEXT("M00_upperbody"), TEXT("M01_lowerbody")};
		const TCHAR* const MurdockTeamSlots[] = {TEXT("M00_Body"), TEXT("M02_Arms"), TEXT("M03_Legs")};
		const TCHAR* const WraithTeamSlots[] = {TEXT("M00_lowerbody"), TEXT("M01_upperbody"), TEXT("M02_backpackhead")};
		const TCHAR* const TwinblastTeamSlots[] = {TEXT("M00_Head"), TEXT("M01_cloth"), TEXT("M03_Arms"), TEXT("M04_LowerBody")};

		/**
		 * The heads' shapes (FBody::HeadLow, HeadHigh, HeadRadius) are fitted to their vertices in the head bone's space (the
		 * smallest capsule, leaning forward or back as the head does, within a centimetre and a half of all but a few in a
		 * hundred: CheeseStrike.Animation.TheHeadShapeFitsEachHero), then reaching on along its lean down to the head bone,
		 * so that its round end is the neck.
		 */
		const FBody Bodies[] = {
			{TEXT("Lt. Belica"), TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Meshes/Belica.Belica"),
				TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/Rifle/"), nullptr, 0, BelicaGrip, nullptr, 0,
				TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Materials/Teams/"), BelicaTeamSlots, UE_ARRAY_COUNT(BelicaTeamSlots),
				FVector(0.0f, 2.3f, -0.6f), FVector(7.0f, -0.3f, -0.6f), 11.5f},
			{TEXT("Murdock"), TEXT("/Game/ParagonMurdock/Characters/Heroes/Murdock/Meshes/Murdock.Murdock"),
				TEXT("/Game/ParagonMurdock/Characters/Heroes/Murdock/Animations/Rifle/"), MurdockWeapons, UE_ARRAY_COUNT(MurdockWeapons), MurdockGrip, MurdockFirstPersonHidden, UE_ARRAY_COUNT(MurdockFirstPersonHidden),
				TEXT("/Game/ParagonMurdock/Characters/Heroes/Murdock/Materials/Teams/"), MurdockTeamSlots, UE_ARRAY_COUNT(MurdockTeamSlots),
				FVector(0.0f, 4.9f, -0.7f), FVector(5.3f, 1.2f, -0.7f), 10.0f},
			{TEXT("Wraith"), TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith.Wraith"),
				TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/Rifle/"), WraithWeapons, UE_ARRAY_COUNT(WraithWeapons), WraithGrip, nullptr, 0,
				TEXT("/Game/ParagonWraith/Characters/Heroes/Wraith/Materials/Teams/"), WraithTeamSlots, UE_ARRAY_COUNT(WraithTeamSlots),
				FVector(0.0f, 4.8f, 0.0f), FVector(5.5f, 0.3f, 0.0f), 9.0f},
			{TEXT("Twinblast"), TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Meshes/TwinBlast.TwinBlast"),
				TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Rifle/"), TwinblastWeapons, UE_ARRAY_COUNT(TwinblastWeapons), TwinblastGrip, nullptr, 0,
				TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Materials/Teams/"), TwinblastTeamSlots, UE_ARRAY_COUNT(TwinblastTeamSlots),
				FVector(0.0f, 2.6f, 0.1f), FVector(8.5f, 0.3f, 0.1f), 9.0f},
		};

		/** The Gouda Guys are Murdock and Twinblast, the Mozza Radicals Lt. Belica and Wraith. */
		const TCHAR* const TeamBodies[CheeseStrikeRules::Teams][HeroesPerTeam] = {{TEXT("Murdock"), TEXT("Twinblast")}, {TEXT("Lt. Belica"), TEXT("Wraith")}};
		const TCHAR* const TeamWords[CheeseStrikeRules::Teams] = {TEXT("Gouda"), TEXT("Mozza")};
		/** Each hero's word in the settings, in Bodies' order. */
		const TCHAR* const HeroKeys[] = {TEXT("Belica"), TEXT("Murdock"), TEXT("Wraith"), TEXT("Twinblast")};
	}

	TConstArrayView<FBody> All()
	{
		return TConstArrayView<FBody>(Bodies, UE_ARRAY_COUNT(Bodies));
	}

	int32 IndexOf(const TCHAR* Name)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Bodies); ++Index)
		{
			if (FCString::Strcmp(Bodies[Index].Name, Name) == 0)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 IndexOfMesh(const FString& Path)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Bodies); ++Index)
		{
			if (Path == Bodies[Index].Mesh)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 ForTeam(int32 Team, int32 Rank)
	{
		return IndexOf(TeamBodies[FMath::Clamp(Team, 0, CheeseStrikeRules::Teams - 1)][FMath::Max(Rank, 0) % HeroesPerTeam]);
	}

	int32 For(bool bTeams, int32 Team, int32 Rank)
	{
		if (bTeams)
		{
			return ForTeam(Team, Rank);
		}
		return FMath::Max(Rank, 0) % UE_ARRAY_COUNT(Bodies);
	}

	int32 BodyFor(int32 Pick, bool bTeams, int32 Team, int32 Rank)
	{
		return All().IsValidIndex(Pick) ? Pick : For(bTeams, Team, Rank);
	}

	const TCHAR* HeroKey(int32 Body)
	{
		static_assert(UE_ARRAY_COUNT(HeroKeys) == UE_ARRAY_COUNT(Bodies), "a key for every hero");
		return All().IsValidIndex(Body) ? HeroKeys[Body] : TEXT("auto");
	}

	int32 HeroFromKey(const FString& Key)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Bodies); ++Index)
		{
			if (Key.Equals(HeroKeys[Index], ESearchCase::IgnoreCase) || Key.Equals(Bodies[Index].Name, ESearchCase::IgnoreCase))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 TeamOf(int32 Body)
	{
		for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
		{
			for (int32 Rank = 0; Rank < HeroesPerTeam; ++Rank)
			{
				if (IndexOf(TeamBodies[Team][Rank]) == Body)
				{
					return Team;
				}
			}
		}
		return INDEX_NONE;
	}

	const TCHAR* TeamWord(int32 Team)
	{
		return TeamWords[FMath::Clamp(Team, 0, CheeseStrikeRules::Teams - 1)];
	}

	FString TeamLook(int32 Body, int32 Team, const FString& Slot)
	{
		if (!All().IsValidIndex(Body))
		{
			return FString();
		}
		const FString Name = FString::Printf(TEXT("MI_%s_%s"), *Slot, TeamWord(Team));
		return FString(Bodies[Body].TeamLooks) + Name + TEXT(".") + Name;
	}
}
