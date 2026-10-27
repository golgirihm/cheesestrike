// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeEmote.h"
#include "Animation/AnimSequenceBase.h"

namespace CheeseStrikeEmote
{
	float OverlapFor(float Length)
	{
		return Length < ShortestLooped ? 0.0f : FMath::Min(WantedOverlap, Length * MostOverlapShare);
	}

	FLoop LoopAt(float Time, float Length, float Overlap)
	{
		FLoop Result;
		if (Overlap <= 0.0f || Length <= Overlap)
		{
			// Too short to blend: plainly round.
			Result.First = Length > 0.0f ? FMath::Fmod(FMath::Max(Time, 0.0f), Length) : 0.0f;
			Result.Second = Result.First;
			return Result;
		}
		const float Period = Length - Overlap;
		const float Into = FMath::Fmod(FMath::Max(Time, 0.0f), Period);
		Result.First = Overlap + Into;
		const float BlendFrom = Period - Overlap;
		if (Into > BlendFrom)
		{
			const float Along = FMath::Clamp((Into - BlendFrom) / Overlap, 0.0f, 1.0f);
			Result.Second = Into - BlendFrom;
			Result.SecondShows = FMath::SmoothStep(0.0f, 1.0f, Along);
		}
		return Result;
	}
}

void FCheeseStrikeEmoteProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	if (!bLinked)
	{
		Blend.A.SetLinkNode(&First);
		Blend.B.SetLinkNode(&Second);
		for (FAnimNode_SequencePlayer_Standalone* Player : {&First, &Second})
		{
			// Placed by the proxy each frame, not played.
			Player->SetLoopAnimation(false);
			Player->SetPlayRate(0.0f);
		}
		bLinked = true;
	}
	Blend.Initialize_AnyThread(FAnimationInitializeContext(this));
}

void FCheeseStrikeEmoteProxy::CacheBones()
{
	FAnimInstanceProxy::CacheBones();
	if (bLinked)
	{
		Blend.CacheBones_AnyThread(FAnimationCacheBonesContext(this));
	}
}

void FCheeseStrikeEmoteProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UCheeseStrikeEmoteAnimInstance* Instance = Cast<UCheeseStrikeEmoteAnimInstance>(InAnimInstance))
	{
		if (Serial != Instance->Serial)
		{
			// A new emote: from where it was told to start.
			Serial = Instance->Serial;
			Emote = Instance->Emote;
			Time = Instance->StartAt;
		}
	}
}

void FCheeseStrikeEmoteProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);
	if (!bLinked || Emote == nullptr)
	{
		return;
	}
	if (AssignedSerial != Serial)
	{
		First.SetSequence(Emote);
		Second.SetSequence(Emote);
		AssignedSerial = Serial;
	}
	Time += DeltaSeconds;
	const float Length = Emote->GetPlayLength();
	const CheeseStrikeEmote::FLoop Loop = CheeseStrikeEmote::LoopAt(Time, Length, CheeseStrikeEmote::OverlapFor(Length));
	First.SetAccumulatedTime(Loop.First);
	Second.SetAccumulatedTime(Loop.Second);
	Blend.Alpha = Loop.SecondShows;
}

void FCheeseStrikeEmoteProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	if (bLinked)
	{
		Blend.Update_AnyThread(InContext);
	}
}

bool FCheeseStrikeEmoteProxy::Evaluate(FPoseContext& Output)
{
	if (!bLinked || Emote == nullptr)
	{
		return false;
	}
	Blend.Evaluate_AnyThread(Output);
	return true;
}

void UCheeseStrikeEmoteAnimInstance::SetEmote(UAnimSequenceBase* InEmote, float InStartAt)
{
	Emote = InEmote;
	StartAt = InStartAt;
	++Serial;
}

FAnimInstanceProxy* UCheeseStrikeEmoteAnimInstance::CreateAnimInstanceProxy()
{
	return &Proxy;
}

void UCheeseStrikeEmoteAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	// Ours lives and dies with the instance.
}
