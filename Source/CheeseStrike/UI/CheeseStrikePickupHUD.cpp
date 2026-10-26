// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePickupHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "CheeseStrikeCharacter.h"
#include "Input/CheeseStrikeInput.h"
#include "Rules/CheeseStrikePickups.h"
#include "UI/CheeseStrikeHUDDraw.h"
#include "UI/CheeseStrikeHUDRules.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikePickup.h"
#include "Weapons/CheeseStrikeWeapon.h"

namespace HudDraw = CheeseStrikeHUDDraw;

namespace CheeseStrikePickupHUD
{
	FString PromptLine(const FString& Key, const FString& Offered, bool bRounds)
	{
		return bRounds ? FString::Printf(TEXT("PRESS %s  ·  TAKE THE %s'S ROUNDS"), *Key, *Offered.ToUpper()) : FString::Printf(TEXT("PRESS %s  ·  TAKE THE %s"), *Key, *Offered.ToUpper());
	}

	FString PromptUnder(const FString& Carried)
	{
		return Carried.IsEmpty() ? FString() : FString::Printf(TEXT("for your %s"), *Carried);
	}

	void Draw(AHUD* Hud, UCanvas* Canvas)
	{
		APlayerController* Player = Hud != nullptr ? Hud->PlayerOwner.Get() : nullptr;
		UWorld* World = Hud != nullptr ? Hud->GetWorld() : nullptr;
		if (Player == nullptr || World == nullptr || Canvas == nullptr || Player->PlayerCameraManager == nullptr)
		{
			return;
		}
		const float U = HudDraw::Unit(Canvas);
		const ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(Player->GetPawn());
		const bool bAlive = Character != nullptr && Character->IsAlive();

		// Over each empty stand in sight and near, the seconds until its weapon is back.
		const FVector Eye = Player->PlayerCameraManager->GetCameraLocation();
		const FVector Ahead = Player->PlayerCameraManager->GetCameraRotation().Vector();
		FCollisionQueryParams Sight(FName(TEXT("PickupSeconds")), false, Character);
		for (TActorIterator<ACheeseStrikePickup> It(World); It; ++It)
		{
			const ACheeseStrikePickup* Pickup = *It;
			const float Left = Pickup->SecondsLeft();
			const FVector Over = Pickup->FloorAt() + FVector(0.0f, 0.0f, CheeseStrikePickups::FloatHeight + 45.0f);
			const float Distance = FVector::Dist(Eye, Over);
			if (!Pickup->IsInPlay() || Pickup->IsDropped() || Left <= 0.0f || Distance > CheeseStrikePickups::SecondsShownWithin || FVector::DotProduct(Over - Eye, Ahead) <= 10.0f)
			{
				continue;
			}
			FHitResult Hit;
			Sight.ClearIgnoredSourceObjects();
			Sight.AddIgnoredActor(Character);
			if (World->LineTraceSingleByChannel(Hit, Eye, Over, ECC_Visibility, Sight))
			{
				continue;
			}
			const FVector OnScreen = Canvas->Project(Over);
			const float Scale = FMath::GetMappedRangeValueClamped(FVector2D(300.0f, CheeseStrikePickups::SecondsShownWithin), FVector2D(1.0f, 0.55f), Distance);
			const float Alpha = FMath::GetMappedRangeValueClamped(FVector2D(CheeseStrikePickups::SecondsShownWithin * 0.7f, CheeseStrikePickups::SecondsShownWithin), FVector2D(1.0f, 0.0f), Distance);
			const FLinearColor Colour = HudDraw::Faded(CheeseStrikePickups::WorthColour(CheeseStrikePickups::Worth(Pickup->GetWeapon())), 0.9f * Alpha);
			const FSlateFontInfo Font = HudDraw::Font(24.0f * U * Scale);
			const FString Seconds = FString::FromInt(CheeseStrikePickups::ShownSeconds(Left));
			HudDraw::Text(Canvas, Seconds, FVector2D(OnScreen.X, OnScreen.Y - HudDraw::Measure(Seconds, Font).Y / 2.0f), Font, Colour, 0.5f);
		}

		// In reach of a weapon to take: what pressing Interact does.
		const UCheeseStrikeInventory* Inventory = Character != nullptr ? Character->GetInventory() : nullptr;
		const ACheeseStrikePickup* Offered = bAlive && Inventory != nullptr ? ACheeseStrikePickup::OfferedTo(Character) : nullptr;
		if (Offered == nullptr)
		{
			return;
		}
		const ACheeseStrikeWeapon* Carried = Inventory->GetInSlot(EWeaponSlot::Primary);
		const bool bRounds = Offered->HowFor(Character) == CheeseStrikePickups::ETake::TopUp;
		const FString Line = PromptLine(UCheeseStrikeInputSet::Prompt(Player, &UCheeseStrikeInputSet::Interact), Offered->GetWeaponName().ToString(), bRounds);
		const FString Under = bRounds ? FString() : PromptUnder(Carried != nullptr ? Carried->GetStats().DisplayName.ToString() : FString());
		const float Middle = Canvas->SizeX / 2.0f;
		const float Y = Canvas->SizeY * 0.64f;
		const FSlateFontInfo Big = HudDraw::Font(24.0f * U);
		const FSlateFontInfo Small = HudDraw::Font(16.0f * U);
		const FVector2D LineSize = HudDraw::Measure(Line, Big);
		HudDraw::FShapes Shapes;
		// A rounded card, its colour strip along the top from side to side, clipped to the card's corners.
		const FVector2D CardMin(Middle - LineSize.X / 2.0f - 14.0f * U, Y - 6.0f * U);
		const FVector2D CardMax = CardMin + FVector2D(LineSize.X + 28.0f * U, LineSize.Y + 30.0f * U);
		Shapes.RoundedRect(CardMin.X, CardMin.Y, CardMax.X - CardMin.X, CardMax.Y - CardMin.Y, 8.0f * U, HudDraw::Panel);
		const FLinearColor Worth = CheeseStrikePickups::WorthColour(CheeseStrikePickups::Worth(Offered->GetWeapon()));
		Shapes.SmoothPolygon(CheeseStrikeHUD::ClipToRect(CheeseStrikeHUD::RoundedRect(CardMin, CardMax, 8.0f * U), CardMin, FVector2D(CardMax.X, CardMin.Y + 4.0f * U)), Worth);
		Shapes.Draw(Canvas);
		HudDraw::Text(Canvas, Line, FVector2D(Middle, Y), Big, HudDraw::Ink, 0.5f);
		HudDraw::Text(Canvas, Under, FVector2D(Middle, Y + LineSize.Y + 2.0f * U), Small, HudDraw::Soft, 0.5f);
	}

	void DrawOnMap(HudDraw::FShapes& Shapes, UWorld* World, TFunctionRef<FVector2D(const FVector&)> OnMap, const FVector2D& Min, const FVector2D& Max, float U)
	{
		if (World == nullptr)
		{
			return;
		}
		for (TActorIterator<ACheeseStrikePickup> It(World); It; ++It)
		{
			const ACheeseStrikePickup* Pickup = *It;
			const FVector2D At = OnMap(Pickup->FloorAt());
			const float Size = 5.0f * U;
			if (!Pickup->IsInPlay() || Pickup->IsDropped() || At.X - Size < Min.X || At.Y - Size < Min.Y || At.X + Size > Max.X || At.Y + Size > Max.Y)
			{
				continue;
			}
			// A diamond, ringed in black to stand off the map; faint while empty.
			const bool bThere = Pickup->IsThere();
			const FLinearColor Colour = HudDraw::Faded(CheeseStrikePickups::WorthColour(CheeseStrikePickups::Worth(Pickup->GetWeapon())), bThere ? 0.95f : 0.3f);
			const float Ring = Size + 1.5f * U;
			Shapes.Quad(At + FVector2D(0.0f, -Ring), At + FVector2D(Ring, 0.0f), At + FVector2D(0.0f, Ring), At + FVector2D(-Ring, 0.0f), FLinearColor(0.0f, 0.0f, 0.0f, bThere ? 0.6f : 0.25f));
			Shapes.Quad(At + FVector2D(0.0f, -Size), At + FVector2D(Size, 0.0f), At + FVector2D(0.0f, Size), At + FVector2D(-Size, 0.0f), Colour);
		}
	}
}
