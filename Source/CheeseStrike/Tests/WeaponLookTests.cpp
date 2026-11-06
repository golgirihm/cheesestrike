// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The weapons' looks (CheeseStrikeWeaponLooks): every weapon has its own
// model, cheese materials, sounds and HUD picture, which all load; its
// muzzle and sights are on its model; the hand that holds it has a place in
// view for each way of holding; and wave 17's grips and draws.

#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Animation/CheeseStrikeArmsPose.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeShot.h"
#include "Weapons/CheeseStrikeWeaponLooks.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace WeaponLookTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	template <typename T>
	T* Load(const TCHAR* Path)
	{
		return Path != nullptr ? LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryWeaponHasItsOwnLook, "CheeseStrike.Weapons.EveryWeaponHasItsOwnLook", WeaponLookTests::Flags)

bool FEveryWeaponHasItsOwnLook::RunTest(const FString& Parameters)
{
	using namespace WeaponLookTests;
	TSet<FString> Models;
	for (const FWeaponStats* Weapon : CheeseStrikeArsenal::All())
	{
		const FString Name = Weapon->Id.ToString();
		const FWeaponLook& Look = CheeseStrikeWeaponLooks::For(Weapon->Id);
		if (!TestEqual(FString::Printf(TEXT("%s has a look"), *Name), FName(Look.Id), Weapon->Id))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s is held as a knife if, and only if, it is one"), *Name), Look.Hold == ECheeseStrikeHold::Knife, Weapon->IsMelee());

		// One model, skinned or plain, of its own, which loads.
		TestTrue(FString::Printf(TEXT("%s has one model"), *Name), (Look.SkinnedModel != nullptr) != (Look.PlainModel != nullptr));
		const FString ModelPath = Look.SkinnedModel != nullptr ? Look.SkinnedModel : Look.PlainModel;
		// A streak's weapon may borrow another's model until it has one (the Gouda Gatling the Munster's).
		if (!Weapon->bSpecial)
		{
			TestFalse(FString::Printf(TEXT("%s's model is its own"), *Name), Models.Contains(ModelPath));
			Models.Add(ModelPath);
		}
		FBox Bounds(ForceInit);
		if (Look.SkinnedModel != nullptr)
		{
			const USkeletalMesh* Mesh = Load<USkeletalMesh>(Look.SkinnedModel);
			TestNotNull(FString::Printf(TEXT("%s's model loads"), *Name), Mesh);
			Bounds = Mesh != nullptr ? Mesh->GetBounds().GetBox() : Bounds;
		}
		else
		{
			const UStaticMesh* Mesh = Load<UStaticMesh>(Look.PlainModel);
			TestNotNull(FString::Printf(TEXT("%s's model loads"), *Name), Mesh);
			Bounds = Mesh != nullptr ? Mesh->GetBounds().GetBox() : Bounds;
		}
		if (Look.FireAnimation != nullptr)
		{
			TestNotNull(FString::Printf(TEXT("%s's firing animation loads"), *Name), Load<UObject>(Look.FireAnimation));
		}

		// Its cheese, in its first slot at least.
		TestNotNull(FString::Printf(TEXT("%s has a material"), *Name), Look.Materials[0]);
		for (const TCHAR* Material : Look.Materials)
		{
			if (Material != nullptr)
			{
				TestNotNull(FString::Printf(TEXT("%s's material %s loads"), *Name, Material), Load<UMaterialInterface>(Material));
			}
		}

		// Its muzzle at the front of the model, its sights behind it; and its sounds.
		if (!Weapon->IsMelee())
		{
			if (Bounds.IsValid)
			{
				const FBox Near = Bounds.ExpandBy(Bounds.GetSize().GetMax() * 0.05f);
				TestTrue(FString::Printf(TEXT("%s's muzzle is on its model"), *Name), Near.IsInside(Look.Muzzle));
				// Forward is Y once the model is turned as its look turns it (one made along another axis).
				const FQuat Turn = Look.Model.GetRotation();
				const FBox Along = Bounds.TransformBy(FTransform(Turn));
				TestTrue(FString::Printf(TEXT("%s's muzzle is at its front"), *Name), Turn.RotateVector(Look.Muzzle).Y > Along.Max.Y - Along.GetSize().Y * 0.15f);
			}
			TestTrue(FString::Printf(TEXT("%s's sights are behind its muzzle"), *Name), !Look.Sight.IsSet() || Look.Model.GetRotation().RotateVector(*Look.Sight).Y < Look.Model.GetRotation().RotateVector(Look.Muzzle).Y);
			TestTrue(FString::Printf(TEXT("%s flashes"), *Name), Look.FlashSize > 0.0f);
			TestNotNull(FString::Printf(TEXT("%s sounds as it fires"), *Name), Look.Shots[0]);
			for (const TCHAR* Shot : Look.Shots)
			{
				if (Shot != nullptr)
				{
					TestNotNull(FString::Printf(TEXT("%s's shot sounds load"), *Name), Load<USoundBase>(Shot));
				}
			}
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s doesn't flash"), *Name), Look.FlashSize, 0.0f);
			// Its own swings and blow, if it has them (the Cheese Stick's whoosh and thump), else the knife's.
			for (const TCHAR* Swing : Look.Shots)
			{
				if (Swing != nullptr)
				{
					TestNotNull(FString::Printf(TEXT("%s's swing loads"), *Name), Load<USoundBase>(Swing));
				}
			}
			if (Look.StrikeSound != nullptr)
			{
				TestNotNull(FString::Printf(TEXT("%s's blow loads"), *Name), Load<USoundBase>(Look.StrikeSound));
			}
		}
		TestTrue(FString::Printf(TEXT("%s's shots are about as loud as the rest"), *Name), Look.ShotVolume > 0.5f && Look.ShotVolume <= 1.3f);

		// Its picture in the HUD's slot list.
		const FString Pictured = Look.IconOf != nullptr ? FString(Look.IconOf) : Name;
		const FString Icon = FString::Printf(TEXT("/Game/CheeseStrike/Weapons/Icons/T_Icon_%s.T_Icon_%s"), *Pictured, *Pictured);
		TestNotNull(FString::Printf(TEXT("%s has a picture"), *Name), Load<UTexture2D>(*Icon));
	}

	// The knife's swings and blows, the draws, and the flash.
	for (const TCHAR* Sound : CheeseStrikeWeaponLooks::KnifeSwings)
	{
		TestNotNull(TEXT("the knife swings"), Load<USoundBase>(Sound));
	}
	TestNotNull(TEXT("and its stab lands"), Load<USoundBase>(CheeseStrikeWeaponLooks::KnifeStab));
	for (const TCHAR* Sound : CheeseStrikeWeaponLooks::DrawSounds)
	{
		TestNotNull(TEXT("a weapon is drawn"), Load<USoundBase>(Sound));
	}
	// A part that turns while it fires (the Gatling's barrels) loads, with its materials.
	for (const FWeaponStats* Weapon : CheeseStrikeArsenal::All())
	{
		const FWeaponLook& Look = CheeseStrikeWeaponLooks::For(Weapon->Id);
		if (Look.SpinningPart != nullptr)
		{
			TestNotNull(FString::Printf(TEXT("%s's spinning part loads"), Look.Id), Load<UStaticMesh>(Look.SpinningPart));
			TestNotNull(FString::Printf(TEXT("%s's spinning part has a material"), Look.Id), Look.SpinningMaterials[0]);
			for (const TCHAR* Material : Look.SpinningMaterials)
			{
				if (Material != nullptr)
				{
					TestNotNull(FString::Printf(TEXT("%s's spinning part's material %s loads"), Look.Id, Material), Load<UMaterialInterface>(Material));
				}
			}
		}
	}
	TestNotNull(TEXT("the flash"), Load<UStaticMesh>(CheeseStrikeWeaponLooks::FlashModel));
	TestNotNull(TEXT("its side"), Load<UMaterialInterface>(CheeseStrikeWeaponLooks::FlashMaterial));
	TestNotNull(TEXT("its front"), Load<UMaterialInterface>(CheeseStrikeWeaponLooks::FlashFrontMaterial));
	TestEqual(TEXT("an unknown weapon looks like the first"), FName(CheeseStrikeWeaponLooks::For(TEXT("bazooka")).Id), CheeseStrikeArsenal::All()[0]->Id);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryHoldHasAPlaceInView, "CheeseStrike.Weapons.EveryHoldHasAPlaceInView", WeaponLookTests::Flags)

bool FEveryHoldHasAPlaceInView::RunTest(const FString& Parameters)
{
	// At the hip every gun sits a little below where it is aimed and to its right, turned only a little, the same for
	// every gun of a hold; the knife is held on guard in front, below the eye and to the right, within an arm's reach.
	for (int32 Each = 0; Each < NumHolds; ++Each)
	{
		const ECheeseStrikeHold Hold = static_cast<ECheeseStrikeHold>(Each);
		if (Hold == ECheeseStrikeHold::Knife)
		{
			continue;
		}
		const FTransform Hip = CheeseStrikeWeaponLooks::HipFromAimed(Hold);
		const FVector Moved = Hip.GetLocation();
		TestTrue(FString::Printf(TEXT("hold %d: lower and to the right than aimed (%.0f, %.0f, %.0f)"), Each, Moved.X, Moved.Y, Moved.Z), Moved.Z < -3.0f && Moved.Y > 3.0f && Moved.Size() < 25.0f);
		TestTrue(FString::Printf(TEXT("hold %d: turned only a little (%.0f degrees)"), Each, FMath::RadiansToDegrees(Hip.GetRotation().GetAngle())), FMath::RadiansToDegrees(Hip.GetRotation().GetAngle()) < 15.0f);
	}
	const FVector Knife = CheeseStrikeWeaponLooks::KnifeHand();
	TestTrue(FString::Printf(TEXT("the knife is on guard in front, low on the right (%.0f, %.0f, %.0f)"), Knife.X, Knife.Y, Knife.Z), Knife.X > 10.0f && Knife.Y > 0.0f && Knife.Z < -5.0f && Knife.Size() < 60.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWave17SetsTheGripsAndTheDraws, "CheeseStrike.Weapons.Wave17SetsTheGripsAndTheDraws", WeaponLookTests::Flags)

bool FWave17SetsTheGripsAndTheDraws::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeArms;
	const auto LookOf = [](const TCHAR* Id) -> const FWeaponLook& { return CheeseStrikeWeaponLooks::For(Id); };
	// Only the Curd Pistol and the two guns are drawn by the equip in their own player's view; the rest come up from below.
	for (const FWeaponStats* Weapon : CheeseStrikeArsenal::All())
	{
		const FWeaponLook& Look = CheeseStrikeWeaponLooks::For(Weapon->Id);
		const bool bKept = Weapon->Id == FName(TEXT("pistol")) || Weapon->bDual;
		TestEqual(FString::Printf(TEXT("%s: drawn by the equip only if the pistol or two guns"), *Weapon->Id.ToString()), Look.bDrawAnimation, bKept);
		TestEqual(FString::Printf(TEXT("%s: the fist round a handle only for the BabyBomb"), *Weapon->Id.ToString()), Look.bFistGrip, Weapon->Id == FName(TEXT("babybomb")));
	}
	// The raise: from all the way down to all the way up in RaiseSeconds (or a shorter draw time), easing in and out.
	const float Rise = RaiseSeconds;
	TestTrue(FString::Printf(TEXT("a short, simple raise (%.2f s)"), Rise), Rise >= 0.3f && Rise <= 0.4f);
	TestEqual(TEXT("drawn: all the way down"), RaiseLowered(0.0f, 0.6f), 1.0f, 1.0e-4f);
	TestEqual(TEXT("halfway up halfway through"), RaiseLowered(Rise * 0.5f, 0.6f), 0.5f, 1.0e-3f);
	TestEqual(TEXT("up once the raise is over, before the draw time"), RaiseLowered(Rise, 0.6f), 0.0f, 1.0e-4f);
	TestEqual(TEXT("a quicker draw (the knife's 0.2 s) is up by its draw time"), RaiseLowered(0.2f, 0.2f), 0.0f, 1.0e-4f);
	TestTrue(TEXT("it starts gently"), RaiseLowered(Rise * 0.05f, 0.6f) > 0.99f);
	TestTrue(TEXT("and ends gently"), RaiseLowered(Rise * 0.95f, 0.6f) < 0.01f);
	float Was = 1.0f;
	float MostStep = 0.0f;
	for (int32 Frame = 1; Frame <= 60; ++Frame)
	{
		const float Now = RaiseLowered(Rise * Frame / 60.0f, 0.6f);
		TestTrue(TEXT("it only comes up"), Now <= Was + 1.0e-5f);
		MostStep = FMath::Max(MostStep, Was - Now);
		Was = Now;
	}
	TestTrue(FString::Printf(TEXT("smoothly: never more than 3%% of the way in a sixtieth of the raise (%.3f)"), MostStep), MostStep < 0.03f);
	// The grips: the Curd Pistol and the Dual Burratas forward in the hand; the knife turned to show its flat in view; the
	// Cheddar Blaster and the Swiss Shotgun brought back and up at the hip so that the left arm bends to the fore-end; the
	// Munster Twin SMGs up at the hip, the hands round their grips showing; the left hand under a thick fore-end on the
	// body, up on its near side in view.
	TestTrue(TEXT("the Curd Pistol forward and to its left in the hand"), LookOf(TEXT("pistol")).Model.GetLocation().Y > 1.0f && LookOf(TEXT("pistol")).Model.GetLocation().X > 0.0f);
	TestTrue(TEXT("the Dual Burratas forward in the hand"), LookOf(TEXT("burratas")).Model.GetLocation().Y > 1.0f);
	TestTrue(FString::Printf(TEXT("the knife turned about its blade in view (%.0f degrees)"), ViewKnifeTurn), ViewKnifeTurn >= 20.0f && ViewKnifeTurn <= 60.0f);
	for (const TCHAR* Id : {TEXT("blaster"), TEXT("shotgun")})
	{
		const FVector Shift = LookOf(Id).HipShift;
		TestTrue(FString::Printf(TEXT("%s: back and up at the hip (%.0f, %.0f, %.0f)"), Id, Shift.X, Shift.Y, Shift.Z), Shift.X < -5.0f && Shift.Z > 0.0f && Shift.Size() < 15.0f);
	}
	TestTrue(TEXT("the Munster Twin SMGs up at the hip"), LookOf(TEXT("munster")).HipShift.Z > 0.0f);
	for (const TCHAR* Id : {TEXT("blaster"), TEXT("carbine")})
	{
		const FWeaponLook& Look = LookOf(Id);
		TestTrue(FString::Printf(TEXT("%s: the left hand under the fore-end on the body, higher in view"), Id), Look.LeftHand.IsSet() && Look.ViewLeftHand.IsSet() && Look.ViewLeftHand->Z > Look.LeftHand->Z + 2.0f
			&& FMath::IsNearlyEqual(Look.ViewLeftHand->Y, Look.LeftHand->Y));
	}
	TestTrue(TEXT("the Roquefort's hold is as it was"), !LookOf(TEXT("roquefort")).ViewLeftHand.IsSet() && LookOf(TEXT("roquefort")).HipShift.IsZero());
	return true;
}

#endif
