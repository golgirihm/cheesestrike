// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeControls.h"
#include "InputAction.h"
#include "UObject/UnrealType.h"
#include "Input/CheeseStrikeInput.h"

namespace CheeseStrikeControls
{
	namespace
	{
		/** The page's rows, in its order: a name, and the input set's properties it shows. */
		struct FKnownRow
		{
			const TCHAR* Name;
			TArray<const TCHAR*> Properties;
		};

		const TArray<FKnownRow>& KnownControlRows()
		{
			static const TArray<FKnownRow> Known = {
				{TEXT("Move"), {TEXT("Move")}},
				{TEXT("Look"), {TEXT("MouseLook"), TEXT("StickLook")}},
				{TEXT("Jump"), {TEXT("Jump")}},
				{TEXT("Sprint"), {TEXT("Sprint"), TEXT("SprintToggle")}},
				{TEXT("Crouch / slide"), {TEXT("Crouch"), TEXT("CrouchToggle")}},
				{TEXT("Fire"), {TEXT("Fire")}},
				{TEXT("Aim"), {TEXT("Aim")}},
				{TEXT("Reload"), {TEXT("Reload")}},
				{TEXT("Primary weapon"), {TEXT("DrawPrimary")}},
				{TEXT("Sidearm"), {TEXT("DrawSidearm")}},
				{TEXT("Knife"), {TEXT("DrawKnife")}},
				{TEXT("Next weapon"), {TEXT("NextWeapon")}},
				{TEXT("Weapon before"), {TEXT("PreviousWeapon")}},
				{TEXT("Grenade"), {TEXT("Grenade"), TEXT("ThrowGrenade")}},
				{TEXT("Call in a streak"), {TEXT("Streak")}},
				{TEXT("Interact"), {TEXT("Interact")}},
				{TEXT("Scoreboard"), {TEXT("Scoreboard")}},
				{TEXT("Pause / back"), {TEXT("Menu")}},
			};
			return Known;
		}

		/** The order keys are written in a cell: the letters the hand rests on, then the arrows. */
		int32 LabelRank(const FString& Label)
		{
			static const TArray<FString> Order = {TEXT("W"), TEXT("A"), TEXT("S"), TEXT("D"), TEXT("↑"), TEXT("←"), TEXT("↓"), TEXT("→")};
			const int32 Found = Order.IndexOfByKey(Label);
			return Found == INDEX_NONE ? Order.Num() : Found;
		}

		/** A property's name as words: "ThrowGrenade" as "Throw grenade". */
		FString PropertyWords(const FString& Name)
		{
			FString Out;
			for (int32 Index = 0; Index < Name.Len(); ++Index)
			{
				const TCHAR Each = Name[Index];
				if (Index > 0 && FChar::IsUpper(Each))
				{
					Out.AppendChar(TEXT(' '));
					Out.AppendChar(FChar::ToLower(Each));
				}
				else
				{
					Out.AppendChar(Each);
				}
			}
			return Out;
		}

		const UInputAction* InputActionNamed(const UCheeseStrikeInputSet& Set, const TCHAR* Property)
		{
			const FObjectProperty* Found = CastField<FObjectProperty>(UCheeseStrikeInputSet::StaticClass()->FindPropertyByName(Property));
			return Found != nullptr && Found->PropertyClass->IsChildOf(UInputAction::StaticClass()) ? Cast<UInputAction>(Found->GetObjectPropertyValue_InContainer(&Set)) : nullptr;
		}

		FRow ControlRowFor(const UCheeseStrikeInputSet& Set, const FString& Name, const TArray<const UInputAction*>& Actions)
		{
			FRow Row;
			Row.Action = Name;
			Row.Actions = Actions;
			TArray<FString> Keyboard;
			TArray<FString> Xbox;
			TArray<FString> Nintendo;
			for (const UInputAction* Action : Actions)
			{
				for (const FKey& Key : Set.KeysFor(Action))
				{
					if (Key.IsGamepadKey())
					{
						Xbox.Add(PadLabel(Key, EPad::Xbox));
						Nintendo.Add(PadLabel(Key, EPad::Nintendo));
					}
					else
					{
						Keyboard.Add(KeyboardLabel(Key));
					}
				}
			}
			Row.Keyboard = JoinLabels(Keyboard);
			Row.Xbox = JoinLabels(Xbox);
			Row.Nintendo = JoinLabels(Nintendo);
			return Row;
		}
	}

	FString KeyboardLabel(const FKey& Key)
	{
		static const TMap<FKey, FString> Labels = {
			{EKeys::Mouse2D, TEXT("MOUSE")}, {EKeys::MouseScrollDown, TEXT("WHEEL DOWN")}, {EKeys::MouseScrollUp, TEXT("WHEEL UP")},
			{EKeys::MiddleMouseButton, TEXT("MMB")}, {EKeys::ThumbMouseButton, TEXT("MOUSE 4")}, {EKeys::ThumbMouseButton2, TEXT("MOUSE 5")},
			{EKeys::Up, TEXT("↑")}, {EKeys::Down, TEXT("↓")}, {EKeys::Left, TEXT("←")}, {EKeys::Right, TEXT("→")},
			{EKeys::RightShift, TEXT("SHIFT")}, {EKeys::RightControl, TEXT("CTRL")}, {EKeys::LeftAlt, TEXT("ALT")}, {EKeys::Enter, TEXT("ENTER")},
			{EKeys::Five, TEXT("5")}, {EKeys::Six, TEXT("6")},
		};
		if (const FString* Label = Labels.Find(Key))
		{
			return *Label;
		}
		return CheeseStrikeInput::KeyLabel(Key);
	}

	FString PadLabel(const FKey& Key, EPad Pad)
	{
		// The same place on both pads: the face buttons' letters are swapped round on a Nintendo one, the triggers are ZL
		// and ZR, the bumpers L and R, and View and Menu are − and +.
		static const TMap<FKey, FString> Nintendo = {
			{EKeys::Gamepad_FaceButton_Bottom, TEXT("B")}, {EKeys::Gamepad_FaceButton_Right, TEXT("A")},
			{EKeys::Gamepad_FaceButton_Left, TEXT("Y")}, {EKeys::Gamepad_FaceButton_Top, TEXT("X")},
			{EKeys::Gamepad_LeftShoulder, TEXT("L")}, {EKeys::Gamepad_RightShoulder, TEXT("R")},
			{EKeys::Gamepad_LeftTrigger, TEXT("ZL")}, {EKeys::Gamepad_RightTrigger, TEXT("ZR")},
			{EKeys::Gamepad_LeftThumbstick, TEXT("PRESS L STICK")}, {EKeys::Gamepad_RightThumbstick, TEXT("PRESS R STICK")},
			{EKeys::Gamepad_Special_Left, TEXT("−")}, {EKeys::Gamepad_Special_Right, TEXT("+")},
			{EKeys::Gamepad_Left2D, TEXT("L STICK")}, {EKeys::Gamepad_Right2D, TEXT("R STICK")},
		};
		static const TMap<FKey, FString> Xbox = {
			{EKeys::Gamepad_LeftThumbstick, TEXT("PRESS LS")}, {EKeys::Gamepad_RightThumbstick, TEXT("PRESS RS")},
			{EKeys::Gamepad_Left2D, TEXT("LEFT STICK")}, {EKeys::Gamepad_Right2D, TEXT("RIGHT STICK")},
		};
		if (const FString* Label = (Pad == EPad::Nintendo ? Nintendo : Xbox).Find(Key))
		{
			return *Label;
		}
		// The d-pad's arrows, and the rest, as the prompts on the screen write them (an Xbox pad's).
		return CheeseStrikeInput::KeyLabel(Key);
	}

	FString JoinLabels(TArray<FString> Labels)
	{
		TArray<FString> Once;
		for (const FString& Label : Labels)
		{
			if (!Label.IsEmpty())
			{
				Once.AddUnique(Label);
			}
		}
		Once.StableSort([](const FString& A, const FString& B) { return LabelRank(A) < LabelRank(B); });
		// WASD and the arrows each as one: "WASD / ↑←↓→".
		FString Out;
		for (int32 Index = 0; Index < Once.Num(); ++Index)
		{
			const bool bRun = Index > 0 && LabelRank(Once[Index]) < 8 && LabelRank(Once[Index - 1]) < 8 && (LabelRank(Once[Index]) < 4) == (LabelRank(Once[Index - 1]) < 4);
			Out += Index == 0 ? Once[Index] : (bRun ? Once[Index] : TEXT(" / ") + Once[Index]);
		}
		return Out;
	}

	TArray<const UInputAction*> AllActions(const UCheeseStrikeInputSet& Set)
	{
		TArray<const UInputAction*> Actions;
		for (TFieldIterator<FObjectProperty> It(UCheeseStrikeInputSet::StaticClass()); It; ++It)
		{
			if (It->PropertyClass->IsChildOf(UInputAction::StaticClass()))
			{
				if (const UInputAction* Action = Cast<UInputAction>(It->GetObjectPropertyValue_InContainer(&Set)))
				{
					Actions.Add(Action);
				}
			}
		}
		return Actions;
	}

	TArray<FRow> Rows(const UCheeseStrikeInputSet& Set)
	{
		TArray<FRow> Rows;
		TSet<const UInputAction*> Shown;
		for (const FKnownRow& Known : KnownControlRows())
		{
			TArray<const UInputAction*> Actions;
			for (const TCHAR* Property : Known.Properties)
			{
				if (const UInputAction* Action = InputActionNamed(Set, Property))
				{
					Actions.Add(Action);
					Shown.Add(Action);
				}
			}
			if (Actions.Num() > 0)
			{
				Rows.Add(ControlRowFor(Set, Known.Name, Actions));
			}
		}
		// An action the page doesn't know of yet, named after itself.
		for (TFieldIterator<FObjectProperty> It(UCheeseStrikeInputSet::StaticClass()); It; ++It)
		{
			const UInputAction* Action = It->PropertyClass->IsChildOf(UInputAction::StaticClass()) ? Cast<UInputAction>(It->GetObjectPropertyValue_InContainer(&Set)) : nullptr;
			if (Action != nullptr && !Shown.Contains(Action))
			{
				Rows.Add(ControlRowFor(Set, PropertyWords(It->GetName()), {Action}));
			}
		}
		return Rows;
	}
}
