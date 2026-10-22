// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the modes and the streaks in the editor (see
// PlayTesting.h): a team winning Deathmatch with friendly fire off, a round
// of Elimination ending and the next beginning, and the streaks called in
// and scoring for their callers.

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "Streaks/CheeseStrikeAerialCheese.h"
#include "Streaks/CheeseStrikeBombs.h"
#include "Modes/CheeseStrikeLastStand.h"
#include "Rules/CheeseStrikeDowned.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "UI/CheeseStrikeHUD.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikeShot.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace ModePlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** Two spots facing each other across 4 m of clear ground on the shooter level, and somewhere out of the way. */
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	constexpr float HostYaw = -90.0f;
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);
	constexpr float JoinerYaw = 90.0f;
	const FVector AsideSpot(1150.0f, -1450.0f, 100.0f);
	constexpr float AsideYaw = 117.0f;
	const TCHAR* const Burst = TEXT("hold Fire\nwait 1.2\nrelease Fire");

	/** Player `Of`'s state as world `Seen` has it. */
	ACheeseStrikePlayerState* StateOf(int32 Of, int32 Seen)
	{
		const ACheeseStrikePlayerController* Own = PlayTesting::Controller(Of);
		const ACheeseStrikeGameState* Match = PlayTesting::GameState(Seen);
		if (Own == nullptr || Own->PlayerState == nullptr || Match == nullptr)
		{
			return nullptr;
		}
		for (APlayerState* Each : Match->PlayerArray)
		{
			if (Each != nullptr && Each->GetPlayerId() == Own->PlayerState->GetPlayerId())
			{
				return Cast<ACheeseStrikePlayerState>(Each);
			}
		}
		return nullptr;
	}

	ACheeseStrikeGameMode* Mode()
	{
		UWorld* Host = PlayTesting::World(0);
		return Host != nullptr ? Host->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	}

	/** How far `At` is from the nearest player start. */
	float FromNearestStart(UWorld* World, const FVector& At)
	{
		float Nearest = TNumericLimits<float>::Max();
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist2D(It->GetActorLocation(), At)));
		}
		return Nearest;
	}

	template <typename T>
	T* First(UWorld* World)
	{
		TActorIterator<T> It(World);
		return It ? *It : nullptr;
	}

	bool Dead(int32 Player)
	{
		const ACheeseStrikeCharacter* Body = PlayTesting::Character(Player);
		return Body != nullptr && !Body->IsAlive();
	}

	/** Beside the host (a metre to its side), and behind it (facing its back when it faces the joiner's way). */
	const FVector BesideSpot(-131.0f, -300.0f, 100.0f);
	const FVector BehindSpot(-231.0f, -410.0f, 100.0f);

	FName HeldId(const ACheeseStrikeCharacter* Character)
	{
		return Character != nullptr && Character->GetWeapon() != nullptr ? Character->GetWeapon()->GetWeaponId() : NAME_None;
	}

	TWeakObjectPtr<ACheeseStrikeCharacter> Remembered;
	/** When the host was killed, by the server's clock. */
	float KilledAt = 0.0f;

	/** How high a bone of the body others see is above the character's feet, on the machine it is seen on (cm). */
	float BoneHeight(const ACheeseStrikeCharacter* Character, const TCHAR* Bone)
	{
		const float Feet = Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		return Character->GetMesh()->GetBoneLocation(FName(Bone)).Z - Feet;
	}

	/** Where a bone of the body others see is, in the body's frame (ahead, right, up from the capsule's axis on the floor), on the machine it is seen on. */
	FVector BoneInBody(const ACheeseStrikeCharacter* Character, const TCHAR* Bone)
	{
		return Character->DownBodyFrame(Character->GetMesh()->GetBoneLocation(FName(Bone)));
	}

	/** How high its own player's view is above its feet. */
	float ViewHeight(const ACheeseStrikeCharacter* Character)
	{
		return Character->GetFirstPersonCameraComponent()->GetComponentLocation().Z - (Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}

	/** The host's pelvis, down, as each machine had it, and the way its body faced down. */
	float DownPelvis[3] = {};
	float DownYaw = 0.0f;
	int32 ShotsBefore = 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATeamWinsDeathmatchAtItsKillLimit, "CheeseStrike.Play.ATeamWinsDeathmatchAtItsKillLimit", ModePlayTests::Flags)

bool FATeamWinsDeathmatchAtItsKillLimit::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ModePlayTests;
	StartPlaying({3, TEXT("?Mode=Deathmatch?KillLimit=1?RespawnDelay=1")});
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		if (!TestNotNull(TEXT("the match"), Match))
		{
			return;
		}
		TestTrue(TEXT("Deathmatch is on"), Match->GetMode() == CheeseStrikeModes::EMode::Deathmatch && Match->HasTeams());
		TestEqual(TEXT("to the host's kill limit"), Match->GetKillLimit(), 1);
		const ACheeseStrikePlayerState* Host = StateOf(0, 0);
		const ACheeseStrikePlayerState* First = StateOf(1, 0);
		const ACheeseStrikePlayerState* Second = StateOf(2, 0);
		if (TestNotNull(TEXT("the host"), Host) && TestNotNull(TEXT("the first joiner"), First) && TestNotNull(TEXT("the second"), Second))
		{
			TestTrue(TEXT("the first joiner is on the other side"), Match->AreEnemies(Host, First));
			TestTrue(TEXT("the second on the host's"), Match->AreTeammates(Host, Second));
		}
		const ACheeseStrikeGameState* Seen = GameState(2);
		TestTrue(TEXT("a joiner knows the mode"), Seen != nullptr && Seen->HasTeams() && Seen->GetMode() == CheeseStrikeModes::EMode::Deathmatch);
	});
	PutAt(0, HostSpot, HostYaw);
	PutAt(2, JoinerSpot, JoinerYaw);
	PutAt(1, AsideSpot, AsideYaw);
	Wait(CheeseStrikeRules::SpawnProtection);
	// A teammate's shots do nothing.
	Play(2, Burst);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikeCharacter* Teammate = Character(2);
		if (TestNotNull(TEXT("the host"), Host) && TestNotNull(TEXT("the teammate"), Teammate))
		{
			TestTrue(FString::Printf(TEXT("the teammate fired (%d shots)"), Teammate->GetWeapon()->ShotsFired), Teammate->GetWeapon()->ShotsFired > 3);
			TestEqual(TEXT("but the host is unhurt: friendly fire is off"), Host->GetHealth(), CheeseStrikeWeapons::MaxHealth);
		}
		TestEqual(TEXT("and nobody has scored"), GameState(0)->GetTeamScore(0) + GameState(0)->GetTeamScore(1), 0);
	});
	PutAt(2, AsideSpot + FVector(300.0f, 0.0f, 0.0f), AsideYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	FireUntil(1, TEXT("the match ending"), 6.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && Match->HasMatchEnded(); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestEqual(TEXT("the other side has its kill"), Match->GetTeamScore(1), 1);
		TestEqual(TEXT("and the host's none"), Match->GetTeamScore(0), 0);
		TestEqual(TEXT("so it wins"), Match->GetWinningTeam(), 1);
		TestEqual(TEXT("by name"), Match->GetWinnerName(), FString(CheeseStrikeRules::TeamNames[1]));
		const ACheeseStrikePlayerState* Killer = StateOf(1, 0);
		TestTrue(TEXT("and the joiner has the kill"), Killer != nullptr && Killer->GetKills() == 1);
	});
	WaitUntil(TEXT("a joiner hearing who won"), 5.0f, []() { const ACheeseStrikeGameState* Match = GameState(2); return Match != nullptr && Match->HasMatchEnded() && Match->GetWinningTeam() == 1; });
	Then([this]()
	{
		const ACheeseStrikeGameState* Seen = GameState(2);
		TestEqual(TEXT("with the score"), Seen->GetTeamScore(1), 1);
		TestEqual(TEXT("and the team's name"), Seen->GetWinnerName(), FString(CheeseStrikeRules::TeamNames[1]));
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnEliminationRoundEndsWhenASideIsOut, "CheeseStrike.Play.AnEliminationRoundEndsWhenASideIsOut", ModePlayTests::Flags)

bool FAnEliminationRoundEndsWhenASideIsOut::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ModePlayTests;
	StartPlaying({2, TEXT("?Mode=Elimination?Rounds=2")});
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		if (TestNotNull(TEXT("the match"), Match))
		{
			TestTrue(TEXT("Elimination is on"), Match->GetMode() == CheeseStrikeModes::EMode::Elimination);
			TestEqual(TEXT("round 1"), Match->GetRound(), 1);
			TestEqual(TEXT("of the first to 2"), Match->GetRoundLimit(), 2);
			TestTrue(FString::Printf(TEXT("with its time running (%.0f s left)"), Match->SecondsLeftInRound()), Match->SecondsLeftInRound() > CheeseStrikeModes::RoundTime - 30.0f);
		}
	});
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Wait(CheeseStrikeRules::SpawnProtection);
	FireUntil(1, TEXT("the host dying"), 5.0f, []() { return Dead(0); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikePlayerState* Host = StateOf(0, 0);
		TestTrue(TEXT("the host is out"), Host != nullptr && Host->IsOut());
		TestTrue(TEXT("the round is over"), Match->IsRoundBreak());
		TestEqual(TEXT("to the side left standing"), Match->GetLastRoundWinner(), 1);
		TestEqual(TEXT("which has a round"), Match->GetTeamScore(1), 1);
		TestFalse(TEXT("and the match goes on"), Match->HasMatchEnded());
		Remembered = Character(0);
		KilledAt = Now(0);
	});
	WaitUntil(TEXT("the joiner hearing the host is out"), 5.0f, []() { const ACheeseStrikePlayerState* Host = StateOf(0, 1); return Host != nullptr && Host->IsOut(); });
	// Past the respawn delay, between rounds, nobody comes back yet.
	WaitUntil(TEXT("the respawn delay passing"), CheeseStrikeRules::RespawnDelay + 1.0f, []() { return Now(0) > KilledAt + CheeseStrikeRules::RespawnDelay + 0.5f; });
	Then([this]()
	{
		TestTrue(TEXT("the host stays dead between rounds"), Dead(0) && Character(0) == Remembered.Get());
		TestTrue(FString::Printf(TEXT("as the next round is coming (in %.1f s)"), GameState(0)->SecondsToNextRound()), GameState(0)->IsRoundBreak() && GameState(0)->SecondsToNextRound() > 0.0f);
	});
	WaitUntil(TEXT("the next round"), CheeseStrikeModes::RoundBreak + 3.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && Match->GetRound() == 2; });
	WaitUntil(TEXT("the host back"), 3.0f, []() { const ACheeseStrikeCharacter* Host = Character(0); return Host != nullptr && Host->IsAlive(); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikePlayerState* HostState = StateOf(0, 0);
		TestFalse(TEXT("the break is over"), Match->IsRoundBreak());
		TestTrue(TEXT("the host is in again"), HostState != nullptr && !HostState->IsOut());
		TestEqual(TEXT("with full health"), Host->GetHealth(), CheeseStrikeWeapons::MaxHealth);
		TestTrue(TEXT("a new character"), Host != Remembered.Get());
		TestTrue(FString::Printf(TEXT("at a start (%.0f cm from one)"), FromNearestStart(World(0), Host->GetActorLocation())), FromNearestStart(World(0), Host->GetActorLocation()) < 150.0f);
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		TestTrue(TEXT("and the joiner too, at full health"), Joiner != nullptr && Joiner->IsAlive() && Joiner->GetHealth() == CheeseStrikeWeapons::MaxHealth);
	});
	WaitUntil(TEXT("the joiner seeing round 2"), 5.0f, []() { const ACheeseStrikeGameState* Match = GameState(1); const ACheeseStrikePlayerState* Host = StateOf(0, 1); return Match != nullptr && Match->GetRound() == 2 && Host != nullptr && !Host->IsOut(); });
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Wait(CheeseStrikeRules::SpawnProtection);
	FireUntil(1, TEXT("the match ending"), 6.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && Match->HasMatchEnded(); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestEqual(TEXT("two rounds to the joiner's side"), Match->GetTeamScore(1), 2);
		TestEqual(TEXT("which wins"), Match->GetWinningTeam(), 1);
		TestEqual(TEXT("by name"), Match->GetWinnerName(), FString(CheeseStrikeRules::TeamNames[1]));
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreaksAreCalledInAndScoreForTheirCallers, "CheeseStrike.Play.StreaksAreCalledInAndScoreForTheirCallers", ModePlayTests::Flags)

bool FStreaksAreCalledInAndScoreForTheirCallers::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ModePlayTests;
	using CheeseStrikeStreaks::EStreak;
	StartPlaying({2, TEXT("?RespawnDelay=4")});
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Wait(CheeseStrikeRules::SpawnProtection);

	// The Unmanned Aerial Cheese: the host's own, in free-for-all.
	Then([]() { Mode()->CheeseStrikeGiveStreak(1); });
	Play(0, TEXT("tap Streak\nwait 0.3"));
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestTrue(TEXT("the drone shows the host the enemy"), Match->IsRevealedTo(StateOf(0, 0)));
		TestFalse(TEXT("but not the joiner"), Match->IsRevealedTo(StateOf(1, 0)));
		TestNotNull(TEXT("and it flies over the map"), First<ACheeseStrikeAerialCheese>(World(0)));
		TestTrue(TEXT("the streak is used"), StateOf(0, 0)->NextStreak() == EStreak::None);
		TestTrue(TEXT("everyone is told"), Match->GetAnnouncements().Num() > 0 && Match->GetAnnouncements().Last().Text.Contains(TEXT("called in an Unmanned Aerial Cheese")));
	});
	WaitUntil(TEXT("the joiner hearing of the drone"), 5.0f, []()
	{
		const ACheeseStrikeGameState* Match = GameState(1);
		return Match != nullptr && Match->GetAnnouncements().Num() > 0 && Match->IsRevealedTo(StateOf(0, 1)) && First<ACheeseStrikeAerialCheese>(World(1)) != nullptr;
	});

	// The Cheese Strike: marked on the ground just past the joiner, it kills them, as the host's kill.
	Then([]()
	{
		Mode()->CheeseStrikeGiveStreak(2);
		ShotsBefore = Character(0)->GetWeapon()->ShotsFired;
	});
	WaitUntil(TEXT("the host hearing of the strike"), 3.0f, []() { const ACheeseStrikePlayerState* Host = StateOf(0, 0); return Host != nullptr && Host->NextStreak() == EStreak::CheeseStrike; });
	Play(0, TEXT("tap Streak\nwait 0.2\nlook 0 -20\nwait 0.2\ntap Fire\nwait 0.3"));
	Then([this]()
	{
		const ACheeseStrikeBombs* Bombs = First<ACheeseStrikeBombs>(World(0));
		if (TestNotNull(TEXT("the strike is coming"), Bombs))
		{
			TestTrue(TEXT("a Cheese Strike"), Bombs->GetKind() == EStreak::CheeseStrike);
			TestEqual(TEXT("of seven wheels"), Bombs->GetDrops().Num(), CheeseStrikeStreaks::StrikeWheels);
		}
		TestFalse(TEXT("the host no longer marks"), StateOf(0, 0)->IsMarkingStrike());
		TestEqual(TEXT("and the shot marked rather than fired"), Character(0)->GetWeapon()->ShotsFired, ShotsBefore);
	});
	WaitUntil(TEXT("the strike landing on the joiner"), CheeseStrikeStreaks::StrikeWarning + 3.0f, []() { return Dead(1); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		const ACheeseStrikeCharacter* Host = Character(0);
		const ACheeseStrikePlayerState* HostState = StateOf(0, 0);
		TestEqual(TEXT("killed by the host"), Joiner->GetKilledBy(), Host->DisplayName());
		if (TestTrue(TEXT("in the feed"), Match->GetKillFeed().Num() > 0))
		{
			TestEqual(TEXT("as the host's"), Match->GetKillFeed().Last().Killer, Host->DisplayName());
			TestEqual(TEXT("by the Cheese Strike"), Match->GetKillFeed().Last().How, FString(TEXT("Cheese Strike")));
		}
		TestEqual(TEXT("the host has the kill"), HostState->GetKills(), 1);
		TestEqual(TEXT("which doesn't run on the streak"), HostState->GetStreak(), 0);
		TestTrue(TEXT("the host's own wheels don't hurt them"), Host->IsAlive() && Host->GetHealth() == CheeseStrikeWeapons::MaxHealth);
	});

	// The Gouda Overload, from the joiner's machine: it rains on the host, standing in the open, and kills them.
	WaitUntil(TEXT("the joiner back"), 4.0f + 4.0f, []() { const ACheeseStrikeCharacter* Joiner = Character(1); return Joiner != nullptr && Joiner->IsAlive(); });
	Then([]() { StateOf(1, 0)->GiveStreak(EStreak::GoudaOverload); });
	WaitUntil(TEXT("the joiner hearing of the Overload"), 3.0f, []() { const ACheeseStrikePlayerState* Joiner = StateOf(1, 1); return Joiner != nullptr && Joiner->NextStreak() == EStreak::GoudaOverload; });
	Play(1, TEXT("tap Streak\nwait 0.3"));
	WaitUntil(TEXT("the Overload coming"), 3.0f, []() { const ACheeseStrikeBombs* Bombs = First<ACheeseStrikeBombs>(World(0)); return Bombs != nullptr && Bombs->GetKind() == EStreak::GoudaOverload; });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		TestTrue(TEXT("with a warning to everyone"), Match->GetAnnouncements().Last().Text.Contains(TEXT("called in the Gouda Overload")));
		TestTrue(TEXT("and the host still standing, for now"), Character(0)->IsAlive());
	});
	WaitUntil(TEXT("the Overload killing the host"), CheeseStrikeStreaks::OverloadWarning + CheeseStrikeStreaks::OverloadRain + 2.0f, []() { return Dead(0); });
	Then([this]()
	{
		const ACheeseStrikeGameState* Match = GameState(0);
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		TestEqual(TEXT("the host killed by the joiner"), Character(0)->GetKilledBy(), Joiner->DisplayName());
		TestEqual(TEXT("by the Gouda Overload"), Match->GetKillFeed().Last().How, FString(TEXT("Gouda Overload")));
		const ACheeseStrikeBombs* Bombs = First<ACheeseStrikeBombs>(World(0));
		TestTrue(TEXT("its wheels landing round the host"), Bombs != nullptr && Bombs->GetLanded() > 0 && Bombs->GetHits() > 0);
	});
	StopPlaying();
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExecutionDownsRevivesAndFinishes, "CheeseStrike.Play.ExecutionDownsRevivesAndFinishes", ModePlayTests::Flags)

bool FExecutionDownsRevivesAndFinishes::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ModePlayTests;
	// The host and the second joiner on one side, the first joiner on the other.
	StartPlaying({3, TEXT("?Mode=Execution?Rounds=1")});
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	PutAt(2, AsideSpot, AsideYaw);
	Wait(CheeseStrikeRules::SpawnProtection);
	// Shot down from four metres: down, not dead.
	Play(1, Burst);
	WaitUntil(TEXT("the host going down"), 5.0f, []() { const ACheeseStrikeCharacter* Host = CharacterOf(0, 0); return Host != nullptr && Host->IsDowned(); });
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		TestTrue(TEXT("down, and alive"), Host->IsAlive());
		TestFalse(TEXT("not out"), StateOf(0, 0)->IsOut());
		TestFalse(TEXT("and the round goes on: a teammate stands"), GameState(0)->IsRoundBreak());
		Remembered = Character(0);
	});
	// The teammate crouches, to set the host's pose against.
	Play(2, TEXT("hold Crouch\nwait 0.2"));
	Wait(1.4f);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		// Down looks it, on every machine: on the back, the hips on the floor, the legs out ahead along it, propped on the
		// left elbow with the head raised over the chest, the pistol held out ahead, and the row says so.
		for (int32 Seen = 0; Seen < 3; ++Seen)
		{
			const ACheeseStrikeCharacter* Down = CharacterOf(0, Seen);
			const ACheeseStrikeCharacter* Crouching = CharacterOf(2, Seen);
			if (!TestTrue(TEXT("the host and the teammate are on every machine"), Down != nullptr && Crouching != nullptr))
			{
				continue;
			}
			const UCheeseStrikeAnimInstance* Animation = Cast<UCheeseStrikeAnimInstance>(Down->GetMesh()->GetAnimInstance());
			TestTrue(FString::Printf(TEXT("the body is animated as down, as player %d sees it"), Seen), Animation != nullptr && Animation->bDowned);
			TestTrue(FString::Printf(TEXT("holding the pistol, as player %d sees it"), Seen), Animation != nullptr && Animation->Hold == ECheeseStrikeHold::Pistol);
			const float Pelvis = BoneHeight(Down, TEXT("pelvis"));
			const float Crouched = BoneHeight(Crouching, TEXT("pelvis"));
			TestTrue(FString::Printf(TEXT("the teammate crouches, as player %d sees it"), Seen), Crouching->bIsCrouched);
			TestTrue(FString::Printf(TEXT("the pelvis on the floor (%.0f cm), far under a crouching one's (%.0f cm), as player %d sees it"), Pelvis, Crouched, Seen), Pelvis < 22.0f && Pelvis < Crouched - 25.0f);
			const FVector Hips = BoneInBody(Down, TEXT("pelvis"));
			const FVector Chest = BoneInBody(Down, TEXT("spine_03"));
			const FVector Head = BoneInBody(Down, TEXT("head"));
			for (const TCHAR* Foot : {TEXT("foot_l"), TEXT("foot_r")})
			{
				const FVector At = BoneInBody(Down, Foot);
				TestTrue(FString::Printf(TEXT("%s out ahead of the hips (%.0f cm) on the floor (%.0f cm), as player %d sees it"), Foot, At.X - Hips.X, At.Z, Seen), At.X - Hips.X > 60.0f && At.Z > 0.0f && At.Z < 20.0f);
			}
			TestTrue(FString::Printf(TEXT("the body lies along the floor: the head %.0f cm behind the feet, as player %d sees it"), BoneInBody(Down, TEXT("foot_r")).X - Head.X, Seen), BoneInBody(Down, TEXT("foot_r")).X - Head.X > 90.0f);
			TestTrue(FString::Printf(TEXT("the head raised (%.0f cm) over the chest (%.0f cm), as player %d sees it"), Head.Z, Chest.Z, Seen), Head.Z > Chest.Z + 10.0f && Head.Z > 30.0f && Head.Z < 65.0f);
			TestTrue(FString::Printf(TEXT("propped on the left elbow (%.0f cm off the floor), as player %d sees it"), BoneInBody(Down, TEXT("lowerarm_l")).Z, Seen), BoneInBody(Down, TEXT("lowerarm_l")).Z < 18.0f);
			const FVector Hand = BoneInBody(Down, TEXT("hand_r"));
			TestTrue(FString::Printf(TEXT("the pistol held out ahead of the chest (%.0f cm) and up (%.0f cm), as player %d sees it"), Hand.X - Chest.X, Hand.Z, Seen), Hand.X - Chest.X > 15.0f && Hand.Z > 20.0f);
			DownPelvis[Seen] = Pelvis;
			const ACheeseStrikePlayerState* Player = StateOf(0, Seen);
			TestEqual(FString::Printf(TEXT("the row shows the host down, as player %d sees it"), Seen), ACheeseStrikeHUD::StatusOf(Player, ACheeseStrikeHUD::BodyOf(Player)), CheeseStrikeHUD::EStatus::Down);
		}
		// The server finds the head and the body where they lie: the eyes at the raised head, shots at the bones.
		const ACheeseStrikeCharacter* OnServer = CharacterOf(0, 0);
		const float EyesOff = FVector::Dist(OnServer->GetEyes()->GetComponentLocation(), OnServer->GetMesh()->GetBoneLocation(TEXT("head")));
		TestTrue(FString::Printf(TEXT("the eyes are at the head (%.0f cm off)"), EyesOff), EyesOff < 18.0f);
		const FHitHistory::FSample Lying = OnServer->SampleNow(0.0f);
		TestTrue(TEXT("shots take the body to be lying"), Lying.bLying);
		for (const TCHAR* Bone : {TEXT("pelvis"), TEXT("spine_02"), TEXT("calf_r"), TEXT("foot_l")})
		{
			const FVector Target = OnServer->GetMesh()->GetBoneLocation(FName(Bone));
			const FVector From = Target + FVector(0.0f, 0.0f, 25.0f) + OnServer->GetActorRightVector() * 400.0f;
			TestTrue(FString::Printf(TEXT("a shot from the side at its %s hits it"), Bone), FHitHistory::RayHitsBody(From, (Target - From).GetSafeNormal(), Lying) > 0.0f);
		}
		const FVector Standing = OnServer->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
		TestEqual(TEXT("but a shot where a standing chest would be misses"), FHitHistory::RayHitsBody(Standing + OnServer->GetActorRightVector() * 400.0f, -OnServer->GetActorRightVector(), Lying), -1.0f);
		// The host's own view sits low too, about 40 cm up, under the crouching teammate's own.
		TestTrue(FString::Printf(TEXT("the host's view (%.0f cm) is low, under a crouching player's (%.0f cm)"), ViewHeight(Host), ViewHeight(Character(2))), ViewHeight(Host) < ViewHeight(Character(2)) - 30.0f && ViewHeight(Host) > 28.0f && ViewHeight(Host) < 52.0f);
		TestEqual(TEXT("with only the pistol in hand"), HeldId(Host), CheeseStrikeWeapons::Pistol().Id);
		TestFalse(TEXT("the rifle put away for good"), Host->GetInventory()->CanDraw(EWeaponSlot::Primary));
		TestTrue(TEXT("crouched"), Host->bIsCrouched);
		TestTrue(TEXT("the host's machine knows it is down"), Host->IsDowned());
		Remembered = Character(0);
		DownYaw = Host->GetActorRotation().Yaw;
	});
	// The host looks far aside and down: the view stays in the cone toward the feet, and the body only begins to turn.
	Play(0, TEXT("look 100 0\nlook 0 -70\nwait 0.2"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		const FRotator View = Host->GetControlRotation();
		const float Off = FRotator::NormalizeAxis(View.Yaw - Host->GetActorRotation().Yaw);
		TestTrue(FString::Printf(TEXT("the view (%.1f off the body, pitch %.1f) is kept to the cone"), Off, FRotator::NormalizeAxis(View.Pitch)), CheeseStrikeDowned::InCone(View, Host->GetActorRotation().Yaw, 1.0f));
		TestTrue(TEXT("at its lowest"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(View.Pitch), CheeseStrikeDowned::LowestPitch, 1.0f));
		const float Turned = FMath::Abs(FRotator::NormalizeAxis(Host->GetActorRotation().Yaw - DownYaw));
		TestTrue(FString::Printf(TEXT("the body turns after it, slowly (%.1f degrees)"), Turned), Turned > 1.0f && Turned < 25.0f);
		// The others see it aim where it does.
		const ACheeseStrikeCharacter* Seen = CharacterOf(0, 1);
		TestTrue(FString::Printf(TEXT("a joiner sees the host aim aside (%.0f)"), Seen != nullptr ? Seen->GetDownAimYaw() : 0.0f), Seen != nullptr && Seen->GetDownAimYaw() > 20.0f);
	});
	// It scoots on its back, toward its head, slowly.
	Play(0, TEXT("look -100 70\nhold Move 0 -1\nwait 1.0"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = Character(0);
		const float Speed = Host->GetVelocity().Size2D();
		TestTrue(FString::Printf(TEXT("scooting at %.0f cm/s, a scoot's speed"), Speed), Speed > CheeseStrikeDowned::ScootSpeed * 0.6f && Speed < CheeseStrikeDowned::ScootSpeed + 2.0f);
		TestTrue(TEXT("toward the head"), FVector::DotProduct(Host->GetVelocity(), Host->GetActorForwardVector()) < 0.0f);
	});
	Play(0, TEXT("release Move\nwait 0.3"));
	PutAt(0, HostSpot, HostYaw);
	// From four metres, more shots do nothing (aimed down at the crouched host, the recoil of the last burst undone).
	PutAt(1, JoinerSpot, JoinerYaw);
	Play(1, FString(TEXT("look 0 -10\n")) + Burst);
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = CharacterOf(0, 0);
		TestTrue(TEXT("shots from afar don't finish them"), Host != nullptr && Host->IsAlive() && Host->IsDowned());
	});
	// A teammate beside them holds Interact: back up.
	Play(2, TEXT("release Crouch"));
	PutAt(2, BesideSpot, 180.0f);
	Play(2, FString::Printf(TEXT("hold Interact\nwait %.1f\nrelease Interact"), CheeseStrikeModes::ReviveTime + 0.6f));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = CharacterOf(0, 0);
		if (TestNotNull(TEXT("the host"), Host))
		{
			TestFalse(TEXT("revived"), Host->IsDowned());
			TestEqual(TEXT("with some health"), Host->GetHealth(), CheeseStrikeModes::ReviveHealth);
			TestTrue(TEXT("and the rifle to draw again"), Host->GetInventory()->CanDraw(EWeaponSlot::Primary));
		}
	});
	WaitUntil(TEXT("every machine seeing the host up"), 3.0f, []()
	{
		for (int32 Seen = 0; Seen < 3; ++Seen)
		{
			const ACheeseStrikeCharacter* Host = CharacterOf(0, Seen);
			if (Host == nullptr || Host->IsDowned())
			{
				return false;
			}
		}
		return true;
	});
	// Getting up takes a moment, and then the body stands as before.
	Wait(1.2f);
	Then([this]()
	{
		for (int32 Seen = 0; Seen < 3; ++Seen)
		{
			const ACheeseStrikeCharacter* Host = CharacterOf(0, Seen);
			const ACheeseStrikePlayerState* Player = StateOf(0, Seen);
			if (TestNotNull(TEXT("the host"), Host))
			{
				TestTrue(FString::Printf(TEXT("up again, the pelvis (%.0f cm) well over where it was down (%.0f cm), as player %d sees it"), BoneHeight(Host, TEXT("pelvis")), DownPelvis[Seen], Seen), BoneHeight(Host, TEXT("pelvis")) > DownPelvis[Seen] + 20.0f);
				TestEqual(FString::Printf(TEXT("the row shows the host alive again, as player %d sees it"), Seen), ACheeseStrikeHUD::StatusOf(Player, ACheeseStrikeHUD::BodyOf(Player)), CheeseStrikeHUD::EStatus::Alive);
			}
		}
	});
	// The teammate goes down, and the enemy walks up and finishes them by hand.
	PutAt(0, AsideSpot, AsideYaw);
	PutAt(2, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Play(1, FString(TEXT("tap Reload\nwait 2.2\n")) + Burst);
	WaitUntil(TEXT("the teammate going down"), 5.0f, []() { const ACheeseStrikeCharacter* Mate = CharacterOf(2, 0); return Mate != nullptr && Mate->IsDowned(); });
	PutAt(1, BesideSpot, 180.0f);
	Play(1, FString::Printf(TEXT("hold Interact\nwait %.1f\nrelease Interact"), CheeseStrikeModes::ExecuteTime + 0.6f));
	WaitUntil(TEXT("the teammate finished"), 3.0f, []() { const ACheeseStrikeCharacter* Mate = CharacterOf(2, 0); return Mate != nullptr && !Mate->IsAlive(); });
	Then([this]()
	{
		const ACheeseStrikeCharacter* Mate = CharacterOf(2, 0);
		const ACheeseStrikeCharacter* Enemy = CharacterOf(1, 0);
		TestEqual(TEXT("finished by the enemy"), Mate->GetKilledBy(), Enemy->DisplayName());
		TestTrue(TEXT("and out"), StateOf(2, 0)->IsOut());
		TestEqual(TEXT("whose kill it is"), StateOf(1, 0)->GetKills(), 1);
	});
	// Down a second time in a life, the host dies; with nobody of theirs standing, the round and the match are the enemy's.
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Play(1, FString(TEXT("tap Reload\nwait 2.2\n")) + Burst);
	WaitUntil(TEXT("the match ending"), 6.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && Match->HasMatchEnded(); });
	Then([this]()
	{
		TestFalse(TEXT("the host, once revived, isn't downed again but killed"), Character(0)->IsAlive());
		TestEqual(TEXT("the other side wins"), GameState(0)->GetWinningTeam(), 1);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGunGameMovesUpTheLadderToTheKnife, "CheeseStrike.Play.GunGameMovesUpTheLadderToTheKnife", ModePlayTests::Flags)

bool FGunGameMovesUpTheLadderToTheKnife::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace ModePlayTests;
	StartPlaying({2, TEXT("?Mode=GunGame?RespawnDelay=1")});
	Then([this]()
	{
		const TArray<FName>& Ladder = CheeseStrikeArsenal::GunGameLadder();
		const ACheeseStrikeCharacter* Host = CharacterOf(0, 0);
		TestEqual(TEXT("everyone starts on the first weapon"), HeldId(Host), Ladder[0]);
		TestTrue(TEXT("with the knife besides"), Host->GetInventory()->CanDraw(EWeaponSlot::Knife));
		TestFalse(TEXT("and nothing else"), Host->GetInventory()->CanDraw(EWeaponSlot::Sidearm));
	});
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Wait(CheeseStrikeRules::SpawnProtection);
	FireUntil(1, TEXT("the host dying"), 5.0f, []() { return Dead(0); });
	Wait(0.5f);
	Then([this]()
	{
		const TArray<FName>& Ladder = CheeseStrikeArsenal::GunGameLadder();
		TestEqual(TEXT("the joiner moves a step up"), StateOf(1, 0)->GetGunLevel(), 1);
		TestEqual(TEXT("and holds the next weapon at once"), HeldId(CharacterOf(1, 0)), Ladder[1]);
		TestEqual(TEXT("on its own machine too"), HeldId(Character(1)), Ladder[1]);
		TestEqual(TEXT("the host stays where it was"), StateOf(0, 0)->GetGunLevel(), 0);
		TestFalse(TEXT("the joiner earns no streak"), StateOf(1, 0)->GetStreak() > 0);
		// Up to the last step: the knife only.
		StateOf(1, 0)->SetGunLevel(Ladder.Num() - 1);
		Mode()->SetPlayerDefaults(CharacterOf(1, 0));
	});
	WaitUntil(TEXT("the host back"), 4.0f, []() { const ACheeseStrikeCharacter* Host = Character(0); return Host != nullptr && Host->IsAlive(); });
	Then([this]()
	{
		TestEqual(TEXT("the host back on the first weapon"), HeldId(CharacterOf(0, 0)), CheeseStrikeArsenal::GunGameLadder()[0]);
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		TestEqual(TEXT("the joiner on the last step holds the knife"), HeldId(Joiner), CheeseStrikeArsenal::Knife());
		TestFalse(TEXT("and nothing else"), Joiner->GetInventory()->CanDraw(EWeaponSlot::Primary) || Joiner->GetInventory()->CanDraw(EWeaponSlot::Sidearm));
		if (ACheeseStrikeCharacter* Host = CharacterOf(0, 0))
		{
			Host->EndProtection();
		}
	});
	// A stroke in the back: a kill with the last weapon wins.
	PutAt(0, HostSpot, JoinerYaw);
	PutAt(1, BehindSpot, JoinerYaw);
	Play(1, TEXT("wait 0.5\ntap Fire\nwait 0.6"));
	WaitUntil(TEXT("the match ending"), 4.0f, []() { const ACheeseStrikeGameState* Match = GameState(0); return Match != nullptr && Match->HasMatchEnded(); });
	Then([this]()
	{
		TestEqual(TEXT("the joiner wins"), GameState(0)->GetWinnerName(), CharacterOf(1, 0)->DisplayName());
	});
	StopPlaying();
	return true;
}

#endif
