// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the cheese grenade in the editor (see PlayTesting.h): a
// player comes in with grenades, throws one with its key over the network,
// the count going down on the server and on the thrower's machine; the
// grenade flies, bounces and bursts after its fuse, hurting the enemy near
// it and never its thrower; and a wall between the burst and someone saves
// them.

#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerState.h"
#include "PlayTesting.h"
#include "Weapons/CheeseStrikeGrenade.h"
#include "Weapons/CheeseStrikeGrenadeRules.h"
#include "Weapons/CheeseStrikeGrenades.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace GrenadePlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** Two spots facing each other across 4 m of clear ground on the shooter level (as PlayTests' own). */
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	constexpr float HostYaw = -90.0f;
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);
	constexpr float JoinerYaw = 90.0f;

	/** Remembered between steps. */
	TWeakObjectPtr<ACheeseStrikeGrenade> Thrown;
	int32 MostBounces = 0;

	/** The grenades flying in `Player`'s world. */
	TArray<ACheeseStrikeGrenade*> Grenades(int32 Player)
	{
		TArray<ACheeseStrikeGrenade*> Found;
		if (UWorld* World = PlayTesting::World(Player))
		{
			for (TActorIterator<ACheeseStrikeGrenade> It(World); It; ++It)
			{
				Found.Add(*It);
			}
		}
		return Found;
	}

	int32 CountOf(const ACheeseStrikeCharacter* Character)
	{
		return Character != nullptr && Character->GetGrenades() != nullptr ? Character->GetGrenades()->GetCount() : -1;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAGrenadeIsThrownAndBursts, "CheeseStrike.Play.AGrenadeIsThrownAndBursts", GrenadePlayTests::Flags)

bool FAGrenadeIsThrownAndBursts::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace GrenadePlayTests;
	Thrown = nullptr;
	MostBounces = 0;
	StartPlaying({2});
	Then([this]()
	{
		TestEqual(TEXT("the host comes in with its grenades"), CountOf(CharacterOf(0, 0)), CheeseStrikeGrenades::SpawnCount);
		TestEqual(TEXT("so does the joiner, on the server"), CountOf(CharacterOf(1, 0)), CheeseStrikeGrenades::SpawnCount);
	});
	WaitUntil(TEXT("the joiner's machine knowing its grenades"), 3.0f, []() { return CountOf(Character(1)) == CheeseStrikeGrenades::SpawnCount; });
	PutAt(0, HostSpot, HostYaw);
	PutAt(1, JoinerSpot, JoinerYaw);
	Then([]()
	{
		CharacterOf(0, 0)->EndProtection();
	});
	// The joiner throws one at the floor between them, toward the host.
	Play(1, TEXT("look 0 -40\ntap Grenade\nwait 0.7"));
	Then([this]()
	{
		const TArray<ACheeseStrikeGrenade*> Flying = Grenades(0);
		TestEqual(TEXT("one grenade flies on the server"), Flying.Num(), 1);
		if (Flying.Num() > 0)
		{
			Thrown = Flying[0];
			TestTrue(TEXT("thrown by the joiner"), Flying[0]->GetThrower() == CharacterOf(1, 0)->GetPlayerState<ACheeseStrikePlayerState>());
			TestFalse(TEXT("not burst yet"), Flying[0]->HasBurst());
		}
		TestEqual(TEXT("the joiner has one fewer, on the server"), CountOf(CharacterOf(1, 0)), CheeseStrikeGrenades::SpawnCount - 1);
		TestEqual(TEXT("the host still has its own"), CountOf(CharacterOf(0, 0)), CheeseStrikeGrenades::SpawnCount);
		TestEqual(TEXT("the joiner's machine sees it too"), Grenades(1).Num(), 1);
		TestEqual(TEXT("and its count"), CountOf(Character(1)), CheeseStrikeGrenades::SpawnCount - 1);
	});
	WaitUntil(TEXT("the grenade bursting"), 4.0f, []()
	{
		const ACheeseStrikeGrenade* Grenade = Thrown.Get();
		MostBounces = Grenade != nullptr ? FMath::Max(MostBounces, Grenade->GetBounces()) : MostBounces;
		return Grenade == nullptr || Grenade->HasBurst();
	});
	Then([this]()
	{
		TestTrue(FString::Printf(TEXT("it bounced on its way (%d)"), MostBounces), MostBounces >= 1);
		TestFalse(TEXT("the thrower's hands are free again"), Character(1)->GetGrenades()->IsThrowing());
		const ACheeseStrikeCharacter* Host = CharacterOf(0, 0);
		TestTrue(FString::Printf(TEXT("the burst hurt the host near it (health %.0f)"), Host->GetHealth()), Host->GetHealth() < CheeseStrikeWeapons::MaxHealth);
		TestEqual(TEXT("and never its thrower"), CharacterOf(1, 0)->GetHealth(), CheeseStrikeWeapons::MaxHealth);
	});
	// A wall between a burst and someone saves them; without it, it would have hurt them.
	Then([this]()
	{
		UWorld* Server = World(0);
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		const FVector At = Joiner->GetActorLocation() + FVector(0.0f, 200.0f, -40.0f);
		float Distance = 0.0f;
		const float Open = ACheeseStrikeGrenade::HarmAt(Server, At, Joiner, Distance);
		TestTrue(FString::Printf(TEXT("in the open, a burst 2 m off hurts (%.0f at %.0f cm)"), Open, Distance), Open > 0.0f);
		TestEqual(TEXT("by its falloff"), Open, CheeseStrikeGrenades::DamageAt(Distance), 0.01f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Wall = Server->SpawnActor<AStaticMeshActor>(Joiner->GetActorLocation() + FVector(0.0f, 100.0f, 0.0f), FRotator::ZeroRotator, Params);
		Wall->SetMobility(EComponentMobility::Movable);
		Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetActorScale3D(FVector(4.0f, 0.2f, 4.0f));
		const float Walled = ACheeseStrikeGrenade::HarmAt(Server, At, Joiner, Distance);
		TestEqual(TEXT("behind a wall, nothing"), Walled, 0.0f);
		Wall->Destroy();
		TestEqual(TEXT("and past its reach, nothing"), ACheeseStrikeGrenade::HarmAt(Server, Joiner->GetActorLocation() + FVector(0.0f, CheeseStrikeGrenades::Radius + 300.0f, 0.0f), Joiner, Distance), 0.0f);
	});
	StopPlaying();
	return true;
}

#endif
