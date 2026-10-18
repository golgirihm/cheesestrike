// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The keys that drive the game, as the Godot game had them.

#include "Misc/AutomationTest.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Input/CheeseStrikeInput.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	bool Has(const TArray<FKey>& Keys, const FKey& Key)
	{
		return Keys.Contains(Key);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheKeysAreTheGodotGames, "CheeseStrike.Input.TheKeysAreTheGodotGames", Flags)

bool FTheKeysAreTheGodotGames::RunTest(const FString& Parameters)
{
	UCheeseStrikeInputSet* Set = UCheeseStrikeInputSet::Build(GetTransientPackage());
	if (!TestNotNull(TEXT("a set is made"), Set))
	{
		return false;
	}
	TestNotNull(TEXT("with a context"), Set->Context.Get());

	const TArray<FKey> Move = Set->KeysFor(Set->Move);
	for (const FKey& Key : {EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right, EKeys::Gamepad_Left2D})
	{
		TestTrue(FString::Printf(TEXT("%s moves"), *Key.ToString()), Has(Move, Key));
	}
	TestEqual(TEXT("and nothing else"), Move.Num(), 9);
	TestEqual(TEXT("moving is two axes"), Set->Move->ValueType, EInputActionValueType::Axis2D);

	TestTrue(TEXT("the mouse looks"), Has(Set->KeysFor(Set->MouseLook), EKeys::Mouse2D));
	TestTrue(TEXT("the right stick looks"), Has(Set->KeysFor(Set->StickLook), EKeys::Gamepad_Right2D));
	TestTrue(TEXT("space jumps"), Has(Set->KeysFor(Set->Jump), EKeys::SpaceBar));
	TestTrue(TEXT("so does the bottom face button"), Has(Set->KeysFor(Set->Jump), EKeys::Gamepad_FaceButton_Bottom));
	TestTrue(TEXT("shift sprints"), Has(Set->KeysFor(Set->Sprint), EKeys::LeftShift));
	TestTrue(TEXT("so does pressing the left stick"), Has(Set->KeysFor(Set->Sprint), EKeys::Gamepad_LeftThumbstick));
	TestTrue(TEXT("C crouches"), Has(Set->KeysFor(Set->Crouch), EKeys::C));
	TestTrue(TEXT("so does control"), Has(Set->KeysFor(Set->Crouch), EKeys::LeftControl));
	TestTrue(TEXT("and the right face button"), Has(Set->KeysFor(Set->Crouch), EKeys::Gamepad_FaceButton_Right));
	TestEqual(TEXT("jumping is a button"), Set->Jump->ValueType, EInputActionValueType::Boolean);

	// No key does two things.
	TArray<FKey> All;
	for (const UInputAction* Action : {Set->Jump.Get(), Set->Sprint.Get(), Set->Crouch.Get()})
	{
		for (const FKey& Key : Set->KeysFor(Action))
		{
			TestFalse(FString::Printf(TEXT("%s is bound once"), *Key.ToString()), All.Contains(Key));
			All.Add(Key);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForwardIsForwardAndUpIsUp, "CheeseStrike.Input.ForwardIsForwardAndUpIsUp", Flags)

bool FForwardIsForwardAndUpIsUp::RunTest(const FString& Parameters)
{
	UCheeseStrikeInputSet* Set = UCheeseStrikeInputSet::Build(GetTransientPackage());
	// W and S go onto the Y axis, S the other way; D is X as it is and A the other way.
	for (const FEnhancedActionKeyMapping& Mapping : Set->Context->GetMappings())
	{
		bool bSwizzled = false;
		bool bNegatedX = false;
		bool bNegatedY = false;
		for (const UInputModifier* Modifier : Mapping.Modifiers)
		{
			if (Modifier->IsA<UInputModifierSwizzleAxis>())
			{
				bSwizzled = true;
			}
			else if (const UInputModifierNegate* Negate = Cast<UInputModifierNegate>(Modifier))
			{
				bNegatedX |= Negate->bX;
				bNegatedY |= Negate->bY;
			}
		}
		const FString Name = Mapping.Key.ToString();
		if (Mapping.Action == Set->Move)
		{
			if (Mapping.Key == EKeys::W || Mapping.Key == EKeys::Up)
			{
				TestTrue(Name + TEXT(" is forward"), bSwizzled && !bNegatedX);
			}
			else if (Mapping.Key == EKeys::S || Mapping.Key == EKeys::Down)
			{
				TestTrue(Name + TEXT(" is back"), bSwizzled && bNegatedX);
			}
			else if (Mapping.Key == EKeys::D || Mapping.Key == EKeys::Right)
			{
				TestTrue(Name + TEXT(" is right"), !bSwizzled && !bNegatedX);
			}
			else if (Mapping.Key == EKeys::A || Mapping.Key == EKeys::Left)
			{
				TestTrue(Name + TEXT(" is left"), !bSwizzled && bNegatedX);
			}
			else
			{
				TestTrue(Name + TEXT(" is as it comes"), !bSwizzled && !bNegatedX);
			}
		}
		else if (Mapping.Action == Set->MouseLook || Mapping.Action == Set->StickLook)
		{
			// The engine's pitch is up positive, and so is the mouse's Y; the
			// controller takes pitch with up positive after one negation.
			TestTrue(Name + TEXT(" looks up for up"), bNegatedY && !bNegatedX);
		}
	}
	TestTrue(TEXT("the mouse turns a few tenths of a degree a count"), CheeseStrikeInput::MouseDegreesPerCount * 0.07f > 0.1f && CheeseStrikeInput::MouseDegreesPerCount * 0.07f < 0.3f);
	TestTrue(TEXT("a stick turns round in a few seconds"), CheeseStrikeInput::StickYawDegreesPerSecond >= 90.0f && CheeseStrikeInput::StickYawDegreesPerSecond <= 360.0f);
	return true;
}

#endif
