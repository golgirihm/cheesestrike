// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The figure on the front end, played in the editor: it holds the Cheddar
// Blaster as a character does, and changing its hero shows nothing out of
// place for a frame: the weapon stays in its hand and no gun of the hero's
// own shows, every frame after each change.

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Animation/CheeseStrikeBodies.h"
#include "PlayTesting.h"
#include "UI/CheeseStrikeMenuFigure.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace MenuFigureTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** The furthest the weapon may be from the hand bone that holds it, in cm (its grip is a couple of centimetres from the bone). */
	constexpr float HandReach = 25.0f;
	/** Frames looked at after each change of hero. */
	constexpr int32 FramesWatched = 15;
	/** The bones a hero's own guns are skinned to (ACheeseStrikeWeapon::HideBuiltInWeapons), hidden while ours shows. */
	const TCHAR* const BuiltInWeaponBones[] = {TEXT("weapon"), TEXT("weapon_l"), TEXT("pistol"), TEXT("bot_hand")};

	/** What is wrong with the figure this frame, or nothing. */
	FString OutOfPlace(const ACheeseStrikeMenuFigure* Figure)
	{
		// (The questions asked of the body aren't const in the engine, though they change nothing.)
		USkeletalMeshComponent* Body = Figure != nullptr ? const_cast<USkeletalMeshComponent*>(Figure->GetBody()) : nullptr;
		const UStaticMeshComponent* Weapon = Figure != nullptr ? Figure->GetWeapon() : nullptr;
		const USkeletalMesh* Model = Body != nullptr ? Body->GetSkeletalMeshAsset() : nullptr;
		if (Model == nullptr || Weapon == nullptr)
		{
			return TEXT("no body or weapon");
		}
		// Hidden, nothing shows out of place: that is the point of hiding it.
		if (!Figure->IsShown())
		{
			return FString();
		}
		if (Weapon->GetVisibleFlag() != Body->GetVisibleFlag())
		{
			return TEXT("the weapon shows while the body doesn't, or the other way round");
		}
		const float Off = FVector::Dist(Weapon->GetComponentLocation(), Body->GetSocketLocation(TEXT("hand_r")));
		if (Off > HandReach)
		{
			return FString::Printf(TEXT("the weapon is %.0f cm from the hand"), Off);
		}
		const CheeseStrikeBodies::FBody& Look = CheeseStrikeBodies::All()[Figure->GetHero()];
		for (int32 Slot = 0; Slot < Look.NumWeaponSlots; ++Slot)
		{
			const FName Name(Look.WeaponSlots[Slot]);
			const int32 Material = Model->GetMaterials().IndexOfByPredicate([&Name](const FSkeletalMaterial& Each) { return Each.MaterialSlotName == Name; });
			if (Material != INDEX_NONE && Body->IsMaterialSectionShown(Material, 0))
			{
				return FString::Printf(TEXT("the hero's own gun (%s) shows"), Look.WeaponSlots[Slot]);
			}
		}
		for (const TCHAR* Bone : BuiltInWeaponBones)
		{
			if (Model->GetRefSkeleton().FindBoneIndex(FName(Bone)) != INDEX_NONE && !Body->IsBoneHiddenByName(FName(Bone)))
			{
				return FString::Printf(TEXT("the hero's own gun's bone (%s) shows"), Bone);
			}
		}
		return FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheFigureChangesHeroWithNothingOutOfPlace, "CheeseStrike.Play.TheFigureChangesHeroWithNothingOutOfPlace", MenuFigureTests::Flags)

bool FTheFigureChangesHeroWithNothingOutOfPlace::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace MenuFigureTests;
	StartPlaying({1});
	// A figure of our own on the level (the front end's is only made while the menu shows), as the menu makes it.
	TSharedRef<TWeakObjectPtr<ACheeseStrikeMenuFigure>> Figure = MakeShared<TWeakObjectPtr<ACheeseStrikeMenuFigure>>();
	Then([this, Figure]()
	{
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		*Figure = World(0)->SpawnActor<ACheeseStrikeMenuFigure>(ACheeseStrikeMenuFigure::StaticClass(), FVector(-231.0f, -500.0f, 6.0f), FRotator(0.0f, -90.0f, 0.0f), Parameters);
		if (!TestNotNull(TEXT("a figure"), Figure->Get()))
		{
			return;
		}
		const ACheeseStrikeMenuFigure* Made = Figure->Get();
		TestEqual(TEXT("it wears the default hero to begin with"), Made->GetHero(), CheeseStrikeBodies::DefaultHero);
		// It holds the Cheddar Blaster, the cheese weapon's model, on the right hand.
		const UStaticMesh* Held = Made->GetWeapon() != nullptr ? Made->GetWeapon()->GetStaticMesh() : nullptr;
		TestTrue(FString::Printf(TEXT("it holds the Cheddar Blaster (%s)"), Held != nullptr ? *Held->GetName() : TEXT("nothing")), Held != nullptr && Held->GetName().Contains(TEXT("Blaster")));
		TestEqual(TEXT("in its right hand"), Made->GetWeapon() != nullptr ? Made->GetWeapon()->GetAttachSocketName() : NAME_None, FName(TEXT("hand_r")));
	});
	Wait(0.5f);
	Then([this, Figure]()
	{
		TestEqual(TEXT("whole, the weapon is in its hand"), OutOfPlace(Figure->Get()), FString());
	});
	// Four changes of hero, round every hero and back: every frame after each, the weapon in the hand and no gun of the hero's own.
	for (const int32 Hero : {1, 2, 3, 0})
	{
		TSharedRef<int32> Frames = MakeShared<int32>(0);
		TSharedRef<int32> Hidden = MakeShared<int32>(0);
		Then([this, Figure, Hero, Frames, Hidden]()
		{
			*Frames = 0;
			*Hidden = 0;
			if (ACheeseStrikeMenuFigure* Made = Figure->Get())
			{
				Made->ShowHero(Hero);
				TestEqual(TEXT("it wears the hero asked for"), Made->GetHero(), Hero);
				TestEqual(FString::Printf(TEXT("the frame of the change to %s"), CheeseStrikeBodies::All()[Hero].Name), OutOfPlace(Made), FString());
			}
		});
		WaitUntil(FString::Printf(TEXT("%d frames after the change to %s"), FramesWatched, CheeseStrikeBodies::All()[Hero].Name), 10.0f, [this, Figure, Hero, Frames, Hidden]()
		{
			const ACheeseStrikeMenuFigure* Made = Figure->Get();
			if (Made == nullptr)
			{
				return true;
			}
			const FString Wrong = OutOfPlace(Made);
			if (!Wrong.IsEmpty())
			{
				AddError(FString::Printf(TEXT("%d frames after the change to %s: %s"), *Frames, CheeseStrikeBodies::All()[Hero].Name, *Wrong));
			}
			*Hidden += Made->IsShown() ? 0 : 1;
			return ++*Frames >= FramesWatched;
		});
		Then([this, Figure, Hero, Hidden]()
		{
			TestTrue(FString::Printf(TEXT("the figure is hidden a frame or two at most through the change to %s (%d)"), CheeseStrikeBodies::All()[Hero].Name, *Hidden), *Hidden <= 2);
			TestTrue(TEXT("and shows after it"), Figure->IsValid() && Figure->Get()->IsShown());
		});
	}
	Then([Figure]()
	{
		if (ACheeseStrikeMenuFigure* Made = Figure->Get())
		{
			Made->Destroy();
		}
	});
	StopPlaying();
	return true;
}

#endif
