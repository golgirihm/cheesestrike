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

		// The cheese weapons are modelled in Blender by tools/art/cheese_weapons.py in the rifle's grip frame (the hand's grip at
		// the origin, forward along Y), so that each sits in the hand as it was made; their cheeses are M_CheeseWeapon's
		// (tools/art/weapon_materials.py), slot by slot.
		const auto Model = [](const TCHAR* Folder, const TCHAR* Name) -> const TCHAR*
		{
			return *Kept.Add_GetRef(FString::Printf(TEXT("/Game/CheeseStrike/Weapons/%s/SM_%s.SM_%s"), Folder, Name, Name));
		};

		// The Cheddar Blaster: blocks of cheddar for its receiver, handguard and stock on a steel barrel, rail and sights; Lyra's
		// rifle's length, and its sight line, fore-end and muzzle where the rifle's were.
		{
			FWeaponLook Look;
			Look.Id = TEXT("blaster");
			Look.PlainModel = Model(TEXT("Blaster"), TEXT("CheddarBlaster"));
			Look.Materials[0] = Material(TEXT("MI_Cheddar"));
			Look.Materials[1] = Material(TEXT("MI_GunSteel"));
			Look.Materials[2] = Material(TEXT("MI_GunBlack"));
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
		// The Cheese Grater, a needler: a steel box grater, its coarse face forward, held by the D handle over its top and down
		// its back (the grip in red wax), Parmesan inside; its shards curled slivers of grated Parmesan, glowing.
		{
			FWeaponLook Look;
			Look.Id = TEXT("grater");
			Look.PlainModel = Model(TEXT("Grater"), TEXT("CheeseGrater"));
			Look.Materials[0] = Material(TEXT("MI_KnifeSteel"));
			Look.Materials[1] = Material(TEXT("MI_KnifeWax"));
			Look.Materials[2] = Material(TEXT("MI_Parmesan"));
			// The eye beside its handle rather than over it, so that the box stands up in view to the right.
			Look.Sight = FVector(6.5f, -16.0f, 10.0f);
			Look.Muzzle = FVector(0.0f, 11.9f, -2.0f);
			Look.FlashSize = 0.5f;
			// Under the box, holding it up.
			Look.LeftHand = FVector(0.0f, 8.2f, -14.2f);
			RifleSounds(Look, 0.7f, 1.5f);
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
			Look.Sight = FVector(0.0f, -14.0f, 11.0f);
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
			RifleSounds(Look, 1.25f, 0.62f);
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
			RifleSounds(Look, 0.8f, 1.25f);
			Looks.Add(Look);
		}
		// The BabyBomb: a shoulder tube of three red-waxed Babybel wheels, the white pull-strip spiralling round them, BABYBOMB
		// embossed in the wax, a wheel waiting in its bore; it fires little red-waxed Babybels, spinning as they fly.
		{
			FWeaponLook Look;
			Look.Id = TEXT("babybomb");
			Look.PlainModel = Model(TEXT("BabyBomb"), TEXT("BabyBomb"));
			Look.Materials[0] = Material(TEXT("MI_BabyWax"));
			Look.Materials[1] = Material(TEXT("MI_BabyStrip"));
			Look.Materials[2] = Material(TEXT("MI_GunBlack"));
			Look.Materials[3] = Material(TEXT("MI_BabyCheese"));
			Look.Sight = FVector(0.0f, -26.0f, 21.0f);
			Look.Muzzle = FVector(0.0f, 26.0f, 9.0f);
			Look.FlashSize = 1.6f;
			// On the fore-grip under the front wheel.
			Look.LeftHand = FVector(0.0f, 17.5f, -4.0f);
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
		// The Cheese Stick: 1.2 m of string cheese in its red wrapper at the hand, peeled strands splaying at the striking end;
		// made along -Y from its butt, gripped 15 cm from it, and turned as the knife is.
		{
			FWeaponLook Look;
			Look.Id = TEXT("stick");
			Look.Hold = ECheeseStrikeHold::Knife;
			Look.PlainModel = Model(TEXT("Stick"), TEXT("CheeseStick"));
			Look.Model = FTransform(FQuat(FRotator(0.0f, 180.0f, 0.0f)) * FQuat(FRotator(0.0f, 0.0f, 40.0f)) * FQuat(FRotator(60.0f, 0.0f, 0.0f)));
			Look.Along = FVector(0.0f, -1.0f, 0.0f);
			Look.Materials[0] = Material(TEXT("MI_StringCheese"));
			Look.Materials[1] = Material(TEXT("MI_KnifeWax"));
			Look.FlashSize = 0.0f;
			Look.ShotVolume = 0.9f;
			Look.ShotPitch = 0.7f;
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
