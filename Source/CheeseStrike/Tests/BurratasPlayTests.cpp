// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the Dual Burratas in the editor (see PlayTesting.h): a gun
// in each hand, on both bodies, side by side and pointing the same way, the
// two firing in turn, a shot a pull; and aimed, both up before the eyes side
// by side, a little converged.

#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CheeseStrikeCharacter.h"
#include "PlayTesting.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace BurratasPlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** Open ground on the shooter level, a long clear run ahead. */
	const FVector Spot(1150.0f, -1450.0f, 100.0f);
	constexpr float SpotYaw = 117.0f;
	int32 ShotsBefore = 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheDualBurrataHoldsAGunInEachHand, "CheeseStrike.Play.TheDualBurrataHoldsAGunInEachHand", BurratasPlayTests::Flags)

bool FTheDualBurrataHoldsAGunInEachHand::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace BurratasPlayTests;
	StartPlaying({1});
	PutAt(0, Spot, SpotYaw);
	Then([]()
	{
		ACheeseStrikeCharacter* Me = Character(0);
		Me->EndProtection();
		Me->GetInventory()->Give(FCheeseStrikeLoadout::Usual(CheeseStrikeWeapons::Burratas().Id));
	});
	Wait(1.5f);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = Character(0);
		const ACheeseStrikeWeapon* Weapon = Me->GetWeapon();
		if (!TestTrue(TEXT("the Dual Burratas in hand"), Weapon != nullptr && Weapon->GetWeaponId() == CheeseStrikeWeapons::Burratas().Id))
		{
			return;
		}
		const FVector Ahead = Me->GetControlRotation().Vector();
		for (const bool bFirstPerson : {true, false})
		{
			const TCHAR* Which = bFirstPerson ? TEXT("in its player's view") : TEXT("on the body");
			const UPrimitiveComponent* Right = Weapon->GetModel(bFirstPerson);
			const UStaticMeshComponent* Left = Weapon->GetOffHandModel(bFirstPerson);
			if (!TestTrue(FString::Printf(TEXT("two guns %s"), Which), Right != nullptr && Left != nullptr && Left->GetStaticMesh() != nullptr && !Left->bHiddenInGame))
			{
				continue;
			}
			// Each model lies along its Y axis, muzzle forward.
			const FVector RightAlong = Right->GetComponentTransform().GetUnitAxis(EAxis::Y);
			const FVector LeftAlong = Left->GetComponentTransform().GetUnitAxis(EAxis::Y);
			TestTrue(FString::Printf(TEXT("the second in the left hand %s"), Which), Left->GetAttachSocketName() == FName(TEXT("hand_l")));
			TestTrue(FString::Printf(TEXT("both pointing ahead %s (%.2f, %.2f)"), Which, FVector::DotProduct(RightAlong, Ahead), FVector::DotProduct(LeftAlong, Ahead)),
				FVector::DotProduct(RightAlong, Ahead) > 0.5f && FVector::DotProduct(LeftAlong, Ahead) > 0.5f);
			TestTrue(FString::Printf(TEXT("about the same way %s (%.2f)"), Which, FVector::DotProduct(RightAlong, LeftAlong)), FVector::DotProduct(RightAlong, LeftAlong) > 0.75f);
			const FVector Across = FVector::CrossProduct(FVector::UpVector, Ahead).GetSafeNormal();
			const float Apart = FVector::DotProduct(Right->GetComponentLocation() - Left->GetComponentLocation(), Across);
			TestTrue(FString::Printf(TEXT("side by side %s, the right one right (%.0f cm apart)"), Which, Apart), Apart > 15.0f && Apart < 80.0f);
		}
		ShotsBefore = Weapon->ShotsFired;
	});
	// Aimed (wave 15): both guns come up to eye level side by side, a little converged, on both bodies; the trigger fires
	// them in turn.
	Play(0, TEXT("hold Aim\nwait 0.7"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = Character(0);
		const ACheeseStrikeWeapon* Weapon = Me->GetWeapon();
		TestTrue(TEXT("two guns are aimed"), Weapon->IsAimed() && Me->GetAimFraction() > 0.9f);
		const FVector Ahead = Me->GetControlRotation().Vector();
		const FVector Across = FVector::CrossProduct(FVector::UpVector, Ahead).GetSafeNormal();
		const float EyeHeight = Me->GetFirstPersonCameraComponent()->GetComponentLocation().Z;
		for (const bool bFirstPerson : {true, false})
		{
			const TCHAR* Which = bFirstPerson ? TEXT("in its player's view") : TEXT("on the body");
			const UPrimitiveComponent* Right = Weapon->GetModel(bFirstPerson);
			const UStaticMeshComponent* Left = Weapon->GetOffHandModel(bFirstPerson);
			if (Right == nullptr || Left == nullptr)
			{
				continue;
			}
			const FVector RightAlong = Right->GetComponentTransform().GetUnitAxis(EAxis::Y);
			const FVector LeftAlong = Left->GetComponentTransform().GetUnitAxis(EAxis::Y);
			// Each gun's grip is at its model's origin; its bore about 7 cm over it.
			const float RightUp = Right->GetComponentLocation().Z + 7.0f - EyeHeight;
			const float LeftUp = Left->GetComponentLocation().Z + 7.0f - EyeHeight;
			TestTrue(FString::Printf(TEXT("aimed, both guns at eye level %s (%.0f, %.0f cm from the eyes)"), Which, RightUp, LeftUp), FMath::Abs(RightUp) < 14.0f && FMath::Abs(LeftUp) < 14.0f);
			const float Apart = FVector::DotProduct(Right->GetComponentLocation() - Left->GetComponentLocation(), Across);
			TestTrue(FString::Printf(TEXT("side by side, close %s (%.0f cm apart)"), Which, Apart), Apart > 6.0f && Apart < 30.0f);
			TestTrue(FString::Printf(TEXT("both pointing ahead %s (%.2f, %.2f)"), Which, FVector::DotProduct(RightAlong, Ahead), FVector::DotProduct(LeftAlong, Ahead)),
				FVector::DotProduct(RightAlong, Ahead) > 0.95f && FVector::DotProduct(LeftAlong, Ahead) > 0.95f);
			// Converged: the right gun points a little to the left of the left gun's way.
			const float Converged = FVector::DotProduct(LeftAlong - RightAlong, Across);
			TestTrue(FString::Printf(TEXT("a little converged %s (%.3f)"), Which, Converged), Converged > 0.02f && Converged < 0.25f);
		}
	});
	// A shot a pull: held, the trigger fires once; pulled again and again, once each time.
	Play(0, TEXT("hold Fire\nwait 0.6\nrelease Fire\nrelease Aim\nwait 0.2"));
	Then([this]()
	{
		const ACheeseStrikeWeapon* Weapon = Character(0)->GetWeapon();
		TestEqual(TEXT("held down, they fire one shot: not automatic"), Weapon->ShotsFired - ShotsBefore, 1);
		ShotsBefore = Weapon->ShotsFired;
	});
	Play(0, TEXT("tap Fire\nwait 0.18\ntap Fire\nwait 0.18\ntap Fire\nwait 0.18\ntap Fire\nwait 0.3"));
	Then([this]()
	{
		const ACheeseStrikeWeapon* Weapon = Character(0)->GetWeapon();
		TestEqual(TEXT("four pulls, four shots"), Weapon->ShotsFired - ShotsBefore, 4);
	});
	StopPlaying();
	return true;
}

#endif
