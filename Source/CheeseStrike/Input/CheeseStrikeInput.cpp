// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeInput.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	/** Maps `Key` to `Action`, and returns the mapping for modifiers to be added to. */
	FEnhancedActionKeyMapping& Map(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key)
	{
		return Context->MapKey(Action, Key);
	}

	/** A key that drives a 2D action's Y axis (forward), `Sign` being +1 or -1. */
	void MapForward(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key, float Sign)
	{
		FEnhancedActionKeyMapping& Mapping = Map(Context, Action, Key);
		if (Sign < 0.0f)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
		// The key's one value is X; this moves it to Y.
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
	}

	/** A key that drives a 2D action's X axis (right), `Sign` being +1 or -1. */
	void MapRight(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key, float Sign)
	{
		FEnhancedActionKeyMapping& Mapping = Map(Context, Action, Key);
		if (Sign < 0.0f)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
	}

	/** A 2D look key: as it comes, right and up positive, which is how the controller takes yaw and pitch. */
	void MapLook(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key)
	{
		Map(Context, Action, Key);
	}

	/** A stick: a 2D key that is never quite still, so its middle is ignored. */
	void MapStick(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = Map(Context, Action, Key);
		UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(Context);
		DeadZone->LowerThreshold = CheeseStrikeInput::StickDeadZone;
		Mapping.Modifiers.Add(DeadZone);
	}
}

UCheeseStrikeInputSet* UCheeseStrikeInputSet::Build(UObject* Outer)
{
	UCheeseStrikeInputSet* Set = NewObject<UCheeseStrikeInputSet>(Outer, TEXT("CheeseStrikeInputSet"));
	Set->Move = MakeAction(Set, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	Set->MouseLook = MakeAction(Set, TEXT("IA_MouseLook"), EInputActionValueType::Axis2D);
	Set->StickLook = MakeAction(Set, TEXT("IA_StickLook"), EInputActionValueType::Axis2D);
	Set->Jump = MakeAction(Set, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	Set->Sprint = MakeAction(Set, TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	Set->Crouch = MakeAction(Set, TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	Set->Fire = MakeAction(Set, TEXT("IA_Fire"), EInputActionValueType::Boolean);
	Set->Aim = MakeAction(Set, TEXT("IA_Aim"), EInputActionValueType::Boolean);
	Set->Reload = MakeAction(Set, TEXT("IA_Reload"), EInputActionValueType::Boolean);
	Set->Scoreboard = MakeAction(Set, TEXT("IA_Scoreboard"), EInputActionValueType::Boolean);
	UInputMappingContext* Context = NewObject<UInputMappingContext>(Set, TEXT("IMC_CheeseStrike"));
	Set->Context = Context;

	// Moving: WASD and the arrows, and the left stick.
	MapForward(Context, Set->Move, EKeys::W, 1.0f);
	MapForward(Context, Set->Move, EKeys::Up, 1.0f);
	MapForward(Context, Set->Move, EKeys::S, -1.0f);
	MapForward(Context, Set->Move, EKeys::Down, -1.0f);
	MapRight(Context, Set->Move, EKeys::D, 1.0f);
	MapRight(Context, Set->Move, EKeys::Right, 1.0f);
	MapRight(Context, Set->Move, EKeys::A, -1.0f);
	MapRight(Context, Set->Move, EKeys::Left, -1.0f);
	MapStick(Context, Set->Move, EKeys::Gamepad_Left2D);
	// Looking.
	MapLook(Context, Set->MouseLook, EKeys::Mouse2D);
	MapStick(Context, Set->StickLook, EKeys::Gamepad_Right2D);
	// Jumping, sprinting and crouching.
	Map(Context, Set->Jump, EKeys::SpaceBar);
	Map(Context, Set->Jump, EKeys::Gamepad_FaceButton_Bottom);
	Map(Context, Set->Sprint, EKeys::LeftShift);
	Map(Context, Set->Sprint, EKeys::Gamepad_LeftThumbstick);
	Map(Context, Set->Crouch, EKeys::C);
	Map(Context, Set->Crouch, EKeys::LeftControl);
	Map(Context, Set->Crouch, EKeys::Gamepad_FaceButton_Right);
	// Firing, aiming down the sights, and reloading.
	Map(Context, Set->Fire, EKeys::LeftMouseButton);
	Map(Context, Set->Fire, EKeys::Gamepad_RightTrigger);
	Map(Context, Set->Aim, EKeys::RightMouseButton);
	Map(Context, Set->Aim, EKeys::Gamepad_LeftTrigger);
	Map(Context, Set->Reload, EKeys::R);
	Map(Context, Set->Reload, EKeys::Gamepad_FaceButton_Left);
	Map(Context, Set->Scoreboard, EKeys::Tab);
	Map(Context, Set->Scoreboard, EKeys::Gamepad_Special_Left);
	return Set;
}

const UCheeseStrikeInputSet* UCheeseStrikeInputSet::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UCheeseStrikeInputSubsystem* Subsystem = GameInstance != nullptr ? GameInstance->GetSubsystem<UCheeseStrikeInputSubsystem>() : nullptr;
	return Subsystem != nullptr ? Subsystem->GetSet() : nullptr;
}

TArray<FKey> UCheeseStrikeInputSet::KeysFor(const UInputAction* Action) const
{
	TArray<FKey> Keys;
	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		if (Mapping.Action == Action)
		{
			Keys.Add(Mapping.Key);
		}
	}
	return Keys;
}

void UCheeseStrikeInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Set = UCheeseStrikeInputSet::Build(this);
}
