// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "CheeseStrikeSettings.generated.h"

class USoundClass;

/**
 * The player's options as plain rules, so that tests can call them without
 * a world: what each may be (the Godot game's ranges, and the design's
 * field of view, docs/unreal/design.md "Controls"), and what follows from
 * them. UCheeseStrikeSettings keeps and applies them.
 */
namespace CheeseStrikeSettings
{
	/** How fast looking turns, as a share of the usual. */
	constexpr float DefaultSensitivity = 1.0f;
	constexpr float MinSensitivity = 0.1f;
	constexpr float MaxSensitivity = 4.0f;
	/** The share of that while aiming down the sights. */
	constexpr float DefaultAimedSensitivity = 1.0f;
	constexpr float MinAimedSensitivity = 0.2f;
	constexpr float MaxAimedSensitivity = 2.0f;
	/** The field of view from the top of the view to the bottom, in degrees: it stays the same whatever the screen's shape, and more is seen across a wider one. */
	constexpr float DefaultFieldOfView = 75.0f;
	constexpr float MinFieldOfView = 60.0f;
	constexpr float MaxFieldOfView = 110.0f;
	/** Loudness, from 0 (silent) to 1. */
	constexpr float DefaultVolume = 1.0f;

	/** The field of view across a view `Aspect` (width over height) wide whose field of view top to bottom is `Vertical`: what the engine's camera takes. */
	float HorizontalFieldOfView(float Vertical, float Aspect);
	/** How much the view turns for a turn of the mouse or stick, across and up and down: the sensitivity, its aimed share while `bAimed`, and up and down the other way round if `bInvert`. */
	FVector2D LookScale(float Sensitivity, float AimedShare, bool bInvert, bool bAimed);

	/** The frame rate limits offered, 0 for none. */
	extern const int32 FrameRateLimits[];
	int32 NumFrameRateLimits();
	/** Which of FrameRateLimits `Limit` is, or the nearest. */
	int32 FrameRateLimitIndex(float Limit);
	/**
	 * The resolutions offered, smallest first: those of the screen's
	 * (`Supported`) that are the common 16:9 ones, no bigger than the
	 * `Desktop`'s, and the desktop's own.
	 */
	TArray<FIntPoint> Resolutions(const TArray<FIntPoint>& Supported, FIntPoint Desktop);
	/** Which of `Choices` is `Resolution`, or the biggest no bigger than it (the first if none). */
	int32 ResolutionIndex(const TArray<FIntPoint>& Choices, FIntPoint Resolution);
}

/**
 * The player's options, kept between runs in GameUserSettings.ini: the
 * engine's graphics settings (UGameUserSettings: the quality preset, the
 * window, the resolution, VSync and the frame rate limit), and the game's
 * own for looking, the view and the sound. Config/DefaultEngine.ini makes
 * this the engine's settings class. Each is applied as it is set: the look
 * by the player controller (AddYawInput), the field of view by the camera
 * manager, the volumes by ApplySound, the graphics by the engine.
 */
UCLASS(config=GameUserSettings, configdonotcheckdefaults)
class CHEESESTRIKE_API UCheeseStrikeSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	/** The game's settings, or null if the engine was given another class. */
	static UCheeseStrikeSettings* Get();

	float GetMouseSensitivity() const { return MouseSensitivity; }
	void SetMouseSensitivity(float Value);
	float GetAimedSensitivity() const { return AimedSensitivity; }
	void SetAimedSensitivity(float Value);
	bool IsLookInverted() const { return bInvertLook; }
	void SetLookInverted(bool bValue) { bInvertLook = bValue; }
	/** Whether a gamepad's look stick gets the aim assist (CheeseStrikeAimAssist). */
	bool IsAimAssistOn() const { return bAimAssist; }
	void SetAimAssistOn(bool bValue) { bAimAssist = bValue; }
	float GetFieldOfView() const { return FieldOfView; }
	void SetFieldOfView(float Value);
	float GetMasterVolume() const { return MasterVolume; }
	void SetMasterVolume(float Value);
	float GetEffectsVolume() const { return EffectsVolume; }
	void SetEffectsVolume(float Value);

	/** The field of view top to bottom the camera has now, from the settings if there are any. */
	static float FieldOfViewNow();
	/** Puts the volumes on the sound: the master volume on everything, the effects volume on every sound but the menu's (MenuSoundClass). Every sound class it sets is kept loaded from then on, as the audio device keeps pointing at it. */
	static void ApplySound();
	/** The sound class the menu's sounds play in, which the effects volume leaves alone. */
	static USoundClass* MenuSoundClass();

	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

protected:
	UPROPERTY(config)
	float MouseSensitivity = CheeseStrikeSettings::DefaultSensitivity;
	UPROPERTY(config)
	float AimedSensitivity = CheeseStrikeSettings::DefaultAimedSensitivity;
	UPROPERTY(config)
	bool bInvertLook = false;
	UPROPERTY(config)
	bool bAimAssist = true;
	UPROPERTY(config)
	float FieldOfView = CheeseStrikeSettings::DefaultFieldOfView;
	UPROPERTY(config)
	float MasterVolume = CheeseStrikeSettings::DefaultVolume;
	UPROPERTY(config)
	float EffectsVolume = CheeseStrikeSettings::DefaultVolume;
};
