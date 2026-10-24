// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CheeseStrikeHUDRules.h"
#include "CheeseStrikeMinimap.h"
#include "CheeseStrikeHUD.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikeGameState;
class ACheeseStrikePlayerController;
class ACheeseStrikePlayerState;
class APlayerState;
class UCheeseStrikeAvatars;
class UTexture2D;
namespace CheeseStrikeHUDDraw { struct FShapes; }
enum class EWeaponSlot : uint8;

/**
 * What a player sees over the view, drawn in code in the design's look: a
 * thin, square, white-and-grey interface with the cheese's yellow for an
 * accent, in the engine's bold condensed Roboto (CheeseStrikeHUDDraw).
 *
 * In a fight: a crosshair whose gap opens with the weapon's spread, a mark
 * when a shot hits (longer ticks for the head), health at the lower left
 * as a row of blocks (never a number), the weapon at the lower right (its
 * name, the magazine and the reserve, a pip a round, the reload), red
 * edges as health falls, and an arc at the edge of the view for each hit,
 * pointing at whoever made it. Always: the minimap at the top left
 * (CheeseStrikeMinimap), turned with the view, the
 * player's side and the cheese on it; every player along the top, grouped
 * by side, with their Steam pictures and whether they are alive, coming
 * back or out; the last few kills at the top right; the score at the
 * bottom; teammates' names over their heads. Killed: the screen melts,
 * "YOU GOT CHEESED", with who did it and how long until they are back. The
 * scoreboard while its key is held, and for good once the match is over,
 * with the winner. The rules it draws by are CheeseStrikeHUD's, tested
 * without a world.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** The crosshair's lines: how long, how thick, and the least gap from the middle. */
	static constexpr float CrosshairLength = 9.0f;
	static constexpr float CrosshairThickness = 2.0f;
	static constexpr float CrosshairGap = 5.0f;
	/** Seconds a hit mark shows. */
	static constexpr float HitMarkLife = 0.25f;
	/** Seconds an enemy stays on the minimap after they were last seen. */
	static constexpr float SeenLife = 1.5f;
	/** The margin from the screen's edges, and the minimap's side, in design pixels (Unit). */
	static constexpr float Margin = 28.0f;
	static constexpr float MapSide = 250.0f;
	/** The widest a name may be (design pixels) over a teammate's head, and in the kill feed; a longer one is cut with dots (CheeseStrikeHUD::FitName). */
	static constexpr float NameTagWidth = 220.0f;
	static constexpr float KillFeedNameWidth = 190.0f;
	/** The score summary's height, at the bottom in the middle: what the modes' HUD keeps clear of. */
	static constexpr float ScoreHeight = 56.0f;
	/** The kill-streak panel's height, over the health at the bottom left (DrawStreak). */
	static constexpr float StreakPanelHeight = 58.0f;

	/** Pixels from the middle of a view `Width` pixels wide a line `Degrees` off the centre lands, at a field of view of `FieldOfView` degrees. */
	static float PixelsForDegrees(float Degrees, float FieldOfView, float Width);

	virtual void DrawHUD() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/**
	 * Who is on whose side, the one rule the minimap, the name tags and the
	 * enemies' sighting follow: as the mode has it
	 * (ACheeseStrikeGameState::AreTeammates), the same side in a team mode,
	 * nobody but oneself in a free-for-all.
	 */
	static bool AreTeammates(const APlayerState* A, const APlayerState* B);

	/**
	 * Shows every enemy on the minimap for `Seconds`, seen or not, on this
	 * machine: what the Unmanned Aerial Cheese does for its team's players.
	 */
	void RevealEnemies(float Seconds);
	bool AreEnemiesRevealed() const;
	/** For trying the reveal (development): CheeseStrikeReveal <seconds>. */
	UFUNCTION(Exec)
	void CheeseStrikeReveal(float Seconds = 10.0f);
	/** For judging the damage arcs (development): a hit from `Degrees` clockwise from straight ahead, 10 m off. With CheeseStrike.HUD.HoldArcs 1 the arcs stay as they are. */
	UFUNCTION(Exec)
	void CheeseStrikeHitFrom(float Degrees);
	/** Whether `Enemy` shows on the minimap now, where they are: seen in the last SeenLife seconds, or revealed here (RevealEnemies). The side's Unmanned Aerial Cheese shows them where its last sweep saw them (UpdateSweep). */
	bool ShowsOnMinimap(const ACheeseStrikeCharacter* Enemy) const;

	/** One hit taken: where it came from, by whom if known, and the world's time it came (this machine's). */
	struct FDamageArc
	{
		FVector From = FVector::ZeroVector;
		TWeakObjectPtr<const AActor> By;
		float At = 0.0f;
	};
	/** A hit from `From` (world), by `By` if known: an arc pointing at them (from ACheeseStrikeCharacter::ClientHurtFrom). Another hit by the same shooter renews theirs. */
	void AddDamageFrom(const FVector& From, const AActor* By);
	/** The arcs showing now. */
	const TArray<FDamageArc>& GetDamageArcs() const { return DamageArcs; }
	/** Where an arc points from now: its shooter where they are, or where they were. */
	static FVector ArcSource(const FDamageArc& Arc);

	/** What the bottom of the screen says about the score: a free-for-all's (CheeseStrikeHUD::FreeForAll), or a team mode's sides and its round (CheeseStrikeHUD::Teams). */
	virtual CheeseStrikeHUD::FScoreSummary ScoreSummary(const ACheeseStrikeGameState* Match) const;

	/** One of the character's weapons, as the list over the ammunition shows it: the key that takes it out, its name, whether it is in hand, and whether it may be taken out. */
	struct FWeaponSlot
	{
		/** The weapon's id, for its icon. */
		FName Id;
		FString Key;
		FString Name;
		bool bHeld = false;
		bool bCanDraw = true;
	};
	/** The character's weapons, for the list over the ammunition (drawn when there are two or more): the inventory's slots in order, 1 the primary, 2 the sidearm, 3 the knife. */
	virtual TArray<FWeaponSlot> WeaponSlots(const ACheeseStrikeCharacter* Character) const;

	/** The minimap's picture of the level, for the tests. */
	const FCheeseStrikeMinimap& GetMinimap() const { return Minimap; }
	/** `Player`'s status in the row along the top, `Body` being their character (BodyOf). */
	static CheeseStrikeHUD::EStatus StatusOf(const ACheeseStrikePlayerState* Player, const ACheeseStrikeCharacter* Body);
	/** The character that is `Player`'s now (the live one if there is one), or null. */
	static ACheeseStrikeCharacter* BodyOf(const APlayerState* Player);

protected:
	/** Pixels for a design pixel at 1080 lines: the HUD is laid out for 1920 by 1080 and scales with the height. */
	float Unit() const;

	void DrawFight(const ACheeseStrikeCharacter* Character, float FieldOfView);
	/** What the crosshair shows over what `Character`'s player aims at: the cancel sign over an enemy down whom a shot would not hurt (CheeseStrikeHUD::CrosshairFor). */
	CheeseStrikeHUD::ECrosshair CrosshairOver(const ACheeseStrikeCharacter* Character) const;
	void DrawHealth(const ACheeseStrikeCharacter* Character);
	/** The kill-streak panel over the health (UI/CheeseStrikeHUDStreak.cpp): the run's count, popping at each kill, and its track to the streaks. */
	void DrawStreak(const ACheeseStrikeCharacter* Character);
	/** Where the kill-streak panel's top is, in the canvas's pixels. */
	float StreakPanelTop() const;
	void DrawWeapon(const ACheeseStrikeCharacter* Character);
	/** The outline of the weapon in `Slot` (a rifle, a pistol, a knife) in the box at `At`, `Size` big, facing left. */
	void DrawWeaponIcon(CheeseStrikeHUDDraw::FShapes& Shapes, EWeaponSlot Slot, const FVector2D& At, const FVector2D& Size, const FLinearColor& Colour) const;
	void DrawVignette(const ACheeseStrikeCharacter* Character);
	void DrawDamageArcs(const ACheeseStrikeCharacter* Character);
	void DrawMinimap(const ACheeseStrikePlayerController* Controller, const ACheeseStrikeCharacter* Character, float FieldOfView);
	/** Where a map of the level is drawn: the screen's square, the world's point (X and Y) at its middle, pixels a cm, its corners' rounding, the yaw it is turned by (up is that way), and whether it is the strike map. */
	struct FMapFrame
	{
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
		FVector2D Middle = FVector2D::ZeroVector;
		float Scale = 1.0f;
		float Corner = 0.0f;
		float ViewYaw = 0.0f;
		bool bStrike = false;
	};
	/** The minimap's map, its players, the hill, the stands and the frame, in `Frame`: the minimap's (DrawMinimap), or the strike map's. */
	void DrawMap(const ACheeseStrikePlayerController* Controller, const ACheeseStrikeCharacter* Character, float FieldOfView, const FMapFrame& Frame);
	/**
	 * Choosing where a Cheese Strike goes (ACheeseStrikePlayerState::IsMarkingStrike;
	 * UI/CheeseStrikeHUDStrike.cpp): the minimap big in the middle of the
	 * screen, turned as the view was, following the mark (the look moves
	 * it), with the strike's area round it, where its wheels will land, and
	 * the keys to call it in or off. Draws nothing and returns false when
	 * not choosing.
	 */
	bool DrawStrikeMap(const ACheeseStrikePlayerController* Controller, const ACheeseStrikeCharacter* Character, float FieldOfView);
	/** The strike map's middle (world X and Y), which follows the mark; whether it showed the frame before. */
	FVector2D StrikeMapMiddle = FVector2D::ZeroVector;
	bool bStrikeMapShown = false;
	void DrawPlayers(const ACheeseStrikeGameState* Match);
	void DrawScoreSummary(const ACheeseStrikeGameState* Match);
	void DrawNameTags(const ACheeseStrikePlayerController* Controller);
	/** The death screen; out of the round (`bOut`), only for a few seconds (CheeseStrikeHUD::ShowsDeathScreen), and without the seconds to coming back. */
	void DrawDeath(const ACheeseStrikeCharacter* Character, const ACheeseStrikeGameState* Match, bool bOut);
	void DrawKillFeed(const ACheeseStrikeGameState* Match);
	void DrawScoreboard(const ACheeseStrikeGameState* Match, bool bOver);
	/** The mice's countdown, red, and word of their coming. */
	void DrawMice(const ACheeseStrikeGameState* Match);
	/** The count before the match and its rounds, big in the middle (UI/CheeseStrikeHUDCountdown.cpp). */
	void DrawCountdown(const ACheeseStrikeGameState* Match);
	/** The countdown's `Word` (3, 2, 1, FIGHT) in cheese, loaded as it is first shown; null where there is none. */
	UTexture2D* CountdownPicture(const FString& Word);
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> CountdownPictures;
	/** The health the HUD shows: the character's, or CheeseStrike.HUD.Health's for judging the red edges (development). */
	static float HealthShown(const ACheeseStrikeCharacter* Character);
	/**
	 * While the player's side has an Unmanned Aerial Cheese up
	 * (ACheeseStrikeGameState::RevealEndsFor), notes where every enemy is at
	 * each of its sweeps (ACheeseStrikePlayerState::SweepMarks), for the
	 * minimap's red dots and the marks in the view.
	 */
	void UpdateSweep(const ACheeseStrikePlayerController* Controller);
	/** Writes to the log when the GPU's memory has spilled into the system's (the engine's warning of it, with its other messages, is kept off the player's screen in the game). */
	void NoteVideoMemory();
	/** Notes which enemies the player can see, ten times a second, for the minimap. */
	void LookForEnemies(const ACheeseStrikePlayerController* Controller);

	/** The red edges' picture: white, clear in the middle and solid at the edges, made in code. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> VignetteTexture;
	/** Each weapon's icon, its silhouette from the side (tools/art/weapon_icons.py), loaded as it is first shown; null where there is none. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> WeaponIcons;
	UTexture2D* WeaponIcon(FName Id);
	/** Draws `Id`'s icon to fit `Size` at `At` (its right side at At.X + Size.X), in `Colour`; false if it has none. */
	bool DrawWeaponPicture(FName Id, const FVector2D& At, const FVector2D& Size, const FLinearColor& Colour);
	/** Looking through a scope: the view round it black, a reticle across it. */
	void DrawScope(const ACheeseStrikeCharacter* Character);
	UPROPERTY(Transient)
	TObjectPtr<UCheeseStrikeAvatars> Avatars;
	FCheeseStrikeMinimap Minimap;
	TArray<FDamageArc> DamageArcs;
	/** When each enemy was last seen (the world's time). */
	TMap<TWeakObjectPtr<const ACheeseStrikeCharacter>, float> LastSeen;
	float NextLook = 0.0f;
	/** Until when every enemy shows on the minimap (the world's time). */
	float RevealedUntil = -1.0f;
	/** When the reload under way began, for its bar (the world's time; -1 for none). */
	float ReloadBegan = -1.0f;
	/** The health bar's trail: the health just lost shows a moment longer, then runs down to what is left (-1 before the first frame). */
	float HealthTrail = -1.0f;
	/** When the health last fell (the world's time), for how long the trail holds, and the health the frame before. */
	float HealthFellAt = -1.0f;
	float HealthBefore = -1.0f;
	/** The kill-streak panel's: the run last drawn, when it last grew and last earned a streak (the world's time), and how much of its track is filled now. */
	int32 StreakSeen = 0;
	float StreakChangedAt = -100.0f;
	float StreakEarnedAt = -100.0f;
	float StreakFill = 0.0f;
	/** Where each kill in the feed happened (the map's name for the place the body fell), by the killed and the time. */
	TMap<FString, FString> KillPlaces;
	/** The place of `Kill`: found once, from the body where it lies, and kept while the kill is in the feed. */
	FString KillPlace(const struct FKillFeedEntry& Kill, float Now);
	/** The Unmanned Aerial Cheese's sweep now, for the minimap's pulse. */
	CheeseStrikeHUD::FSweep Sweep;
	/** How much video memory had spilled over when last written to the log (MB). */
	float VideoMemorySpilled = 0.0f;
	float NextVideoMemoryNote = 0.0f;
};
