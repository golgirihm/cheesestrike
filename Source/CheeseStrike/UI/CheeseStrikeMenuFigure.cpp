// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMenuFigure.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Rules/CheeseStrikeLoadout.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeShot.h"
#include "Weapons/CheeseStrikeWeapon.h"
#include "Weapons/CheeseStrikeWeaponLooks.h"

namespace
{
	TAutoConsoleVariable<float> CVarFigureTurn(TEXT("CheeseStrike.FigureTurn"), 1.0f,
		TEXT("How fast the front end's figure turns, as a share of its usual rate: 0 holds it still, its animation paused too, for comparing screenshots of it (tools/play/hero_switch.txt)."));
}

ACheeseStrikeMenuFigure::ACheeseStrikeMenuFigure()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	// A low round pedestal, from the engine's own shapes, in the level's grey.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grey(TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_Round.MI_PrototypeGrid_Gray_Round"));
	Pedestal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pedestal"));
	RootComponent = Pedestal;
	Pedestal->SetStaticMesh(Cylinder.Object);
	if (Grey.Object != nullptr)
	{
		Pedestal->SetMaterial(0, Grey.Object);
	}
	// The engine's cylinder is a metre wide and tall, standing on its middle.
	Pedestal->SetRelativeScale3D(FVector(PedestalWidth / 100.0f, PedestalWidth / 100.0f, PedestalHeight / 100.0f));
	Pedestal->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The character as players see one another, animated as in the game, holding the weapon: the hero picked (ShowHero).
	// Every hero's mesh is held here (hard references), so all four, with their materials and textures, stay loaded while
	// the figure exists.
	for (const CheeseStrikeBodies::FBody& Each : CheeseStrikeBodies::All())
	{
		ConstructorHelpers::FObjectFinder<USkeletalMesh> Hero(Each.Mesh);
		Heroes.Add(Hero.Object);
	}
	Shown = CheeseStrikeBodies::DefaultHero;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Pedestal);
	Body->SetSkeletalMeshAsset(Heroes.IsValidIndex(Shown) ? Heroes[Shown].Get() : nullptr);
	Body->SetAnimInstanceClass(UCheeseStrikeHeroAnimInstance::StaticClass());
	// Undo the pedestal's scale, and stand on its top, facing the way the pedestal faces.
	Body->SetRelativeScale3D(FVector(100.0f / PedestalWidth, 100.0f / PedestalWidth, 100.0f / PedestalHeight));
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	Body->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// The loadout's primary (the Colby Carbine to begin with), as a character holds it: its model and materials are
	// loaded by path as the figure begins (ACheeseStrikeWeapon::DressModel), as a weapon's are, never in a constructor.
	Primary = CheeseStrikeLoadout::DefaultPrimary();
	Weapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));
	Weapon->SetupAttachment(Body);
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OffHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OffHand"));
	OffHand->SetupAttachment(Body);
	OffHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ACheeseStrikeMenuFigure::HoldsTwo() const
{
	const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Primary);
	return Stats != nullptr && Stats->bDual;
}

void ACheeseStrikeMenuFigure::DressWeapons()
{
	const FWeaponLook& Look = CheeseStrikeWeaponLooks::For(Primary);
	ACheeseStrikeWeapon::DressModel(Weapon, Look);
	// Two guns: the same model in the left hand; one: nothing there.
	if (HoldsTwo())
	{
		ACheeseStrikeWeapon::DressModel(OffHand, Look);
	}
	else
	{
		OffHand->SetStaticMesh(nullptr);
	}
}

void ACheeseStrikeMenuFigure::TellHold()
{
	if (UCheeseStrikeAnimInstance* Animation = Cast<UCheeseStrikeAnimInstance>(Body->GetAnimInstance()))
	{
		Animation->HoldWithoutCharacter(CheeseStrikeWeaponLooks::For(Primary), HoldsTwo(), Weapon);
	}
}

void ACheeseStrikeMenuFigure::BeginPlay()
{
	Super::BeginPlay();
	BegunAt = FPlatformTime::Seconds();
	DressWeapons();
	ArmHero();
	KeepTexturesResident(true);
	WarmUpHeroes();
}

void ACheeseStrikeMenuFigure::WarmUpHeroes()
{
	// Each hero a couple of centimetres tall in the middle of the pedestal (inside its cylinder, so never seen), with no
	// animation or shadow: enough to be drawn, which is what readies its materials. The pedestal is scaled; the body's
	// relative scale undoes that.
	const float Tiny = 0.012f;
	const FVector Scale(Tiny * 100.0f / PedestalWidth, Tiny * 100.0f / PedestalWidth, Tiny * 100.0f / PedestalHeight);
	for (int32 Hero = 0; Hero < Heroes.Num(); ++Hero)
	{
		if (Heroes[Hero] == nullptr)
		{
			continue;
		}
		USkeletalMeshComponent* Warm = NewObject<USkeletalMeshComponent>(this, *FString::Printf(TEXT("Warm%d"), Hero));
		Warm->SetSkeletalMeshAsset(Heroes[Hero]);
		Warm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Warm->SetCastShadow(false);
		Warm->SetRelativeScale3D(Scale);
		Warm->SetupAttachment(Pedestal);
		Warm->RegisterComponent();
		WarmBodies.Add(Warm);
	}
}

void ACheeseStrikeMenuFigure::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	KeepTexturesResident(false);
	Super::EndPlay(EndPlayReason);
}

void ACheeseStrikeMenuFigure::ArmHero()
{
	const USkeletalMesh* Model = Body->GetSkeletalMeshAsset();
	if (Model == nullptr || !CheeseStrikeBodies::All().IsValidIndex(Shown))
	{
		return;
	}
	// Only our weapon shows: the hero's own guns are hidden, its material slots (Murdock's, Wraith's, Twinblast's) and the
	// bones they are skinned to (Belica's), before the body is ever drawn with them.
	const CheeseStrikeBodies::FBody& Look = CheeseStrikeBodies::All()[Shown];
	for (int32 Slot = 0; Slot < Look.NumWeaponSlots; ++Slot)
	{
		const FName Name(Look.WeaponSlots[Slot]);
		const int32 Material = Model->GetMaterials().IndexOfByPredicate([&Name](const FSkeletalMaterial& Each) { return Each.MaterialSlotName == Name; });
		for (int32 Level = 0; Material != INDEX_NONE && Level < Model->GetLODNum(); ++Level)
		{
			Body->ShowMaterialSection(Material, INDEX_NONE, false, Level);
		}
	}
	ACheeseStrikeWeapon::HideBuiltInWeapons(Body);
	// In the hand as the game holds it, at this hero's grip; a second gun in the other as a player holding two has it, and
	// the body posed as theirs is.
	ACheeseStrikeWeapon::PutInHand(Weapon, Body, CheeseStrikeWeaponLooks::For(Primary));
	if (HoldsTwo())
	{
		ACheeseStrikeWeapon::PutInOffHand(OffHand, Body, Weapon);
	}
	TellHold();
}

void ACheeseStrikeMenuFigure::ShowPrimary(FName Id)
{
	if (Id == Primary || Id.IsNone())
	{
		return;
	}
	Primary = Id;
	if (HasActorBegunPlay())
	{
		DressWeapons();
		ArmHero();
	}
}

void ACheeseStrikeMenuFigure::ShowHero(int32 Hero)
{
	const int32 Wanted = CheeseStrikeBodies::BodyFor(Hero);
	USkeletalMesh* const Model = Heroes.IsValidIndex(Wanted) ? Heroes[Wanted].Get() : nullptr;
	if (Model == nullptr || (Wanted == Shown && Body->GetSkeletalMeshAsset() == Model))
	{
		return;
	}
	Shown = Wanted;
	// Hidden through the swap, the weapon with it: a mesh just swapped stands in its reference pose, its hands at its sides,
	// until its animation is next evaluated, and the weapon would float beside the figure meanwhile; and the new hero's own
	// guns show until they are hidden. Shown again on the next tick (Tick), after the first pose.
	Body->SetVisibility(false, true);
	HiddenTicks = 0;
	Body->SetSkeletalMeshAsset(Model);
	// A fresh animation, which plays the new skeleton's set; nothing the last hero hid stays hidden.
	Body->SetAnimInstanceClass(nullptr);
	Body->SetAnimInstanceClass(UCheeseStrikeHeroAnimInstance::StaticClass());
	Body->EmptyOverrideMaterials();
	for (int32 Level = 0; Level < Model->GetLODNum(); ++Level)
	{
		Body->ShowAllMaterialSections(Level);
	}
	for (int32 Bone = 0; Bone < Model->GetRefSkeleton().GetNum(); ++Bone)
	{
		if (Body->IsBoneHiddenByName(Model->GetRefSkeleton().GetBoneName(Bone)))
		{
			Body->UnHideBoneByName(Model->GetRefSkeleton().GetBoneName(Bone));
		}
	}
	ArmHero();
	// Its first pose now, on this thread, so that the weapon rides the animated hand from the first frame it is drawn in.
	Body->TickAnimation(0.0f, false);
	Body->RefreshBoneTransforms();
}

bool ACheeseStrikeMenuFigure::IsShown() const
{
	return Body != nullptr && Body->GetVisibleFlag();
}

void ACheeseStrikeMenuFigure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Held still (CheeseStrike.FigureTurn 0): neither turning nor breathing, so that two screenshots of it differ in nothing but their drawing.
	const float Turn = CVarFigureTurn.GetValueOnGameThread();
	AddActorWorldRotation(FRotator(0.0f, TurnRate * TurnShare * Turn * DeltaSeconds, 0.0f));
	Body->bPauseAnims = Turn <= 0.0f;
	// The hold, told afresh (the animation is made anew with each hero).
	TellHold();
	// The tick after a change of hero: the body has its first pose (ShowHero evaluated one, and its own animation tick
	// follows this), so it can be seen again, weapon and all.
	if (HiddenTicks != INDEX_NONE && ++HiddenTicks >= 1)
	{
		HiddenTicks = INDEX_NONE;
		Body->SetVisibility(true, true);
	}
}

TArray<UTexture*> ACheeseStrikeMenuFigure::TexturesOf(int32 Hero) const
{
	TArray<UTexture*> Textures;
	const USkeletalMesh* Model = Heroes.IsValidIndex(Hero) ? Heroes[Hero].Get() : nullptr;
	if (Model == nullptr)
	{
		return Textures;
	}
	for (const FSkeletalMaterial& Each : Model->GetMaterials())
	{
		if (Each.MaterialInterface != nullptr)
		{
			TArray<UTexture*> Used;
			Each.MaterialInterface->GetUsedTextures(Used);
			for (UTexture* Texture : Used)
			{
				Textures.AddUnique(Texture);
			}
		}
	}
	return Textures;
}

void ACheeseStrikeMenuFigure::KeepTexturesResident(bool bKeep)
{
	// Every mip of every hero's textures, and every level of detail of its mesh, loaded and kept (bForceMiplevelsToBeResident),
	// fetched at once when asked for, so that a hero switched to is as whole and sharp on its first frame as on its thirtieth;
	// let go as the figure goes.
	for (int32 Hero = 0; Hero < Heroes.Num(); ++Hero)
	{
		USkeletalMesh* Model = Heroes[Hero].Get();
		if (Model == nullptr)
		{
			continue;
		}
		Model->bForceMiplevelsToBeResident = bKeep;
		if (bKeep)
		{
			Model->SetForceMipLevelsToBeResident(TexturesWaitSeconds);
		}
		for (const FSkeletalMaterial& Each : Model->GetMaterials())
		{
			if (Each.MaterialInterface != nullptr)
			{
				Each.MaterialInterface->SetForceMipLevelsToBeResident(true, bKeep, -1.0f, 0, bKeep);
			}
		}
	}
}

bool ACheeseStrikeMenuFigure::AreTexturesResident() const
{
	USkeletalMesh* Model = Heroes.IsValidIndex(Shown) ? Heroes[Shown].Get() : nullptr;
	if (Model != nullptr && !Model->IsFullyStreamedIn())
	{
		return false;
	}
	for (UTexture* Texture : TexturesOf(Shown))
	{
		if (UTexture2D* Streamed = Cast<UTexture2D>(Texture); Streamed != nullptr && !Streamed->IsFullyStreamedIn())
		{
			return false;
		}
	}
	return true;
}

bool ACheeseStrikeMenuFigure::IsReady() const
{
	return FPlatformTime::Seconds() - BegunAt > TexturesWaitSeconds || AreTexturesResident();
}
