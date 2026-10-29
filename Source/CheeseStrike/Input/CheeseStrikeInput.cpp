// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeInput.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Settings/CheeseStrikeSettings.h"
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
	Set->SprintToggle = MakeAction(Set, TEXT("IA_SprintToggle"), EInputActionValueType::Boolean);
	Set->CrouchToggle = MakeAction(Set, TEXT("IA_CrouchToggle"), EInputActionValueType::Boolean);
	Set->Fire = MakeAction(Set, TEXT("IA_Fire"), EInputActionValueType::Boolean);
	Set->Aim = MakeAction(Set, TEXT("IA_Aim"), EInputActionValueType::Boolean);
	Set->Reload = MakeAction(Set, TEXT("IA_Reload"), EInputActionValueType::Boolean);
	Set->DrawPrimary = MakeAction(Set, TEXT("IA_DrawPrimary"), EInputActionValueType::Boolean);
	Set->DrawSidearm = MakeAction(Set, TEXT("IA_DrawSidearm"), EInputActionValueType::Boolean);
	Set->DrawKnife = MakeAction(Set, TEXT("IA_DrawKnife"), EInputActionValueType::Boolean);
	Set->NextWeapon = MakeAction(Set, TEXT("IA_NextWeapon"), EInputActionValueType::Boolean);
	Set->PreviousWeapon = MakeAction(Set, TEXT("IA_PreviousWeapon"), EInputActionValueType::Boolean);
	Set->Scoreboard = MakeAction(Set, TEXT("IA_Scoreboard"), EInputActionValueType::Boolean);
	Set->Menu = MakeAction(Set, TEXT("IA_Menu"), EInputActionValueType::Boolean);
	Set->Streak = MakeAction(Set, TEXT("IA_Streak"), EInputActionValueType::Boolean);
	Set->Interact = MakeAction(Set, TEXT("IA_Interact"), EInputActionValueType::Boolean);
	Set->Grenade = MakeAction(Set, TEXT("IA_Grenade"), EInputActionValueType::Boolean);
	UInputMappingContext* Context = NewObject<UInputMappingContext>(Set, TEXT("IMC_CheeseStrike"));
	Set->Context = Context;

	// The gamepad's, as consoles' shooters lay them out (Call of Duty's defaults): the left stick moves and, pressed,
	// sprints; the right looks and, pressed, draws the knife; A jumps, B crouches, X reloads, Y changes to the next weapon;
	// the left trigger aims, the right fires; the left bumper interacts, the right throws a grenade; the d-pad draws a
	// weapon (up the primary, left the sidearm, right the knife) and, down, calls in a streak; View shows the scoreboard,
	// Menu pauses.
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
	// Jumping, sprinting and crouching: held on the keyboard, pressed on a gamepad.
	Map(Context, Set->Jump, EKeys::SpaceBar);
	Map(Context, Set->Jump, EKeys::Gamepad_FaceButton_Bottom);
	Map(Context, Set->Sprint, EKeys::LeftShift);
	Map(Context, Set->SprintToggle, EKeys::Gamepad_LeftThumbstick);
	Map(Context, Set->Crouch, EKeys::C);
	Map(Context, Set->Crouch, EKeys::LeftControl);
	Map(Context, Set->CrouchToggle, EKeys::Gamepad_FaceButton_Right);
	// Firing, aiming down the sights, and reloading.
	Map(Context, Set->Fire, EKeys::LeftMouseButton);
	Map(Context, Set->Fire, EKeys::Gamepad_RightTrigger);
	Map(Context, Set->Aim, EKeys::RightMouseButton);
	Map(Context, Set->Aim, EKeys::Gamepad_LeftTrigger);
	Map(Context, Set->Reload, EKeys::R);
	Map(Context, Set->Reload, EKeys::Gamepad_FaceButton_Left);
	// Changing weapons: the number keys, the wheel (down for the next); on a gamepad, the d-pad for each,
	// the top face button for the next, and pressing the right stick for the knife (a console shooter's melee). With three
	// weapons the d-pad reaches each at once, so the one before has no button of their own.
	Map(Context, Set->DrawPrimary, EKeys::One);
	Map(Context, Set->DrawPrimary, EKeys::Gamepad_DPad_Up);
	Map(Context, Set->DrawSidearm, EKeys::Two);
	Map(Context, Set->DrawSidearm, EKeys::Gamepad_DPad_Left);
	Map(Context, Set->DrawKnife, EKeys::Three);
	Map(Context, Set->DrawKnife, EKeys::Gamepad_DPad_Right);
	Map(Context, Set->DrawKnife, EKeys::Gamepad_RightThumbstick);
	Map(Context, Set->NextWeapon, EKeys::MouseScrollDown);
	Map(Context, Set->NextWeapon, EKeys::Gamepad_FaceButton_Top);
	Map(Context, Set->PreviousWeapon, EKeys::MouseScrollUp);
	Map(Context, Set->Scoreboard, EKeys::Tab);
	Map(Context, Set->Scoreboard, EKeys::Gamepad_Special_Left);
	Map(Context, Set->Menu, EKeys::Escape);
	Map(Context, Set->Menu, EKeys::Gamepad_Special_Right);
	// Kill streaks: Q (wave 9; it was 4, and Q the last weapon), and the d-pad's last way.
	Map(Context, Set->Streak, EKeys::Q);
	Map(Context, Set->Streak, EKeys::Gamepad_DPad_Down);
	// Grenades: G, and the right bumper.
	Map(Context, Set->Grenade, EKeys::G);
	Map(Context, Set->Grenade, EKeys::Gamepad_RightShoulder);
	// Interacting.
	Map(Context, Set->Interact, EKeys::E);
	Map(Context, Set->Interact, EKeys::Gamepad_LeftShoulder);
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

FString UCheeseStrikeInputSet::PromptFor(const UInputAction* Action, bool bGamepad, CheeseStrikeInput::EGlyphs Glyphs) const
{
	for (const FKey& Key : KeysFor(Action))
	{
		if (Key.IsGamepadKey() == bGamepad)
		{
			return CheeseStrikeInput::KeyLabel(Key, Glyphs);
		}
	}
	return FString();
}

FString UCheeseStrikeInputSet::Prompt(const APlayerController* Player, TObjectPtr<UInputAction> UCheeseStrikeInputSet::* Action)
{
	const UCheeseStrikeInputSet* Set = Get(Player);
	const bool bGamepad = CheeseStrikeInput::IsUsingGamepad(Player);
	return Set != nullptr ? Set->PromptFor(Set->*Action, bGamepad, bGamepad ? CheeseStrikeInput::GlyphsOf(Player) : CheeseStrikeInput::EGlyphs::Xbox) : FString();
}

namespace CheeseStrikeInput
{
	FVector2D StickTurn(const FVector2D& Stick, float DeltaSeconds, float& EdgeSeconds)
	{
		// The curve on how far over the stick is, its direction kept.
		const double Over = FMath::Min(1.0, Stick.Size());
		if (Over <= UE_KINDA_SMALL_NUMBER)
		{
			EdgeSeconds = 0.0f;
			return FVector2D::ZeroVector;
		}
		const FVector2D Curved = Stick / Stick.Size() * FMath::Pow(Over, static_cast<double>(StickExponent));
		// Held at the edge across, a turn speeds up after a moment.
		EdgeSeconds = FMath::Abs(Stick.X) >= StickEdge ? EdgeSeconds + DeltaSeconds : 0.0f;
		const float Boost = 1.0f + (StickBoost - 1.0f) * FMath::Clamp((EdgeSeconds - StickBoostDelay) / StickBoostRamp, 0.0f, 1.0f);
		return FVector2D(Curved.X * StickYawDegreesPerSecond * Boost * DeltaSeconds, Curved.Y * StickPitchDegreesPerSecond * DeltaSeconds);
	}

	EDevice DeviceAfterKey(EDevice Now, const FKey& Key)
	{
		if (!Key.IsValid())
		{
			return Now;
		}
		if (Key.IsGamepadKey())
		{
			return EDevice::Gamepad;
		}
		return Key.IsMouseButton() ? EDevice::Mouse : EDevice::Keyboard;
	}

	EDevice DeviceAfterAnalog(EDevice Now, const FKey& Key, float Value)
	{
		return Key.IsGamepadKey() && FMath::Abs(Value) >= DeviceStickThreshold ? EDevice::Gamepad : Now;
	}

	EDevice DeviceAfterMouseMove(EDevice Now, float Pixels)
	{
		return Pixels >= DeviceMouseThreshold ? EDevice::Mouse : Now;
	}

	FString KeyLabel(const FKey& Key)
	{
		static const TMap<FKey, FString> Labels = {
			{EKeys::Gamepad_FaceButton_Bottom, TEXT("A")}, {EKeys::Gamepad_FaceButton_Right, TEXT("B")},
			{EKeys::Gamepad_FaceButton_Left, TEXT("X")}, {EKeys::Gamepad_FaceButton_Top, TEXT("Y")},
			{EKeys::Gamepad_LeftShoulder, TEXT("LB")}, {EKeys::Gamepad_RightShoulder, TEXT("RB")},
			{EKeys::Gamepad_LeftTrigger, TEXT("LT")}, {EKeys::Gamepad_RightTrigger, TEXT("RT")},
			{EKeys::Gamepad_LeftThumbstick, TEXT("LS")}, {EKeys::Gamepad_RightThumbstick, TEXT("RS")},
			{EKeys::Gamepad_Special_Left, TEXT("VIEW")}, {EKeys::Gamepad_Special_Right, TEXT("MENU")},
			{EKeys::Gamepad_DPad_Up, TEXT("↑")}, {EKeys::Gamepad_DPad_Down, TEXT("↓")},
			{EKeys::Gamepad_DPad_Left, TEXT("←")}, {EKeys::Gamepad_DPad_Right, TEXT("→")},
			{EKeys::Escape, TEXT("ESC")}, {EKeys::SpaceBar, TEXT("SPACE")}, {EKeys::LeftShift, TEXT("SHIFT")},
			{EKeys::LeftControl, TEXT("CTRL")}, {EKeys::Tab, TEXT("TAB")}, {EKeys::LeftMouseButton, TEXT("LMB")},
			{EKeys::RightMouseButton, TEXT("RMB")}, {EKeys::One, TEXT("1")}, {EKeys::Two, TEXT("2")},
			{EKeys::Three, TEXT("3")}, {EKeys::Four, TEXT("4")},
		};
		if (const FString* Label = Labels.Find(Key))
		{
			return *Label;
		}
		// The letter keys by their letter: "E", "Q".
		return Key.GetDisplayName(false).ToString().ToUpper();
	}

	const TCHAR* GlyphsName(EGlyphs Glyphs)
	{
		static const TCHAR* const Names[NumGlyphs] = {TEXT("AUTO"), TEXT("XBOX"), TEXT("PLAYSTATION"), TEXT("NINTENDO")};
		return Names[FMath::Clamp(static_cast<int32>(Glyphs), 0, NumGlyphs - 1)];
	}

	FString KeyLabel(const FKey& Key, EGlyphs Glyphs)
	{
		// The same place on each pad: a PlayStation pad's shapes and numbered shoulders, a Nintendo pad's letters swapped
		// round, its triggers ZL and ZR, its bumpers L and R, and View and Menu − and +.
		static const TMap<FKey, FString> PlayStation = {
			{EKeys::Gamepad_FaceButton_Bottom, TEXT("CROSS")}, {EKeys::Gamepad_FaceButton_Right, TEXT("CIRCLE")},
			{EKeys::Gamepad_FaceButton_Left, TEXT("SQUARE")}, {EKeys::Gamepad_FaceButton_Top, TEXT("TRIANGLE")},
			{EKeys::Gamepad_LeftShoulder, TEXT("L1")}, {EKeys::Gamepad_RightShoulder, TEXT("R1")},
			{EKeys::Gamepad_LeftTrigger, TEXT("L2")}, {EKeys::Gamepad_RightTrigger, TEXT("R2")},
			{EKeys::Gamepad_LeftThumbstick, TEXT("L3")}, {EKeys::Gamepad_RightThumbstick, TEXT("R3")},
			{EKeys::Gamepad_Special_Left, TEXT("TOUCHPAD")}, {EKeys::Gamepad_Special_Right, TEXT("OPTIONS")},
		};
		static const TMap<FKey, FString> Nintendo = {
			{EKeys::Gamepad_FaceButton_Bottom, TEXT("B")}, {EKeys::Gamepad_FaceButton_Right, TEXT("A")},
			{EKeys::Gamepad_FaceButton_Left, TEXT("Y")}, {EKeys::Gamepad_FaceButton_Top, TEXT("X")},
			{EKeys::Gamepad_LeftShoulder, TEXT("L")}, {EKeys::Gamepad_RightShoulder, TEXT("R")},
			{EKeys::Gamepad_LeftTrigger, TEXT("ZL")}, {EKeys::Gamepad_RightTrigger, TEXT("ZR")},
			{EKeys::Gamepad_Special_Left, TEXT("−")}, {EKeys::Gamepad_Special_Right, TEXT("+")},
		};
		const TMap<FKey, FString>* Own = Glyphs == EGlyphs::PlayStation ? &PlayStation : (Glyphs == EGlyphs::Nintendo ? &Nintendo : nullptr);
		if (const FString* Label = Own != nullptr ? Own->Find(Key) : nullptr)
		{
			return *Label;
		}
		return KeyLabel(Key);
	}

	EGlyphs GlyphsOfDevice(const FString& DeviceName)
	{
		for (const TCHAR* Word : {TEXT("DualSense"), TEXT("DualShock"), TEXT("PlayStation"), TEXT("PS4"), TEXT("PS5"), TEXT("Sony")})
		{
			if (DeviceName.Contains(Word, ESearchCase::IgnoreCase))
			{
				return EGlyphs::PlayStation;
			}
		}
		for (const TCHAR* Word : {TEXT("Nintendo"), TEXT("Switch"), TEXT("Pro Controller"), TEXT("Joy-Con")})
		{
			if (DeviceName.Contains(Word, ESearchCase::IgnoreCase))
			{
				return EGlyphs::Nintendo;
			}
		}
		return EGlyphs::Xbox;
	}

	EGlyphs GlyphsToShow(EGlyphs Setting, EGlyphs Detected)
	{
		if (Setting != EGlyphs::Auto)
		{
			return Setting;
		}
		return Detected == EGlyphs::Auto ? EGlyphs::Xbox : Detected;
	}

	EGlyphs GlyphsOf(const APlayerController* Player)
	{
		const UCheeseStrikeSettings* Settings = UCheeseStrikeSettings::Get();
		// (The tests read the prompts as they come, whatever this PC's player picked.)
		const EGlyphs Setting = Settings != nullptr && !GIsAutomationTesting ? static_cast<EGlyphs>(FMath::Clamp<int32>(Settings->GetGamepadGlyphs(), 0, NumGlyphs - 1)) : EGlyphs::Auto;
		if (Setting != EGlyphs::Auto)
		{
			return Setting;
		}
		// The pad last used, as the engine knows it: XInput (and Steam Input, which shows every pad as an Xbox one) can't tell.
		EGlyphs Detected = EGlyphs::Xbox;
		const ULocalPlayer* Local = Player != nullptr ? Player->GetLocalPlayer() : nullptr;
		if (const UInputDeviceSubsystem* Devices = Local != nullptr ? UInputDeviceSubsystem::Get() : nullptr)
		{
			const FHardwareDeviceIdentifier Hardware = Devices->GetMostRecentlyUsedHardwareDevice(Local->GetPlatformUserId());
			if (Hardware.IsValid())
			{
				Detected = GlyphsOfDevice(Hardware.HardwareDeviceIdentifier.ToString() + TEXT(" ") + Hardware.InputClassName.ToString());
			}
		}
		return GlyphsToShow(Setting, Detected);
	}

	EDevice DeviceOf(const APlayerController* Player)
	{
		const ULocalPlayer* Local = Player != nullptr ? Player->GetLocalPlayer() : nullptr;
		const UGameInstance* Game = Player != nullptr ? Player->GetGameInstance() : nullptr;
		const UCheeseStrikeInputSubsystem* Input = Game != nullptr ? Game->GetSubsystem<UCheeseStrikeInputSubsystem>() : nullptr;
		if (Local == nullptr || Input == nullptr)
		{
			return EDevice::Keyboard;
		}
		const TSharedPtr<const FSlateUser> User = Local->GetSlateUser();
		return Input->GetDevice(User.IsValid() ? User->GetUserIndex() : 0);
	}

	bool IsUsingGamepad(const APlayerController* Player)
	{
		return DeviceOf(Player) == EDevice::Gamepad;
	}
}

/** Watches everything Slate gets for the device each of its users plays with; takes nothing from anyone. */
class FCheeseStrikeDeviceWatcher : public IInputProcessor
{
public:
	CheeseStrikeInput::EDevice Get(int32 User) const
	{
		return Devices.IsValidIndex(User) ? Devices[User] : CheeseStrikeInput::EDevice::Keyboard;
	}

	void Set(int32 User, CheeseStrikeInput::EDevice Device)
	{
		if (User < 0 || User >= 16)
		{
			return;
		}
		if (!Devices.IsValidIndex(User))
		{
			Devices.SetNum(User + 1);
		}
		Devices[User] = Device;
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		Set(Event.GetUserIndex(), CheeseStrikeInput::DeviceAfterKey(Get(Event.GetUserIndex()), Event.GetKey()));
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& Event) override
	{
		Set(Event.GetUserIndex(), CheeseStrikeInput::DeviceAfterAnalog(Get(Event.GetUserIndex()), Event.GetKey(), Event.GetAnalogValue()));
		return false;
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		Set(Event.GetUserIndex(), CheeseStrikeInput::DeviceAfterMouseMove(Get(Event.GetUserIndex()), static_cast<float>(Event.GetCursorDelta().Size())));
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		Set(Event.GetUserIndex(), CheeseStrikeInput::EDevice::Mouse);
		return false;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event, const FPointerEvent* Gesture) override
	{
		Set(Event.GetUserIndex(), CheeseStrikeInput::EDevice::Mouse);
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("CheeseStrikeDevices"); }

private:
	TArray<CheeseStrikeInput::EDevice> Devices;
};

void UCheeseStrikeInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Set = UCheeseStrikeInputSet::Build(this);
	Watcher = MakeShared<FCheeseStrikeDeviceWatcher>();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RegisterInputPreProcessor(Watcher);
	}
}

void UCheeseStrikeInputSubsystem::Deinitialize()
{
	if (Watcher.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Watcher);
	}
	Watcher.Reset();
	Super::Deinitialize();
}

CheeseStrikeInput::EDevice UCheeseStrikeInputSubsystem::GetDevice(int32 UserIndex) const
{
	return Watcher.IsValid() ? Watcher->Get(UserIndex) : CheeseStrikeInput::EDevice::Keyboard;
}

void UCheeseStrikeInputSubsystem::SetDevice(int32 UserIndex, CheeseStrikeInput::EDevice Device)
{
	if (Watcher.IsValid())
	{
		Watcher->Set(UserIndex, Device);
	}
}
