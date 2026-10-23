// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Execution's executions, as Gears of War 2 has them: a press of Interact
 * over an enemy down starts one of two at random, seen by everyone in third
 * person, the executor's own view pulled out to watch it, and the executor
 * can't be hurt while it plays. The rules of them, plain arithmetic tested
 * without a world (CheeseStrike.Rules.Executions*): which one, how long,
 * when the blow lands and kills, where the executor stands to deal it and
 * where the camera watches from. The pose is FCheeseStrikeExecutionPose's
 * (Animation/), the component that runs it UCheeseStrikeLastStand's.
 *
 * Lengths are centimetres, times seconds from the press. Places "in the
 * body" are in the frame of the player down (CheeseStrikeDowned's: X ahead
 * toward the feet, Y to the right, Z up from the capsule's axis on the
 * floor).
 */
namespace CheeseStrikeExecution
{
	enum class EKind : uint8
	{
		None,
		/** Knelt beside the chest, the cheese knife driven down into it twice. */
		Stab,
		/** Stood at the head, a boot raised high and brought down on it: the head bursts into chunks of cheese. */
		Stomp,
	};
	constexpr int32 NumKinds = 2;
	/** The kind for a roll of the dice (any number): a stab or a stomp, half the time each. */
	EKind Pick(int32 Roll);
	/** The kind with number `Index` (as replicated); None for one there isn't. */
	EKind KindAt(int32 Index);
	/** As the log and the tests name it: "stab", "stomp". */
	const TCHAR* Name(EKind Kind);

	/** How long each lasts, start to the executor standing again. */
	constexpr float StabSeconds = 2.6f;
	constexpr float StompSeconds = 2.3f;
	float Seconds(EKind Kind);
	/** When the knife goes in (twice), and when the boot lands, the head bursting. */
	constexpr float StabLands[2] = {1.0f, 1.6f};
	constexpr float StompLands = 1.05f;
	/** When the one down dies: as the knife comes out after the second blow; as the boot lands. */
	float KillAt(EKind Kind);
	/** Whether it is still playing `Elapsed` seconds in (the executor unhurt, nobody moving). */
	bool IsPlaying(EKind Kind, float Elapsed);

	/**
	 * Where the executor stands, in the body: beside the chest, StabStandOff
	 * out on side `Side` (1 right, -1 left), to kneel over it; beside the
	 * head, StompStandOff out, to stamp on it. They face the point they
	 * deal the blow to (AimPoint), across the body.
	 */
	constexpr float StabStandOff = 58.0f;
	constexpr float StompStandOff = 48.0f;
	FVector SpotInBody(EKind Kind, float Side);
	/** What the executor faces and strikes, in the body: the chest for the knife, the head for the boot. */
	FVector AimPoint(EKind Kind);
	/** The yaw the executor faces, standing at `Spot` (world) to strike `Aim` (world). */
	float FacingYaw(const FVector& Spot, const FVector& Aim);

	/**
	 * Where the camera watching it is, in the body of the one down, `Elapsed`
	 * in: on the far side from the executor, toward the feet and up, so that
	 * the executor's face and the blow are in view, the one down in front;
	 * drifting round a little as it plays. It looks at LookAt.
	 */
	constexpr float CameraBack = 240.0f;
	constexpr float CameraAlong = 220.0f;
	constexpr float CameraUp = 195.0f;
	constexpr float CameraDrift = 18.0f;
	FVector CameraInBody(EKind Kind, float Side, float Elapsed);
	FVector LookAtInBody(EKind Kind, float Side);
	/** The view blends out to it and back over these. */
	constexpr float ViewBlendOut = 0.35f;
	constexpr float ViewBlendBack = 0.3f;

	/** How many chunks of cheese the head bursts into, and how fast they fly (cm/s). */
	constexpr int32 Chunks = 16;
	constexpr float ChunkSpeed = 380.0f;
	constexpr float ChunkLife = 6.0f;

	/** Eases 0 to 1 over [From, To] (smoothly at both ends); 0 before, 1 after. */
	float Ease(float Time, float From, float To);
	/** 0 before `In`, up to 1 by `Full`, 1 until `Out`, back to 0 by `Gone`, smoothly. */
	float Envelope(float Time, float In, float Full, float Out, float Gone);
}
