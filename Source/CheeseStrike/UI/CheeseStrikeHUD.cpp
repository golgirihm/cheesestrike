// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHUD.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "GameFramework/GameMode.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "CheeseStrikeHUDDraw.h"
#include "CheeseStrikeMenu.h"
#include "Input/CheeseStrikeInput.h"
#include "Rules/CheeseStrikeMice.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeModes.h"
#include "UI/CheeseStrikeModeHUD.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Map/CheeseStrikePlace.h"
#include "RHIGlobals.h"
#include "CheeseStrike.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "Weapons/CheeseStrikeWeapon.h"

/** The HUD's drawing, named through an alias rather than used as a whole: the packaged game's unity build would carry a using-directive into the files after this one. */
namespace HudDraw = CheeseStrikeHUDDraw;

/** For judging the HUD from screenshots (development): the health it shows, and arcs held as they are. */
static TAutoConsoleVariable<float> CVarHealthShown(TEXT("CheeseStrike.HUD.Health"), -1.0f, TEXT("Show this health on the HUD and its red edges instead of the character's (-1: the character's)."));
static TAutoConsoleVariable<int32> CVarHoldArcs(TEXT("CheeseStrike.HUD.HoldArcs"), 0, TEXT("Hold the damage arcs as they are, neither fading nor going (1), for screenshots."));

float ACheeseStrikeHUD::PixelsForDegrees(float Degrees, float FieldOfView, float Width)
{
	const float Focal = (Width / 2.0f) / FMath::Tan(FMath::DegreesToRadians(FieldOfView / 2.0f));
	return Focal * FMath::Tan(FMath::DegreesToRadians(Degrees));
}

float ACheeseStrikeHUD::Unit() const
{
	return HudDraw::Unit(Canvas);
}

void ACheeseStrikeHUD::BeginPlay()
{
	Super::BeginPlay();
	// The engine's messages on the screen (its debug messages, the map's and the renderer's warnings, such as video memory
	// running out) are for developers: in the game they are kept off the player's screen, as the engine's own
	// DISABLEALLSCREENMESSAGES does, and the one that matters to a player is written to the log (NoteVideoMemory). Play in
	// the editor keeps them.
	if (!GIsEditor)
	{
		GAreScreenMessagesEnabled = false;
	}
}

void ACheeseStrikeHUD::NoteVideoMemory()
{
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (Now < NextVideoMemoryNote)
	{
		return;
	}
	NextVideoMemoryNote = Now + 5.0f;
	// Not drawn, the engine's debug messages would only pile up.
	if (!GAreScreenMessagesEnabled && GEngine != nullptr)
	{
		GEngine->ClearOnScreenDebugMessages();
	}
	const float Spilled = static_cast<float>(GDemotedLocalMemorySize) / (1024.0f * 1024.0f);
	if (FMath::Abs(Spilled - VideoMemorySpilled) >= 64.0f)
	{
		if (Spilled > 0.0f)
		{
			UE_LOG(LogCheeseStrike, Warning, TEXT("Video memory has run out: %.0f MB over budget, moved to system memory"), Spilled);
		}
		else
		{
			UE_LOG(LogCheeseStrike, Display, TEXT("Video memory is within budget again"));
		}
		VideoMemorySpilled = Spilled;
	}
}

void ACheeseStrikeHUD::UpdateSweep(const ACheeseStrikePlayerController* Controller)
{
	ACheeseStrikePlayerState* Mine = Controller->GetPlayerState<ACheeseStrikePlayerState>();
	const ACheeseStrikeGameState* Match = GetWorld()->GetGameState<ACheeseStrikeGameState>();
	Sweep = Match != nullptr && Mine != nullptr
		? CheeseStrikeHUD::SweepAt(Match->GetServerWorldTimeSeconds(), Match->RevealEndsFor(Mine), CheeseStrikeStreaks::RevealTime, CheeseStrikeStreaks::SweepPeriod)
		: CheeseStrikeHUD::FSweep();
	if (Mine == nullptr)
	{
		return;
	}
	if (Sweep.Index == INDEX_NONE)
	{
		Mine->SweepIndex = INDEX_NONE;
		Mine->SweepMarks.Reset();
		return;
	}
	// Each sweep sees where every enemy is, and that is what shows until the next.
	if (Sweep.Index != Mine->SweepIndex)
	{
		Mine->SweepIndex = Sweep.Index;
		Mine->SweepMarks.Reset();
		for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsAlive() && Match->AreEnemies(Mine, It->GetPlayerState()))
			{
				Mine->SweepMarks.Add(It->GetActorLocation());
			}
		}
	}
}

bool ACheeseStrikeHUD::AreTeammates(const APlayerState* A, const APlayerState* B)
{
	// The mode decides (ACheeseStrikeGameState::AreTeammates): in a team mode, the same side; in a free-for-all, nobody else.
	const UWorld* World = A != nullptr ? A->GetWorld() : nullptr;
	const ACheeseStrikeGameState* Match = World != nullptr ? World->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Match != nullptr)
	{
		return Match->AreTeammates(A, B);
	}
	return A != nullptr && A == B;
}

void ACheeseStrikeHUD::RevealEnemies(float Seconds)
{
	if (GetWorld() != nullptr)
	{
		RevealedUntil = FMath::Max(RevealedUntil, GetWorld()->GetTimeSeconds() + Seconds);
	}
}

bool ACheeseStrikeHUD::AreEnemiesRevealed() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimeSeconds() < RevealedUntil;
}

void ACheeseStrikeHUD::CheeseStrikeReveal(float Seconds)
{
	RevealEnemies(Seconds);
}

void ACheeseStrikeHUD::CheeseStrikeHitFrom(float Degrees)
{
	const APlayerController* Viewer = GetOwningPlayerController();
	if (Viewer != nullptr && Viewer->PlayerCameraManager != nullptr)
	{
		const FRotator Way(0.0f, Viewer->PlayerCameraManager->GetCameraRotation().Yaw + Degrees, 0.0f);
		AddDamageFrom(Viewer->PlayerCameraManager->GetCameraLocation() + Way.Vector() * 1000.0f, nullptr);
	}
}

float ACheeseStrikeHUD::HealthShown(const ACheeseStrikeCharacter* Character)
{
	return CVarHealthShown.GetValueOnGameThread() >= 0.0f ? CVarHealthShown.GetValueOnGameThread() : Character->GetHealth();
}

bool ACheeseStrikeHUD::ShowsOnMinimap(const ACheeseStrikeCharacter* Enemy) const
{
	if (Enemy == nullptr || !Enemy->IsAlive())
	{
		return false;
	}
	if (AreEnemiesRevealed())
	{
		return true;
	}
	const float* Seen = LastSeen.Find(Enemy);
	return Seen != nullptr && GetWorld() != nullptr && GetWorld()->GetTimeSeconds() - *Seen < SeenLife;
}

void ACheeseStrikeHUD::AddDamageFrom(const FVector& From, const AActor* By)
{
	const float Now = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (By != nullptr)
	{
		for (FDamageArc& Arc : DamageArcs)
		{
			if (Arc.By.Get() == By)
			{
				Arc.From = From;
				Arc.At = Now;
				return;
			}
		}
	}
	DamageArcs.Add({From, By, Now});
}

FVector ACheeseStrikeHUD::ArcSource(const FDamageArc& Arc)
{
	const AActor* By = Arc.By.Get();
	return By != nullptr && !By->IsActorBeingDestroyed() ? By->GetActorLocation() : Arc.From;
}

CheeseStrikeHUD::FScoreSummary ACheeseStrikeHUD::ScoreSummary(const ACheeseStrikeGameState* Match) const
{
	const APlayerController* Viewer = GetOwningPlayerController();
	const ACheeseStrikePlayerState* Mine = Viewer != nullptr ? Viewer->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	if (Match == nullptr)
	{
		return CheeseStrikeHUD::FScoreSummary();
	}
	if (Match->GetModeInfo().WinBy == CheeseStrikeModes::EWinBy::Ladder)
	{
		// Gun Game: each player's step up the ladder of weapons, racing to the last.
		TArray<CheeseStrikeRules::FStanding> Steps;
		for (const APlayerState* Each : Match->PlayerArray)
		{
			if (const ACheeseStrikePlayerState* Ours = Cast<ACheeseStrikePlayerState>(Each))
			{
				CheeseStrikeRules::FStanding Line = Ours->Standing();
				Line.Kills = Ours->GetGunLevel() + 1;
				Steps.Add(Line);
			}
		}
		return CheeseStrikeHUD::GunGame(CheeseStrikeRules::Sorted(Steps), Mine != nullptr ? Mine->GetPlayerName() : FString(), CheeseStrikeArsenal::GunGameLadder().Num());
	}
	if (!Match->HasTeams())
	{
		return CheeseStrikeHUD::FreeForAll(Match->Standings(), Mine != nullptr ? Mine->GetPlayerName() : FString(), Match->GetKillLimit());
	}
	// (Execution, like Elimination, is a mode of rounds: its sides' rounds won, and the round.)
	// A team mode: the sides' scores (rounds won, in Elimination), and the round and its time while one is on.
	const bool bRounds = Match->GetModeInfo().WinBy == CheeseStrikeModes::EWinBy::Rounds;
	FString Goal = Match->GoalText();
	if (bRounds && Match->GetRound() > 0 && !Match->HasMatchEnded())
	{
		const int32 Left = FMath::CeilToInt(Match->SecondsLeftInRound());
		Goal = Match->IsRoundBreak() ? FString::Printf(TEXT("NEXT ROUND IN %d"), FMath::CeilToInt(Match->SecondsToNextRound()))
			: FString::Printf(TEXT("ROUND %d  %d:%02d"), Match->GetRound(), Left / 60, Left % 60);
	}
	const int32 Scores[CheeseStrikeRules::Teams] = {Match->GetTeamScore(0), Match->GetTeamScore(1)};
	return CheeseStrikeHUD::Teams(Scores, Mine != nullptr ? Mine->GetTeam() : INDEX_NONE, bRounds ? Match->GetRoundLimit() : Match->GetKillLimit(), Goal);
}

TArray<ACheeseStrikeHUD::FWeaponSlot> ACheeseStrikeHUD::WeaponSlots(const ACheeseStrikeCharacter* Character) const
{
	// The arsenal's slots in order, each taken out by its number: 1 the primary, 2 the sidearm, 3 the knife; on a gamepad, if
	// the player last used one, the d-pad's up, left and right.
	static TObjectPtr<UInputAction> UCheeseStrikeInputSet::* const SlotActions[] = {&UCheeseStrikeInputSet::DrawPrimary, &UCheeseStrikeInputSet::DrawSidearm, &UCheeseStrikeInputSet::DrawKnife};
	TArray<FWeaponSlot> Slots;
	const UCheeseStrikeInventory* Inventory = Character != nullptr ? Character->GetInventory() : nullptr;
	if (Inventory == nullptr)
	{
		return Slots;
	}
	EWeaponSlot Held = EWeaponSlot::Primary;
	const bool bHolding = Inventory->GetHeldSlot(Held);
	for (int32 Index = 0; Index < NumWeaponSlots; ++Index)
	{
		const EWeaponSlot Slot = static_cast<EWeaponSlot>(Index);
		if (const ACheeseStrikeWeapon* Carried = Inventory->GetInSlot(Slot))
		{
			const FString Key = Index < UE_ARRAY_COUNT(SlotActions) ? UCheeseStrikeInputSet::Prompt(PlayerOwner, SlotActions[Index]) : FString();
			Slots.Add({Carried->GetWeaponId(), Key.IsEmpty() ? FString::FromInt(Index + 1) : Key, Carried->GetStats().DisplayName.ToString(), bHolding && Held == Slot, Inventory->CanDraw(Slot)});
		}
	}
	return Slots;
}

ACheeseStrikeCharacter* ACheeseStrikeHUD::BodyOf(const APlayerState* Player)
{
	if (Player == nullptr)
	{
		return nullptr;
	}
	ACheeseStrikeCharacter* Body = Cast<ACheeseStrikeCharacter>(Player->GetPawn());
	if (Body != nullptr && Body->IsAlive())
	{
		return Body;
	}
	// A joiner's machine may know the character before the player state knows it, and a dead body keeps its player until the new one comes.
	for (TActorIterator<ACheeseStrikeCharacter> It(Player->GetWorld()); It; ++It)
	{
		if (It->GetPlayerState() == Player && (Body == nullptr || It->IsAlive()))
		{
			Body = *It;
		}
	}
	return Body;
}

CheeseStrikeHUD::EStatus ACheeseStrikeHUD::StatusOf(const ACheeseStrikePlayerState* Player, const ACheeseStrikeCharacter* Body)
{
	return CheeseStrikeHUD::StatusOf(Player != nullptr && Player->IsOut(), Body != nullptr && !Body->IsAlive(), Body != nullptr && Body->IsDowned());
}

void ACheeseStrikeHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (CVarHoldArcs.GetValueOnGameThread() != 0)
	{
		for (FDamageArc& Arc : DamageArcs)
		{
			Arc.At = FMath::Min(Arc.At + DeltaSeconds, Now);
		}
	}
	DamageArcs.RemoveAll([Now](const FDamageArc& Arc) { return Now - Arc.At >= CheeseStrikeHUD::DamageArcLife; });
	if (const ACheeseStrikePlayerController* Controller = Cast<ACheeseStrikePlayerController>(GetOwningPlayerController()))
	{
		LookForEnemies(Controller);
		UpdateSweep(Controller);
	}
	NoteVideoMemory();
}

void ACheeseStrikeHUD::DrawHUD()
{
	Super::DrawHUD();
	const ACheeseStrikePlayerController* Controller = Cast<ACheeseStrikePlayerController>(GetOwningPlayerController());
	// Nothing under the front end, the lobby and their pages: only in a match, paused or not.
	if (Canvas == nullptr || Controller == nullptr || (Controller->GetMenuPage() != EMenuPage::None && Controller->GetMenuPage() != EMenuPage::Pause))
	{
		return;
	}
	const ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(Controller->GetPawn());
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	// Over, and not just going to another level (from the lobby, or back to it): the lobby covers that.
	const bool bOver = Match != nullptr && Match->HasMatchEnded() && Match->GetMatchState() != MatchState::LeavingMap;
	const float FieldOfView = Controller->PlayerCameraManager != nullptr ? Controller->PlayerCameraManager->GetFOVAngle() : 90.0f;
	const bool bAlive = Character != nullptr && Character->IsAlive() && !bOver;
	// Out of the round (Elimination), the mode's HUD says so instead of the death screen.
	const bool bDead = Character != nullptr && !Character->IsAlive() && !bOver && !CheeseStrikeModeHUD::IsOut(this);
	if (!bOver)
	{
		DrawNameTags(Controller);
	}
	if (bAlive)
	{
		DrawVignette(Character);
		DrawDamageArcs(Character);
		DrawFight(Character, FieldOfView);
	}
	if (!bOver)
	{
		DrawMinimap(Controller, Character, FieldOfView);
		DrawPlayers(Match);
		DrawScoreSummary(Match);
	}
	DrawKillFeed(Match);
	// The mode's and the streaks' own: announcements, the streak to call in, the Cheese Strike's mark, being out of the round.
	CheeseStrikeModeHUD::Draw(this, Canvas);
	if (!bOver)
	{
		DrawMice(Match);
		DrawCountdown(Match);
	}
	if (bDead)
	{
		DrawDeath(Character, Match);
	}
	if (bOver || Controller->IsScoreboardShown())
	{
		DrawScoreboard(Match, bOver);
	}
}

void ACheeseStrikeHUD::DrawMice(const ACheeseStrikeGameState* Match)
{
	if (Match == nullptr || Match->GetMiceAt() <= 0.0f)
	{
		return;
	}
	// The design's red countdown, then word that they are here.
	const float ToGo = Match->GetMiceAt() - static_cast<float>(Match->GetServerWorldTimeSeconds());
	const float U = Unit();
	const FLinearColor Warning(1.0f, 0.18f, 0.12f);
	const FVector2D At(Canvas->SizeX / 2.0f, Canvas->SizeY * 0.15f);
	if (ToGo > 0.0f && ToGo <= CheeseStrikeMice::Warning)
	{
		HudDraw::Text(Canvas, FString::Printf(TEXT("THE MICE ARE COMING  %.0f"), FMath::CeilToFloat(ToGo)), At, HudDraw::Font(26.0f * U), Warning, 0.5f);
	}
	else if (ToGo <= 0.0f && ToGo > -4.0f)
	{
		HudDraw::Text(Canvas, TEXT("THE MICE ARE HERE"), At, HudDraw::Font(26.0f * U), Warning, 0.5f);
	}
}

void ACheeseStrikeHUD::DrawFight(const ACheeseStrikeCharacter* Character, float FieldOfView)
{
	const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
	const float Width = Canvas->SizeX;
	const float Height = Canvas->SizeY;
	const FVector2D Middle(Width / 2.0f, Height / 2.0f);
	HudDraw::FShapes Shapes;

	// The crosshair: four lines, as far apart as the weapon's spread, and no closer than the least gap; down the sights, the
	// sights are the crosshair, with a small dot where the shots go.
	const float Spread = Weapon != nullptr ? Weapon->CurrentSpread() : 0.0f;
	const float Gap = CrosshairGap + PixelsForDegrees(Spread, FieldOfView, Width);
	const FLinearColor Cross(1.0f, 1.0f, 1.0f, 0.9f);
	if (Weapon == nullptr || !Weapon->IsAimed())
	{
		Shapes.Line(FVector2D(Middle.X - Gap - CrosshairLength, Middle.Y), FVector2D(Middle.X - Gap, Middle.Y), CrosshairThickness, Cross);
		Shapes.Line(FVector2D(Middle.X + Gap, Middle.Y), FVector2D(Middle.X + Gap + CrosshairLength, Middle.Y), CrosshairThickness, Cross);
		Shapes.Line(FVector2D(Middle.X, Middle.Y - Gap - CrosshairLength), FVector2D(Middle.X, Middle.Y - Gap), CrosshairThickness, Cross);
		Shapes.Line(FVector2D(Middle.X, Middle.Y + Gap), FVector2D(Middle.X, Middle.Y + Gap + CrosshairLength), CrosshairThickness, Cross);
	}
	else
	{
		Shapes.Rect(Middle.X - 1.5f, Middle.Y - 1.5f, 3.0f, 3.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
	}

	// A hit: four diagonal ticks, longer and yellow for the head.
	if (Weapon != nullptr && Weapon->SinceHit() < HitMarkLife)
	{
		const float Tick = Weapon->bLastHitHead ? 16.0f : 11.0f;
		const FLinearColor Colour = Weapon->bLastHitHead ? HudDraw::Accent : Cross;
		for (const FVector2D& Way : {FVector2D(1, 1), FVector2D(-1, 1), FVector2D(1, -1), FVector2D(-1, -1)})
		{
			const FVector2D From = Middle + Way * (Gap + 2.0f);
			const FVector2D To = Middle + Way * (Gap + 2.0f + Tick);
			Shapes.Line(From, To, CrosshairThickness + 1.0f, Colour);
		}
	}
	Shapes.Draw(Canvas);

	DrawScope(Character);
	DrawHealth(Character);
	DrawWeapon(Character);
}

UTexture2D* ACheeseStrikeHUD::WeaponIcon(FName Id)
{
	if (const TObjectPtr<UTexture2D>* Known = WeaponIcons.Find(Id))
	{
		return Known->Get();
	}
	const FString Name = FString::Printf(TEXT("T_Icon_%s"), *Id.ToString());
	UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/CheeseStrike/Weapons/Icons/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	WeaponIcons.Add(Id, Icon);
	return Icon;
}

bool ACheeseStrikeHUD::DrawWeaponPicture(FName Id, const FVector2D& At, const FVector2D& Size, const FLinearColor& Colour)
{
	UTexture2D* Icon = WeaponIcon(Id);
	if (Icon == nullptr || Icon->GetResource() == nullptr || Icon->GetSizeY() <= 0)
	{
		return false;
	}
	// As big as fits, its own shape kept, against the right of the space given.
	const float Aspect = static_cast<float>(Icon->GetSizeX()) / Icon->GetSizeY();
	const FVector2D Fitted = Size.X / Size.Y > Aspect ? FVector2D(Size.Y * Aspect, Size.Y) : FVector2D(Size.X, Size.X / Aspect);
	FCanvasTileItem Tile(FVector2D(At.X + Size.X - Fitted.X, At.Y + (Size.Y - Fitted.Y) * 0.5f), Icon->GetResource(), Fitted, Colour);
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);
	return true;
}

void ACheeseStrikeHUD::DrawScope(const ACheeseStrikeCharacter* Character)
{
	const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
	if (Weapon == nullptr || !Weapon->GetStats().bScoped || Character->GetAimFraction() < ACheeseStrikeCharacter::ScopeFrom)
	{
		return;
	}
	// The lens a circle most of the view high, everything round it black; across it thin hairs, thick towards the rim.
	const FVector2D Middle(Canvas->SizeX / 2.0f, Canvas->SizeY / 2.0f);
	const float Radius = Canvas->SizeY * 0.46f;
	const float Far = FMath::Max(Canvas->SizeX, Canvas->SizeY) * 2.0f;
	const FLinearColor Black(0.0f, 0.0f, 0.0f, 1.0f);
	HudDraw::FShapes Scope;
	const int32 Sides = 72;
	for (int32 Side = 0; Side < Sides; ++Side)
	{
		const float A = 2.0f * PI * Side / Sides;
		const float B = 2.0f * PI * (Side + 1) / Sides;
		const FVector2D InA = Middle + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius;
		const FVector2D InB = Middle + FVector2D(FMath::Cos(B), FMath::Sin(B)) * Radius;
		Scope.Quad(InA, InB, Middle + FVector2D(FMath::Cos(B), FMath::Sin(B)) * Far, Middle + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Far, Black);
	}
	// A dark ring just inside the rim, as the tube's edge.
	Scope.Arc(Middle, Radius - 6.0f, 12.0f, 0.0f, 360.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), false);
	const float Thin = 1.5f;
	const float Thick = 4.0f;
	const float Gap = Radius * 0.42f;
	Scope.Line(Middle - FVector2D(Radius, 0.0f), Middle + FVector2D(Radius, 0.0f), Thin, Black);
	Scope.Line(Middle - FVector2D(0.0f, Radius), Middle + FVector2D(0.0f, Radius), Thin, Black);
	for (const FVector2D& Way : {FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1)})
	{
		Scope.Line(Middle + Way * Gap, Middle + Way * Radius, Thick, Black);
	}
	Scope.Draw(Canvas);
}

void ACheeseStrikeHUD::DrawHealth(const ACheeseStrikeCharacter* Character)
{
	const float U = Unit();
	const float Left = Margin * U;
	const float Bottom = Canvas->SizeY - Margin * U;
	const float Max = CheeseStrikeWeapons::MaxHealth;
	const float Health = FMath::Clamp(HealthShown(Character), 0.0f, Max);
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bLow = CheeseStrikeHUD::IsHealthLow(Health, Max);

	// The trail: health just lost stays lit a moment, then runs down to what is left; health coming back shows at once.
	if (HealthTrail < 0.0f || Health >= HealthTrail)
	{
		HealthTrail = Health;
	}
	else
	{
		if (Health < HealthBefore)
		{
			HealthFellAt = Now;
		}
		if (Now - HealthFellAt > 0.45f)
		{
			HealthTrail = FMath::Max(Health, HealthTrail - Max * 0.8f * GetWorld()->GetDeltaSeconds());
		}
	}
	HealthBefore = Health;

	// No number: a cross, then a row of blocks, each a share of full health; red and breathing when low.
	const float Pulse = bLow ? 0.9f + 0.1f * FMath::Sin(Now * 2.0f * UE_PI * 1.2f) : 1.0f;
	const FLinearColor Fill = bLow ? FLinearColor(HudDraw::Danger.R, HudDraw::Danger.G, HudDraw::Danger.B, Pulse) : HudDraw::Ink;
	// What was just lost: red against the white, pale against the red when low.
	const FLinearColor Lost = bLow ? FLinearColor(1.0f, 1.0f, 1.0f, 0.5f) : FLinearColor(HudDraw::Danger.R, HudDraw::Danger.G, HudDraw::Danger.B, 0.8f);
	const FLinearColor Backing(0.0f, 0.0f, 0.0f, 0.45f);
	const int32 Count = CheeseStrikeHUD::HealthSegments;
	const TArray<float> Fills = CheeseStrikeHUD::HealthFills(Health, Max, Count);
	const TArray<float> Trails = CheeseStrikeHUD::HealthFills(HealthTrail, Max, Count);
	const float Height = 18.0f * U;
	const float Top = Bottom - Height;
	const float CrossSize = 26.0f * U;
	const float BarLeft = Left + CrossSize + 12.0f * U;
	const float Gap = 5.0f * U;
	const float Block = (320.0f * U - Gap * (Count - 1)) / Count;

	HudDraw::FShapes Shapes;
	// The cross, centred on the bar.
	const float Arm = CrossSize / 3.0f;
	const float CrossTop = Top + Height * 0.5f - CrossSize * 0.5f;
	Shapes.Rect(Left - U, CrossTop + Arm - U, CrossSize + 2.0f * U, Arm + 2.0f * U, Backing);
	Shapes.Rect(Left + Arm - U, CrossTop - U, Arm + 2.0f * U, CrossSize + 2.0f * U, Backing);
	Shapes.Rect(Left, CrossTop + Arm, CrossSize, Arm, Fill);
	Shapes.Rect(Left + Arm, CrossTop, Arm, CrossSize, Fill);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float X = BarLeft + Index * (Block + Gap);
		Shapes.Rect(X - U, Top - U, Block + 2.0f * U, Height + 2.0f * U, Backing);
		Shapes.Rect(X, Top, Block, Height, HudDraw::Dim);
		if (Trails[Index] > Fills[Index])
		{
			Shapes.Rect(X, Top, Block * Trails[Index], Height, Lost);
		}
		if (Fills[Index] > 0.0f)
		{
			Shapes.Rect(X, Top, Block * Fills[Index], Height, Fill);
		}
	}
	Shapes.Draw(Canvas);
	if (Character->IsProtected())
	{
		const FSlateFontInfo Label = HudDraw::Font(12.0f * U);
		HudDraw::Text(Canvas, TEXT("PROTECTED"), FVector2D(Left, CrossTop - 8.0f * U - HudDraw::Measure(TEXT("P"), Label).Y), Label, HudDraw::Accent);
	}
}

void ACheeseStrikeHUD::DrawWeaponIcon(HudDraw::FShapes& Shapes, EWeaponSlot Slot, const FVector2D& At, const FVector2D& Size, const FLinearColor& Colour) const
{
	// The weapon's outline in convex parts on a 100 by 40 grid, facing left, towards the middle of the screen: a rifle, a pistol or a knife.
	const auto P = [&At, &Size](float X, float Y) { return At + FVector2D(X / 100.0f * Size.X, Y / 40.0f * Size.Y); };
	TArray<TArray<FVector2D>> Parts;
	switch (Slot)
	{
	case EWeaponSlot::Sidearm:
		Parts = {
			{P(30, 12), P(78, 12), P(78, 21), P(30, 21)},
			{P(26, 13.5f), P(30, 13.5f), P(30, 19.5f), P(26, 19.5f)},
			{P(34, 21), P(74, 21), P(74, 24), P(36, 24)},
			{P(60, 24), P(73, 24), P(78, 37), P(65, 37)},
			{P(52, 24), P(56, 24), P(58, 29), P(52, 29)},
		};
		break;
	case EWeaponSlot::Knife:
		Parts = {
			{P(4, 20), P(14, 15), P(58, 14), P(58, 23), P(12, 23)},
			{P(58, 10.5f), P(63, 10.5f), P(63, 26.5f), P(58, 26.5f)},
			{P(63, 15), P(94, 15), P(97, 17), P(97, 21), P(94, 23), P(63, 23)},
		};
		break;
	default:
		Parts = {
			{P(0, 15), P(5, 15), P(5, 20), P(0, 20)},
			{P(5, 16), P(24, 16), P(24, 19), P(5, 19)},
			{P(22, 13.5f), P(46, 13.5f), P(46, 22), P(23, 22)},
			{P(45, 11), P(76, 11), P(76, 23), P(45, 23)},
			{P(52, 7.5f), P(68, 7.5f), P(68, 11), P(52, 11)},
			{P(53, 23), P(61, 23), P(58, 36), P(49.5f, 34.5f)},
			{P(67, 23), P(73, 23), P(76.5f, 33), P(70, 33)},
			{P(75, 12), P(99, 14.5f), P(99, 27), P(93, 27), P(75, 20)},
		};
		break;
	}
	for (const TArray<FVector2D>& Part : Parts)
	{
		Shapes.Polygon(Part, Colour);
	}
}

void ACheeseStrikeHUD::DrawWeapon(const ACheeseStrikeCharacter* Character)
{
	const ACheeseStrikeWeapon* Weapon = Character->GetWeapon();
	if (Weapon == nullptr)
	{
		return;
	}
	const float U = Unit();
	const float Now = GetWorld()->GetTimeSeconds();
	const float Right = Canvas->SizeX - Margin * U;
	const float Bottom = Canvas->SizeY - Margin * U;
	const FWeaponStats& Stats = Weapon->GetStats();
	const bool bRounds = Stats.UsesRounds();
	const int32 Magazine = Weapon->GetMagazine();
	const bool bReloading = bRounds && Weapon->IsReloading();
	if (bReloading && ReloadBegan < 0.0f)
	{
		ReloadBegan = Now;
	}
	else if (!bReloading)
	{
		ReloadBegan = -1.0f;
	}
	const bool bLow = bRounds && !bReloading && Magazine <= FMath::Max(1, Stats.MagazineSize / 4);
	HudDraw::FShapes Shapes;

	// Along the bottom: a pip a round (a bar for a big magazine), or the reload filling; nothing for the knife.
	const float BarHeight = 6.0f * U;
	const float BarTop = Bottom - BarHeight;
	const float BarWidth = 190.0f * U;
	if (bReloading)
	{
		const float Done = Stats.ReloadTime > 0.0f ? FMath::Clamp((Now - ReloadBegan) / Stats.ReloadTime, 0.0f, 1.0f) : 1.0f;
		Shapes.Rect(Right - BarWidth, BarTop + 2.0f * U, BarWidth, 2.0f * U, HudDraw::Dim);
		Shapes.Rect(Right - BarWidth, BarTop + 2.0f * U, BarWidth * Done, 2.0f * U, HudDraw::Accent);
	}
	else if (bRounds && Stats.MagazineSize <= 40)
	{
		const float Pip = FMath::Min(4.0f * U, (BarWidth - (Stats.MagazineSize - 1) * 2.0f * U) / Stats.MagazineSize);
		const float Step = Pip + 2.0f * U;
		for (int32 Round = 0; Round < Stats.MagazineSize; ++Round)
		{
			// The rounds left are the leftmost: a magazine empties from the right, as it is fired.
			const float X = Right - (Stats.MagazineSize - Round) * Step + 2.0f * U;
			Shapes.Rect(X, BarTop, Pip, BarHeight, Round < Magazine ? (bLow ? HudDraw::Danger : HudDraw::Ink) : HudDraw::Dim);
		}
	}
	else if (bRounds)
	{
		Shapes.Rect(Right - BarWidth, BarTop, BarWidth, BarHeight, HudDraw::Dim);
		Shapes.Rect(Right - BarWidth, BarTop, BarWidth * Magazine / FMath::Max(1, Stats.MagazineSize), BarHeight, bLow ? HudDraw::Danger : HudDraw::Ink);
	}

	// Over it, the magazine large and the reserve beside it (none for rounds without end); the name over them; the weapon's
	// outline to their left. The knife has its name where the rounds would be.
	const FSlateFontInfo Big = HudDraw::Font(40.0f * U);
	const FSlateFontInfo Small = HudDraw::Font(18.0f * U);
	const FSlateFontInfo Label = HudDraw::Font(13.0f * U);
	const FString Name = Stats.DisplayName.ToString().ToUpper();
	const FString Reserve = bRounds && !Stats.HasEndlessReserve() ? FString::Printf(TEXT("/ %d"), Weapon->GetReserve()) : FString();
	const FString Rounds = bRounds ? FString::FromInt(Magazine) : FString();
	const FVector2D ReserveSize = Reserve.IsEmpty() ? FVector2D::ZeroVector : HudDraw::Measure(Reserve, Small);
	const FVector2D Line = HudDraw::Measure(TEXT("0"), Big);
	const FVector2D WidestRounds = bRounds ? HudDraw::Measure(FString::FromInt(FMath::Max(Stats.MagazineSize, 10)), Big) : FVector2D::ZeroVector;
	const float Baseline = (bRounds ? BarTop : Bottom) - 8.0f * U;
	const float RoundsTop = Baseline - Line.Y * 0.84f;
	const float ReserveLeft = Right - ReserveSize.X;
	const float RoundsRight = Reserve.IsEmpty() ? Right : ReserveLeft - 8.0f * U;
	const float NameTop = RoundsTop - HudDraw::Measure(TEXT("A"), Label).Y + 2.0f * U;
	const float IconWidth = (Stats.Slot == EWeaponSlot::Primary ? 116.0f : 80.0f) * U;
	const FVector2D IconSize(IconWidth, IconWidth * 0.4f * (Stats.Slot == EWeaponSlot::Primary ? 1.0f : 1.3f));
	const float IconRight = bRounds ? RoundsRight - WidestRounds.X - 22.0f * U : Right - HudDraw::Measure(Name, Small).X - 22.0f * U;
	// The weapon's own silhouette if it has one (tools/art/weapon_icons.py), or its kind's outline.
	const FVector2D IconAt(IconRight - IconSize.X, Baseline - IconSize.Y + 2.0f * U);
	const FLinearColor IconColour(1.0f, 1.0f, 1.0f, 0.42f);
	const bool bPicture = WeaponIcon(Weapon->GetWeaponId()) != nullptr;
	if (!bPicture)
	{
		DrawWeaponIcon(Shapes, Stats.Slot, IconAt, IconSize, IconColour);
	}
	Shapes.Draw(Canvas);
	if (bPicture)
	{
		DrawWeaponPicture(Weapon->GetWeaponId(), IconAt, IconSize, IconColour);
	}

	float SlotsBottom = NameTop;
	if (!bRounds)
	{
		const FVector2D NameSize = HudDraw::Measure(Name, Small);
		HudDraw::Text(Canvas, Name, FVector2D(Right, Baseline - NameSize.Y * 0.84f), Small, HudDraw::Ink, 1.0f);
		SlotsBottom = Baseline - NameSize.Y;
	}
	else
	{
		HudDraw::Text(Canvas, Rounds, FVector2D(RoundsRight, RoundsTop), Big, bLow ? HudDraw::Danger : HudDraw::Ink, 1.0f);
		if (!Reserve.IsEmpty())
		{
			HudDraw::Text(Canvas, Reserve, FVector2D(ReserveLeft, Baseline - ReserveSize.Y * 0.84f), Small, HudDraw::Soft);
		}
		if (bReloading)
		{
			HudDraw::Text(Canvas, TEXT("RELOADING"), FVector2D(Right, NameTop), Label, HudDraw::Accent, 1.0f);
		}
		else if (Weapon->IsDry())
		{
			HudDraw::Text(Canvas, TEXT("NO ROUNDS"), FVector2D(Right, NameTop), Label, HudDraw::Danger, 1.0f);
		}
		else if (Magazine == 0)
		{
			HudDraw::Text(Canvas, TEXT("RELOAD"), FVector2D(Right, NameTop), Label, HudDraw::Danger, 1.0f);
		}
		else
		{
			HudDraw::Text(Canvas, Name, FVector2D(Right, NameTop), Label, HudDraw::Soft, 1.0f);
		}
	}

	// The weapons carried, each with the key that takes it out, over the rest, once there is more than one; the one in
	// hand bright, one the mode doesn't allow dim.
	const TArray<FWeaponSlot> Slots = WeaponSlots(Character);
	if (Slots.Num() > 1)
	{
		const float Row = 20.0f * U;
		float Y = SlotsBottom - Row * Slots.Num() - 8.0f * U;
		HudDraw::FShapes Keys;
		for (const FWeaponSlot& Slot : Slots)
		{
			if (Slot.bHeld)
			{
				Keys.Rect(Right + 6.0f * U, Y + 3.0f * U, 2.0f * U, Row - 6.0f * U, HudDraw::Accent);
			}
			Y += Row;
		}
		Keys.Draw(Canvas);
		Y = SlotsBottom - Row * Slots.Num() - 8.0f * U;
		for (const FWeaponSlot& Slot : Slots)
		{
			const FLinearColor Colour = Slot.bHeld ? HudDraw::Ink : Slot.bCanDraw ? HudDraw::Soft : HudDraw::Dim;
			const FString Text = FString::Printf(TEXT("%s   %s"), *Slot.Name.ToUpper(), *Slot.Key);
			HudDraw::Text(Canvas, Text, FVector2D(Right, Y), Label, Colour, 1.0f);
			// Each weapon's silhouette before its name.
			const FVector2D Thumb(Row * 2.4f, Row * 0.8f);
			DrawWeaponPicture(Slot.Id, FVector2D(Right - HudDraw::Measure(Text, Label).X - 8.0f * U - Thumb.X, Y + Row * 0.05f), Thumb, FLinearColor(Colour.R, Colour.G, Colour.B, Colour.A * 0.8f));
			Y += Row;
		}
	}
}

void ACheeseStrikeHUD::DrawVignette(const ACheeseStrikeCharacter* Character)
{
	const float Now = GetWorld()->GetTimeSeconds();
	CheeseStrikeHUD::FVignette Look = CheeseStrikeHUD::VignetteFor(HealthShown(Character), CheeseStrikeWeapons::MaxHealth, Now);
	// Each hit flashes the edges for a moment, however healthy.
	float Since = TNumericLimits<float>::Max();
	for (const FDamageArc& Arc : DamageArcs)
	{
		Since = FMath::Min(Since, FMath::Max(0.0f, Now - Arc.At));
	}
	const float Flash = Since < 0.4f ? 0.3f * (1.0f - Since / 0.4f) : 0.0f;
	Look.Alpha = FMath::Min(1.0f, Look.Alpha + Flash);
	if (Look.Alpha <= 0.0f)
	{
		return;
	}
	if (VignetteTexture == nullptr)
	{
		// White, clear in the middle and growing solid towards the edges and most in the corners.
		constexpr int32 Side = 128;
		TArray<uint8> Pixels;
		Pixels.SetNumUninitialized(Side * Side * 4);
		for (int32 Y = 0; Y < Side; ++Y)
		{
			for (int32 X = 0; X < Side; ++X)
			{
				const float DX = (X + 0.5f) / (Side / 2.0f) - 1.0f;
				const float DY = (Y + 0.5f) / (Side / 2.0f) - 1.0f;
				const float Out = FMath::Clamp((FMath::Sqrt(DX * DX + DY * DY) - 0.55f) / 0.55f, 0.0f, 1.0f);
				uint8* Pixel = &Pixels[(Y * Side + X) * 4];
				Pixel[0] = Pixel[1] = Pixel[2] = 255;
				Pixel[3] = static_cast<uint8>(FMath::RoundToInt(255.0f * FMath::Pow(Out, 1.5f)));
			}
		}
		VignetteTexture = HudDraw::MakeTexture(Side, Side, Pixels, PF_B8G8R8A8);
	}
	// Drawn larger than the screen when faint, so that it reaches in only at the edges, and the screen's size as it grows.
	const float Out = (1.0f - Look.Reach) * 0.15f;
	const FVector2D Size(Canvas->SizeX * (1.0f + 2.0f * Out), Canvas->SizeY * (1.0f + 2.0f * Out));
	HudDraw::Picture(Canvas, VignetteTexture, FVector2D(-Canvas->SizeX * Out, -Canvas->SizeY * Out), Size, FLinearColor(0.5f, 0.0f, 0.0f, Look.Alpha));
}

void ACheeseStrikeHUD::DrawDamageArcs(const ACheeseStrikeCharacter* Character)
{
	const float Now = GetWorld()->GetTimeSeconds();
	DamageArcs.RemoveAll([Now](const FDamageArc& Arc) { return Now - Arc.At >= CheeseStrikeHUD::DamageArcLife; });
	const APlayerController* Viewer = GetOwningPlayerController();
	if (DamageArcs.Num() == 0 || Viewer == nullptr || Viewer->PlayerCameraManager == nullptr)
	{
		return;
	}
	const FVector Eye = Viewer->PlayerCameraManager->GetCameraLocation();
	const float ViewYaw = Viewer->PlayerCameraManager->GetCameraRotation().Yaw;
	const float U = Unit();
	const FVector2D Middle(Canvas->SizeX / 2.0f, Canvas->SizeY / 2.0f);
	const float Radius = Canvas->SizeY * 0.34f;
	HudDraw::FShapes Shapes;
	for (const FDamageArc& Arc : DamageArcs)
	{
		const FVector From = ArcSource(Arc);
		const float Angle = CheeseStrikeHUD::DamageArcAngle(ViewYaw, FVector2D(Eye.X, Eye.Y), FVector2D(From.X, From.Y));
		const float Fade = CheeseStrikeHUD::DamageArcFade(Now - Arc.At);
		const FLinearColor Blood(0.9f, 0.05f, 0.03f, 1.0f);
		Shapes.Arc(Middle, Radius, 30.0f * U, Angle - 28.0f, Angle + 28.0f, HudDraw::Faded(Blood, 0.25f * Fade), true);
		Shapes.Arc(Middle, Radius, 10.0f * U, Angle - 24.0f, Angle + 24.0f, HudDraw::Faded(Blood, 0.95f * Fade), true);
		// A point outwards, at its middle: that way.
		const float Radians = FMath::DegreesToRadians(Angle);
		const FVector2D Out(FMath::Sin(Radians), -FMath::Cos(Radians));
		const FVector2D Across(-Out.Y, Out.X);
		const FVector2D Base = Middle + Out * (Radius + 7.0f * U);
		Shapes.Triangle(Base - Across * 11.0f * U, Base + Across * 11.0f * U, Base + Out * 13.0f * U, HudDraw::Faded(Blood, 0.95f * Fade));
	}
	Shapes.Draw(Canvas);
}

void ACheeseStrikeHUD::DrawScoreSummary(const ACheeseStrikeGameState* Match)
{
	const CheeseStrikeHUD::FScoreSummary Summary = ScoreSummary(Match);
	if (Summary.Lines.Num() == 0)
	{
		return;
	}
	const float U = Unit();
	const float BlockWidth = 200.0f * U;
	// The goal between the blocks, as wide as its words need.
	const FSlateFontInfo Goal = HudDraw::Font(13.0f * U);
	const FString GoalText = Summary.Goal.ToUpper();
	const float GoalWidth = FMath::Max(104.0f * U, HudDraw::Measure(GoalText, Goal).X + 28.0f * U);
	const float BlockHeight = ScoreHeight * U;
	const int32 OnLeft = (Summary.Lines.Num() + 1) / 2;
	const float Total = Summary.Lines.Num() * BlockWidth + GoalWidth;
	float X = (Canvas->SizeX - Total) / 2.0f;
	const float Top = Canvas->SizeY - Margin * U - BlockHeight;
	const FSlateFontInfo Caption = HudDraw::Font(12.0f * U);
	const FSlateFontInfo Name = HudDraw::Font(14.0f * U);
	const FSlateFontInfo Score = HudDraw::Font(32.0f * U);
	const FVector2D ScoreSize = HudDraw::Measure(TEXT("0"), Score);
	HudDraw::FShapes Shapes;
	struct FPlaced
	{
		float X;
		const CheeseStrikeHUD::FScoreLine* Line;
	};
	TArray<FPlaced> Placed;
	float GoalX = 0.0f;
	for (int32 Index = 0; Index < Summary.Lines.Num(); ++Index)
	{
		if (Index == OnLeft)
		{
			GoalX = X;
			X += GoalWidth;
		}
		const CheeseStrikeHUD::FScoreLine& Line = Summary.Lines[Index];
		const FLinearColor Mark = Line.Team != INDEX_NONE ? HudDraw::SideColour(Line.Team) : Line.bMine ? HudDraw::Accent : HudDraw::Ink;
		Shapes.Rect(X, Top, BlockWidth, BlockHeight, HudDraw::Panel);
		Shapes.Rect(X, Top, BlockWidth, 2.0f * U, Line.bMine ? Mark : HudDraw::Faded(Mark, 0.5f));
		// How far to the limit, along the bottom.
		if (Summary.Limit > 0)
		{
			const float Fraction = FMath::Clamp(static_cast<float>(Line.Score) / Summary.Limit, 0.0f, 1.0f);
			Shapes.Rect(X, Top + BlockHeight - 3.0f * U, BlockWidth, 3.0f * U, HudDraw::Faded(HudDraw::Dim, 0.6f));
			Shapes.Rect(X, Top + BlockHeight - 3.0f * U, BlockWidth * Fraction, 3.0f * U, HudDraw::Faded(Mark, 0.85f));
		}
		Placed.Add({X, &Line});
		X += BlockWidth;
	}
	if (OnLeft >= Summary.Lines.Num())
	{
		GoalX = X;
	}
	Shapes.Rect(GoalX, Top, GoalWidth, BlockHeight, HudDraw::Faded(HudDraw::Panel, 0.6f));
	Shapes.Draw(Canvas);
	for (const FPlaced& Block : Placed)
	{
		const float Pad = 12.0f * U;
		HudDraw::Text(Canvas, Block.Line->Caption.ToUpper(), FVector2D(Block.X + Pad, Top + 9.0f * U), Caption, Block.Line->bMine ? HudDraw::Accent : HudDraw::Soft);
		if (!Block.Line->Name.IsEmpty())
		{
			// As much of the name as fits beside the score.
			FString Shown = Block.Line->Name.ToUpper();
			const float Room = BlockWidth - 2.0f * Pad - HudDraw::Measure(FString::FromInt(Block.Line->Score), Score).X - 8.0f * U;
			if (HudDraw::Measure(Shown, Name).X > Room)
			{
				while (Shown.Len() > 1 && HudDraw::Measure(Shown + TEXT("..."), Name).X > Room)
				{
					Shown.LeftChopInline(1);
				}
				Shown = Shown.TrimEnd() + TEXT("...");
			}
			HudDraw::Text(Canvas, Shown, FVector2D(Block.X + Pad, Top + 27.0f * U), Name, HudDraw::Ink);
		}
		HudDraw::Text(Canvas, FString::FromInt(Block.Line->Score), FVector2D(Block.X + BlockWidth - Pad, Top + (BlockHeight - ScoreSize.Y) / 2.0f - 1.0f * U), Score, HudDraw::Ink, 1.0f);
	}
	HudDraw::Text(Canvas, GoalText, FVector2D(GoalX + GoalWidth / 2.0f, Top + (BlockHeight - HudDraw::Measure(TEXT("F"), Goal).Y) / 2.0f), Goal, HudDraw::Soft, 0.5f);
}

FString ACheeseStrikeHUD::KillPlace(const FKillFeedEntry& Kill, float Now)
{
	const FString Key = FString::Printf(TEXT("%s@%.2f"), *Kill.Victim, Kill.At);
	if (const FString* Known = KillPlaces.Find(Key))
	{
		return *Known;
	}
	// The killed's body lies where it fell, for a while: the place it is in.
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive() && It->DisplayName() == Kill.Victim)
		{
			return KillPlaces.Add(Key, ACheeseStrikePlace::NameAt(GetWorld(), It->GetActorLocation()));
		}
	}
	// Not here yet (or never, too far off to be seen): asked again until a second has gone.
	return Now - Kill.At > 1.0f ? KillPlaces.Add(Key, FString()) : FString();
}

void ACheeseStrikeHUD::DrawKillFeed(const ACheeseStrikeGameState* Match)
{
	if (Match == nullptr)
	{
		return;
	}
	const float U = Unit();
	const FSlateFontInfo Line = HudDraw::Font(16.0f * U);
	const float Now = Match->GetServerWorldTimeSeconds();
	const APlayerController* Viewer = GetOwningPlayerController();
	const FString Me = Viewer != nullptr && Viewer->PlayerState != nullptr ? Viewer->PlayerState->GetPlayerName() : FString();
	const float Right = Canvas->SizeX - Margin * U;
	float Y = Margin * U;
	TSet<FString> Showing;
	for (const FKillFeedEntry& Kill : Match->GetKillFeed())
	{
		if (Now - Kill.At > CheeseStrikeRules::KillFeedLife)
		{
			continue;
		}
		// And where, as the design has it: "... · The Hall"; a death nobody did is "<the killed>  fell into the fondue", which says where.
		const bool bNobody = Kill.Killer.IsEmpty() && !Kill.How.IsEmpty();
		const FString Where = bNobody ? FString() : KillPlace(Kill, Now);
		Showing.Add(FString::Printf(TEXT("%s@%.2f"), *Kill.Victim, Kill.At));
		const FString Place = Where.IsEmpty() ? FString() : FString::Printf(TEXT("  ·  %s"), *Where);
		const float PlaceWidth = Place.IsEmpty() ? 0.0f : HudDraw::Measure(Place, Line).X;
		// Killer, then "killed", then the killed, right to left from the edge; the player's own name in the accent.
		const FString Killer = bNobody ? Kill.Victim : (Kill.Killer.IsEmpty() ? TEXT("nobody") : Kill.Killer);
		// A streak's kill says which ("Cheese Strike") where the weapon's says "killed".
		const FString Middle = bNobody ? FString() : !Kill.How.IsEmpty() ? FString::Printf(TEXT("  %s  "), *Kill.How) : Kill.bHead ? TEXT("  killed (head)  ") : TEXT("  killed  ");
		const FString Victim = bNobody ? FString::Printf(TEXT("  %s"), *Kill.How) : Kill.Victim;
		const float VictimWidth = HudDraw::Measure(Victim, Line).X;
		const float MiddleWidth = Middle.IsEmpty() ? 0.0f : HudDraw::Measure(Middle, Line).X;
		const float KillerWidth = HudDraw::Measure(Killer, Line).X;
		const float Height = HudDraw::Measure(TEXT("A"), Line).Y;
		HudDraw::FShapes Shapes;
		const float Total = PlaceWidth + VictimWidth + MiddleWidth + KillerWidth;
		Shapes.Rect(Right - Total - 16.0f * U, Y - 2.0f * U, Total + 16.0f * U, Height + 4.0f * U, HudDraw::Panel);
		Shapes.Draw(Canvas);
		float X = Right - 8.0f * U;
		if (!Place.IsEmpty())
		{
			HudDraw::Text(Canvas, Place, FVector2D(X, Y), Line, HudDraw::Soft, 1.0f);
			X -= PlaceWidth;
		}
		HudDraw::Text(Canvas, Victim, FVector2D(X, Y), Line, bNobody ? HudDraw::Soft : (Kill.Victim == Me ? HudDraw::Accent : HudDraw::Ink), 1.0f);
		X -= VictimWidth;
		if (!Middle.IsEmpty())
		{
			HudDraw::Text(Canvas, Middle, FVector2D(X, Y), Line, Kill.bHead || !Kill.How.IsEmpty() ? HudDraw::Accent : HudDraw::Soft, 1.0f);
			X -= MiddleWidth;
		}
		HudDraw::Text(Canvas, Killer, FVector2D(X, Y), Line, (bNobody ? Kill.Victim : Kill.Killer) == Me ? HudDraw::Accent : HudDraw::Ink, 1.0f);
		Y += Height + 8.0f * U;
	}
	// Forget the places of kills gone from the feed.
	for (auto Known = KillPlaces.CreateIterator(); Known; ++Known)
	{
		if (!Showing.Contains(Known.Key()))
		{
			Known.RemoveCurrent();
		}
	}
}
