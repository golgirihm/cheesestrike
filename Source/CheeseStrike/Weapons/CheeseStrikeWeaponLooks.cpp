// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeWeaponLooks.h"

namespace
{
	const TCHAR* const LookMaterials = TEXT("/Game/CheeseStrike/Weapons/Materials/");
	const TCHAR* const RifleShots[] = {
		TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Punch-Close_01.Weapons_Rifle_Punch-Close_01"),
		TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Punch-Close_02.Weapons_Rifle_Punch-Close_02"),
		TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Punch-Close_03.Weapons_Rifle_Punch-Close_03"),
	};
	const TCHAR* const RifleDry = TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_DryFire_01.Weapons_Rifle_DryFire_01");
	const TCHAR* const RifleReload[] = {
		TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_ClipOut_01.Weapons_Rifle_ClipOut_01"),
		TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_ClipIn_01.Weapons_Rifle_ClipIn_01"),
		TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Bolt_01.Weapons_Rifle_Bolt_01"),
	};

	/** A model made along its Y axis, sized by `Scale`, whose grip is at `Grip`: put with the grip at the origin. */
	FTransform Gripped(const FVector& Grip, float Scale, const FRotator& Turn = FRotator::ZeroRotator)
	{
		return FTransform(Turn, -Turn.RotateVector(Grip) * Scale, FVector(Scale));
	}

	void RifleSounds(FWeaponLook& Look, float Volume, float Pitch)
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Look.Shots[Index] = RifleShots[Index];
			Look.ReloadSounds[Index] = RifleReload[Index];
		}
		Look.DryFire = RifleDry;
		Look.ShotVolume = Volume;
		Look.ShotPitch = Pitch;
	}

	TArray<FWeaponLook> MakeLooks()
	{
		TArray<FWeaponLook> Looks;
		const FString Materials(LookMaterials);
		// Paths kept for the life of the game: the table is made once.
		static TArray<FString> Kept;
		Kept.Reserve(64);
		const auto Material = [&Materials](const TCHAR* Name) -> const TCHAR*
		{
			return *Kept.Add_GetRef(Materials + Name + TEXT(".") + Name);
		};

		// The Cheddar Blaster: Lyra's rifle, its panels cheddar.
		{
			FWeaponLook Look;
			Look.Id = TEXT("blaster");
			Look.SkinnedModel = TEXT("/Game/Weapons/Rifle/Mesh/SK_Rifle.SK_Rifle");
			Look.FireAnimation = TEXT("/Game/Weapons/Rifle/Animations/Weap_Rifle_Fire.Weap_Rifle_Fire");
			Look.ReloadAnimation = TEXT("/Game/Weapons/Rifle/Animations/Weap_Rifle_Reload.Weap_Rifle_Reload");
			Look.Materials[0] = Material(TEXT("MI_Blaster"));
			Look.Muzzle = FVector(0.0f, 69.0f, 4.0f);
			Look.LeftHand = FVector(0.0f, 33.0f, 3.5f);
			RifleSounds(Look, 1.0f, 1.0f);
			Looks.Add(Look);
		}
		// The Colby Carbine: SpatialNeglect's M4, its furniture Colby.
		{
			FWeaponLook Look;
			Look.Id = TEXT("carbine");
			Look.PlainModel = TEXT("/Game/Fab/SpatialNeglect/ClassicM4/SM_ClassicM4.SM_ClassicM4");
			Look.Model = Gripped(FVector(0.0f, -7.0f, -4.0f), 1.12f);
			Look.Materials[0] = Material(TEXT("MI_Carbine"));
			Look.Materials[1] = Material(TEXT("MI_CarbineSling"));
			Look.Materials[2] = Material(TEXT("MI_CarbineScope"));
			// Its scope is taken off (MI_CarbineScope): aimed along the rail, the HUD's dot the sight.
			Look.Sight = FVector(0.0f, -17.0f, 8.0f);
			Look.Muzzle = FVector(0.0f, 34.4f, 0.0f);
			Look.LeftHand = FVector(0.0f, 17.0f, 0.0f);
			RifleSounds(Look, 0.95f, 1.08f);
			Looks.Add(Look);
		}
		// The Parmesan Grater: Max3d's PPSh-41, its perforated jacket a grater, its stock Parmesan.
		{
			FWeaponLook Look;
			Look.Id = TEXT("grater");
			Look.PlainModel = TEXT("/Game/Fab/Max3d/PPSh41/SM_PPSh41.SM_PPSh41");
			Look.Model = Gripped(FVector(-1.5f, -12.0f, 9.0f), 1.15f);
			Look.Materials[0] = Material(TEXT("MI_Grater"));
			Look.Sight = FVector(-1.5f, -23.0f, 18.6f);
			Look.Muzzle = FVector(-1.5f, 32.3f, 15.0f);
			Look.FlashSize = 0.8f;
			Look.LeftHand = FVector(-1.5f, 14.0f, 13.0f);
			RifleSounds(Look, 0.85f, 1.22f);
			Looks.Add(Look);
		}
		// The Munster Machine Gun: Max3d's DP-28, its pan magazine a wheel of Munster.
		{
			FWeaponLook Look;
			Look.Id = TEXT("munster");
			Look.PlainModel = TEXT("/Game/Fab/Max3d/DP28/SM_DP28.SM_DP28");
			Look.Model = Gripped(FVector(8.7f, -9.5f, 16.0f), 1.25f);
			Look.Materials[0] = Material(TEXT("MI_Munster"));
			Look.Sight = FVector(8.7f, -14.0f, 23.6f);
			Look.Muzzle = FVector(8.7f, 48.0f, 20.3f);
			Look.FlashSize = 1.2f;
			Look.LeftHand = FVector(8.7f, 13.0f, 18.5f);
			RifleSounds(Look, 1.1f, 0.85f);
			Looks.Add(Look);
		}
		// The Swiss Shotgun: Lyra's shotgun, its shroud holed like Emmental.
		{
			FWeaponLook Look;
			Look.Id = TEXT("shotgun");
			Look.Hold = ECheeseStrikeHold::Shotgun;
			Look.SkinnedModel = TEXT("/Game/Weapons/Shotgun/Mesh/SKM_Shotgun.SKM_Shotgun");
			Look.FireAnimation = TEXT("/Game/Weapons/Shotgun/Animations/Weap_Shotgun_Fire.Weap_Shotgun_Fire");
			Look.ReloadAnimation = TEXT("/Game/Weapons/Shotgun/Animations/Weap_Shotgun_Reload.Weap_Shotgun_Reload");
			Look.Materials[0] = Material(TEXT("MI_Shotgun"));
			Look.Sight = FVector(0.0f, -12.0f, 15.5f);
			Look.Muzzle = FVector(0.0f, 43.0f, 3.0f);
			Look.FlashSize = 1.5f;
			Look.LeftHand = FVector(0.0f, 32.0f, 3.0f);
			// Real shotgun blasts (OpenGameArt, CC0; tools/art/wave8_sounds.py), not Lyra's.
			Look.Shots[0] = TEXT("/Game/OpenGameArt/Shotgun/SW_ShotgunShot1.SW_ShotgunShot1");
			Look.Shots[1] = TEXT("/Game/OpenGameArt/Shotgun/SW_ShotgunShot2.SW_ShotgunShot2");
			Look.Shots[2] = TEXT("/Game/OpenGameArt/Shotgun/SW_ShotgunShot3.SW_ShotgunShot3");
			Look.DryFire = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_DryFire_01.Weapons_Shotgun_DryFire_01");
			Look.ReloadSounds[0] = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_ClipOut_01.Weapons_Shotgun_ClipOut_01");
			Look.ReloadSounds[1] = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_ClipIn_01.Weapons_Shotgun_ClipIn_01");
			Look.ReloadSounds[2] = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_Mech_01.Weapons_Shotgun_Mech_01");
			Looks.Add(Look);
		}
		// The Roquefort Rifle: SpatialNeglect's SV-98, made in millimetres, its stock blue-veined.
		{
			FWeaponLook Look;
			Look.Id = TEXT("roquefort");
			Look.PlainModel = TEXT("/Game/Fab/SpatialNeglect/SV98/SM_SV98.SM_SV98");
			Look.Model = Gripped(FVector(295.0f, -50.0f, -5.0f), 0.1f);
			Look.Materials[0] = Material(TEXT("MI_Roquefort"));
			Look.Materials[1] = Material(TEXT("MI_RoquefortScope"));
			Look.Sight = FVector(295.0f, -130.0f, 75.0f);
			Look.Muzzle = FVector(295.0f, 671.0f, 50.0f);
			Look.FlashSize = 1.4f;
			Look.LeftHand = FVector(295.0f, 200.0f, 12.0f);
			RifleSounds(Look, 1.25f, 0.62f);
			Looks.Add(Look);
		}
		// The Curd Pistol: Lyra's pistol, its panels fresh white curd.
		{
			FWeaponLook Look;
			Look.Id = TEXT("pistol");
			Look.Hold = ECheeseStrikeHold::Pistol;
			Look.SkinnedModel = TEXT("/Game/Weapons/Pistol/Mesh/SK_Pistol.SK_Pistol");
			Look.FireAnimation = TEXT("/Game/Weapons/Pistol/Animations/Weap_Pistol_Fire.Weap_Pistol_Fire");
			Look.ReloadAnimation = TEXT("/Game/Weapons/Pistol/Animations/Weap_Pistol_Reload.Weap_Pistol_Reload");
			Look.Materials[0] = Material(TEXT("MI_Pistol"));
			Look.Sight = FVector(0.0f, -32.0f, 10.6f);
			Look.Muzzle = FVector(0.0f, 19.0f, 7.5f);
			Look.FlashSize = 0.6f;
			Look.LeftHand = FVector(-3.0f, 9.5f, 1.5f);
			Look.Shots[0] = TEXT("/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_Punch-Close_01.Weapons_Pistol_Punch-Close_01");
			Look.Shots[1] = TEXT("/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_Punch-Close_02.Weapons_Pistol_Punch-Close_02");
			Look.Shots[2] = TEXT("/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_Punch-Close_03.Weapons_Pistol_Punch-Close_03");
			Look.Shots[3] = TEXT("/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_Punch-Close_04.Weapons_Pistol_Punch-Close_04");
			Look.DryFire = TEXT("/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_DryFire_01.Weapons_Pistol_DryFire_01");
			Look.ReloadSounds[0] = TEXT("/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_ClipOut_01.Weapons_Pistol_ClipOut_01");
			Look.ReloadSounds[1] = TEXT("/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_ClipIn_01.Weapons_Pistol_ClipIn_01");
			Look.ReloadSounds[2] = TEXT("/Game/Audio/Sounds/Weapons/Pistol/Weapons_Pistol_Slide_01.Weapons_Pistol_Slide_01");
			Look.ShotVolume = 0.85f;
			Looks.Add(Look);
		}
		// The Dual Burratas: a pair of Xeradev's M1911s, one in each hand, soft white burrata with creamy grips, at life size
		// (turned along Y as it came in); the hand at the grip, a little behind and under the bore.
		{
			FWeaponLook Look = Looks.Last();
			Look.Id = TEXT("burratas");
			Look.SkinnedModel = nullptr;
			Look.FireAnimation = nullptr;
			Look.ReloadAnimation = nullptr;
			Look.PlainModel = TEXT("/Game/Fab/Xeradev/M1911/SM_M1911.SM_M1911");
			Look.Model = Gripped(FVector(0.0f, -3.4f, -3.5f), 1.0f);
			Look.Materials[0] = Material(TEXT("MI_BurratasCream"));
			Look.Materials[1] = Material(TEXT("MI_Burratas"));
			Look.Materials[2] = Material(TEXT("MI_Burratas"));
			Look.Materials[3] = Material(TEXT("MI_BurratasSteel"));
			Look.Sight = FVector(0.0f, -35.0f, 7.2f);
			Look.Muzzle = FVector(0.0f, 15.6f, 4.0f);
			// Each hand holds its own: the left hand isn't on the first gun (FCheeseStrikeDualHold places it).
			Look.LeftHand.Reset();
			Look.ShotVolume = 0.8f;
			Look.ShotPitch = 1.06f;
			Looks.Add(Look);
		}
		// The Gouda Gatling: for now the Munster's DP-28 made all gouda (its pan in red wax), firing yellow tracers of molten
		// cheese, faster and higher-pitched; group 9's rotary barrel of gouda wheels will take its place.
		{
			FWeaponLook Look = *Looks.FindByPredicate([](const FWeaponLook& Each) { return FCString::Strcmp(Each.Id, TEXT("munster")) == 0; });
			Look.Id = TEXT("gatling");
			Look.Materials[0] = Material(TEXT("MI_Gatling"));
			Look.FlashSize = 1.0f;
			Look.Tracer = FLinearColor(1.0f, 0.72f, 0.08f, 1.0f);
			Look.IconOf = TEXT("munster");
			RifleSounds(Look, 0.8f, 1.25f);
			Looks.Add(Look);
		}
		// The Cheese Knife: our own (tools/art/weapon_models.py), its handle in red wax. It came in pointing back.
		{
			FWeaponLook Look;
			Look.Id = TEXT("knife");
			Look.Hold = ECheeseStrikeHold::Knife;
			Look.PlainModel = TEXT("/Game/CheeseStrike/Weapons/Knife/SM_CheeseKnife.SM_CheeseKnife");
			// Turned about its length so that its flat shows, tipped up out of the fist, and turned to point forward.
			Look.Model = FTransform(FQuat(FRotator(0.0f, 180.0f, 0.0f)) * FQuat(FRotator(0.0f, 0.0f, 40.0f)) * FQuat(FRotator(60.0f, 0.0f, 0.0f)));
			// It came in pointing back: its tip is along -Y.
			Look.Along = FVector(0.0f, -1.0f, 0.0f);
			Look.Materials[0] = Material(TEXT("MI_KnifeSteel"));
			Look.Materials[1] = Material(TEXT("MI_KnifeWax"));
			Look.FlashSize = 0.0f;
			Look.ShotVolume = 0.7f;
			Looks.Add(Look);
		}
		return Looks;
	}
}

namespace CheeseStrikeWeaponLooks
{
	FTransform HipFromAimed(ECheeseStrikeHold Hold)
	{
		// Low on the right, the muzzle turned in a little toward the middle of the view and the top canted in.
		switch (Hold)
		{
		case ECheeseStrikeHold::Pistol:
			return FTransform(FRotator(2.0f, -2.0f, -8.0f), FVector(2.0f, 8.0f, -9.0f));
		default:
			return FTransform(FRotator(0.0f, -2.0f, -10.0f), FVector(10.0f, 9.0f, -7.0f));
		}
	}

	FVector KnifeHand()
	{
		return FVector(30.0f, 16.0f, -17.0f);
	}

	const TCHAR* const KnifeSwings[4] = {
		TEXT("/Game/Audio/Sounds/SFX/Lyra_MeleeWhoosh_01.Lyra_MeleeWhoosh_01"),
		TEXT("/Game/Audio/Sounds/SFX/Lyra_MeleeWhoosh_02.Lyra_MeleeWhoosh_02"),
		TEXT("/Game/Audio/Sounds/SFX/Lyra_MeleeWhoosh_03.Lyra_MeleeWhoosh_03"),
		TEXT("/Game/Audio/Sounds/SFX/Lyra_MeleeWhoosh_04.Lyra_MeleeWhoosh_04"),
	};
	const TCHAR* const KnifeStab = TEXT("/Game/OpenGameArt/Stab/SW_KnifeStabIn.SW_KnifeStabIn");
	const TCHAR* const DrawSounds[3] = {
		TEXT("/Game/Audio/Sounds/Foley_Misc/Lyra_WeaponSwap_01.Lyra_WeaponSwap_01"),
		TEXT("/Game/Audio/Sounds/Foley_Misc/Lyra_WeaponSwap_02.Lyra_WeaponSwap_02"),
		TEXT("/Game/Audio/Sounds/Foley_Misc/Lyra_WeaponSwap_03.Lyra_WeaponSwap_03"),
	};
	const TCHAR* const FlashModel = TEXT("/Game/CheeseStrike/Weapons/Flash/SM_MuzzleFlash.SM_MuzzleFlash");
	const TCHAR* const FlashMaterial = TEXT("/Game/CheeseStrike/Weapons/Materials/MI_MuzzleFlash.MI_MuzzleFlash");
	const TCHAR* const FlashFrontMaterial = TEXT("/Game/CheeseStrike/Weapons/Materials/MI_MuzzleFlashFront.MI_MuzzleFlashFront");
	const TCHAR* const TracerMaterial = TEXT("/Game/CheeseStrike/Weapons/Materials/M_Tracer.M_Tracer");

	const FWeaponLook& For(FName Id)
	{
		static const TArray<FWeaponLook> Looks = MakeLooks();
		for (const FWeaponLook& Look : Looks)
		{
			if (Id == FName(Look.Id))
			{
				return Look;
			}
		}
		return Looks[0];
	}
}
