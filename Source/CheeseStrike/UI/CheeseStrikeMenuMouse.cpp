// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMenuMouse.h"
#include "Animation/AnimSequence.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "CheeseStrike.h"

namespace
{
	/** How long a change of animation crossfades, seconds. */
	constexpr float MenuMouseBlend = 0.25f;
	/** How many bones from the skull down its neck share the head's turn. */
	constexpr int32 MenuMouseNeckBones = 3;
	/** How far ahead of its middle its head is, at its size (centimetres along the way it faces). */
	constexpr float MenuMouseHeadAhead = 40.0f;
}

// In from the right of the view (the camera looks along +Y, so -X is to its right), stopping right of the figure, its cheese
// ahead of it and further back, between it and the figure; then off to the left, in front of the cheese and behind the figure. The floor there is clear between
// the pedestal and the cheese walls behind it on the front end's level.
const FVector ACheeseStrikeMenuMouse::PathFrom(-480.0f, 95.0f, -6.0f);
const FVector ACheeseStrikeMenuMouse::PathStop(-200.0f, 95.0f, -6.0f);
const FVector ACheeseStrikeMenuMouse::PathTo(480.0f, 95.0f, -6.0f);
const FVector ACheeseStrikeMenuMouse::CheeseAt(-105.0f, 150.0f, -6.0f);

void FCheeseStrikeMenuMouseProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	if (!bLinked)
	{
		Blend.A.SetLinkNode(&First);
		Blend.B.SetLinkNode(&Second);
		for (FAnimNode_SequencePlayer_Standalone* Player : {&First, &Second})
		{
			Player->SetLoopAnimation(true);
		}
		bLinked = true;
	}
	Blend.Initialize_AnyThread(FAnimationInitializeContext(this));
}

void FCheeseStrikeMenuMouseProxy::CacheBones()
{
	FAnimInstanceProxy::CacheBones();
	if (!bLinked)
	{
		return;
	}
	Blend.CacheBones_AnyThread(FAnimationCacheBonesContext(this));
	// The skull and the bones above it up the neck, which its head turns by.
	HeadBones.Reset();
	const FBoneContainer& Bones = GetRequiredBones();
	FBoneReference Skull(TEXT("head_head_001"));
	Skull.Initialize(Bones);
	if (Skull.IsValidToEvaluate(Bones))
	{
		for (FCompactPoseBoneIndex At = Skull.GetCompactPoseIndex(Bones); At.IsValid() && HeadBones.Num() < MenuMouseNeckBones; At = Bones.GetParentBoneIndex(At))
		{
			HeadBones.Insert(At, 0);
		}
	}
}

void FCheeseStrikeMenuMouseProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UCheeseStrikeMenuMouseAnimInstance* Instance = Cast<UCheeseStrikeMenuMouseAnimInstance>(InAnimInstance))
	{
		Wanted = Instance->Sequence;
		WantedRate = Instance->PlayRate;
		HeadYaw = Instance->HeadYaw;
	}
}

void FCheeseStrikeMenuMouseProxy::Update(float DeltaSeconds)
{
	FAnimInstanceProxy::Update(DeltaSeconds);
	if (!bLinked || Wanted == nullptr)
	{
		return;
	}
	if (Playing == nullptr)
	{
		// The first: shown at once.
		First.SetSequence(Wanted);
		First.SetAccumulatedTime(0.0f);
		Playing = Wanted;
		bOnSecond = false;
		Alpha = 0.0f;
	}
	else if (Wanted != Playing)
	{
		// Another: started on the other player, and blended over to it.
		FAnimNode_SequencePlayer_Standalone& Next = bOnSecond ? First : Second;
		Next.SetSequence(Wanted);
		Next.SetAccumulatedTime(0.0f);
		Playing = Wanted;
		bOnSecond = !bOnSecond;
	}
	(bOnSecond ? Second : First).SetPlayRate(WantedRate);
	Alpha = FMath::FInterpConstantTo(Alpha, bOnSecond ? 1.0f : 0.0f, DeltaSeconds, 1.0f / MenuMouseBlend);
	Blend.Alpha = Alpha;
}

void FCheeseStrikeMenuMouseProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	if (bLinked && Playing != nullptr)
	{
		Blend.Update_AnyThread(InContext);
	}
}

bool FCheeseStrikeMenuMouseProxy::Evaluate(FPoseContext& Output)
{
	if (!bLinked || Playing == nullptr)
	{
		return false;
	}
	Blend.Evaluate_AnyThread(Output);
	if (FMath::Abs(HeadYaw) < 0.1f || HeadBones.Num() == 0)
	{
		return true;
	}
	// The head turned about the up axis, a share at each bone from the neck to the skull, each about itself and what is
	// beyond it with it: the bones' own axes don't matter.
	FCSPose<FCompactPose> Pose;
	Pose.InitPose(Output.Pose);
	const FQuat Share(FVector::UpVector, FMath::DegreesToRadians(HeadYaw / HeadBones.Num()));
	for (const FCompactPoseBoneIndex Bone : HeadBones)
	{
		FTransform Now = Pose.GetComponentSpaceTransform(Bone);
		Now.SetRotation((Share * Now.GetRotation()).GetNormalized());
		const FBoneTransform Set(Bone, Now);
		Pose.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
	}
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Pose, Output.Pose);
	return true;
}

FAnimInstanceProxy* UCheeseStrikeMenuMouseAnimInstance::CreateAnimInstanceProxy()
{
	return &Proxy;
}

void UCheeseStrikeMenuMouseAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	// Ours lives and dies with the instance.
}

ACheeseStrikeMenuMouse::ACheeseStrikeMenuMouse()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	Stage = CreateDefaultSubobject<USceneComponent>(TEXT("Stage"));
	RootComponent = Stage;

	// The mice's rat and its walk and sniff, loaded in BeginPlay as the mice's are (their scripts may rebuild them).
	Model = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat.SK_BlackRat")));
	Walk = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat_Anim_Mammals_walk_A1.SK_BlackRat_Anim_Mammals_walk_A1")));
	Sniff = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat_Anim_Mammals_idle_A2.SK_BlackRat_Anim_Mammals_idle_A2")));
	// Down on all fours again, still, to look at the cheese.
	Stand = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Fab/Nestaeric/BlackRat/SK_BlackRat_Anim_Mammals_idle_A1.SK_BlackRat_Anim_Mammals_idle_A1")));
	Wheel = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/CheeseStrike/Meshes/SM_CheeseWheel.SM_CheeseWheel")));
	WheelLook = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/CheeseStrike/Materials/M_Cheese.M_Cheese")));

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Stage);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetRelativeScale3D(FVector(CheeseStrikeMenuMouse::Scale));
	Mesh->SetAnimInstanceClass(UCheeseStrikeMenuMouseAnimInstance::StaticClass());
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh->SetVisibility(false);

	// A wheel of cheese on the floor in the background, for it to find.
	Cheese = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cheese"));
	Cheese->SetupAttachment(Stage);
	Cheese->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cheese->SetCanEverAffectNavigation(false);
	// The wheel is a metre across and high, standing on its middle.
	Cheese->SetRelativeLocation(CheeseAt + FVector(0.0f, 0.0f, CheeseHeight / 2.0f));
	Cheese->SetRelativeRotation(FRotator(0.0f, 30.0f, 0.0f));
	Cheese->SetRelativeScale3D(FVector(CheeseWidth / 100.0f, CheeseWidth / 100.0f, CheeseHeight / 100.0f));
}

void ACheeseStrikeMenuMouse::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetSkeletalMeshAsset(Model.LoadSynchronous());
	Mesh->SetAnimInstanceClass(UCheeseStrikeMenuMouseAnimInstance::StaticClass());
	WalkAnimation = Walk.LoadSynchronous();
	SniffAnimation = Sniff.LoadSynchronous();
	StandAnimation = Stand.LoadSynchronous();
	Cheese->SetStaticMesh(Wheel.LoadSynchronous());
	if (UMaterialInterface* Look = WheelLook.LoadSynchronous())
	{
		Cheese->SetMaterial(0, Look);
	}
	Clock = 0.0f;
	NextAt = CheeseStrikeMenuMouse::NextVisitIn(true, FMath::FRand());
	Place();
}

void ACheeseStrikeMenuMouse::ComeNow()
{
	StartedAt = Clock;
	Place();
}

void ACheeseStrikeMenuMouse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	if (StartedAt < 0.0f && Clock >= NextAt)
	{
		StartedAt = Clock;
	}
	Place();
}

void ACheeseStrikeMenuMouse::Place()
{
	using namespace CheeseStrikeMenuMouse;
	const float WalkIn = (PathStop - PathFrom).Size() / WalkSpeed;
	const float WalkOff = (PathTo - PathStop).Size() / WalkSpeed;
	Moment = StartedAt >= 0.0f ? At(Clock - StartedAt, WalkIn, WalkOff) : FMoment();
	if (StartedAt >= 0.0f && Moment.Phase == EPhase::Away)
	{
		// The visit is over: the next comes later.
		StartedAt = -1.0f;
		NextAt = Clock + NextVisitIn(false, FMath::FRand());
	}
	const bool bShown = Moment.Phase != EPhase::Away;
	if (Mesh->IsVisible() != bShown)
	{
		Mesh->SetVisibility(bShown);
	}
	if (!bShown)
	{
		return;
	}
	const FVector Where = Moment.Phase == EPhase::WalkIn ? FMath::Lerp(PathFrom, PathStop, Moment.Along)
		: (Moment.Phase == EPhase::WalkOff ? FMath::Lerp(PathStop, PathTo, Moment.Along) : PathStop);
	// Nose along the way it walks; the rat is modelled along its Y axis.
	const float FacingYaw = (PathTo - PathFrom).Rotation().Yaw;
	Mesh->SetRelativeLocationAndRotation(Where, FRotator(0.0f, FacingYaw - 90.0f, 0.0f));
	if (UCheeseStrikeMenuMouseAnimInstance* Animation = Cast<UCheeseStrikeMenuMouseAnimInstance>(Mesh->GetAnimInstance()))
	{
		const bool bWalking = Moment.Phase == EPhase::WalkIn || Moment.Phase == EPhase::WalkOff;
		UAnimSequence* Playing = bWalking ? WalkAnimation.Get() : (Moment.Phase == EPhase::Sniff ? SniffAnimation.Get() : StandAnimation.Get());
		Animation->SetSequence(Playing, bWalking ? WalkPlayRate : 1.0f);
		const FVector Head = Where + FRotator(0.0f, FacingYaw, 0.0f).Vector() * MenuMouseHeadAhead;
		Animation->SetHeadYaw(Moment.HeadShare * HeadTurnToward(FacingYaw, (CheeseAt - Head).Rotation().Yaw));
	}
}

namespace
{
	/** Brings the front end's mouse at once, for looking at it. */
	FAutoConsoleCommandWithWorldAndArgs MenuMouseCommand(TEXT("CheeseStrikeMenuMouse"), TEXT("CheeseStrikeMenuMouse now: the front end's mouse comes at once; with nothing, logs what it is doing."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (TActorIterator<ACheeseStrikeMenuMouse> It(World); It; ++It)
		{
			if (Args.Num() > 0 && Args[0].Equals(TEXT("now"), ESearchCase::IgnoreCase))
			{
				It->ComeNow();
			}
			UE_LOG(LogCheeseStrike, Display, TEXT("CheeseStrikeMenuMouse: phase %d, next in %.1f s"), static_cast<int32>(It->GetPhase()), It->GetNextIn());
		}
	}));
}
