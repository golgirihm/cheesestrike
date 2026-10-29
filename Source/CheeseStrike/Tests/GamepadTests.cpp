// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Playing with a gamepad: every action has a button, the look stick's
// curve, the aim assist, the prompts that name the keys of the device in
// use, the menu's focus kept through a page drawn again; and, played in the
// editor, a gamepad's player with another in front of them.

#include "Misc/AutomationTest.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/UnrealType.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerController.h"
#include "Input/CheeseStrikeAimAssist.h"
#include "Input/CheeseStrikeInput.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "PlayTesting.h"
#include "Settings/CheeseStrikeSettings.h"
#include "UI/CheeseStrikeHUD.h"
#include "UI/CheeseStrikeMenu.h"
#include "Weapons/CheeseStrikeInventory.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GamepadTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** Every action of the set, by its property's name. */
	TMap<FString, const UInputAction*> Actions(const UCheeseStrikeInputSet* Set)
	{
		TMap<FString, const UInputAction*> Found;
		for (TFieldIterator<FObjectProperty> It(UCheeseStrikeInputSet::StaticClass()); It; ++It)
		{
			if (It->PropertyClass->IsChildOf(UInputAction::StaticClass()))
			{
				Found.Add(It->GetName(), Cast<UInputAction>(It->GetObjectPropertyValue_InContainer(Set)));
			}
		}
		return Found;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryActionHasAGamepadButton, "CheeseStrike.Input.EveryActionHasAGamepadButton", GamepadTests::Flags)

bool FEveryActionHasAGamepadButton::RunTest(const FString& Parameters)
{
	UCheeseStrikeInputSet* Set = UCheeseStrikeInputSet::Build(GetTransientPackage());
	const TMap<FString, const UInputAction*> Actions = GamepadTests::Actions(Set);
	TestTrue(TEXT("the set has its actions"), Actions.Num() >= 20);
	// With three weapons the d-pad reaches each at once, so the one before has no button (the d-pad's down
	// calls in a streak, the right bumper throws a grenade). What a key is held for, a gamepad
	// has an action of its own for: the look stick, and the sprint and crouch that a press turns on and off.
	const TSet<FString> KeyboardOnly = {TEXT("PreviousWeapon")};
	const TMap<FString, FString> GamepadsOwn = {{TEXT("MouseLook"), TEXT("StickLook")}, {TEXT("Sprint"), TEXT("SprintToggle")}, {TEXT("Crouch"), TEXT("CrouchToggle")}};
	const auto Has = [Set](const UInputAction* Action, bool bGamepad)
	{
		return Action != nullptr && Set->KeysFor(Action).ContainsByPredicate([bGamepad](const FKey& Key) { return Key.IsGamepadKey() == bGamepad; });
	};
	for (const TPair<FString, const UInputAction*>& Each : Actions)
	{
		if (!TestNotNull(FString::Printf(TEXT("%s is made"), *Each.Key), Each.Value))
		{
			continue;
		}
		const bool bGamepadsOwn = GamepadsOwn.FindKey(Each.Key) != nullptr;
		if (!KeyboardOnly.Contains(Each.Key))
		{
			const FString* Own = GamepadsOwn.Find(Each.Key);
			TestTrue(FString::Printf(TEXT("%s has a gamepad button"), *Each.Key), Has(Own != nullptr ? Actions.FindRef(*Own) : Each.Value, true));
		}
		if (!bGamepadsOwn)
		{
			TestTrue(FString::Printf(TEXT("%s has a key or a mouse button"), *Each.Key), Has(Each.Value, false));
		}
	}
	// The layout: Call of Duty's defaults, the d-pad for the weapons.
	const auto Pad = [Set](const UInputAction* Action) { return Set->PromptFor(Action, true); };
	TestEqual(TEXT("A jumps"), Pad(Set->Jump), FString(TEXT("A")));
	TestEqual(TEXT("B crouches"), Pad(Set->CrouchToggle), FString(TEXT("B")));
	TestEqual(TEXT("X reloads"), Pad(Set->Reload), FString(TEXT("X")));
	TestEqual(TEXT("Y changes to the next weapon"), Pad(Set->NextWeapon), FString(TEXT("Y")));
	TestEqual(TEXT("the left trigger aims"), Pad(Set->Aim), FString(TEXT("LT")));
	TestEqual(TEXT("the right fires"), Pad(Set->Fire), FString(TEXT("RT")));
	TestEqual(TEXT("the left bumper interacts"), Pad(Set->Interact), FString(TEXT("LB")));
	TestEqual(TEXT("the right throws a grenade"), Pad(Set->Grenade), FString(TEXT("RB")));
	TestEqual(TEXT("the d-pad's down calls in a streak"), Pad(Set->Streak), FString(TEXT("↓")));
	TestEqual(TEXT("the left stick sprints"), Pad(Set->SprintToggle), FString(TEXT("LS")));
	TestEqual(TEXT("View shows the scoreboard"), Pad(Set->Scoreboard), FString(TEXT("VIEW")));
	TestEqual(TEXT("Menu pauses"), Pad(Set->Menu), FString(TEXT("MENU")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePromptsFollowTheDeviceInUse, "CheeseStrike.Input.ThePromptsFollowTheDeviceInUse", GamepadTests::Flags)

bool FThePromptsFollowTheDeviceInUse::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeInput;
	// What was used last: a gamepad's button or a stick pushed well over, a key, the mouse moved or clicked.
	TestEqual(TEXT("a gamepad's button makes it the gamepad"), DeviceAfterKey(EDevice::Keyboard, EKeys::Gamepad_FaceButton_Bottom), EDevice::Gamepad);
	TestEqual(TEXT("a key the keyboard"), DeviceAfterKey(EDevice::Gamepad, EKeys::E), EDevice::Keyboard);
	TestEqual(TEXT("a click the mouse"), DeviceAfterKey(EDevice::Gamepad, EKeys::LeftMouseButton), EDevice::Mouse);
	TestEqual(TEXT("a stick pushed over the gamepad"), DeviceAfterAnalog(EDevice::Mouse, EKeys::Gamepad_RightX, 0.8f), EDevice::Gamepad);
	TestEqual(TEXT("but not one drifting at rest"), DeviceAfterAnalog(EDevice::Mouse, EKeys::Gamepad_RightX, 0.1f), EDevice::Mouse);
	TestEqual(TEXT("the mouse moved the mouse"), DeviceAfterMouseMove(EDevice::Gamepad, 20.0f), EDevice::Mouse);
	TestEqual(TEXT("but not knocked a pixel"), DeviceAfterMouseMove(EDevice::Gamepad, 1.0f), EDevice::Gamepad);

	// The prompts name the key of the device in use.
	UCheeseStrikeInputSet* Set = UCheeseStrikeInputSet::Build(GetTransientPackage());
	TestEqual(TEXT("hold E to revive"), Set->PromptFor(Set->Interact, false), FString(TEXT("E")));
	TestEqual(TEXT("or hold LB"), Set->PromptFor(Set->Interact, true), FString(TEXT("LB")));
	TestEqual(TEXT("press Q for a streak"), Set->PromptFor(Set->Streak, false), FString(TEXT("Q")));
	TestEqual(TEXT("or the d-pad's down"), Set->PromptFor(Set->Streak, true), FString(TEXT("↓")));
	TestEqual(TEXT("press G for a grenade"), Set->PromptFor(Set->Grenade, false), FString(TEXT("G")));
	TestEqual(TEXT("or RB"), Set->PromptFor(Set->Grenade, true), FString(TEXT("RB")));
	TestEqual(TEXT("Escape calls a strike off"), Set->PromptFor(Set->Menu, false), FString(TEXT("ESC")));
	TestEqual(TEXT("the weapons are 1"), Set->PromptFor(Set->DrawPrimary, false), FString(TEXT("1")));
	TestEqual(TEXT("2"), Set->PromptFor(Set->DrawSidearm, false), FString(TEXT("2")));
	TestEqual(TEXT("and 3"), Set->PromptFor(Set->DrawKnife, false), FString(TEXT("3")));
	TestEqual(TEXT("or the d-pad's up"), Set->PromptFor(Set->DrawPrimary, true), FString(TEXT("↑")));
	TestEqual(TEXT("left"), Set->PromptFor(Set->DrawSidearm, true), FString(TEXT("←")));
	TestEqual(TEXT("and right"), Set->PromptFor(Set->DrawKnife, true), FString(TEXT("→")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePromptsNameEachPadsButtons, "CheeseStrike.Input.ThePromptsNameEachPadsButtons", GamepadTests::Flags)

bool FThePromptsNameEachPadsButtons::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeInput;
	UCheeseStrikeInputSet* Set = UCheeseStrikeInputSet::Build(GetTransientPackage());
	const auto Pad = [Set](const UInputAction* Action, EGlyphs Glyphs) { return Set->PromptFor(Action, true, Glyphs); };
	// A DualShock's or DualSense's: the shapes, the numbered shoulders, Options and the touchpad.
	TestEqual(TEXT("Cross jumps"), Pad(Set->Jump, EGlyphs::PlayStation), FString(TEXT("CROSS")));
	TestEqual(TEXT("Circle crouches"), Pad(Set->CrouchToggle, EGlyphs::PlayStation), FString(TEXT("CIRCLE")));
	TestEqual(TEXT("Square reloads"), Pad(Set->Reload, EGlyphs::PlayStation), FString(TEXT("SQUARE")));
	TestEqual(TEXT("Triangle changes weapon"), Pad(Set->NextWeapon, EGlyphs::PlayStation), FString(TEXT("TRIANGLE")));
	TestEqual(TEXT("L2 aims"), Pad(Set->Aim, EGlyphs::PlayStation), FString(TEXT("L2")));
	TestEqual(TEXT("R2 fires"), Pad(Set->Fire, EGlyphs::PlayStation), FString(TEXT("R2")));
	TestEqual(TEXT("L1 interacts"), Pad(Set->Interact, EGlyphs::PlayStation), FString(TEXT("L1")));
	TestEqual(TEXT("R1 throws"), Pad(Set->Grenade, EGlyphs::PlayStation), FString(TEXT("R1")));
	TestEqual(TEXT("L3 sprints"), Pad(Set->SprintToggle, EGlyphs::PlayStation), FString(TEXT("L3")));
	TestEqual(TEXT("the touchpad shows the scoreboard"), Pad(Set->Scoreboard, EGlyphs::PlayStation), FString(TEXT("TOUCHPAD")));
	TestEqual(TEXT("Options pauses"), Pad(Set->Menu, EGlyphs::PlayStation), FString(TEXT("OPTIONS")));
	TestEqual(TEXT("the d-pad is the d-pad"), Pad(Set->Streak, EGlyphs::PlayStation), FString(TEXT("↓")));
	// A Nintendo pad's: the same places, its own letters.
	TestEqual(TEXT("B jumps on a Nintendo pad"), Pad(Set->Jump, EGlyphs::Nintendo), FString(TEXT("B")));
	TestEqual(TEXT("ZR fires"), Pad(Set->Fire, EGlyphs::Nintendo), FString(TEXT("ZR")));
	TestEqual(TEXT("+ pauses"), Pad(Set->Menu, EGlyphs::Nintendo), FString(TEXT("+")));
	// An Xbox pad's, and Auto's, as ever; the keyboard's whatever the pad.
	TestEqual(TEXT("A jumps on an Xbox pad"), Pad(Set->Jump, EGlyphs::Xbox), FString(TEXT("A")));
	TestEqual(TEXT("and with Auto"), Pad(Set->Jump, EGlyphs::Auto), FString(TEXT("A")));
	TestEqual(TEXT("E is E"), Set->PromptFor(Set->Interact, false, EGlyphs::PlayStation), FString(TEXT("E")));
	// Which pad it is, by its name, when the engine can tell; the setting first.
	TestEqual(TEXT("a DualSense"), GlyphsOfDevice(TEXT("DualSense Wireless Controller")), EGlyphs::PlayStation);
	TestEqual(TEXT("a DualShock 4"), GlyphsOfDevice(TEXT("PS4 DualShock4")), EGlyphs::PlayStation);
	TestEqual(TEXT("a Switch Pro pad"), GlyphsOfDevice(TEXT("Nintendo Switch Pro Controller")), EGlyphs::Nintendo);
	TestEqual(TEXT("XInput is an Xbox pad"), GlyphsOfDevice(TEXT("XInputController Gamepad")), EGlyphs::Xbox);
	TestEqual(TEXT("Auto shows the pad's"), GlyphsToShow(EGlyphs::Auto, EGlyphs::PlayStation), EGlyphs::PlayStation);
	TestEqual(TEXT("or an Xbox pad's if it can't be told"), GlyphsToShow(EGlyphs::Auto, EGlyphs::Auto), EGlyphs::Xbox);
	TestEqual(TEXT("a pick shows what is picked"), GlyphsToShow(EGlyphs::Nintendo, EGlyphs::PlayStation), EGlyphs::Nintendo);
	TestEqual(TEXT("the settings' names"), FString(GlyphsName(EGlyphs::PlayStation)), FString(TEXT("PLAYSTATION")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLookStickTurnsOnACurve, "CheeseStrike.Input.TheLookStickTurnsOnACurve", GamepadTests::Flags)

bool FTheLookStickTurnsOnACurve::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeInput;
	float Edge = 0.0f;
	const FVector2D Full = StickTurn(FVector2D(1.0, 0.0), 1.0f / 60.0f, Edge);
	TestTrue(TEXT("held full over, it turns at the full rate"), FMath::IsNearlyEqual(Full.X * 60.0, static_cast<double>(StickYawDegreesPerSecond), 0.5));
	Edge = 0.0f;
	const FVector2D Half = StickTurn(FVector2D(0.5, 0.0), 1.0f / 60.0f, Edge);
	TestTrue(TEXT("half over, at a quarter of it: fine aim near the middle"), FMath::IsNearlyEqual(Half.X * 60.0, StickYawDegreesPerSecond * 0.25, 0.5));
	TestEqual(TEXT("and not at the edge"), Edge, 0.0f);
	Edge = 0.0f;
	const FVector2D Up = StickTurn(FVector2D(0.0, 1.0), 1.0f / 60.0f, Edge);
	TestTrue(TEXT("up and down more slowly than across"), Up.Y > 0.0 && Up.Y < Full.X);
	const FVector2D Diagonal = StickTurn(FVector2D(0.6, 0.6), 1.0f / 60.0f, Edge);
	TestTrue(TEXT("a diagonal keeps its direction"), Diagonal.X > 0.0 && Diagonal.Y > 0.0);
	// Held at the edge, the turn speeds up after a moment, for turning round.
	Edge = 0.0f;
	FVector2D Last;
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Last = StickTurn(FVector2D(1.0, 0.0), 1.0f / 60.0f, Edge);
	}
	TestTrue(TEXT("after a second at the edge it turns faster"), FMath::IsNearlyEqual(Last.X * 60.0, StickYawDegreesPerSecond * StickBoost, 0.5));
	const FVector2D Let = StickTurn(FVector2D(0.0, 0.0), 1.0f / 60.0f, Edge);
	TestTrue(TEXT("let go, it stops"), Let.IsZero() && Edge == 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheAimAssistIsLight, "CheeseStrike.Input.TheAimAssistIsLight", GamepadTests::Flags)

bool FTheAimAssistIsLight::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeAimAssist;
	const FTuning Tuning;
	const FVector Eye(0.0, 0.0, 160.0);
	// An enemy 10 m ahead, standing.
	const TArray<FTarget> Ahead = {{FVector(1000.0, 0.0, 100.0), FVector(1000.0, 0.0, 170.0)}};
	const FRotator OnThem(0.0, 0.0, 0.0);
	FAssist Help = Assist(Eye, OnThem, Ahead, false, false, 1.0f / 60.0f, Tuning);
	TestEqual(TEXT("on an enemy, the stick turns slower"), Help.TurnScale, Tuning.Slowdown);
	TestTrue(TEXT("a little, not much"), Tuning.Slowdown >= 0.4f && Tuning.Slowdown < 1.0f && Tuning.AimedSlowdown >= 0.3f);
	TestEqual(TEXT("down the sights slower still"), Assist(Eye, OnThem, Ahead, true, false, 1.0f / 60.0f, Tuning).TurnScale, Tuning.AimedSlowdown);
	TestTrue(TEXT("no pull while the stick is still: it never aims by itself"), Help.PullYaw == 0.0f && Help.PullPitch == 0.0f);
	// Just off them, turning: a pull toward them, no more than a few degrees a second.
	const FRotator Beside(0.0, 3.0, 0.0);
	Help = Assist(Eye, Beside, Ahead, false, true, 1.0f / 60.0f, Tuning);
	TestTrue(TEXT("3 degrees off at 10 m still holds"), Help.Strength > 0.0f && Help.TurnScale < 1.0f);
	TestTrue(TEXT("and turning, it pulls toward them"), Help.PullYaw < 0.0f);
	TestTrue(TEXT("lightly"), FMath::Abs(Help.PullYaw) <= Tuning.Pull / 60.0f + KINDA_SMALL_NUMBER);
	// Never past them.
	Help = Assist(Eye, FRotator(0.0, 0.05, 0.0), Ahead, false, true, 1.0f, Tuning);
	TestTrue(TEXT("and never past them"), FMath::Abs(Help.PullYaw) <= 0.05f + KINDA_SMALL_NUMBER);
	// Away from them, behind, or too far: nothing.
	TestEqual(TEXT("20 degrees off, nothing"), Assist(Eye, FRotator(0.0, 20.0, 0.0), Ahead, false, true, 1.0f / 60.0f, Tuning).TurnScale, 1.0f);
	TestEqual(TEXT("behind, nothing"), Assist(Eye, FRotator(0.0, 180.0, 0.0), Ahead, false, true, 1.0f / 60.0f, Tuning).Target, static_cast<int32>(INDEX_NONE));
	const TArray<FTarget> Far = {{FVector(Tuning.Range + 500.0, 0.0, 100.0), FVector(Tuning.Range + 500.0, 0.0, 170.0)}};
	TestEqual(TEXT("too far, nothing"), Assist(Eye, OnThem, Far, false, true, 1.0f / 60.0f, Tuning).TurnScale, 1.0f);
	TestEqual(TEXT("with nobody in view, nothing"), Assist(Eye, OnThem, {}, false, true, 1.0f / 60.0f, Tuning).TurnScale, 1.0f);
	// Up and down the body alike: the head and the hips hold as the chest does.
	TestTrue(TEXT("the head holds"), Assist(Eye, (FVector(1000.0, 0.0, 170.0) - Eye).Rotation(), Ahead, false, false, 1.0f / 60.0f, Tuning).Strength == 1.0f);
	// The nearer of two holds.
	const TArray<FTarget> Two = {{FVector(1000.0, 300.0, 100.0), FVector(1000.0, 300.0, 170.0)}, {FVector(1000.0, 0.0, 100.0), FVector(1000.0, 0.0, 170.0)}};
	TestEqual(TEXT("the one under the crosshair is the one helped toward"), Assist(Eye, OnThem, Two, false, true, 1.0f / 60.0f, Tuning).Target, 1);
	// Its strength fades out past the radius.
	TestEqual(TEXT("inside the radius, all of it"), Strength(1.0f, 2.0f, Tuning), 1.0f);
	TestTrue(TEXT("past it, less"), Strength(3.0f, 2.0f, Tuning) < 1.0f && Strength(3.0f, 2.0f, Tuning) > 0.0f);
	TestEqual(TEXT("and none past its falloff"), Strength(2.0f * Tuning.Falloff + 0.1f, 2.0f, Tuning), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMenuKeepsItsFocus, "CheeseStrike.Rules.TheMenuKeepsItsFocus", GamepadTests::Flags)

bool FTheMenuKeepsItsFocus::RunTest(const FString& Parameters)
{
	using CheeseStrikeMenu::FocusAfterRedraw;
	const TArray<FString> Names = {TEXT("Dropdown 1"), TEXT("Dropdown 2"), TEXT("LEAVE"), TEXT("START THE MATCH")};
	TestEqual(TEXT("a page shown afresh focuses its first item"), FocusAfterRedraw(Names, FString(), 0), 0);
	TestEqual(TEXT("drawn again, the same item"), FocusAfterRedraw(Names, TEXT("Dropdown 2"), 1), 1);
	TestEqual(TEXT("found by its name wherever it is now"), FocusAfterRedraw({TEXT("Dropdown 1"), TEXT("Dropdown 2"), TEXT("Dropdown 3"), TEXT("LEAVE")}, TEXT("LEAVE"), 2), 3);
	TestEqual(TEXT("an item gone (Start became Cancel): the one in its place"), FocusAfterRedraw({TEXT("Dropdown 1"), TEXT("Dropdown 2"), TEXT("LEAVE"), TEXT("CANCEL")}, TEXT("START THE MATCH"), 3), 3);
	TestEqual(TEXT("or the last, if the page is shorter"), FocusAfterRedraw({TEXT("LEAVE")}, TEXT("START THE MATCH"), 3), 0);
	TestEqual(TEXT("a page with nothing to focus has none"), FocusAfterRedraw({}, TEXT("LEAVE"), 0), static_cast<int32>(INDEX_NONE));

	// Moving the focus goes to the nearest item that way on the screen, keeping to a row or a column: the options' tabs, two
	// sliders, a row of OFF and ON, and Back at the bottom.
	using CheeseStrikeMenu::NextInDirection;
	const TArray<FSlateRect> Items = {FSlateRect(32, 92, 112, 112), FSlateRect(117, 92, 163, 112), FSlateRect(200, 130, 420, 150), FSlateRect(200, 157, 420, 177),
		FSlateRect(198, 226, 236, 246), FSlateRect(240, 226, 275, 246), FSlateRect(32, 642, 232, 674)};
	TestEqual(TEXT("down from the first tab, the first slider"), NextInDirection(Items[0], Items, EUINavigation::Down), 2);
	TestEqual(TEXT("right from it, the next tab"), NextInDirection(Items[0], Items, EUINavigation::Right), 1);
	TestEqual(TEXT("left from it, nothing"), NextInDirection(Items[0], Items, EUINavigation::Left), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("down a slider, the next"), NextInDirection(Items[2], Items, EUINavigation::Down), 3);
	TestEqual(TEXT("down from the second, the row of choices"), NextInDirection(Items[3], Items, EUINavigation::Down), 4);
	TestEqual(TEXT("along the row"), NextInDirection(Items[4], Items, EUINavigation::Right), 5);
	TestEqual(TEXT("down from it, Back"), NextInDirection(Items[5], Items, EUINavigation::Down), 6);
	TestEqual(TEXT("and up from Back, the choice over it"), NextInDirection(Items[6], Items, EUINavigation::Up), 4);
	TestEqual(TEXT("down from Back, nothing"), NextInDirection(Items[6], Items, EUINavigation::Down), static_cast<int32>(INDEX_NONE));

	// The menu itself: its pages' items in order, the first focused, and the focus kept through a page drawn again.
	UCheeseStrikeSettings* Settings = UCheeseStrikeSettings::Get();
	if (!TestNotNull(TEXT("the settings"), Settings))
	{
		return false;
	}
	const TSharedRef<SCheeseStrikeMenu> Menu = SNew(SCheeseStrikeMenu).Controller(nullptr);
	Menu->Show(EMenuPage::Main);
	TestEqual(TEXT("the front end's items"), Menu->GetFocusableNames(), TArray<FString>({TEXT("HOST A MATCH"), TEXT("FIND A MATCH"), TEXT("SETTINGS"), TEXT("CREDITS"), TEXT("QUIT")}));
	TestEqual(TEXT("the first has the focus"), Menu->GetFocusName(), FString(TEXT("HOST A MATCH")));
	Menu->FocusItem(TEXT("SETTINGS"));
	Menu->Show(EMenuPage::Options);
	TestEqual(TEXT("the settings' first item is their first tab"), Menu->GetFocusName(), FString(TEXT("CONTROLS")));
	TestEqual(TEXT("the controls' tab has nothing to focus but the tabs and Back"), Menu->GetFocusableNames(), TArray<FString>({TEXT("CONTROLS"), TEXT("AIMING"), TEXT("SOUND"), TEXT("GRAPHICS"), TEXT("BACK")}));
	Menu->SetOptionsTab(EOptionsTab::Aiming);
	Menu->Refresh();
	const TArray<FString> Options = Menu->GetFocusableNames();
	TestTrue(TEXT("the sliders, the choices and Back can all take it"), Options.Contains(TEXT("Slider")) && Options.Contains(TEXT("Slider #2")) && Options.Contains(TEXT("ON #2")) && Options.Last() == TEXT("BACK"));
	// Picking the aim assist's choice draws the page again: the focus stays on the choice.
	const bool bWasOn = Settings->IsAimAssistOn();
	Menu->FocusItem(TEXT("ON #2"));
	Settings->SetAimAssistOn(!bWasOn);
	Menu->Refresh();
	TestEqual(TEXT("drawn again, the same item has the focus"), Menu->GetFocusName(), FString(TEXT("ON #2")));
	Settings->SetAimAssistOn(bWasOn);
	Menu->Refresh();
	// Back on the front end, the focus is where it was left.
	Menu->Show(EMenuPage::Main);
	TestEqual(TEXT("back on the front end, on Settings again"), Menu->GetFocusName(), FString(TEXT("SETTINGS")));
	Menu->Show(EMenuPage::Credits);
	TestEqual(TEXT("the credits' Back"), Menu->GetFocusName(), FString(TEXT("BACK")));
	return true;
}

#if WITH_EDITOR

namespace GamepadTests
{
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);

	TArray<FString> SlotKeys(int32 Player)
	{
		TArray<FString> Keys;
		const ACheeseStrikePlayerController* Controller = PlayTesting::Controller(Player);
		const ACheeseStrikeHUD* Hud = Controller != nullptr ? Cast<ACheeseStrikeHUD>(Controller->GetHUD()) : nullptr;
		for (const ACheeseStrikeHUD::FWeaponSlot& Slot : Hud != nullptr ? Hud->WeaponSlots(PlayTesting::Character(Player)) : TArray<ACheeseStrikeHUD::FWeaponSlot>())
		{
			Keys.Add(Slot.Key);
		}
		return Keys;
	}

	/** Turns `Player`'s view onto `Target`'s chest. */
	void LookAt(int32 Player, int32 Target)
	{
		ACheeseStrikePlayerController* Controller = PlayTesting::Controller(Player);
		const ACheeseStrikeCharacter* Them = PlayTesting::CharacterOf(Target, Player);
		if (Controller != nullptr && Them != nullptr && Controller->PlayerCameraManager != nullptr)
		{
			Controller->SetControlRotation((Them->GetActorLocation() + FVector(0.0, 0.0, 30.0) - Controller->PlayerCameraManager->GetCameraLocation()).Rotation());
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAGamepadPlays, "CheeseStrike.Play.AGamepadPlays", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAGamepadPlays::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	StartPlaying({2});
	PutAt(0, GamepadTests::HostSpot, -90.0f);
	PutAt(1, GamepadTests::JoinerSpot, 90.0f);
	Wait(0.5f);

	// The prompts follow the device: the weapons' keys over the ammunition, the d-pad's with a gamepad.
	Play(0, TEXT("device gamepad"));
	Then([this]()
	{
		TestEqual(TEXT("with a gamepad, the d-pad's arrows by the weapons"), GamepadTests::SlotKeys(0), TArray<FString>({TEXT("↑"), TEXT("←"), TEXT("→")}));
		TestEqual(TEXT("and LB to revive"), UCheeseStrikeInputSet::Prompt(Controller(0), &UCheeseStrikeInputSet::Interact), FString(TEXT("LB")));
	});
	Play(0, TEXT("device keyboard"));
	Then([this]()
	{
		TestEqual(TEXT("with the keyboard, the numbers"), GamepadTests::SlotKeys(0), TArray<FString>({TEXT("1"), TEXT("2"), TEXT("3")}));
		TestEqual(TEXT("and E"), UCheeseStrikeInputSet::Prompt(Controller(0), &UCheeseStrikeInputSet::Interact), FString(TEXT("E")));
		GamepadTests::LookAt(0, 1);
	});

	// The aim assist: turning slowly with the stick over the other player, the turn slows and pulls toward them.
	Play(0, TEXT("hold Look 0.3 0\nwait 0.15\nrelease Look"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		TestTrue(TEXT("over an enemy, the stick is held"), Host != nullptr && Host->GetAimAssist().Strength > 0.5f && Host->GetAimAssist().TurnScale < 1.0f);
		TestTrue(TEXT("and pulled toward them"), Host != nullptr && (Host->GetAimAssist().PullYaw != 0.0f || Host->GetAimAssist().PullPitch != 0.0f));
		if (Controller(0) != nullptr)
		{
			Controller(0)->SetControlRotation(Controller(0)->GetControlRotation() + FRotator(0.0, 90.0, 0.0));
		}
	});
	Play(0, TEXT("hold Look 0.3 0\nwait 0.15\nrelease Look"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		TestTrue(TEXT("looking away, nothing"), Host != nullptr && Host->GetAimAssist().Strength == 0.0f && Host->GetAimAssist().TurnScale == 1.0f);
	});

	// Up and down: the stick turns the view the other way to the mouse for the same sign of its value, as a pad's stick
	// and the mouse report up oppositely; unturned, a pad's up looked down while the mouse's looked up.
	TSharedRef<double> Pitch = MakeShared<double>(0.0);
	TSharedRef<double> MouseTurn = MakeShared<double>(0.0);
	const auto ReadPitch = [this]() { return Controller(0) != nullptr ? FRotator::NormalizeAxis(Controller(0)->GetControlRotation().Pitch) : 0.0; };
	Then([Pitch, ReadPitch]() { *Pitch = ReadPitch(); });
	Play(0, TEXT("hold MouseLook 0 1\nwait 0.1\nrelease MouseLook\nwait 0.1"));
	Then([Pitch, MouseTurn, ReadPitch]() { *MouseTurn = ReadPitch() - *Pitch; *Pitch = ReadPitch(); });
	Play(0, TEXT("hold Look 0 1\nwait 0.3\nrelease Look\nwait 0.1"));
	Then([this, Pitch, MouseTurn, ReadPitch]()
	{
		const double StickTurn = ReadPitch() - *Pitch;
		TestTrue(FString::Printf(TEXT("the mouse turned the view up or down (%.2f)"), *MouseTurn), FMath::Abs(*MouseTurn) > 0.01);
		TestTrue(FString::Printf(TEXT("the stick turns it the other way for the same sign (%.2f against the mouse's %.2f)"), StickTurn, *MouseTurn), StickTurn * *MouseTurn < 0.0);
	});

	// A gamepad's sprint: pressed, it goes on while the stick is pushed, and stops when it is let go.
	Play(0, TEXT("hold Move 0 1\ntap SprintToggle\nwait 0.4"));
	Then([this]()
	{
		const UCheeseStrikeMovementComponent* Movement = Character(0) != nullptr ? Character(0)->GetCheeseStrikeMovement() : nullptr;
		TestTrue(TEXT("pressed once, the sprint goes on without holding it"), Movement != nullptr && Movement->WantsToSprint());
	});
	Play(0, TEXT("release Move\nwait 0.3"));
	Then([this]()
	{
		const UCheeseStrikeMovementComponent* Movement = Character(0) != nullptr ? Character(0)->GetCheeseStrikeMovement() : nullptr;
		TestTrue(TEXT("and stops with the stick let go"), Movement != nullptr && !Movement->WantsToSprint());
	});
	// B crouches and stands.
	Play(0, TEXT("tap CrouchToggle\nwait 0.4"));
	Then([this]() { TestTrue(TEXT("B crouches"), Character(0) != nullptr && Character(0)->bIsCrouched); });
	Play(0, TEXT("tap CrouchToggle\nwait 0.4"));
	Then([this]() { TestTrue(TEXT("and B again stands"), Character(0) != nullptr && !Character(0)->bIsCrouched); });
	StopPlaying();
	return true;
}

#endif

#endif
