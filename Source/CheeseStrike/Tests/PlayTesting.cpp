// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "PlayTesting.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
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
		/** Players in each world at the start: all of them in one match, or one each apart. */
		int32 PlayersTogether = 1;
		/** Whether the match waits in the lobby, with nobody spawned, rather than being on. */
		bool bLobbyWanted = false;

		/** The server's world, or the only one: the first PIE world the editor made. */
		UWorld* FirstWorld()
		{
			UWorld* First = nullptr;
			int32 FirstInstance = MAX_int32;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr && Context.PIEInstance < FirstInstance)
				{
					First = Context.World();
					FirstInstance = Context.PIEInstance;
				}
			}
			return First;
		}

		/**
		 * Seconds of play since it was made, counted by the server's clock where there is one, else the PC's. The game keeps
		 * its time in the world's seconds, which fall behind the PC's when frames take longer than the engine lets a world
		 * step at once (a busy PC), so a wait or a limit counted on the PC's clock would end early by the game's.
		 */
		class FPlayClock
		{
		public:
			/** Moves the clock on; call once an update. It starts at the first. */
			void Tick()
			{
				const double Real = FPlatformTime::Seconds();
				UWorld* Server = FirstWorld();
				const double Game = Server != nullptr ? Server->GetTimeSeconds() : 0.0;
				if (!bStarted)
				{
					bStarted = true;
					StartReal = Real;
					LastReal = Real;
				}
				double Step = Real - LastReal;
				// The same world as last time: its own seconds (not limited by the PC's since the last update, which can be fewer:
				// a stall between a frame's start and this update counts on the PC's clock in one update and the world's in the
				// next). A new world (a level loading, a travel) has only the PC's.
				if (Server != nullptr && Server == LastWorld.Get())
				{
					Step = FMath::Max(Game - LastGame, 0.0);
				}
				Elapsed += Step;
				LastReal = Real;
				LastGame = Game;
				LastWorld = Server;
			}

			double Seconds() const { return Elapsed; }
			double RealSeconds() const { return LastReal - StartReal; }

		private:
			bool bStarted = false;
			double Elapsed = 0.0;
			double LastReal = 0.0;
			double StartReal = 0.0;
			double LastGame = 0.0;
			TWeakObjectPtr<UWorld> LastWorld;
		};

		/** Waits `Seconds` of play. */
		class FWaitPlayCommand : public IAutomationLatentCommand
		{
		public:
			explicit FWaitPlayCommand(float InSeconds) : Duration(InSeconds) {}

			virtual bool Update() override
			{
				Clock.Tick();
				return Clock.Seconds() >= Duration;
			}

		private:
			float Duration;
			FPlayClock Clock;
		};

		/** Waits for a condition, for a reason, up to a limit in seconds of play (and, should the game stall, a few times that on the PC's clock). */
		class FWaitUntilCommand : public IAutomationLatentCommand
		{
		public:
			FWaitUntilCommand(const FString& InWhat, float InTimeout, TFunction<bool()> InCondition)
				: What(InWhat), Timeout(InTimeout), Condition(MoveTemp(InCondition))
			{
			}

			virtual bool Update() override
			{
				Clock.Tick();
				if (Condition())
				{
					return true;
				}
				if (Clock.Seconds() > Timeout || Clock.RealSeconds() > 4.0 * Timeout)
				{
					if (FAutomationTestBase* Current = Test())
					{
						Current->AddError(FString::Printf(TEXT("%s didn't happen in %.0f s (%.0f s of play, %.0f s on the PC's clock)"), *What, Timeout, Clock.Seconds(), Clock.RealSeconds()));
					}
					return true;
				}
				return false;
			}

		private:
			FString What;
			float Timeout;
			TFunction<bool()> Condition;
			FPlayClock Clock;
		};

		/** The worlds being played, in the order the editor made them: the host's (or the only one) first, then the joiners'. A world's place holds across a travel. */
		TArray<UWorld*> Worlds()
		{
			TArray<TPair<int32, UWorld*>> Found;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
				{
					Found.Emplace(Context.PIEInstance, Context.World());
				}
			}
			Found.Sort([](const TPair<int32, UWorld*>& A, const TPair<int32, UWorld*>& B) { return A.Key < B.Key; });
			TArray<UWorld*> InOrder;
			for (const TPair<int32, UWorld*>& Each : Found)
			{
				InOrder.Add(Each.Value);
			}
			return InOrder;
		}

		bool IsIn(UWorld* InWorld)
		{
			const ACheeseStrikeGameState* State = InWorld->GetGameState<ACheeseStrikeGameState>();
			const ACheeseStrikePlayerController* Local = Cast<ACheeseStrikePlayerController>(InWorld->GetFirstPlayerController());
			const ACheeseStrikeCharacter* Pawn = Local != nullptr ? Cast<ACheeseStrikeCharacter>(Local->GetPawn()) : nullptr;
			if (State == nullptr || Local == nullptr)
			{
				return false;
			}
			// Every player is on the scoreboard; bots aside.
			int32 Players = 0;
			for (const APlayerState* Each : State->PlayerArray)
			{
				const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Each);
				Players += Ours != nullptr && !Ours->IsABot() ? 1 : 0;
			}
			if (Players != PlayersTogether)
			{
				return false;
			}
			if (bLobbyWanted)
			{
				// In the lobby: the match waits, and nobody has a character.
				return !State->HasMatchStarted() && Local->PlayerState != nullptr && Pawn == nullptr;
			}
			return State->HasMatchStarted() && Pawn != nullptr && Pawn->IsAlive() && Pawn->GetWeapon() != nullptr && Pawn->GetPlayerState() != nullptr;
		}

		/** The server's clock as the match's state last brought it to this world: it comes ten times a second while the server's news is reaching the player. */
		double ServerTimeHeard(const AGameStateBase* State)
		{
			static const FDoubleProperty* Heard = FindFProperty<FDoubleProperty>(AGameStateBase::StaticClass(), TEXT("ReplicatedWorldTimeSecondsDouble"));
			return Heard != nullptr && State != nullptr ? Heard->GetPropertyValue_InContainer(State) : -1.0;
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
			// And the server has every player's character, or nobody's in the lobby; bots aside.
			int32 Characters = 0;
			for (TActorIterator<ACheeseStrikeCharacter> It(Playing[0]); It; ++It)
			{
				Characters += It->IsPlayerControlled() ? 1 : 0;
			}
			return Characters == (bLobbyWanted ? 0 : PlayersTogether);
		}
	}

	FAutomationTestBase* Test()
	{
		return FAutomationTestFramework::Get().GetCurrentTest();
	}

	void StartPlaying(const FSession& Session)
	{
		PlayersWanted = Session.Players;
		PlayersTogether = Session.bApart ? 1 : Session.Players;
		bLobbyWanted = Session.bLobby;
		// A match starts at once, with no bots to get in the way, unless the test wants the lobby, which has them.
		// No bots unless the test asks for them: the players are the only characters.
		const FString NoBots = (Session.Options.Contains(TEXT("?Bots=")) ? FString() : FString(TEXT("?Bots=0"))) + (Session.Options.Contains(TEXT("?Mice=")) ? FString() : FString(TEXT("?Mice=0")));
		// No count before the match (or its rounds) unless the test asks for one, so that the tests are quick.
		const FString NoCount = Session.Options.Contains(TEXT("?Countdown=")) ? FString() : FString(TEXT("?Countdown=0"));
		const FString Options = Session.bLobby ? Session.Options + NoCount : Session.Options + TEXT("?StartNow") + NoBots + NoCount;
		Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
		Settings->SetPlayNetMode(Session.Players > 1 && !Session.bApart ? EPlayNetMode::PIE_ListenServer : EPlayNetMode::PIE_Standalone);
		Settings->SetPlayNumberOfClients(Session.Players);
		// A port of this working copy's own (tools/worktree.py makes others). The editor finds a free port when the one it is
		// given is taken, but a match that travels listens again on the one it was given, where another copy's test may be.
		Settings->SetServerPort(static_cast<uint16>(18000 + FCrc::StrCrc32(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()).ToLower()) % 1000));
		Settings->SetRunUnderOneProcess(true);
		Settings->bLaunchSeparateServer = false;
		// The host's options ride on the URL the editor plays, as a person's would from the Play menu.
		const FString Level = Session.Level.IsEmpty() ? FString(ShooterLevel) : Session.Level;
		GEditor->UserEditedPlayWorldURL = Options.IsEmpty() ? FString() : Level + Options;
		FRequestPlaySessionParams Params;
		Params.EditorPlaySettings = Settings.Get();
		Params.GlobalMapOverride = Level;
		// Asked for on its own, not through the engine's command for it, which waits for the match to have started: a lobby never does.
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Params]()
		{
			GEditor->RequestPlaySession(Params);
			GEditor->StartQueuedPlaySessionRequest();
			return true;
		}));
		WaitUntil(FString::Printf(TEXT("%d player(s) being in with their weapons"), Session.Players), 90.0f, &EveryoneIsIn);
		// Playing the level the editor has open copies the editor's world, with its navigation mesh as it is then, and the
		// copy builds only where something changes after. The editor builds its own a number of frames after opening the
		// level, so a test that came first on a busy PC (few frames) got a mesh with holes, where its bots never went.
		Then([Level]()
		{
			const UWorld* Editing = GEditor->GetEditorWorldContext().World();
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
			if (Editing != nullptr && Navigation != nullptr && Editing->GetOutermost()->GetName() == FPackageName::ObjectPathToPackageName(Level))
			{
				Navigation->Build();
			}
		});
		if (PlayersTogether > 1)
		{
			// A joiner that has just come in hears nothing more from the server for a while (a second, or several on a busy PC),
			// so what the test does next would reach them late. Go on once every joiner has heard the server's clock for a
			// second, and lately.
			TSharedRef<TArray<double>> FirstHeard = MakeShared<TArray<double>>();
			WaitUntil(TEXT("every joiner hearing the server"), 30.0f, [FirstHeard]()
			{
				const ACheeseStrikeGameState* Server = GameState(0);
				if (Server == nullptr)
				{
					return false;
				}
				FirstHeard->SetNum(PlayersTogether);
				bool bAll = true;
				for (int32 Joiner = 1; Joiner < PlayersTogether; ++Joiner)
				{
					const double Heard = ServerTimeHeard(GameState(Joiner));
					double& First = (*FirstHeard)[Joiner];
					if (Heard <= 0.0)
					{
						bAll = false;
						continue;
					}
					First = First > 0.0 ? First : Heard;
					bAll &= Heard >= First + 1.0 && Heard >= Server->GetServerWorldTimeSeconds() - 0.5;
				}
				return bAll;
			});
		}
		// Nothing is drawn here, and a body that isn't drawn isn't posed; the tests look at the bones, so pose them anyway.
		Then([]()
		{
			for (UWorld* Each : Worlds())
			{
				for (TActorIterator<ACheeseStrikeCharacter> It(Each); It; ++It)
				{
					for (USkeletalMeshComponent* Body : {It->GetMesh(), It->GetFirstPersonMesh()})
					{
						if (Body != nullptr)
						{
							Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
						}
					}
				}
			}
		});
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
		ADD_LATENT_AUTOMATION_COMMAND(FWaitPlayCommand(Seconds));
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

	void FireUntil(int32 Shooter, const FString& What, float Timeout, TFunction<bool()> Done)
	{
		Play(Shooter, TEXT("hold Fire"));
		WaitUntil(What, Timeout, MoveTemp(Done));
		Play(Shooter, TEXT("release Fire"));
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
