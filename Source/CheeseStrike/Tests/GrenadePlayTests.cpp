// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the cheese grenade in the editor (see PlayTesting.h): a
// player comes in with grenades, throws one with its key over the network,
// the count going down on the server and on the thrower's machine; the
// grenade flies, bounces and bursts after its fuse, hurting the enemy near
// it; one at its thrower's feet kills them, as nobody's kill; and a wall between the burst and someone saves
// them. Held, a throw shows its arc and where it comes down, goes as the key is let go and lands there, and is put
// away by drawing another weapon.

#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerController.h"
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
	/** Open ground well away from both. */
	const FVector AsideSpot(1150.0f, -1450.0f, 100.0f);

	/** Remembered between steps. */
	TWeakObjectPtr<ACheeseStrikeGrenade> Thrown;
	int32 MostBounces = 0;
	int32 KillsBefore = 0;
	int32 DeathsBefore = 0;
	TWeakObjectPtr<AActor> Backstop;

	/** Player `Of`'s state, on the server. */
	ACheeseStrikePlayerState* StateOf(int32 Of)
	{
		const ACheeseStrikeCharacter* Body = PlayTesting::CharacterOf(Of, 0);
		return Body != nullptr ? Body->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	}

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
	StartPlaying({2, TEXT("?RespawnDelay=1")});
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
	});
	// The thrower is hurt by their own, as anyone: one at their feet (off a wall just ahead) kills them, as their own doing, nobody's kill. The
	// host goes well out of the way.
	PutAt(0, AsideSpot, 0.0f);
	Then([]()
	{
		// Hurt already, so that wherever at their feet it comes to rest, it kills.
		CharacterOf(1, 0)->SetHealth(30.0f);
		// Looking down at their feet, a wall just ahead, so that it bounces back to them rather than on.
		Controller(1)->SetControlRotation(FRotator(-89.0f, JoinerYaw, 0.0f));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		AStaticMeshActor* Wall = World(0)->SpawnActor<AStaticMeshActor>(Joiner->GetActorLocation() + FVector(0.0f, 130.0f, 0.0f), FRotator::ZeroRotator, Params);
		Wall->SetMobility(EComponentMobility::Movable);
		Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetActorScale3D(FVector(4.0f, 0.2f, 4.0f));
		Backstop = Wall;
		KillsBefore = StateOf(0)->GetKills();
		DeathsBefore = StateOf(1)->GetDeaths();
	});
	Play(1, TEXT("tap Grenade\nwait 0.5"));
	WaitUntil(TEXT("the thrower dying by their own grenade"), 5.0f, []() { const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0); return Joiner == nullptr || !Joiner->IsAlive(); });
	Then([this]()
	{
		const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0);
		TestTrue(TEXT("their own grenade at their feet killed them"), Joiner != nullptr && !Joiner->IsAlive());
		TestEqual(TEXT("as nobody's kill"), Joiner != nullptr ? Joiner->GetKilledBy() : FString(TEXT("?")), FString());
		TestEqual(TEXT("saying how"), Joiner != nullptr ? Joiner->GetDiedHow() : FString(), FString(CheeseStrikeGrenades::OwnWords));
		TestEqual(TEXT("a death for them"), StateOf(1)->GetDeaths(), DeathsBefore + 1);
		TestEqual(TEXT("and no kill for anyone"), StateOf(0)->GetKills(), KillsBefore);
		if (Backstop.IsValid())
		{
			Backstop->Destroy();
		}
	});
	WaitUntil(TEXT("the joiner back"), 6.0f, []() { const ACheeseStrikeCharacter* Joiner = CharacterOf(1, 0); return Joiner != nullptr && Joiner->IsAlive(); });
	PutAt(1, JoinerSpot, JoinerYaw);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHeldThrowShowsItsArcAndGoesOnRelease, "CheeseStrike.Play.AHeldThrowShowsItsArcAndGoesOnRelease", GrenadePlayTests::Flags)

bool FAHeldThrowShowsItsArcAndGoesOnRelease::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace GrenadePlayTests;
	static FVector Predicted = FVector::ZeroVector;
	static FVector FirstBounce = FVector::ZeroVector;
	Thrown = nullptr;
	StartPlaying({2, TEXT("?RespawnDelay=1")});
	// The joiner throws, over the network; the host well out of the way.
	PutAt(0, AsideSpot, 0.0f);
	PutAt(1, JoinerSpot, JoinerYaw);
	WaitUntil(TEXT("the joiner's machine knowing its grenades"), 3.0f, []() { return CountOf(Character(1)) == CheeseStrikeGrenades::SpawnCount; });
	// Held: the arm wound back, the arc shown with where it comes down, nothing thrown.
	Play(1, TEXT("look 0 -25\nhold Grenade\nwait 0.6"));
	Then([this]()
	{
		const UCheeseStrikeGrenades* Mine = Character(1)->GetGrenades();
		TestTrue(TEXT("held, the throw waits"), Mine->IsHolding() && Mine->IsBusy());
		TestTrue(FString::Printf(TEXT("its arc is shown (%d dots)"), Mine->GetPreview().Num()), Mine->GetPreview().Num() >= 10);
		TestTrue(TEXT("with where it comes down"), Mine->GetPreviewLanding(Predicted));
		TestTrue(FString::Printf(TEXT("a way off, on the floor (%.0f cm)"), FVector::Dist2D(Predicted, Character(1)->GetActorLocation())),
			FVector::Dist2D(Predicted, Character(1)->GetActorLocation()) > 300.0f);
		TestEqual(TEXT("nothing thrown yet"), Grenades(0).Num(), 0);
		TestEqual(TEXT("and none spent"), CountOf(CharacterOf(1, 0)), CheeseStrikeGrenades::SpawnCount);
	});
	// Let go: thrown, along the arc shown.
	Play(1, TEXT("release Grenade\nwait 0.6"));
	Then([this]()
	{
		TestFalse(TEXT("let go, the arc goes"), Character(1)->GetGrenades()->IsHolding() || Character(1)->GetGrenades()->GetPreview().Num() > 0);
		const TArray<ACheeseStrikeGrenade*> Flying = Grenades(0);
		TestEqual(TEXT("one grenade flies on the server"), Flying.Num(), 1);
		Thrown = Flying.Num() > 0 ? Flying[0] : nullptr;
		TestEqual(TEXT("one spent"), CountOf(CharacterOf(1, 0)), CheeseStrikeGrenades::SpawnCount - 1);
	});
	WaitUntil(TEXT("its first bounce"), 3.0f, []()
	{
		const ACheeseStrikeGrenade* Grenade = Thrown.Get();
		if (Grenade != nullptr && Grenade->GetBounces() > 0)
		{
			FirstBounce = Grenade->GetFirstBounceAt();
			return true;
		}
		return Grenade == nullptr;
	});
	Then([this]()
	{
		TestTrue(FString::Printf(TEXT("it comes down where the arc said (%.0f cm off: %s against %s)"), FVector::Dist2D(FirstBounce, Predicted), *FirstBounce.ToCompactString(), *Predicted.ToCompactString()), FVector::Dist2D(FirstBounce, Predicted) < 120.0f);
	});
	WaitUntil(TEXT("the grenade gone"), 6.0f, []() { return Grenades(0).Num() == 0; });
	// Held, then a weapon drawn: put away, still carried, and letting go throws nothing.
	Play(1, TEXT("hold Grenade\nwait 0.4\ntap Sidearm\nwait 0.3"));
	Then([this]()
	{
		TestFalse(TEXT("drawing another weapon puts the throw away"), Character(1)->GetGrenades()->IsHolding());
		TestEqual(TEXT("its arc gone"), Character(1)->GetGrenades()->GetPreview().Num(), 0);
	});
	Play(1, TEXT("release Grenade\nwait 0.8"));
	Then([this]()
	{
		TestEqual(TEXT("letting go then throws nothing"), Grenades(0).Num(), 0);
		TestEqual(TEXT("and the grenade is still carried"), CountOf(CharacterOf(1, 0)), CheeseStrikeGrenades::SpawnCount - 1);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHeldGrenadeIsSeenInTheRightHand, "CheeseStrike.Play.AHeldGrenadeIsSeenInTheRightHand", GrenadePlayTests::Flags)

bool FAHeldGrenadeIsSeenInTheRightHand::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	StartPlaying({1, TEXT("")});
	Play(0, TEXT("hold Grenade\nwait 0.8"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = Character(0);
		if (!TestNotNull(TEXT("a character"), Me) || !TestNotNull(TEXT("with grenades"), Me->GetGrenades()))
		{
			return;
		}
		const UStaticMeshComponent* Held = Me->GetGrenades()->GetHeldInView();
		if (!TestNotNull(TEXT("the held grenade is made"), Held))
		{
			return;
		}
		const USkeletalMeshComponent* Body = Me->GetFirstPersonMesh();
		const FTransform Eye(Me->GetFirstPersonCameraComponent()->GetComponentRotation(), Me->GetFirstPersonCameraComponent()->GetComponentLocation());
		const FVector InView = Eye.InverseTransformPosition(Held->GetComponentLocation());
		const FVector RightHand = Eye.InverseTransformPosition(Body->GetBoneLocation(TEXT("hand_r")));
		const FVector LeftHand = Eye.InverseTransformPosition(Body->GetBoneLocation(TEXT("hand_l")));
		AddInfo(FString::Printf(TEXT("held grenade: visible %d hidden %d registered %d rendered %d parent %s socket %s fp-type %d owner-only %d scale %.3f world scale %.3f bounds %.1f cm; in the view at (%.1f, %.1f, %.1f); hand_r at (%.1f, %.1f, %.1f), hand_l at (%.1f, %.1f, %.1f); mesh %s materials %d"),
			Held->IsVisible(), Held->bHiddenInGame, Held->IsRegistered(), Held->IsRenderStateCreated(), *GetNameSafe(Held->GetAttachParent()), *Held->GetAttachSocketName().ToString(), static_cast<int32>(Held->FirstPersonPrimitiveType), Held->bOnlyOwnerSee,
			Held->GetRelativeScale3D().X, Held->GetComponentScale().X, Held->Bounds.SphereRadius * 2.0f, InView.X, InView.Y, InView.Z, RightHand.X, RightHand.Y, RightHand.Z, LeftHand.X, LeftHand.Y, LeftHand.Z, *GetNameSafe(Held->GetStaticMesh()), Held->GetNumMaterials()));
		TestTrue(TEXT("shown"), Held->IsVisible());
		TestTrue(TEXT("in the first-person body's hand"), Held->GetAttachParent() == Body);
		TestEqual(TEXT("the right"), Held->GetAttachSocketName(), FName(TEXT("hand_r")));
		TestTrue(TEXT("a grenade's size"), Held->Bounds.SphereRadius > 2.0f && Held->Bounds.SphereRadius < 20.0f);
		TestTrue(FString::Printf(TEXT("near that hand (%.1f cm)"), FVector::Dist(InView, RightHand)), FVector::Dist(InView, RightHand) < 45.0f);
		// What the first-person view shows is within its field of view (76 degrees across, 16 by 9 at the narrowest, 21 by 9):
		// the tangents of the angles off the view's middle are at most 0.78 across and 0.33 up and down. It was at 0.42 down, off the screen.
		TestTrue(TEXT("ahead of the eye"), InView.X > 15.0f && InView.X < 120.0f);
		TestTrue(FString::Printf(TEXT("on the right of the view, within the screen (across %.2f, down %.2f)"), InView.Y / InView.X, InView.Z / InView.X), InView.Y > 0.0f && InView.Y / InView.X < 0.6f && FMath::Abs(InView.Z / InView.X) < 0.3f);
	});
	Play(0, TEXT("release Grenade\nwait 1.0"));
	StopPlaying();
	return true;
}

#endif
