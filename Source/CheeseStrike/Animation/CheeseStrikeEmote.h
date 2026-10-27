// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "CheeseStrikeEmote.generated.h"

class UAnimSequenceBase;

/**
 * An emote played in a loop without a seam. A loop of an emote that wasn't
 * made to loop jumps from its last pose to its first; this plays it as two
 * players of the same sequence, the end of each pass blended into the start
 * of the next over Overlap seconds, so that the motion runs on.
 */
namespace CheeseStrikeEmote
{
	/** The most of a sequence's length the blend may take (a share), and the seconds it takes if the sequence is long enough. */
	constexpr float MostOverlapShare = 0.3f;
	constexpr float WantedOverlap = 0.6f;
	/** Sequences shorter than this (seconds) are played plainly: there is too little of them to blend. */
	constexpr float ShortestLooped = 0.2f;

	/** Seconds the blend takes for a sequence `Length` seconds long. */
	float OverlapFor(float Length);

	/** Where the two players of a sequence are at one moment, and how much of the second shows. */
	struct FLoop
	{
		float First = 0.0f;
		float Second = 0.0f;
		float SecondShows = 0.0f;
	};

	/**
	 * `Time` seconds into playing a sequence `Length` seconds long with a blend of `Overlap`: the loop repeats every
	 * Length - Overlap seconds. The first player runs from Overlap to Length through each pass; in the pass's last Overlap
	 * seconds the second, from the sequence's start, is blended in (smoothly), so that at the end of the pass the second
	 * is at Overlap and shows alone, where the first begins the next pass: the picture does not change at the seam.
	 */
	FLoop LoopAt(float Time, float Length, float Overlap);
}

/** Plays one emote on a hero's mesh through two sequence players and a blend (FCheeseStrikeEmoteProxy). */
struct FCheeseStrikeEmoteProxy : public FAnimInstanceProxy
{
	FCheeseStrikeEmoteProxy() = default;
	explicit FCheeseStrikeEmoteProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void CacheBones() override;

	/** What the instance has been asked to play, copied in on the game thread, and how far in it is. */
	TObjectPtr<UAnimSequenceBase> Emote;
	float Time = 0.0f;
	int32 Serial = 0;

protected:
	FAnimNode_SequencePlayer_Standalone First;
	FAnimNode_SequencePlayer_Standalone Second;
	FAnimNode_TwoWayBlend Blend;
	bool bLinked = false;
	int32 AssignedSerial = -1;
};

/**
 * The animation of a podium hero: one emote (SetEmote), looped without a
 * seam. No animation Blueprint, as the characters' (UCheeseStrikeAnimInstance).
 */
UCLASS(Transient)
class CHEESESTRIKE_API UCheeseStrikeEmoteAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Plays `InEmote` from `StartAt` seconds in (it loops, so the start is only where in its motion). */
	void SetEmote(UAnimSequenceBase* InEmote, float StartAt);
	UAnimSequenceBase* GetEmote() const { return Emote; }
	/** Whether an emote is playing. */
	bool IsEmoting() const { return Emote != nullptr; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	friend struct FCheeseStrikeEmoteProxy;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Emote;
	float StartAt = 0.0f;
	int32 Serial = 0;
	FCheeseStrikeEmoteProxy Proxy;
};
