// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Footsteps: when a body sounds a step and how loudly (Rules/CheeseStrikeFootsteps.h),
// and the sounds that are made in code for them and for the streaks
// (Streaks/CheeseStrikeSynth.h).

#include "Misc/AutomationTest.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Rules/CheeseStrikeFootsteps.h"
#include "Streaks/CheeseStrikeSynth.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace FootstepTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** The steps a body makes going at `Speed` in `Gait` for `Seconds`, frame by frame at 60 a second. */
	int32 StepsIn(CheeseStrikeFootsteps::EGait Gait, float Speed, float Seconds)
	{
		float Progress = CheeseStrikeFootsteps::StartProgress;
		int32 Steps = 0;
		const int32 Frames = FMath::RoundToInt(Seconds * 60.0f);
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Steps += CheeseStrikeFootsteps::Advance(Progress, Gait, Speed / 60.0f) ? 1 : 0;
		}
		return Steps;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFootstepGaits, "CheeseStrike.Rules.FootstepGaits", FootstepTests::Flags)

bool FFootstepGaits::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFootsteps;
	const float Walk = CheeseStrikeMovement::WalkSpeed;
	TestEqual(TEXT("walking steps"), GaitFor(true, true, false, false, false, Walk), EGait::Walk);
	TestEqual(TEXT("sprinting steps as a sprint"), GaitFor(true, true, false, false, true, Walk * CheeseStrikeMovement::SprintFactor), EGait::Sprint);
	TestEqual(TEXT("a crouched walk is soft"), GaitFor(true, true, false, true, false, Walk * CheeseStrikeMovement::CrouchFactor), EGait::Crouch);
	TestEqual(TEXT("a crouch-still is silent"), GaitFor(true, true, false, true, false, 0.0f), EGait::None);
	TestEqual(TEXT("standing still is silent"), GaitFor(true, true, false, false, false, 5.0f), EGait::None);
	TestEqual(TEXT("in the air, silent"), GaitFor(true, false, false, false, true, Walk * 2.0f), EGait::None);
	TestEqual(TEXT("a slide is silent"), GaitFor(true, true, true, true, false, Walk * 1.5f), EGait::None);
	TestEqual(TEXT("the dead are silent"), GaitFor(false, true, false, false, false, Walk), EGait::None);
	TestTrue(TEXT("a sprint's step is louder than a walk's"), Volume(EGait::Sprint) > Volume(EGait::Walk));
	TestTrue(TEXT("a walk's step is louder than a crouch's"), Volume(EGait::Walk) > Volume(EGait::Crouch));
	TestEqual(TEXT("no gait, no volume"), Volume(EGait::None), 0.0f);
	TestEqual(TEXT("no gait, no stride"), Stride(EGait::None), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFootstepsComeOnTheStride, "CheeseStrike.Rules.FootstepsComeOnTheStride", FootstepTests::Flags)

bool FFootstepsComeOnTheStride::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFootsteps;
	const float Walk = CheeseStrikeMovement::WalkSpeed;
	const float Sprint = Walk * CheeseStrikeMovement::SprintFactor;
	// Ten seconds of walking covers 2880 cm: 18 strides of 152 (the first sooner, as a walk begins part way through one).
	const int32 Walking = FootstepTests::StepsIn(EGait::Walk, Walk, 10.0f);
	TestTrue(TEXT("a walk steps about twice a second"), Walking >= 18 && Walking <= 20);
	const int32 Sprinting = FootstepTests::StepsIn(EGait::Sprint, Sprint, 10.0f);
	TestTrue(TEXT("a sprint steps faster than a walk"), Sprinting > Walking);
	TestEqual(TEXT("standing steps never"), FootstepTests::StepsIn(EGait::None, 0.0f, 10.0f), 0);
	// A long frame makes one step, not several at once.
	float Progress = 0.9f;
	TestTrue(TEXT("a long frame steps"), Advance(Progress, EGait::Walk, 2000.0f));
	TestTrue(TEXT("but only once"), Progress < 1.0f);
	// Coming to rest readies the first step of the next walk to come sooner than a whole stride.
	Progress = 0.0f;
	Advance(Progress, EGait::None, 0.0f);
	TestEqual(TEXT("at rest, part way through a stride"), Progress, StartProgress);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSynthesisedSounds, "CheeseStrike.Rules.SynthesisedSoundsAreWhole", FootstepTests::Flags)

bool FSynthesisedSounds::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeSynth;
	auto Peak = [](const TArray<int16>& Samples)
	{
		int32 Loudest = 0;
		for (const int16 Sample : Samples)
		{
			Loudest = FMath::Max(Loudest, FMath::Abs(static_cast<int32>(Sample)));
		}
		return Loudest;
	};
	const TArray<int16> Step = Footstep(0);
	TestTrue(TEXT("a footstep is a moment long"), Step.Num() > SampleRate / 20 && Step.Num() < SampleRate / 4);
	TestTrue(TEXT("and can be heard"), Peak(Step) > 10000);
	TestTrue(TEXT("and clipped never"), Peak(Step) < 32767);
	TestTrue(TEXT("the variants differ"), Footstep(0) != Footstep(1));
	const TArray<int16> Jet = JetPass(3.0f);
	TestEqual(TEXT("a jet pass lasts as asked"), Jet.Num(), 3 * SampleRate);
	TestTrue(TEXT("and can be heard"), Peak(Jet) > 10000);
	TestTrue(TEXT("it swells to the fly-by: the middle is louder than the start"), Peak(TArray<int16>(Jet.GetData() + Jet.Num() * 4 / 10, Jet.Num() / 5)) > 3 * Peak(TArray<int16>(Jet.GetData(), Jet.Num() / 20)));
	const TArray<int16> Blast = Explosion(3.0f, 1);
	TestEqual(TEXT("an explosion lasts as asked"), Blast.Num(), 3 * SampleRate);
	TestTrue(TEXT("it begins with its crack"), Peak(TArray<int16>(Blast.GetData(), SampleRate / 20)) > 20000);
	TestTrue(TEXT("and has died down by the end"), Peak(TArray<int16>(Blast.GetData() + Blast.Num() * 9 / 10, Blast.Num() / 10)) < 4000);
	return true;
}

#endif
