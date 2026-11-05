// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeWeaponLooks.h"
#include "Animation/CheeseStrikeDualHold.h"

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

		// The cheese weapons are modelled in Blender by tools/art/cheese_weapons.py in the rifle's grip frame (the hand's grip at
		// the origin, forward along Y), so that each sits in the hand as it was made; their cheeses are M_CheeseWeapon's
		// (tools/art/weapon_materials.py), slot by slot.
		const auto Model = [](const TCHAR* Folder, const TCHAR* Name) -> const TCHAR*
		{
			return *Kept.Add_GetRef(FString::Printf(TEXT("/Game/CheeseStrike/Weapons/%s/SM_%s.SM_%s"), Folder, Name, Name));
		};
		// The game's own sounds, made by tools/art/wave15_sounds.py.
		const auto Sound = [](const TCHAR* Name) -> const TCHAR*
		{
			return *Kept.Add_GetRef(FString::Printf(TEXT("/Game/CheeseStrike/Sounds/%s.%s"), Name, Name));
		};

		// The Cheddar Blaster: blocks of cheddar for its receiver, handguard and stock on a steel barrel, rail and sights, the
		// receiver's block still in its wrapper (dark blue with a yellow band and CRAFT in plain white letters: nobody's trade
		// dress), the cheddar showing at the cut end; Lyra's rifle's length, and its sight line, fore-end and muzzle where the
		// rifle's were.
		{
			FWeaponLook Look;
			Look.Id = TEXT("blaster");
			Look.PlainModel = Model(TEXT("Blaster"), TEXT("CheddarBlaster"));
			Look.Materials[0] = Material(TEXT("MI_Cheddar"));
			Look.Materials[1] = Material(TEXT("MI_GunSteel"));
			Look.Materials[2] = Material(TEXT("MI_GunBlack"));
			Look.Materials[3] = Material(TEXT("MI_WrapBlue"));
			Look.Materials[4] = Material(TEXT("MI_WrapYellow"));
			Look.Materials[5] = Material(TEXT("MI_BabyStrip"));
			Look.Sight = FVector(-0.25f, -13.0f, 16.2f);
			Look.Muzzle = FVector(0.0f, 69.0f, 4.0f);
			Look.LeftHand = FVector(0.0f, 33.0f, 3.5f);
			RifleSounds(Look, 1.0f, 1.0f);
			Looks.Add(Look);
		}
		// The Colby Carbine: a steel receiver and barrel, its stock and handguard longhorns of marbled Colby; the M4's sights,
		// fore-end and muzzle where they were.
		{
			FWeaponLook Look;
			Look.Id = TEXT("carbine");
			Look.PlainModel = Model(TEXT("Carbine"), TEXT("ColbyCarbine"));
			Look.Materials[0] = Material(TEXT("MI_Colby"));
			Look.Materials[1] = Material(TEXT("MI_GunSteel"));
			Look.Materials[2] = Material(TEXT("MI_GunBlack"));
			// Aimed along the rail, the HUD's dot the sight.
			Look.Sight = FVector(0.0f, -11.2f, 13.4f);
			Look.Muzzle = FVector(0.0f, 46.4f, 4.5f);
			Look.LeftHand = FVector(0.0f, 26.9f, 4.5f);
			RifleSounds(Look, 0.95f, 1.08f);
			Looks.Add(Look);
		}
		// The Cheese Grater, a needler: a steel box grater lying along the aim, the open end the shreds come out of forward
		// (the muzzle), its small plain steel handle at the back in the hand, Parmesan inside; its shards curled slivers of
		// grated Parmesan, glowing.
		{
			FWeaponLook Look;
			Look.Id = TEXT("grater");
			Look.PlainModel = Model(TEXT("Grater"), TEXT("CheeseGrater"));
			Look.Materials[0] = Material(TEXT("MI_KnifeSteel"));
			Look.Materials[1] = Material(TEXT("MI_Parmesan"));
			Look.Materials[2] = Material(TEXT("MI_GunSteel"));
			// Along the top of the box, over its bead.
			Look.Sight = FVector(0.0f, -14.0f, 12.6f);
			Look.Muzzle = FVector(0.0f, 37.5f, 4.5f);
			Look.FlashSize = 0.5f;
			// Under the box, ahead, holding it up.
			Look.LeftHand = FVector(0.0f, 24.0f, -1.6f);
			// Its own rasping "zzzip" (tools/art/wave15_sounds.py), not a gun's; the rifle's dry click and reload.
			RifleSounds(Look, 0.9f, 1.0f);
			Look.Shots[0] = Sound(TEXT("SW_GraterShot1"));
			Look.Shots[1] = Sound(TEXT("SW_GraterShot2"));
			Look.Shots[2] = Sound(TEXT("SW_GraterShot3"));
			Look.ProjectileModel = Model(TEXT("Grater"), TEXT("GraterShard"));
			Look.ProjectileMaterials[0] = Material(TEXT("MI_Parmesan"));
			Look.ProjectileShape = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.2f));
			Look.ProjectileGlow = FLinearColor(1.0f, 0.78f, 0.2f, 1.0f);
			Looks.Add(Look);
		}
		// The Munster Twin SMGs: stubby blocks of Munster in their orange rind, a short steel barrel, a belt of cheese slices
		// hanging from each; one in each hand (FCheeseStrikeDualHold).
		{
			FWeaponLook Look;
			Look.Id = TEXT("munster");
			Look.Hold = ECheeseStrikeHold::Pistol;
			Look.PlainModel = Model(TEXT("Munster"), TEXT("MunsterSMG"));
			Look.Materials[0] = Material(TEXT("MI_MunsterCheese"));
			Look.Materials[1] = Material(TEXT("MI_MunsterRind"));
			Look.Materials[2] = Material(TEXT("MI_GunSteel"));
			Look.Materials[3] = Material(TEXT("MI_GunBlack"));
			// Aimed, the eye between the two guns (FCheeseStrikeDualHold brings them up side by side either side of it).
			Look.Sight = FVector(CheeseStrikeDual::AimHalfApart, -14.0f, 11.0f);
			Look.Muzzle = FVector(0.0f, 25.3f, 6.0f);
			Look.FlashSize = 0.7f;
			// Each hand holds its own.
			Look.LeftHand.Reset();
			RifleSounds(Look, 0.8f, 1.3f);
			Looks.Add(Look);
		}
		// The Swiss Shotgun: a wedge of Swiss on its side, tip forward, holed through, its double barrel out of the tip, its
		// rind the stock, the grip and the fore-end under the barrels.
		{
			FWeaponLook Look;
			Look.Id = TEXT("shotgun");
			Look.Hold = ECheeseStrikeHold::Shotgun;
			Look.PlainModel = Model(TEXT("Shotgun"), TEXT("SwissShotgun"));
			Look.Materials[0] = Material(TEXT("MI_Swiss"));
			Look.Materials[1] = Material(TEXT("MI_SwissRind"));
			Look.Materials[2] = Material(TEXT("MI_GunSteel"));
			Look.Sight = FVector(0.0f, -12.0f, 14.0f);
			Look.Muzzle = FVector(0.0f, 48.0f, 3.0f);
			Look.FlashSize = 1.5f;
			Look.LeftHand = FVector(0.0f, 40.0f, 0.8f);
			// Real shotgun blasts (OpenGameArt, CC0), not Lyra's, with a thump of 60-120 Hz laid under each crack and a
			// longer rumbling tail (tools/art/wave15_sounds.py).
			Look.Shots[0] = TEXT("/Game/OpenGameArt/Shotgun/SW_ShotgunBoom1.SW_ShotgunBoom1");
			Look.Shots[1] = TEXT("/Game/OpenGameArt/Shotgun/SW_ShotgunBoom2.SW_ShotgunBoom2");
			Look.Shots[2] = TEXT("/Game/OpenGameArt/Shotgun/SW_ShotgunBoom3.SW_ShotgunBoom3");
			Look.DryFire = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_DryFire_01.Weapons_Shotgun_DryFire_01");
			Look.ReloadSounds[0] = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_ClipOut_01.Weapons_Shotgun_ClipOut_01");
			Look.ReloadSounds[1] = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_ClipIn_01.Weapons_Shotgun_ClipIn_01");
			Look.ReloadSounds[2] = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_Mech_01.Weapons_Shotgun_Mech_01");
			Looks.Add(Look);
		}
		// The Roquefort Rifle: a long thumbhole stock of blue-veined cheese, a steel action and barrel, and a scope dipped in
		// dark red wax; the SV-98's sight, fore-end and muzzle where they were.
		{
			FWeaponLook Look;
			Look.Id = TEXT("roquefort");
			Look.PlainModel = Model(TEXT("Roquefort"), TEXT("RoquefortRifle"));
			Look.Materials[0] = Material(TEXT("MI_RoquefortCheese"));
			Look.Materials[1] = Material(TEXT("MI_GunSteel"));
			Look.Materials[2] = Material(TEXT("MI_ScopeWax"));
			Look.Materials[3] = Material(TEXT("MI_GunBlack"));
			Look.Sight = FVector(0.0f, -8.0f, 8.0f);
			Look.Muzzle = FVector(0.0f, 72.1f, 5.5f);
			Look.FlashSize = 1.4f;
			Look.LeftHand = FVector(0.0f, 25.0f, 1.7f);
			RifleSounds(Look, 1.0f, 1.0f);
			// Wave 16: a powerful report, real bolt-action rifles (the Tikka T3, the Arisaka and the M1917 of "The Free Firearm
			// Sound Library", CC0) with a deep thump under the crack and a long rolling tail (tools/art/wave16_sounds.py).
			Look.Shots[0] = TEXT("/Game/OpenGameArt/Rifle/SW_RoquefortShot1.SW_RoquefortShot1");
			Look.Shots[1] = TEXT("/Game/OpenGameArt/Rifle/SW_RoquefortShot2.SW_RoquefortShot2");
			Look.Shots[2] = TEXT("/Game/OpenGameArt/Rifle/SW_RoquefortShot3.SW_RoquefortShot3");
			Looks.Add(Look);
		}
		// The Curd Pistol: lumps of fresh curd melted into a pistol with a steel muzzle, sights and trigger guard; Lyra's
		// pistol's sight line and grip where they were.
		{
			FWeaponLook Look;
			Look.Id = TEXT("pistol");
			Look.Hold = ECheeseStrikeHold::Pistol;
			Look.PlainModel = Model(TEXT("Pistol"), TEXT("CurdPistol"));
			Look.Materials[0] = Material(TEXT("MI_Curd"));
			Look.Materials[1] = Material(TEXT("MI_GunSteel"));
			Look.Sight = FVector(0.0f, -32.0f, 10.6f);
			Look.Muzzle = FVector(0.0f, 20.8f, 7.6f);
			Look.FlashSize = 0.6f;
			// The left hand cupped round the right on the grip (a two-handed hold): its fingers close over the right hand's,
			// a little below the grip's top and ahead of it.
			Look.LeftHand = FVector(-4.5f, 2.5f, -2.5f);
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
		// The Dual Burratas: a pair of pistols that are each a white burrata pouch, its tied knot at the top the hammer, a
		// creamy short barrel and a grip of cheese rind, in the grip frame the M1911 had (tools/art/cheese_weapons.py); one in
		// each hand, the hand round the grip's middle, a little behind and under the bore.
		{
			FWeaponLook Look = Looks.Last();
			Look.Id = TEXT("burratas");
			Look.PlainModel = Model(TEXT("Burratas"), TEXT("Burrata"));
			Look.Model = FTransform::Identity;
			Look.Materials[0] = Material(TEXT("MI_Burratas"));
			Look.Materials[1] = Material(TEXT("MI_BurratasCream"));
			Look.Materials[2] = Material(TEXT("MI_SwissRind"));
			Look.Materials[3] = Material(TEXT("MI_GunSteel"));
			// Aimed, the eye between the two guns (FCheeseStrikeDualHold brings them up side by side either side of it).
			Look.Sight = FVector(CheeseStrikeDual::AimHalfApart, -31.6f, 10.7f);
			Look.Muzzle = FVector(0.0f, 19.0f, 7.5f);
			// Each hand holds its own: the left hand isn't on the first gun (FCheeseStrikeDualHold places it).
			Look.LeftHand.Reset();
			Look.ShotVolume = 0.8f;
			Look.ShotPitch = 1.06f;
			Looks.Add(Look);
		}
		// The Gouda Gatling: six steel barrels through three gouda wheels before a housing that is a whole gouda in yellow
		// wax, a carry handle over it and a waxed grip under the barrels for the other hand; yellow tracers of molten cheese.
		{
			FWeaponLook Look;
			Look.Id = TEXT("gatling");
			Look.PlainModel = Model(TEXT("Gatling"), TEXT("GoudaGatling"));
			Look.Materials[0] = Material(TEXT("MI_Gatling"));
			Look.Materials[1] = Material(TEXT("MI_GoudaWax"));
			Look.Materials[2] = Material(TEXT("MI_GunSteel"));
			Look.Materials[3] = Material(TEXT("MI_GunBlack"));
			Look.Sight = FVector(0.0f, -14.0f, 20.0f);
			Look.Muzzle = FVector(0.0f, 72.0f, 6.0f);
			Look.LeftHand = FVector(0.0f, 29.0f, 1.5f);
			Look.FlashSize = 1.0f;
			Look.Tracer = FLinearColor(1.0f, 0.72f, 0.08f, 1.0f);
			// The six barrels, their gouda wheels and their clamp turn about the axle while it fires (ACheeseStrikeWeapon's
			// spinner, CheeseStrikeWeapons::BarrelSpeed).
			Look.SpinningPart = Model(TEXT("Gatling"), TEXT("GoudaGatlingBarrels"));
			Look.SpinningMaterials[0] = Material(TEXT("MI_Gatling"));
			Look.SpinningMaterials[1] = Material(TEXT("MI_GunSteel"));
			Look.SpinAxis = FVector(0.0f, 1.0f, 0.0f);
			Look.SpinCentre = FVector(0.0f, 0.0f, 6.0f);
			RifleSounds(Look, 0.8f, 1.25f);
			Looks.Add(Look);
		}
		// The BabyBomb: one big red-waxed Babybel lying flat on the end of a short haft, the white pull-strip wide across its
		// top, held in both hands like a sledgehammer (the right at the haft's end, the left ahead on it), its bore out of the
		// wheel's front rim; it fires little red-waxed Babybels, spinning as they fly.
		{
			FWeaponLook Look;
			Look.Id = TEXT("babybomb");
			Look.PlainModel = Model(TEXT("BabyBomb"), TEXT("BabyBomb"));
			Look.Materials[0] = Material(TEXT("MI_BabyWax"));
			Look.Materials[1] = Material(TEXT("MI_BabyStrip"));
			Look.Materials[2] = Material(TEXT("MI_GunBlack"));
			Look.Materials[3] = Material(TEXT("MI_BabyCheese"));
			Look.Materials[4] = Material(TEXT("MI_GunSteel"));
			// Over the wheel's top, along the strip.
			Look.Sight = FVector(0.0f, -20.0f, 12.6f);
			Look.Muzzle = FVector(0.0f, 59.5f, 5.5f);
			Look.FlashSize = 1.6f;
			// Ahead on the haft.
			Look.LeftHand = FVector(0.0f, 22.0f, -1.0f);
			Look.Shots[0] = TEXT("/Game/OpenGameArt/Shotgun/SW_ShotgunShot1.SW_ShotgunShot1");
			Look.Shots[1] = TEXT("/Game/OpenGameArt/Shotgun/SW_ShotgunShot2.SW_ShotgunShot2");
			Look.DryFire = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_DryFire_01.Weapons_Shotgun_DryFire_01");
			Look.ReloadSounds[0] = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_ClipOut_01.Weapons_Shotgun_ClipOut_01");
			Look.ReloadSounds[1] = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_ClipIn_01.Weapons_Shotgun_ClipIn_01");
			Look.ReloadSounds[2] = TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_Mech_01.Weapons_Shotgun_Mech_01");
			Look.ShotVolume = 0.9f;
			Look.ShotPitch = 0.6f;
			Look.ProjectileModel = Model(TEXT("BabyBomb"), TEXT("BabyBombRound"));
			Look.ProjectileMaterials[0] = Material(TEXT("MI_BabyWax"));
			Look.ProjectileMaterials[1] = Material(TEXT("MI_BabyStrip"));
			// Its faces fore and aft (it is made along X), spinning about its flight.
			Look.ProjectileSpin = 720.0f;
			Look.ProjectileGlow = FLinearColor(1.0f, 0.35f, 0.12f, 1.0f);
			Looks.Add(Look);
		}
		// The Cheese Stick: 1.2 m of string cheese, 9 cm across, in its red wrapper at the hand, peeled strands splaying at the
		// striking end; made along -Y from its butt, gripped 15 cm from it, and turned as the knife is, but held upright in
		// both hands (its tip pitched up about 60 degrees at rest, the left hand on it above the right).
		{
			FWeaponLook Look;
			Look.Id = TEXT("stick");
			Look.Hold = ECheeseStrikeHold::Knife;
			Look.PlainModel = Model(TEXT("Stick"), TEXT("CheeseStick"));
			Look.Model = FTransform(FQuat(FRotator(0.0f, 180.0f, 0.0f)) * FQuat(FRotator(0.0f, 0.0f, 40.0f)) * FQuat(FRotator(60.0f, 0.0f, 0.0f)));
			Look.Along = FVector(0.0f, -1.0f, 0.0f);
			// Up to about 60 degrees from the knife guard's 13.
			Look.GuardPitchUp = 47.0f;
			Look.LeftHand = FVector(0.0f, -24.0f, 0.0f);
			Look.Materials[0] = Material(TEXT("MI_StringCheese"));
			Look.Materials[1] = Material(TEXT("MI_KnifeWax"));
			Look.FlashSize = 0.0f;
			// A heavy, slow whoosh for its swing (about 350 ms of body, for the slower stroke, tools/art/wave15_sounds.py), in
			// place of the knife's; and (wave 16) a deep, heavy pound as its blow lands on someone, heard by everyone from there
			// (tools/art/wave16_sounds.py).
			Look.Shots[0] = Sound(TEXT("SW_StickSwing1"));
			Look.Shots[1] = Sound(TEXT("SW_StickSwing2"));
			Look.Shots[2] = Sound(TEXT("SW_StickSwing3"));
			Look.StrikeSound = Sound(TEXT("SW_StickPound"));
			Look.ShotVolume = 1.0f;
			Look.ShotPitch = 1.0f;
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
