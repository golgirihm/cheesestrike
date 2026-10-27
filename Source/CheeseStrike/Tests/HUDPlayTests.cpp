// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The HUD in a match over the network (see PlayTesting.h): a hit reaches the
// hurt player's machine as an arc pointing at the shooter, the minimap reads
// the level and shows an enemy where they fired or were swept, never just for being seen, and the row of players
// says who is coming back.

#include "Misc/AutomationTest.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"
#include "UI/CheeseStrikeHUD.h"
#include "UI/CheeseStrikeMinimap.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace HUDPlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** Two spots facing each other across 4 m of clear ground on the shooter level. */
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	constexpr float HostYaw = -90.0f;
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);
	constexpr float JoinerYaw = 90.0f;

	ACheeseStrikeHUD* HUD(int32 Player)
	{
		const ACheeseStrikePlayerController* Controller = PlayTesting::Controller(Player);
		return Controller != nullptr ? Cast<ACheeseStrikeHUD>(Controller->GetHUD()) : nullptr;
	}

	/** Which way `Player`'s only arc points now, as their view has it (degrees clockwise from ahead); -1000 without one. */
	float ArcAngle(int32 Player)
	{
		const ACheeseStrikeHUD* Hud = HUD(Player);
		const ACheeseStrikePlayerController* Controller = PlayTesting::Controller(Player);
		if (Hud == nullptr || Controller == nullptr || Controller->PlayerCameraManager == nullptr || Hud->GetDamageArcs().Num() != 1)
		{
			return -1000.0f;
		}
		const FVector Eye = Controller->PlayerCameraManager->GetCameraLocation();
		const FVector From = ACheeseStrikeHUD::ArcSource(Hud->GetDamageArcs()[0]);
		return CheeseStrikeHUD::DamageArcAngle(Controller->PlayerCameraManager->GetCameraRotation().Yaw, FVector2D(Eye.X, Eye.Y), FVector2D(From.X, From.Y));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHUDShowsWhereHitsComeFrom, "CheeseStrike.Play.TheHUDShowsWhereHitsComeFrom", HUDPlayTests::Flags)

bool FTheHUDShowsWhereHitsComeFrom::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	StartPlaying({2});
	PutAt(0, HUDPlayTests::HostSpot, HUDPlayTests::HostYaw);
	PutAt(1, HUDPlayTests::JoinerSpot, HUDPlayTests::JoinerYaw);
	Then([this]()
	{
		// The minimap's picture of the level, as the joiner's would be: walls about the floor they stand on.
		FCheeseStrikeMinimap Map;
		Map.Gather(World(1));
		const ACheeseStrikeCharacter* Joiner = Character(1);
		if (!TestNotNull(TEXT("the joiner has a character"), Joiner))
		{
			return;
		}
		const float Feet = Joiner->GetActorLocation().Z - Joiner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		int32 Floors = 0;
		int32 Walls = 0;
		for (const FCheeseStrikeMinimap::FPiece& Piece : Map.GetPieces())
		{
			const CheeseStrikeHUD::ELayer Layer = CheeseStrikeHUD::LayerFor(Piece.Bottom, Piece.Top, Feet, Piece.bRamp, Piece.bWide);
			Floors += Layer == CheeseStrikeHUD::ELayer::Floor ? 1 : 0;
			Walls += Layer == CheeseStrikeHUD::ELayer::Wall ? 1 : 0;
		}
		TestTrue(FString::Printf(TEXT("the minimap reads the level (%d pieces: %d floors and %d walls at the joiner's height)"), Map.GetPieces().Num(), Floors, Walls), Floors > 0 && Walls > 10);
		TestTrue(TEXT("the joiner has the HUD"), HUDPlayTests::HUD(1) != nullptr);
		TestFalse(TEXT("the two are on different sides"), ACheeseStrikeHUD::AreTeammates(Controller(0)->PlayerState, Controller(1)->PlayerState));
		TestTrue(TEXT("and each is their own side"), ACheeseStrikeHUD::AreTeammates(Controller(1)->PlayerState, Controller(1)->PlayerState));
	});
	// Facing the host 4 m away, the joiner sees them, but the minimap doesn't show them for that: only a shot or a sweep does.
	Wait(CheeseStrikeRules::SpawnProtection);
	Then([this]()
	{
		const ACheeseStrikeHUD* Hud = HUDPlayTests::HUD(1);
		TestFalse(TEXT("an enemy merely in sight is not on the minimap"), Hud == nullptr || Hud->ShowsOnMinimap(CharacterOf(0, 1)));
	});
	Then([this]() { TestEqual(TEXT("no arcs before a hit"), HUDPlayTests::HUD(1)->GetDamageArcs().Num(), 0); });
	// The host shoots the joiner once; the joiner's machine hears where from.
	Play(0, TEXT("tap Fire"));
	WaitUntil(TEXT("the hit reaching the joiner"), 5.0f, []() { const ACheeseStrikeCharacter* Joiner = Character(1); return Joiner != nullptr && Joiner->GetHealth() < 100.0f; });
	WaitUntil(TEXT("the arc"), 3.0f, []() { return HUDPlayTests::HUD(1)->GetDamageArcs().Num() > 0; });
	Then([this]()
	{
		const ACheeseStrikeHUD* Hud = HUDPlayTests::HUD(1);
		const ACheeseStrikeCharacter* HostSeen = CharacterOf(0, 1);
		if (!TestEqual(TEXT("one arc for one shooter"), Hud->GetDamageArcs().Num(), 1) || !TestNotNull(TEXT("the joiner sees the host"), HostSeen))
		{
			return;
		}
		const float Off = FVector::Dist2D(ACheeseStrikeHUD::ArcSource(Hud->GetDamageArcs()[0]), HostSeen->GetActorLocation());
		TestTrue(FString::Printf(TEXT("it comes from the host (%.0f cm off)"), Off), Off < 100.0f);
		const float Angle = HUDPlayTests::ArcAngle(1);
		TestTrue(FString::Printf(TEXT("and points ahead, where the host is (%.0f degrees)"), Angle), FMath::Abs(Angle) < 15.0f);
		TestEqual(TEXT("the host, unhurt, has none"), HUDPlayTests::HUD(0)->GetDamageArcs().Num(), 0);
	});
	// Turned a quarter to the right, the host is on the joiner's left, and soon out of their sight.
	Play(1, TEXT("look 90 0"));
	Then([this]()
	{
		const float Angle = HUDPlayTests::ArcAngle(1);
		TestTrue(FString::Printf(TEXT("turned right, the arc points left (%.0f degrees)"), Angle), FMath::Abs(Angle + 90.0f) < 15.0f);
	});
	Wait(CheeseStrikeHUD::EnemyMarkLife + 0.5f);
	Then([this]()
	{
		ACheeseStrikeHUD* Hud = HUDPlayTests::HUD(1);
		const ACheeseStrikeCharacter* HostSeen = CharacterOf(0, 1);
		TestFalse(TEXT("firing no more, the host leaves the minimap"), Hud->ShowsOnMinimap(HostSeen));
		TestEqual(TEXT("the arc is gone with its time"), Hud->GetDamageArcs().Num(), 0);
	});
	// The host fires again: the joiner's minimap shows them where they fired from, not following them after.
	Play(0, TEXT("tap Fire"));
	WaitUntil(TEXT("the host's shot putting them on the joiner's minimap"), 3.0f, []() { return HUDPlayTests::HUD(1)->EnemyMark(CharacterOf(0, 1)) != nullptr; });
	Then([this]()
	{
		ACheeseStrikeHUD* Hud = HUDPlayTests::HUD(1);
		const ACheeseStrikeCharacter* HostSeen = CharacterOf(0, 1);
		const CheeseStrikeHUD::FEnemyMark* Mark = Hud->EnemyMark(HostSeen);
		if (TestNotNull(TEXT("the host's mark"), Mark))
		{
			const float Off = FVector::Dist2D(Mark->Where, HostSeen->GetActorLocation());
			TestTrue(FString::Printf(TEXT("where the host fired from (%.0f cm off)"), Off), Off < 100.0f);
			TestFalse(TEXT("heard, not swept"), Mark->bSwept);
		}
		Hud->RevealEnemies(5.0f);
		TestTrue(TEXT("revealed, the host is on it unseen"), Hud->ShowsOnMinimap(HostSeen));
	});
	// The host calls in an Unmanned Aerial Cheese: its sweeps mark the joiner, an enemy in a free-for-all, for the host's minimap.
	Play(0, TEXT("cmd CheeseStrikeGiveStreak 1\ntap Streak"));
	WaitUntil(TEXT("the host's sweep marking the joiner"), 5.0f, []()
	{
		const ACheeseStrikePlayerState* Host = Controller(0) != nullptr ? Controller(0)->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
		return Host != nullptr && Host->SweepMarks.Num() > 0;
	});
	Then([this]()
	{
		const ACheeseStrikePlayerState* Host = Controller(0)->GetPlayerState<ACheeseStrikePlayerState>();
		const ACheeseStrikeCharacter* JoinerSeen = CharacterOf(1, 0);
		if (TestEqual(TEXT("one enemy, one mark"), Host->SweepMarks.Num(), 1) && TestNotNull(TEXT("the host sees the joiner"), JoinerSeen))
		{
			const float Off = FVector::Dist2D(Host->SweepMarks[0], JoinerSeen->GetActorLocation());
			TestTrue(FString::Printf(TEXT("where the joiner is (%.0f cm off)"), Off), Off < 100.0f);
		}
		TestTrue(TEXT("the joiner's own sweeps mark nobody"), Controller(1)->GetPlayerState<ACheeseStrikePlayerState>()->SweepMarks.Num() == 0);
	});
	// Killed, the joiner shows as coming back in the row along the top, on the host's machine and their own.
	Play(1, TEXT("cmd CheeseStrikeDie"));
	WaitUntil(TEXT("the joiner dead, as both see it"), 5.0f, []()
	{
		const ACheeseStrikeCharacter* AsHostSees = CharacterOf(1, 0);
		const ACheeseStrikeCharacter* AsJoinerSees = CharacterOf(1, 1);
		return AsHostSees != nullptr && !AsHostSees->IsAlive() && AsJoinerSees != nullptr && !AsJoinerSees->IsAlive() && AsJoinerSees->GetDiedAt() > 0.0f;
	});
	Then([this]()
	{
		for (int32 Seen = 0; Seen < 2; ++Seen)
		{
			const ACheeseStrikeGameState* Match = GameState(Seen);
			const ACheeseStrikeCharacter* JoinerBody = CharacterOf(1, Seen);
			const ACheeseStrikeCharacter* HostBody = CharacterOf(0, Seen);
			if (!TestNotNull(TEXT("the joiner's body"), JoinerBody) || !TestNotNull(TEXT("the host"), HostBody) || !TestNotNull(TEXT("the match"), Match))
			{
				continue;
			}
			const ACheeseStrikePlayerState* Joiner = JoinerBody->GetPlayerState<ACheeseStrikePlayerState>();
			const ACheeseStrikePlayerState* Host = HostBody->GetPlayerState<ACheeseStrikePlayerState>();
			if (!TestNotNull(TEXT("the joiner's player"), Joiner) || !TestNotNull(TEXT("the host's player"), Host))
			{
				continue;
			}
			const ACheeseStrikeCharacter* Body = ACheeseStrikeHUD::BodyOf(Joiner);
			TestFalse(TEXT("nobody is out"), Joiner->IsOut());
			TestEqual(FString::Printf(TEXT("the joiner is coming back, as player %d sees it"), Seen), ACheeseStrikeHUD::StatusOf(Joiner, Body), CheeseStrikeHUD::EStatus::Respawning);
			if (Body != nullptr)
			{
				const int32 Back = CheeseStrikeHUD::SecondsToComeBack(Match->GetRespawnDelay(), Match->GetServerWorldTimeSeconds(), Body->GetDiedAt());
				TestTrue(FString::Printf(TEXT("in a few seconds (%d)"), Back), Back > 0 && Back <= FMath::CeilToInt(Match->GetRespawnDelay()));
			}
			TestEqual(FString::Printf(TEXT("the host is alive, as player %d sees it"), Seen), ACheeseStrikeHUD::StatusOf(Host, ACheeseStrikeHUD::BodyOf(Host)), CheeseStrikeHUD::EStatus::Alive);
		}
	});
	StopPlaying();
	return true;
}

#endif
