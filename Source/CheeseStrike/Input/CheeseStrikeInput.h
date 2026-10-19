// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Object.h"
#include "CheeseStrikeInput.generated.h"

class UInputAction;
class UInputMappingContext;

/** How looking turns for what the player moves. */
namespace CheeseStrikeInput
{
	/**
	 * Degrees the view turns for each count the mouse reports, before the
	 * mouse axes' sensitivity in Config/DefaultInput.ini (0.07) is applied:
	 * what the engine's own scale was.
	 */
	constexpr float MouseDegreesPerCount = 2.5f;
	/** Degrees a second a stick held full over turns the view. */
	constexpr float StickYawDegreesPerSecond = 150.0f;
	constexpr float StickPitchDegreesPerSecond = 100.0f;
	/** How far a stick must be pushed before it counts: a stick at rest is never quite centred. */
	constexpr float StickDeadZone = 0.2f;
}

/**
 * The game's input actions and the keys that drive them, made in code rather
 * than kept as assets, so that the bindings can be read, changed and tested
 * as text. The keys are the Godot game's (docs/unreal/design.md,
 * "Controls"). One set is made for the game instance (see
 * UCheeseStrikeInputSubsystem); the player controller puts its context on,
 * and the character binds its actions.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeInputSet : public UObject
{
	GENERATED_BODY()

public:
	/** Makes a set owned by `Outer`. */
	static UCheeseStrikeInputSet* Build(UObject* Outer);

	/** The game's set, from the game instance of `WorldContext`'s world; null outside a game. */
	static const UCheeseStrikeInputSet* Get(const UObject* WorldContext);

	/** The keys mapped to `Action` in the context. */
	TArray<FKey> KeysFor(const UInputAction* Action) const;

	/** Moving: a 2D axis, X right and Y forward. */
	UPROPERTY()
	TObjectPtr<UInputAction> Move;
	/** Looking with the mouse: counts moved, Y up. */
	UPROPERTY()
	TObjectPtr<UInputAction> MouseLook;
	/** Looking with a stick: how far over it is held, Y up. */
	UPROPERTY()
	TObjectPtr<UInputAction> StickLook;
	UPROPERTY()
	TObjectPtr<UInputAction> Jump;
	UPROPERTY()
	TObjectPtr<UInputAction> Sprint;
	UPROPERTY()
	TObjectPtr<UInputAction> Crouch;
	UPROPERTY()
	TObjectPtr<UInputMappingContext> Context;
};

/** Holds the game's one input set for as long as the game instance lives. */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeInputSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const UCheeseStrikeInputSet* GetSet() const { return Set; }

private:
	UPROPERTY()
	TObjectPtr<UCheeseStrikeInputSet> Set;
};
