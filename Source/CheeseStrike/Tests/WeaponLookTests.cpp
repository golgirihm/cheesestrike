// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The weapons' looks (CheeseStrikeWeaponLooks): every weapon has its own
// model, cheese materials, sounds and HUD picture, which all load; its
// muzzle and sights are on its model; and the hand that holds it has a place
// in view for each way of holding.

#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
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
		TestFalse(FString::Printf(TEXT("%s's model is its own"), *Name), Models.Contains(ModelPath));
		Models.Add(ModelPath);
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
		}
		TestTrue(FString::Printf(TEXT("%s's shots are about as loud as the rest"), *Name), Look.ShotVolume > 0.5f && Look.ShotVolume <= 1.3f);

		// Its picture in the HUD's slot list.
		const FString Icon = FString::Printf(TEXT("/Game/CheeseStrike/Weapons/Icons/T_Icon_%s.T_Icon_%s"), *Name, *Name);
		TestNotNull(FString::Printf(TEXT("%s has a picture"), *Name), Load<UTexture2D>(*Icon));
	}

	// The knife's swings and blows, the draws, and the flash.
	for (const TCHAR* Sound : CheeseStrikeWeaponLooks::KnifeSwings)
	{
		TestNotNull(TEXT("the knife swings"), Load<USoundBase>(Sound));
	}
	for (const TCHAR* Sound : CheeseStrikeWeaponLooks::KnifeHits)
	{
		TestNotNull(TEXT("and lands"), Load<USoundBase>(Sound));
	}
	for (const TCHAR* Sound : CheeseStrikeWeaponLooks::DrawSounds)
	{
		TestNotNull(TEXT("a weapon is drawn"), Load<USoundBase>(Sound));
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

#endif
