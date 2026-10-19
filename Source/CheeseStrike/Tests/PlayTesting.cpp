// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "PlayTesting.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "Dev/CheeseStrikePlayScript.h"
#include "Weapons/CheeseStrikeWeapon.h"

namespace PlayTesting
{
	namespace
	{
		const TCHAR* ShooterLevel = TEXT("/Game/Variant_Shooter/Lvl_Shooter");
		/** The settings the session plays with, held until it ends. */
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		int32 PlayersWanted = 1;

		/** Waits for a condition, for a reason, up to a limit. */
		class FWaitUntilCommand : public IAutomationLatentCommand
		{
		public:
			FWaitUntilCommand(const FString& InWhat, float InTimeout, TFunction<bool()> InCondition)
				: What(InWhat), Timeout(InTimeout), Condition(MoveTemp(InCondition))
			{
			}

			virtual bool Update() override
			{
				if (Condition())
				{
					return true;
				}
				if (GetCurrentRunTime() > Timeout)
				{
					if (FAutomationTestBase* Current = Test())
					{
						Current->AddError(FString::Printf(TEXT("%s didn't happen in %.0f s"), *What, Timeout));
					}
					return true;
				}
				return false;
			}

		private:
			FString What;
			float Timeout;
			TFunction<bool()> Condition;
		};

		/** The worlds being played: the server's (or the only one) first, then the joiners' in the order they came. */
		TArray<UWorld*> Worlds()
		{
			TArray<UWorld*> Found;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
				{
					if (Context.World()->GetNetMode() == NM_Client)
					{
						Found.Add(Context.World());
					}
					else
					{
						Found.Insert(Context.World(), 0);
					}
				}
			}
			return Found;
		}

		bool IsIn(UWorld* InWorld)
		{
			const ACheeseStrikeGameState* State = InWorld->GetGameState<ACheeseStrikeGameState>();
			const ACheeseStrikePlayerController* Local = Cast<ACheeseStrikePlayerController>(InWorld->GetFirstPlayerController());
			const ACheeseStrikeCharacter* Pawn = Local != nullptr ? Cast<ACheeseStrikeCharacter>(Local->GetPawn()) : nullptr;
			return State != nullptr && State->HasMatchStarted() && State->PlayerArray.Num() == PlayersWanted
				&& Pawn != nullptr && Pawn->IsAlive() && Pawn->GetWeapon() != nullptr && Pawn->GetPlayerState() != nullptr;
		}

		bool EveryoneIsIn()
		{
			const TArray<UWorld*> Playing = Worlds();
			if (Playing.Num() != PlayersWanted)
			{
				return false;
			}
			for (UWorld* Each : Playing)
			{
				if (!IsIn(Each))
				{
					return false;
				}
			}
			// And the server has everyone's character.
			int32 Characters = 0;
			for (TActorIterator<ACheeseStrikeCharacter> It(Playing[0]); It; ++It)
			{
				++Characters;
			}
			return Characters == PlayersWanted;
		}
	}

	FAutomationTestBase* Test()
	{
		return FAutomationTestFramework::Get().GetCurrentTest();
	}

	void StartPlaying(const FSession& Session)
	{
		PlayersWanted = Session.Players;
		// The template's first-person weapon animation looks for a camera
		// and a controller its own character has and ours doesn't, and says
		// so every frame; it is the known gap until the game has arms of its
		// own (docs/unreal/spike.md, day 7), not what these tests check.
		if (FAutomationTestBase* Current = Test())
		{
			Current->AddExpectedErrorPlain(TEXT("Blueprint:  ABP_FP_Weapon"), EAutomationExpectedErrorFlags::Contains, -1);
		}
		Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
		Settings->SetPlayNetMode(Session.Players > 1 ? EPlayNetMode::PIE_ListenServer : EPlayNetMode::PIE_Standalone);
		Settings->SetPlayNumberOfClients(Session.Players);
		Settings->SetRunUnderOneProcess(true);
		Settings->bLaunchSeparateServer = false;
		// The host's options ride on the URL the editor plays, as a person's would from the Play menu.
		GEditor->UserEditedPlayWorldURL = Session.Options.IsEmpty() ? FString() : FString(ShooterLevel) + Session.Options;
		FRequestPlaySessionParams Params;
		Params.EditorPlaySettings = Settings.Get();
		Params.GlobalMapOverride = ShooterLevel;
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(Params));
		WaitUntil(FString::Printf(TEXT("%d player(s) being in with their weapons"), Session.Players), 90.0f, &EveryoneIsIn);
	}

	void StopPlaying()
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
		WaitUntil(TEXT("the play ending"), 30.0f, []() { return Worlds().Num() == 0; });
		Then([]()
		{
			GEditor->UserEditedPlayWorldURL.Empty();
			Settings.Reset();
		});
	}

	void WaitUntil(const FString& What, float Timeout, TFunction<bool()> Condition)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitUntilCommand(What, Timeout, MoveTemp(Condition)));
	}

	void Wait(float Seconds)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(Seconds));
	}

	void Then(TFunction<void()> Do)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Do = MoveTemp(Do)]() { Do(); return true; }));
	}

	void Play(int32 Player, const FString& Script, float Timeout)
	{
		Then([Player, Script]()
		{
			if (ACheeseStrikePlayerController* Local = Controller(Player))
			{
				Local->Play(Script);
			}
			else if (FAutomationTestBase* Current = Test())
			{
				Current->AddError(FString::Printf(TEXT("player %d has no controller to play a script"), Player));
			}
		});
		WaitUntil(FString::Printf(TEXT("player %d's script ending"), Player), Timeout, [Player]()
		{
			const ACheeseStrikePlayerController* Local = Controller(Player);
			return Local == nullptr || Local->GetPlayScript() == nullptr || Local->GetPlayScript()->IsFinished();
		});
	}

	void PutAt(int32 Player, const FVector& At, float Yaw)
	{
		Then([Player, At, Yaw]()
		{
			// The server moves the character; the player turns their own view, which the server doesn't set.
			if (ACheeseStrikeCharacter* Pawn = CharacterOf(Player, 0))
			{
				Pawn->StartAt(At, Yaw);
			}
			if (ACheeseStrikePlayerController* Local = Controller(Player))
			{
				Local->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
			}
		});
		// Long enough for the move to reach the player and settle.
		Wait(0.5f);
	}

	UWorld* World(int32 Player)
	{
		const TArray<UWorld*> Playing = Worlds();
		return Playing.IsValidIndex(Player) ? Playing[Player] : nullptr;
	}

	ACheeseStrikePlayerController* Controller(int32 Player)
	{
		UWorld* Theirs = World(Player);
		return Theirs != nullptr ? Cast<ACheeseStrikePlayerController>(Theirs->GetFirstPlayerController()) : nullptr;
	}

	ACheeseStrikeCharacter* Character(int32 Player)
	{
		const ACheeseStrikePlayerController* Local = Controller(Player);
		return Local != nullptr ? Cast<ACheeseStrikeCharacter>(Local->GetPawn()) : nullptr;
	}

	ACheeseStrikeGameState* GameState(int32 Player)
	{
		UWorld* Theirs = World(Player);
		return Theirs != nullptr ? Theirs->GetGameState<ACheeseStrikeGameState>() : nullptr;
	}

	ACheeseStrikeCharacter* CharacterOf(int32 Of, int32 Seen)
	{
		const ACheeseStrikeCharacter* Own = Character(Of);
		UWorld* Viewer = World(Seen);
		if (Own == nullptr || Own->GetPlayerState() == nullptr || Viewer == nullptr)
		{
			return nullptr;
		}
		const int32 Id = Own->GetPlayerState()->GetPlayerId();
		ACheeseStrikeCharacter* Body = nullptr;
		for (TActorIterator<ACheeseStrikeCharacter> It(Viewer); It; ++It)
		{
			if (It->GetPlayerState() != nullptr && It->GetPlayerState()->GetPlayerId() == Id)
			{
				if (It->IsAlive())
				{
					return *It;
				}
				Body = *It;
			}
		}
		return Body;
	}

	float Now(int32 Player)
	{
		const ACheeseStrikeGameState* State = GameState(Player);
		return State != nullptr ? State->GetServerWorldTimeSeconds() : 0.0f;
	}
}

#endif
