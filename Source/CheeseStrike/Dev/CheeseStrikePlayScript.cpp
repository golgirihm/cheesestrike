// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePlayScript.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerController.h"
#include "Input/CheeseStrikeInput.h"

UCheeseStrikePlayScript* UCheeseStrikePlayScript::FromCommandLine(ACheeseStrikePlayerController* InController)
{
	FString Path;
	if (InController == nullptr || !InController->IsLocalPlayerController() || !FParse::Value(FCommandLine::Get(), TEXT("PlayScript="), Path, false))
	{
		return nullptr;
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogCheeseStrike, Error, TEXT("PlayScript: can't read %s"), *Path);
		return nullptr;
	}
	UCheeseStrikePlayScript* Script = FromText(InController, InController, Text);
	UE_LOG(LogCheeseStrike, Display, TEXT("PlayScript: %d commands from %s"), Script->NumCommands(), *Path);
	return Script;
}

UCheeseStrikePlayScript* UCheeseStrikePlayScriptSubsystem::ScriptFor(ACheeseStrikePlayerController* Controller)
{
	if (Current == nullptr && !bAskedCommandLine)
	{
		bAskedCommandLine = true;
		Current = UCheeseStrikePlayScript::FromCommandLine(Controller);
		if (Current != nullptr)
		{
			// Owned by the game, not by a controller that a travel will take away.
			Current->Rename(nullptr, this);
		}
	}
	if (Current != nullptr)
	{
		Current->Attach(Controller);
	}
	return Current;
}

UCheeseStrikePlayScript* UCheeseStrikePlayScript::FromText(UObject* Outer, ACheeseStrikePlayerController* InController, const FString& Text)
{
	UCheeseStrikePlayScript* Script = NewObject<UCheeseStrikePlayScript>(Outer);
	Script->Controller = InController;
	Script->Load(Text);
	return Script;
}

int32 UCheeseStrikePlayScript::Load(const FString& Text)
{
	Commands.Empty();
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	for (FString& Line : Lines)
	{
		Line.TrimStartAndEndInline();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}
		FCommand Command;
		TArray<FString> Words;
		Line.ParseIntoArrayWS(Words);
		Command.Verb = Words[0].ToLower();
		Words.RemoveAt(0);
		Command.Args = Words;
		Commands.Add(Command);
	}
	return Commands.Num();
}

void UCheeseStrikePlayScript::TakeOver(const UCheeseStrikePlayScript* Previous)
{
	if (Previous != nullptr)
	{
		Held = Previous->Held;
		Tapped = Previous->Tapped;
	}
}

void UCheeseStrikePlayScript::Advance(float DeltaSeconds)
{
	if (Waiting > 0.0f)
	{
		Waiting -= DeltaSeconds;
	}
	while (Waiting <= 0.0f && Next < Commands.Num())
	{
		const FCommand& Command = Commands[Next];
		++Next;
		if (!Run(Command))
		{
			break;
		}
	}
	// Taps let go after a few frames.
	for (auto It = Tapped.CreateIterator(); It; ++It)
	{
		if (--It.Value() <= 0)
		{
			Held.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
	InjectHeld();
}

bool UCheeseStrikePlayScript::Run(const FCommand& Command)
{
	UE_LOG(LogCheeseStrike, Display, TEXT("PlayScript: %s"), *Describe(Command));
	const auto Number = [&Command](int32 Index, float Otherwise) { return Command.Args.IsValidIndex(Index) ? FCString::Atof(*Command.Args[Index]) : Otherwise; };
	const FString Name = Command.Args.IsValidIndex(0) ? Command.Args[0] : FString();
	if (Command.Verb == TEXT("wait"))
	{
		Waiting = Number(0, 1.0f);
		return false;
	}
	if (Command.Verb == TEXT("hold") || Command.Verb == TEXT("tap"))
	{
		if (ActionNamed(Name) == nullptr)
		{
			UE_LOG(LogCheeseStrike, Error, TEXT("PlayScript: no action called %s"), *Name);
			return true;
		}
		Held.Add(Name, FVector2D(Number(1, 1.0f), Number(2, 0.0f)));
		if (Command.Verb == TEXT("tap"))
		{
			Tapped.Add(Name, 3);
		}
	}
	else if (Command.Verb == TEXT("release"))
	{
		Held.Remove(Name);
		Tapped.Remove(Name);
	}
	else if (Command.Verb == TEXT("look"))
	{
		// Set outright: input added here, after the controller's own tick, would be dropped before it was used.
		const FRotator Now = Controller->GetControlRotation();
		Controller->SetControlRotation(FRotator(FMath::Clamp(FRotator::NormalizeAxis(Now.Pitch) + Number(1, 0.0f), -70.0f, 80.0f), Now.Yaw + Number(0, 0.0f), 0.0f));
	}
	else if (Command.Verb == TEXT("shot"))
	{
		// With the interface in it: the menus are drawn in Slate, over the view.
		FScreenshotRequest::RequestScreenshot(Name + TEXT(".png"), true, false);
		// Taken at the end of this frame: nothing more this frame, or it would be in the picture.
		Waiting = 0.05f;
		return false;
	}
	else if (Command.Verb == TEXT("state"))
	{
		if (ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(Controller->GetPawn()))
		{
			Character->CheeseStrikeState();
		}
	}
	else if (Command.Verb == TEXT("log"))
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("PlayScript says: %s"), *FString::Join(Command.Args, TEXT(" ")));
	}
	else if (Command.Verb == TEXT("cmd"))
	{
		Controller->ConsoleCommand(FString::Join(Command.Args, TEXT(" ")));
	}
	else if (Command.Verb == TEXT("quit"))
	{
		Held.Empty();
		UKismetSystemLibrary::QuitGame(Controller, Controller, EQuitPreference::Quit, false);
	}
	else
	{
		UE_LOG(LogCheeseStrike, Error, TEXT("PlayScript: unknown command %s"), *Command.Verb);
	}
	return true;
}

void UCheeseStrikePlayScript::InjectHeld()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
	if (Subsystem == nullptr)
	{
		return;
	}
	for (const TPair<FString, FVector2D>& Input : Held)
	{
		const UInputAction* Action = ActionNamed(Input.Key);
		if (Action == nullptr)
		{
			continue;
		}
		if (Action->ValueType == EInputActionValueType::Boolean)
		{
			Subsystem->InjectInputForAction(Action, FInputActionValue(true), {}, {});
		}
		else
		{
			Subsystem->InjectInputForAction(Action, FInputActionValue(Input.Value), {}, {});
		}
	}
}

const UInputAction* UCheeseStrikePlayScript::ActionNamed(const FString& Name) const
{
	const UCheeseStrikeInputSet* Input = UCheeseStrikeInputSet::Get(Controller);
	if (Input == nullptr)
	{
		return nullptr;
	}
	const FString Lower = Name.ToLower();
	if (Lower == TEXT("move")) return Input->Move;
	if (Lower == TEXT("look")) return Input->StickLook;
	if (Lower == TEXT("jump")) return Input->Jump;
	if (Lower == TEXT("sprint")) return Input->Sprint;
	if (Lower == TEXT("crouch")) return Input->Crouch;
	if (Lower == TEXT("fire")) return Input->Fire;
	if (Lower == TEXT("aim")) return Input->Aim;
	if (Lower == TEXT("reload")) return Input->Reload;
	if (Lower == TEXT("scoreboard")) return Input->Scoreboard;
	if (Lower == TEXT("menu")) return Input->Menu;
	return nullptr;
}

FString UCheeseStrikePlayScript::Describe(const FCommand& Command) const
{
	return Command.Verb + (Command.Args.Num() > 0 ? TEXT(" ") + FString::Join(Command.Args, TEXT(" ")) : FString());
}
