// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/Object.h"
#include "CheeseStrikeInput.generated.h"

class APlayerController;
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
	/**
	 * The look stick's response curve: past the dead zone, how far over it is
	 * held is raised to this power, so that small pushes make fine
	 * adjustments and a full push still turns at the full rate.
	 */
	constexpr float StickExponent = 2.0f;
	/** Held at the edge (past StickEdge) for StickBoostDelay seconds, a turn speeds up over StickBoostRamp seconds to StickBoost times across, for turning round. */
	constexpr float StickEdge = 0.95f;
	constexpr float StickBoostDelay = 0.25f;
	constexpr float StickBoostRamp = 0.35f;
	constexpr float StickBoost = 1.6f;

	/**
	 * How far the view turns this frame for the look stick held at `Stick`
	 * (past the dead zone, each axis -1 to 1), in degrees across and up and
	 * down, before the options' sensitivity: the curve, then the rates, the
	 * boost by `EdgeSeconds` (how long it has been held at the edge across,
	 * kept between frames by the caller and updated here).
	 */
	FVector2D StickTurn(const FVector2D& Stick, float DeltaSeconds, float& EdgeSeconds);

	/** What a player last played with, for the prompts on the screen and the menu's focus. */
	enum class EDevice : uint8
	{
		/** Keys on the keyboard. */
		Keyboard,
		/** The mouse: moved or clicked. */
		Mouse,
		Gamepad,
	};
	/** How far a gamepad's stick or trigger must go, and the mouse move in pixels, to count as being used: a stick at rest drifts, and a desk is knocked. */
	constexpr float DeviceStickThreshold = 0.3f;
	constexpr float DeviceMouseThreshold = 4.0f;
	/** The device after a key goes down (any key: the keyboard's, the mouse's buttons, a gamepad's). */
	EDevice DeviceAfterKey(EDevice Now, const FKey& Key);
	/** The device after a gamepad's stick or trigger reads `Value`. */
	EDevice DeviceAfterAnalog(EDevice Now, const FKey& Key, float Value);
	/** The device after the mouse moves `Pixels`. */
	EDevice DeviceAfterMouseMove(EDevice Now, float Pixels);

	/** A key as a prompt on the screen names it: "E", "4", "ESC", or a gamepad's button as an Xbox pad has it ("A", "LB", "RT", "VIEW", the d-pad's arrows). */
	FString KeyLabel(const FKey& Key);

	/** The device `Player`'s player last used (Keyboard until they have used any), from the game's input subsystem. */
	EDevice DeviceOf(const APlayerController* Player);
	/** Whether `Player`'s player is playing with a gamepad: they used one last. */
	bool IsUsingGamepad(const APlayerController* Player);
}

/**
 * The game's input actions and the keys that drive them, made in code rather
 * than kept as assets, so that the bindings can be read, changed and tested
 * as text. The keys are the Godot game's (docs/unreal/design.md,
 * "Controls"); a gamepad's are as consoles' shooters lay them out (Call of
 * Duty's defaults), with the d-pad for the weapons. One set is made for the game instance (see
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
	/**
	 * What a prompt on the screen calls the key for `Action`: its first
	 * gamepad key's label when `bGamepad`, else its first key on the keyboard
	 * or mouse (CheeseStrikeInput::KeyLabel); empty if it has none.
	 */
	FString PromptFor(const UInputAction* Action, bool bGamepad) const;
	/** The same for `Player`'s player, by the device they last used: `Prompt(Player, &UCheeseStrikeInputSet::Interact)`. */
	static FString Prompt(const APlayerController* Player, TObjectPtr<UInputAction> UCheeseStrikeInputSet::* Action);

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
	/** Sprinting and crouching while the key is held (shift, C or control). */
	UPROPERTY()
	TObjectPtr<UInputAction> Sprint;
	UPROPERTY()
	TObjectPtr<UInputAction> Crouch;
	/**
	 * A gamepad's: pressing the left stick sprints until the stick is let go
	 * (or the player aims or fires, or presses it again), and the right face
	 * button crouches or stands (slides at a sprint), as consoles' shooters
	 * have them; holding a button down under the thumb that looks is no way
	 * to play.
	 */
	UPROPERTY()
	TObjectPtr<UInputAction> SprintToggle;
	UPROPERTY()
	TObjectPtr<UInputAction> CrouchToggle;
	UPROPERTY()
	TObjectPtr<UInputAction> Fire;
	UPROPERTY()
	TObjectPtr<UInputAction> Aim;
	UPROPERTY()
	TObjectPtr<UInputAction> Reload;
	/** Changing weapons: to the primary (1), the sidearm (2), the knife (3), the next and the one before (the wheel), and back to the last (Q; no gamepad button). */
	UPROPERTY()
	TObjectPtr<UInputAction> DrawPrimary;
	UPROPERTY()
	TObjectPtr<UInputAction> DrawSidearm;
	UPROPERTY()
	TObjectPtr<UInputAction> DrawKnife;
	UPROPERTY()
	TObjectPtr<UInputAction> NextWeapon;
	UPROPERTY()
	TObjectPtr<UInputAction> PreviousWeapon;
	UPROPERTY()
	TObjectPtr<UInputAction> LastWeapon;
	/** Shows the scoreboard while held. */
	UPROPERTY()
	TObjectPtr<UInputAction> Scoreboard;
	/** Opens and closes the menu: the pause menu in a match, back out of a page otherwise. */
	UPROPERTY()
	TObjectPtr<UInputAction> Menu;
	/** Calls in the next kill streak earned (4, the d-pad's down). */
	UPROPERTY()
	TObjectPtr<UInputAction> Streak;
	/** Interact (E), held: reviving a teammate or finishing an enemy down, in Execution. */
	UPROPERTY()
	TObjectPtr<UInputAction> Interact;
	/** Throws a cheese grenade (G, the right bumper; UCheeseStrikeGrenades). */
	UPROPERTY()
	TObjectPtr<UInputAction> Grenade;
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
	virtual void Deinitialize() override;

	const UCheeseStrikeInputSet* GetSet() const { return Set; }

	/** What Slate's user `UserIndex` (a local player) last played with: watched in every key, stick and mouse move Slate gets, menus and game alike. */
	CheeseStrikeInput::EDevice GetDevice(int32 UserIndex) const;
	/** Says `UserIndex` last played with `Device`: for a play script, whose actions are injected past Slate. */
	void SetDevice(int32 UserIndex, CheeseStrikeInput::EDevice Device);

private:
	UPROPERTY()
	TObjectPtr<UCheeseStrikeInputSet> Set;
	/** Watches Slate's input for the devices (CheeseStrikeInput::DeviceAfterKey and the rest). */
	TSharedPtr<class FCheeseStrikeDeviceWatcher> Watcher;
};
