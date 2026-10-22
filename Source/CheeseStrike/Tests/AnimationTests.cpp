// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The body's animation: the numbers it blends by, the animations it is made
// of, and that the character and the weapon are made of Lyra's mannequin
// and rifle.

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Materials/MaterialInterface.h"
#include "Rules/CheeseStrikeRules.h"
#include "CheeseStrikeCharacter.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnimationTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	bool Near(float Value, float Wanted)
	{
		return FMath::Abs(Value - Wanted) < 0.001f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCyclesWeighByDirection, "CheeseStrike.Animation.TheCyclesWeighByDirection", AnimationTests::Flags)

bool FTheCyclesWeighByDirection::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeAnimation;
	float Forward, Backward, Left, Right;
	DirectionWeights(0.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("straight ahead is all forward"), AnimationTests::Near(Forward, 1.0f) && AnimationTests::Near(Backward + Left + Right, 0.0f));
	DirectionWeights(90.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("to the right is all right"), AnimationTests::Near(Right, 1.0f));
	DirectionWeights(-90.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("to the left is all left"), AnimationTests::Near(Left, 1.0f));
	DirectionWeights(180.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("backwards is all backward"), AnimationTests::Near(Backward, 1.0f));
	DirectionWeights(-180.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("either way round"), AnimationTests::Near(Backward, 1.0f));
	DirectionWeights(45.0f, Forward, Backward, Left, Right);
	TestTrue(TEXT("between, half and half"), AnimationTests::Near(Forward, 0.5f) && AnimationTests::Near(Right, 0.5f));
	TestTrue(TEXT("summing to one"), AnimationTests::Near(Forward + Backward + Left + Right, 1.0f));

	TestTrue(TEXT("standing still weighs nothing"), AnimationTests::Near(MovingWeight(0.0f), 0.0f));
	TestTrue(TEXT("a walk weighs the cycles fully"), AnimationTests::Near(MovingWeight(WalkFull), 1.0f) && AnimationTests::Near(JogWeight(WalkFull), 0.0f));
	TestTrue(TEXT("and a jog the jog"), AnimationTests::Near(JogWeight(JogFull), 1.0f) && AnimationTests::Near(JogWeight(JogFull + 500.0f), 1.0f));
	TestTrue(TEXT("halfway between, half"), AnimationTests::Near(JogWeight((WalkFull + JogFull) / 2.0f), 0.5f));

	TestTrue(TEXT("moving the way one faces is forward"), AnimationTests::Near(DirectionOf(FVector(100.0, 0.0, 0.0), 0.0f), 0.0f));
	TestTrue(TEXT("to one's right is right"), AnimationTests::Near(DirectionOf(FVector(0.0, 100.0, 0.0), 0.0f), 90.0f));
	TestTrue(TEXT("whichever way one faces"), AnimationTests::Near(DirectionOf(FVector(0.0, 100.0, 0.0), 90.0f), 0.0f));
	TestTrue(TEXT("and standing still is forward"), AnimationTests::Near(DirectionOf(FVector::ZeroVector, 45.0f), 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheBodyHasEveryAnimation, "CheeseStrike.Animation.TheBodyHasEveryAnimation", AnimationTests::Flags)

bool FTheBodyHasEveryAnimation::RunTest(const FString& Parameters)
{
	// The mannequin's rifle set, and each hero's sets, retargeted to its skeleton.
	const UCheeseStrikeAnimInstance* Mannequin = GetDefault<UCheeseStrikeAnimInstance>();
	const UCheeseStrikeAnimInstance* Heroes = GetDefault<UCheeseStrikeHeroAnimInstance>();
	TestEqual(TEXT("a body's sets for every hero"), Heroes->GetSets().Num(), CheeseStrikeBodies::All().Num());
	TArray<const FCheeseStrikeAnimSet*> Sets = {&Mannequin->GetSet()};
	for (const FCheeseStrikeAnimSets& Body : Heroes->GetSets())
	{
		Sets.Add(&Body.For(ECheeseStrikeHold::Rifle));
	}
	for (int32 Index = 0; Index < Sets.Num(); ++Index)
	{
		const FCheeseStrikeAnimSet& Set = *Sets[Index];
		const FString Who = Index == 0 ? FString(TEXT("the mannequin")) : FString(CheeseStrikeBodies::All()[Index - 1].Name);
		TestTrue(FString::Printf(TEXT("%s: every animation was found"), *Who), Set.IsComplete());
		TestNotNull(TEXT("standing"), Set.Idle.Get());
		TestNotNull(TEXT("crouched"), Set.CrouchIdle.Get());
		TestNotNull(TEXT("crouch walking"), Set.CrouchWalkForward.Get());
		TestNotNull(TEXT("aiming from the hip"), Set.AimHip.Get());
		TestNotNull(TEXT("firing"), Set.Fire.Get());
		TestNotNull(TEXT("reloading"), Set.Reload.Get());
		if (Index > 0 && Set.Idle != nullptr)
		{
			const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, CheeseStrikeBodies::All()[Index - 1].Mesh);
			TestTrue(FString::Printf(TEXT("%s's are its own skeleton's"), *Who), Mesh != nullptr && Set.Idle->GetSkeleton() == Mesh->GetSkeleton());
			TestTrue(TEXT("not the mannequin's"), Mannequin->GetSet().Idle == nullptr || Set.Idle->GetSkeleton() != Mannequin->GetSet().Idle->GetSkeleton());
		}
	}
	// Every hero holds every weapon its own way: the pistol's set, the shotgun's firing and reloading, the knife's
	// stroke and guard, and draws them; all retargeted to its own skeleton.
	for (int32 Index = 0; Index < Heroes->GetSets().Num(); ++Index)
	{
		const FCheeseStrikeAnimSets& Body = Heroes->GetSets()[Index];
		const FString Who = CheeseStrikeBodies::All()[Index].Name;
		const FCheeseStrikeAnimSet& Rifle = Body.For(ECheeseStrikeHold::Rifle);
		const FCheeseStrikeAnimSet& Pistol = Body.For(ECheeseStrikeHold::Pistol);
		TestTrue(FString::Printf(TEXT("%s has the pistol's set"), *Who), Body.Holds[static_cast<uint8>(ECheeseStrikeHold::Pistol)].IsComplete() && Pistol.Idle != Rifle.Idle);
		TestNotNull(FString::Printf(TEXT("%s draws the pistol"), *Who), Pistol.Equip.Get());
		TestNotNull(FString::Printf(TEXT("%s draws the rifle"), *Who), Rifle.Equip.Get());
		const FCheeseStrikeAnimSet& Shotgun = Body.For(ECheeseStrikeHold::Shotgun);
		TestTrue(TEXT("the shotgun walks as the rifle does"), Shotgun.Idle == Rifle.Idle);
		TestTrue(FString::Printf(TEXT("%s fires and reloads the shotgun its own way"), *Who), Shotgun.Fire != nullptr && Shotgun.Fire != Rifle.Fire && Shotgun.Reload != nullptr && Shotgun.Reload != Rifle.Reload);
		const FCheeseStrikeAnimSet& Knife = Body.For(ECheeseStrikeHold::Knife);
		TestTrue(FString::Printf(TEXT("%s strikes with the knife, the free arm on guard"), *Who), Knife.Stroke != nullptr && Knife.Guard != nullptr && Knife.Idle == Pistol.Idle);
		for (const UAnimationAsset* Each : {static_cast<const UAnimationAsset*>(Pistol.Idle.Get()), static_cast<const UAnimationAsset*>(Pistol.AimHip.Get()),
			static_cast<const UAnimationAsset*>(Pistol.Equip.Get()), static_cast<const UAnimationAsset*>(Rifle.Equip.Get()), static_cast<const UAnimationAsset*>(Shotgun.Fire.Get()),
			static_cast<const UAnimationAsset*>(Shotgun.Reload.Get()), static_cast<const UAnimationAsset*>(Knife.Stroke.Get()), static_cast<const UAnimationAsset*>(Knife.Guard.Get())})
		{
			TestTrue(FString::Printf(TEXT("%s's are all on its skeleton"), *Who), Each != nullptr && Rifle.Idle != nullptr && Each->GetSkeleton() == Rifle.Idle->GetSkeleton());
		}
	}
	TestTrue(TEXT("a body without a pistol's set uses its rifle's"), &Mannequin->GetSet(ECheeseStrikeHold::Pistol) == &Mannequin->GetSet(ECheeseStrikeHold::Rifle));
	TestEqual(TEXT("the mannequin's grip follows weapon_r"), Mannequin->GetGripAnchor(), FName(TEXT("weapon_r")));
	TestEqual(TEXT("a hero's, its right hand"), Heroes->GetGripAnchor(), FName(TEXT("hand_r")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCharacterIsAHeroInBothViews, "CheeseStrike.Animation.TheCharacterIsAHeroInBothViews", AnimationTests::Flags)

bool FTheCharacterIsAHeroInBothViews::RunTest(const FString& Parameters)
{
	const ACheeseStrikeCharacter* Character = GetDefault<ACheeseStrikeCharacter>();
	// The body others see and the one its own player sees are the same: the arms in view are the character's own.
	for (const USkeletalMeshComponent* Body : {Character->GetMesh(), Character->GetFirstPersonMesh()})
	{
		if (!TestNotNull(TEXT("a body"), Body))
		{
			continue;
		}
		TestNotNull(TEXT("has a mesh"), Body->GetSkeletalMeshAsset());
		if (Body->GetSkeletalMeshAsset() != nullptr)
		{
			TestEqual(TEXT("which is the first hero's"), CheeseStrikeBodies::IndexOfMesh(Body->GetSkeletalMeshAsset()->GetPathName()), 0);
		}
		TestTrue(TEXT("animated in code, by the heroes' sets"), Body->AnimClass == UCheeseStrikeHeroAnimInstance::StaticClass());
	}
	const ACheeseStrikeWeapon* Weapon = GetDefault<ACheeseStrikeWeapon>();
	for (const USkeletalMeshComponent* Rifle : {Weapon->GetFirstPersonMesh(), Weapon->GetThirdPersonMesh()})
	{
		if (TestNotNull(TEXT("a rifle"), Rifle) && TestNotNull(TEXT("with a mesh"), Rifle->GetSkeletalMeshAsset()))
		{
			TestEqual(TEXT("which is Lyra's"), Rifle->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Rifle")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryBodyIsComplete, "CheeseStrike.Animation.EveryBodyIsComplete", AnimationTests::Flags)

bool FEveryBodyIsComplete::RunTest(const FString& Parameters)
{
	for (int32 Index = 0; Index < CheeseStrikeBodies::All().Num(); ++Index)
	{
		const CheeseStrikeBodies::FBody& Body = CheeseStrikeBodies::All()[Index];
		const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Body.Mesh);
		if (!TestNotNull(FString::Printf(TEXT("%s has a mesh"), Body.Name), Mesh))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s is found by its mesh"), Body.Name), CheeseStrikeBodies::IndexOfMesh(Mesh->GetPathName()), Index);
		TestTrue(TEXT("with a hand for the rifle"), Mesh->GetRefSkeleton().FindBoneIndex(FName(TEXT("hand_r"))) != INDEX_NONE);
		FName Left;
		FName Right;
		TestTrue(TEXT("eyes for the camera"), ACheeseStrikeCharacter::FindEyes(Mesh, Left, Right));
		TestTrue(TEXT("its own ragdoll"), Mesh->GetPhysicsAsset() != nullptr && Mesh->GetPhysicsAsset()->GetName().EndsWith(TEXT("_Ragdoll")));
		for (int32 Slot = 0; Slot < Body.NumWeaponSlots; ++Slot)
		{
			const FName Name(Body.WeaponSlots[Slot]);
			TestTrue(FString::Printf(TEXT("its weapons' slot %s, to hide"), Body.WeaponSlots[Slot]), Mesh->GetMaterials().ContainsByPredicate([&Name](const FSkeletalMaterial& Each) { return Each.MaterialSlotName == Name; }));
		}
		for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
		{
			for (int32 Slot = 0; Slot < Body.NumTeamSlots; ++Slot)
			{
				const FString Look = CheeseStrikeBodies::TeamLook(Index, Team, Body.TeamSlots[Slot]);
				TestNotNull(FString::Printf(TEXT("and %s"), *Look), LoadObject<UMaterialInterface>(nullptr, *Look));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheSidesHaveBodiesOfTheirOwn, "CheeseStrike.Animation.TheSidesHaveBodiesOfTheirOwn", AnimationTests::Flags)

bool FTheSidesHaveBodiesOfTheirOwn::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBodies;
	TestEqual(TEXT("the Gouda Guys are Murdock"), ForTeam(0, 0), IndexOf(TEXT("Murdock")));
	TestEqual(TEXT("and Twinblast"), ForTeam(0, 1), IndexOf(TEXT("Twinblast")));
	TestEqual(TEXT("the Mozza Radicals Lt. Belica"), ForTeam(1, 0), IndexOf(TEXT("Lt. Belica")));
	TestEqual(TEXT("and Wraith"), ForTeam(1, 1), IndexOf(TEXT("Wraith")));
	// On a side, its heroes in turn, never the other side's; every hero is on a side.
	for (int32 Rank = 0; Rank < 8; ++Rank)
	{
		TestEqual(TEXT("a Gouda Guy is one of theirs"), TeamOf(For(true, 0, Rank)), 0);
		TestEqual(TEXT("a Mozza Radical one of theirs"), TeamOf(For(true, 1, Rank)), 1);
		TestNotEqual(TEXT("the next on a side is the other hero"), For(true, 0, Rank), For(true, 0, Rank + 1));
	}
	for (int32 Body = 0; Body < All().Num(); ++Body)
	{
		TestTrue(FString::Printf(TEXT("%s is on a side"), All()[Body].Name), TeamOf(Body) != INDEX_NONE);
	}
	// Without sides, as many players as heroes have every hero.
	TSet<int32> Seen;
	for (int32 Rank = 0; Rank < All().Num(); ++Rank)
	{
		Seen.Add(For(false, 0, Rank));
	}
	TestEqual(TEXT("without sides, every hero in turn"), Seen.Num(), All().Num());
	TestTrue(TEXT("the sides' words name their looks"), TeamLook(0, 1, TEXT("M_Body")).Contains(TEXT("MI_M_Body_Mozza")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheRagdollBendsAsABodyDoes, "CheeseStrike.Animation.TheRagdollBendsAsABodyDoes", AnimationTests::Flags)

bool FTheRagdollBendsAsABodyDoes::RunTest(const FString& Parameters)
{
	for (const CheeseStrikeBodies::FBody& Hero : CheeseStrikeBodies::All())
	{
		const USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, Hero.Mesh);
		const UPhysicsAsset* Ragdoll = Body != nullptr ? Body->GetPhysicsAsset() : nullptr;
		if (!TestNotNull(FString::Printf(TEXT("%s has a ragdoll"), Hero.Name), Ragdoll))
		{
			continue;
		}
		// A body for each part that moves on its own; none under the head (the face and jaw would slide on the
		// skull) or for the hero's own guns, which are hidden.
		for (const TCHAR* Bone : {TEXT("pelvis"), TEXT("spine_03"), TEXT("head"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("upperarm_r"), TEXT("lowerarm_r"),
			TEXT("hand_r"), TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")})
		{
			TestTrue(FString::Printf(TEXT("%s: a body for %s"), Hero.Name, Bone), Ragdoll->FindBodyIndex(Bone) != INDEX_NONE);
		}
		const FReferenceSkeleton& Skeleton = Body->GetRefSkeleton();
		const int32 Head = Skeleton.FindBoneIndex(TEXT("head"));
		for (const USkeletalBodySetup* Part : Ragdoll->SkeletalBodySetups)
		{
			const int32 Bone = Skeleton.FindBoneIndex(Part->BoneName);
			const FString Name = Part->BoneName.ToString();
			TestFalse(FString::Printf(TEXT("%s: no body under the head (%s)"), Hero.Name, *Name), Bone != Head && Skeleton.BoneIsChildOf(Bone, Head));
			TestFalse(FString::Printf(TEXT("%s: no body for a gun (%s)"), Hero.Name, *Name), Name.Contains(TEXT("pistol")) || Name.Contains(TEXT("weapon")) || Name.Contains(TEXT("gun")));
		}
		// Every joint limited every way, on hard limits, twisting no further than a person's; the knees and elbows
		// hinges; and the spine twisting little at each joint.
		float SpineTwist = 0.0f;
		for (const UPhysicsConstraintTemplate* Template : Ragdoll->ConstraintSetup)
		{
			const FConstraintProfileProperties& Joint = Template->DefaultInstance.ProfileInstance;
			const FString Bone = FString(Hero.Name) + TEXT("'s ") + Template->DefaultInstance.ConstraintBone1.ToString();
			const FString Bare = Template->DefaultInstance.ConstraintBone1.ToString();
			TestTrue(FString::Printf(TEXT("%s is limited every way"), *Bone), Joint.ConeLimit.Swing1Motion != ACM_Free && Joint.ConeLimit.Swing2Motion != ACM_Free && Joint.TwistLimit.TwistMotion != ACM_Free);
			TestFalse(FString::Printf(TEXT("%s on hard limits"), *Bone), Joint.ConeLimit.bSoftConstraint || Joint.TwistLimit.bSoftConstraint);
			TestTrue(FString::Printf(TEXT("%s twists no more than 40 degrees (%.0f)"), *Bone, Joint.TwistLimit.TwistLimitDegrees), Joint.TwistLimit.TwistLimitDegrees <= 40.0f);
			if (Bare.StartsWith(TEXT("lowerarm")) || Bare.StartsWith(TEXT("calf")))
			{
				TestTrue(FString::Printf(TEXT("%s is a hinge (%.0f to the side)"), *Bone, Joint.ConeLimit.Swing1LimitDegrees), Joint.ConeLimit.Swing1LimitDegrees <= 5.0f);
			}
			if (Bare.StartsWith(TEXT("spine")))
			{
				SpineTwist += Joint.TwistLimit.TwistLimitDegrees;
			}
		}
		TestTrue(FString::Printf(TEXT("%s's spine twists no more than 30 degrees in all (%.0f)"), Hero.Name, SpineTwist), SpineTwist > 0.0f && SpineTwist <= 30.0f);
	}
	return true;
}

#endif
