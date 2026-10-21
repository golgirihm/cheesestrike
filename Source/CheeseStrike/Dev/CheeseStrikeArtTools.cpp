// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeArtTools.h"
#include "AnimationRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "MeshUtilitiesCommon.h"
#include "MeshUtilitiesEngine.h"
#include "PhysicsAssetUtils.h"
#include "UObject/Package.h"
#endif

namespace
{
	/** Where `Bone` is in the mesh's space as its reference pose stands, unscaled. */
	FTransform RefPose(const USkeletalMesh* Mesh, FName Bone)
	{
		const int32 Index = Mesh->GetRefSkeleton().FindBoneIndex(Bone);
		FTransform Pose = Index != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), Index) : FTransform::Identity;
		Pose.RemoveScaling();
		return Pose;
	}

	/** The middle of a body's shapes in the mesh's space, as the reference pose stands. */
	FVector BodyMiddle(const USkeletalMesh* Mesh, const USkeletalBodySetup* Body)
	{
		return Body->AggGeom.CalcAABB(RefPose(Mesh, Body->BoneName)).GetCenter();
	}

	/** A body's shapes as segments with a radius (a sphere is a segment of no length), in the mesh's space. */
	struct FRagdollPiece
	{
		FVector A;
		FVector B;
		float Radius;
	};

	TArray<FRagdollPiece> Pieces(const USkeletalMesh* Mesh, const USkeletalBodySetup* Body)
	{
		const FTransform Bone = RefPose(Mesh, Body->BoneName);
		TArray<FRagdollPiece> Out;
		for (const FKSphylElem& Capsule : Body->AggGeom.SphylElems)
		{
			const FTransform At = Capsule.GetTransform() * Bone;
			Out.Add({At.TransformPosition(FVector(0.0f, 0.0f, Capsule.Length / 2.0f)), At.TransformPosition(FVector(0.0f, 0.0f, -Capsule.Length / 2.0f)), Capsule.Radius});
		}
		for (const FKSphereElem& Sphere : Body->AggGeom.SphereElems)
		{
			const FVector At = Bone.TransformPosition(Sphere.Center);
			Out.Add({At, At, Sphere.Radius});
		}
		for (const FKBoxElem& Box : Body->AggGeom.BoxElems)
		{
			const FVector At = Bone.TransformPosition(Box.Center);
			Out.Add({At, At, 0.5f * static_cast<float>(FVector(Box.X, Box.Y, Box.Z).Size())});
		}
		return Out;
	}

	/** How far apart two bodies' shapes are (negative where they overlap). */
	float Gap(const TArray<FRagdollPiece>& First, const TArray<FRagdollPiece>& Second)
	{
		float Nearest = TNumericLimits<float>::Max();
		for (const FRagdollPiece& One : First)
		{
			for (const FRagdollPiece& Other : Second)
			{
				FVector P1;
				FVector P2;
				FMath::SegmentDistToSegmentSafe(One.A, One.B, Other.A, Other.B, P1, P2);
				Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist(P1, P2)) - One.Radius - Other.Radius);
			}
		}
		return Nearest;
	}

	/** The joint holding `Bone`'s body to the one above it, or null. */
	UPhysicsConstraintTemplate* JointOf(UPhysicsAsset* Ragdoll, FName Bone)
	{
		for (UPhysicsConstraintTemplate* Template : Ragdoll->ConstraintSetup)
		{
			if (Template != nullptr && Template->DefaultInstance.ConstraintBone1 == Bone)
			{
				return Template;
			}
		}
		return nullptr;
	}
}

UPhysicsAsset* UCheeseStrikeArtTools::MakeRagdoll(USkeletalMesh* Mesh, const FString& AssetPath, const TArray<FName>& Bones, const TArray<FName>& Leave)
{
#if WITH_EDITOR
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	TArray<int32> BodyBones;
	for (const FName Bone : Bones)
	{
		const int32 Index = Skeleton.FindBoneIndex(Bone);
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogTemp, Error, TEXT("MakeRagdoll: %s has no bone %s"), *Mesh->GetName(), *Bone.ToString());
			return nullptr;
		}
		BodyBones.AddUnique(Index);
	}
	// Parents before children, as the engine wants its bodies.
	BodyBones.Sort();
	if (BodyBones.IsEmpty())
	{
		return nullptr;
	}

	// The vertices each body is made to fit: those mostly on its bone, and on every bone under it that has no
	// body of its own (twist bones, fingers, the face, armour), except under the bones left out (Belica's own guns,
	// which are hidden). Bones above the first body give it theirs too.
	TArray<FBoneVertInfo> OnBone;
	FMeshUtilitiesEngine::CalcBoneVertInfos(Mesh, OnBone, true, 0);
	TArray<FTransform> Pose;
	for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
	{
		Pose.Add(FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Bone));
	}
	TMap<int32, FBoneVertInfo> Fitted;
	for (int32 Bone = 0; Bone < Skeleton.GetNum() && Bone < OnBone.Num(); ++Bone)
	{
		int32 Owner = Bone;
		bool bLeft = false;
		while (Owner != INDEX_NONE && !BodyBones.Contains(Owner))
		{
			bLeft |= Leave.Contains(Skeleton.GetBoneName(Owner));
			Owner = Skeleton.GetParentIndex(Owner);
		}
		if (Owner == INDEX_NONE)
		{
			Owner = BodyBones[0];
		}
		if (bLeft)
		{
			continue;
		}
		FBoneVertInfo& Into = Fitted.FindOrAdd(Owner);
		const FTransform ToOwner = Pose[Bone].GetRelativeTransform(Pose[Owner]);
		for (const FVector3f& Position : OnBone[Bone].Positions)
		{
			Into.Positions.Add(FVector3f(ToOwner.TransformPosition(FVector(Position))));
		}
		for (const FVector3f& Normal : OnBone[Bone].Normals)
		{
			Into.Normals.Add(FVector3f(ToOwner.TransformVectorNoScale(FVector(Normal))));
		}
	}

	UPackage* Package = CreatePackage(*AssetPath);
	UPhysicsAsset* Ragdoll = NewObject<UPhysicsAsset>(Package, FName(*FPaths::GetBaseFilename(AssetPath)), RF_Public | RF_Standalone | RF_Transactional);
	FPhysAssetCreateParams Params;
	// Fitted to the vertices given here, not to every vertex under the bone.
	Params.bAlwaysUseVertices = false;
	for (const int32 Bone : BodyBones)
	{
		const FName Name = Skeleton.GetBoneName(Bone);
		const int32 Body = FPhysicsAssetUtils::CreateNewBody(Ragdoll, Name, Params);
		FPhysicsAssetUtils::CreateCollisionFromBone(Ragdoll->SkeletalBodySetups[Body], Mesh, Bone, Params, Fitted.FindOrAdd(Bone), nullptr);
		// Joined to the nearest body above it, at its bone, which can't collide with it.
		int32 Above = Skeleton.GetParentIndex(Bone);
		while (Above != INDEX_NONE && !BodyBones.Contains(Above))
		{
			Above = Skeleton.GetParentIndex(Above);
		}
		if (Above != INDEX_NONE)
		{
			const int32 Joint = FPhysicsAssetUtils::CreateNewConstraint(Ragdoll, Name);
			FConstraintInstance& Instance = Ragdoll->ConstraintSetup[Joint]->DefaultInstance;
			Instance.SetAngularSwing1Motion(ACM_Limited);
			Instance.SetAngularSwing2Motion(ACM_Limited);
			Instance.SetAngularTwistMotion(ACM_Limited);
			Instance.ConstraintBone1 = Name;
			Instance.ConstraintBone2 = Skeleton.GetBoneName(Above);
			Instance.SnapTransformsToDefault(EConstraintTransformComponentFlags::All, Ragdoll);
			Ragdoll->ConstraintSetup[Joint]->SetDefaultProfile(Instance);
			Ragdoll->DisableCollision(Body, Ragdoll->FindBodyIndex(Instance.ConstraintBone2));
		}
	}
	Ragdoll->UpdateBodySetupIndexMap();
	Ragdoll->UpdateBoundsBodiesArray();
	Mesh->SetPhysicsAsset(Ragdoll);
	FAssetRegistryModule::AssetCreated(Ragdoll);
	Ragdoll->MarkPackageDirty();
	Mesh->MarkPackageDirty();
	return Ragdoll;
#else
	return nullptr;
#endif
}

TArray<FName> UCheeseStrikeArtTools::RagdollBodies(UPhysicsAsset* Ragdoll)
{
	TArray<FName> Bones;
	if (Ragdoll != nullptr)
	{
		for (const TObjectPtr<USkeletalBodySetup>& Body : Ragdoll->SkeletalBodySetups)
		{
			Bones.Add(Body->BoneName);
		}
	}
	return Bones;
}

bool UCheeseStrikeArtTools::LimitRagdollJoint(UPhysicsAsset* Ragdoll, USkeletalMesh* Mesh, FName Bone, FVector BendToward, float BendFrom, float BendTo, float Side, float Twist, bool bFromStraight)
{
#if WITH_EDITOR
	UPhysicsConstraintTemplate* Template = Ragdoll != nullptr && Mesh != nullptr ? JointOf(Ragdoll, Bone) : nullptr;
	const int32 BodyIndex = Ragdoll != nullptr ? Ragdoll->FindBodyIndex(Bone) : INDEX_NONE;
	if (Template == nullptr || BodyIndex == INDEX_NONE)
	{
		return false;
	}
	FConstraintInstance& Joint = Template->DefaultInstance;
	const FTransform Child = RefPose(Mesh, Joint.ConstraintBone1);
	const FTransform Parent = RefPose(Mesh, Joint.ConstraintBone2);
	const FVector At = Child.GetLocation();

	// The joint's axes, as the reference pose stands: X along the body, the twist (towards the joint of the body
	// below it, or of the one most in line where it has several, as a chest has a neck and two arms; towards its
	// middle where it has none); Y across it, about which it bends, turned so that bending forward moves it
	// towards BendToward; Z, about which it leans to the side. The engine twists about X, swings about Z by its
	// Swing1 limit and about Y by Swing2.
	FVector Along = (BodyMiddle(Mesh, Ragdoll->SkeletalBodySetups[BodyIndex]) - At).GetSafeNormal();
	const FVector ToMiddle = Along;
	float MostInLine = -2.0f;
	for (const UPhysicsConstraintTemplate* Below : Ragdoll->ConstraintSetup)
	{
		if (Below->DefaultInstance.ConstraintBone2 == Bone)
		{
			const FVector ToJoint = (RefPose(Mesh, Below->DefaultInstance.ConstraintBone1).GetLocation() - At).GetSafeNormal();
			if ((ToJoint | ToMiddle) > MostInLine)
			{
				MostInLine = ToJoint | ToMiddle;
				Along = ToJoint;
			}
		}
	}
	if (Along.IsNearlyZero())
	{
		Along = Child.GetUnitAxis(EAxis::X);
	}
	FVector Across = FVector::CrossProduct(Along, BendToward.GetSafeNormal()).GetSafeNormal();
	if (Across.IsNearlyZero())
	{
		UE_LOG(LogTemp, Error, TEXT("LimitRagdollJoint: %s bends towards where it points"), *Bone.ToString());
		return false;
	}
	if ((FQuat(Across, FMath::DegreesToRadians(5.0f)).RotateVector(Along) | BendToward) < (Along | BendToward))
	{
		Across = -Across;
	}
	const FVector Up = FVector::CrossProduct(Along, Across);
	const FTransform Frame(FMatrix(Along, Across, Up, At));

	// The bend the limits are centred on: the middle of the range, from the reference pose, which for a hinge
	// measured from straight is already bent by so much.
	float Bent = 0.0f;
	if (bFromStraight)
	{
		const FVector Above = (At - Parent.GetLocation()).GetSafeNormal();
		Bent = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Above, Along) | Across, Above | Along));
	}
	const float From = BendFrom - Bent;
	const float To = BendTo - Bent;
	const FQuat Centre(Across, FMath::DegreesToRadians((From + To) / 2.0f));
	const FTransform Centred(Centre * Frame.GetRotation(), At);
	Joint.SetRefFrame(EConstraintFrame::Frame1, Frame.GetRelativeTransform(Child));
	Joint.SetRefFrame(EConstraintFrame::Frame2, Centred.GetRelativeTransform(Parent));

	const auto Limit = [](float Degrees) { return Degrees < 0.5f ? ACM_Locked : ACM_Limited; };
	const float Bend = FMath::Max((To - From) / 2.0f, 0.5f);
	Joint.SetAngularSwing1Limit(Limit(Side), FMath::Max(Side, 0.5f));
	Joint.SetAngularSwing2Limit(Limit(Bend), Bend);
	Joint.SetAngularTwistLimit(Limit(Twist), FMath::Max(Twist, 0.5f));
	// Hard limits: the engine's are soft springs by default, which a falling body pushes far past.
	const FConstraintProfileProperties& Profile = Joint.ProfileInstance;
	Joint.SetSoftSwingLimitParams(false, Profile.ConeLimit.Stiffness, Profile.ConeLimit.Damping, 0.0f, Profile.ConeLimit.ContactDistance);
	Joint.SetSoftTwistLimitParams(false, Profile.TwistLimit.Stiffness, Profile.TwistLimit.Damping, 0.0f, Profile.TwistLimit.ContactDistance);
	Template->SetDefaultProfile(Joint);
	Ragdoll->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

void UCheeseStrikeArtTools::SettleRagdoll(UPhysicsAsset* Ragdoll, float LinearDamping, float AngularDamping, float JointDamping, int32 PositionIterations, int32 VelocityIterations)
{
#if WITH_EDITOR
	if (Ragdoll == nullptr)
	{
		return;
	}
	for (USkeletalBodySetup* Body : Ragdoll->SkeletalBodySetups)
	{
		FBodyInstance& Instance = Body->DefaultInstance;
		Instance.LinearDamping = LinearDamping;
		Instance.AngularDamping = AngularDamping;
		Instance.SetOverrideIterationCounts(true);
		Instance.SetPositionSolverIterationCount(static_cast<uint8>(FMath::Clamp(PositionIterations, 1, 255)));
		Instance.SetVelocitySolverIterationCount(static_cast<uint8>(FMath::Clamp(VelocityIterations, 1, 255)));
		Instance.SetProjectionSolverIterationCount(1);
	}
	for (UPhysicsConstraintTemplate* Template : Ragdoll->ConstraintSetup)
	{
		FConstraintInstance& Joint = Template->DefaultInstance;
		// A velocity drive towards no turning, with no spring: a joint that resists being turned fast, as a body's
		// muscles do, and comes to rest rather than swinging on.
		Joint.SetAngularDriveMode(EAngularDriveMode::SLERP);
		Joint.SetAngularVelocityTarget(FVector::ZeroVector);
		Joint.SetAngularVelocityDriveSLERP(JointDamping > 0.0f);
		Joint.SetAngularDriveParams(0.0f, JointDamping, 0.0f);
		// Pulled back inside its limits, part of the way each step, when the solver leaves it outside them.
		float LinearAlpha;
		float AngularAlpha;
		float LinearTolerance;
		float AngularTolerance;
		Joint.GetProjectionParams(LinearAlpha, AngularAlpha, LinearTolerance, AngularTolerance);
		Joint.SetProjectionParams(true, 1.0f, 0.5f, LinearTolerance, AngularTolerance);
		Template->SetDefaultProfile(Joint);
	}
	Ragdoll->MarkPackageDirty();
#endif
}

TArray<FString> UCheeseStrikeArtTools::LetRagdollBodiesCollide(UPhysicsAsset* Ragdoll, USkeletalMesh* Mesh, const TArray<FName>& Bones, const TArray<FName>& Others, float Margin)
{
	TArray<FString> Pairs;
#if WITH_EDITOR
	if (Ragdoll == nullptr || Mesh == nullptr)
	{
		return Pairs;
	}
	// Bodies joined to each other directly, or through one body between.
	const auto Neighbours = [Ragdoll](FName Bone)
	{
		TSet<FName> Near;
		for (const UPhysicsConstraintTemplate* Template : Ragdoll->ConstraintSetup)
		{
			const FConstraintInstance& Joint = Template->DefaultInstance;
			if (Joint.ConstraintBone1 == Bone)
			{
				Near.Add(Joint.ConstraintBone2);
			}
			else if (Joint.ConstraintBone2 == Bone)
			{
				Near.Add(Joint.ConstraintBone1);
			}
		}
		return Near;
	};
	for (const FName Bone : Bones)
	{
		const int32 First = Ragdoll->FindBodyIndex(Bone);
		if (First == INDEX_NONE)
		{
			continue;
		}
		const TSet<FName> Near = Neighbours(Bone);
		for (const FName Other : Others)
		{
			const int32 Second = Ragdoll->FindBodyIndex(Other);
			if (Second == INDEX_NONE || Second == First || Near.Contains(Other))
			{
				continue;
			}
			bool bThroughOne = false;
			for (const FName Between : Near)
			{
				bThroughOne |= Neighbours(Between).Contains(Other);
			}
			const float Apart = Gap(Pieces(Mesh, Ragdoll->SkeletalBodySetups[First]), Pieces(Mesh, Ragdoll->SkeletalBodySetups[Second]));
			if (!bThroughOne && Apart > Margin)
			{
				Ragdoll->EnableCollision(First, Second);
				Pairs.Add(FString::Printf(TEXT("%s-%s (%.0f cm apart)"), *Bone.ToString(), *Other.ToString(), Apart));
			}
		}
	}
	Ragdoll->MarkPackageDirty();
#endif
	return Pairs;
}

TArray<FString> UCheeseStrikeArtTools::DescribeRagdoll(UPhysicsAsset* Ragdoll, USkeletalMesh* Mesh)
{
	TArray<FString> Lines;
	if (Ragdoll == nullptr || Mesh == nullptr)
	{
		return Lines;
	}
	const auto Show = [](const FVector& V) { return FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), V.X, V.Y, V.Z); };
	const auto Turn = [](const FVector& V) { return FString::Printf(TEXT("(%.2f, %.2f, %.2f)"), V.X, V.Y, V.Z); };
	for (const USkeletalBodySetup* Body : Ragdoll->SkeletalBodySetups)
	{
		FString Shapes;
		for (const FKSphylElem& Capsule : Body->AggGeom.SphylElems)
		{
			Shapes += FString::Printf(TEXT(" capsule r %.1f long %.1f"), Capsule.Radius, Capsule.Length);
		}
		Shapes += FString::Printf(TEXT(" spheres %d boxes %d convex %d"), Body->AggGeom.SphereElems.Num(), Body->AggGeom.BoxElems.Num(), Body->AggGeom.ConvexElems.Num());
		const FBodyInstance& Instance = Body->DefaultInstance;
		Lines.Add(FString::Printf(TEXT("body %s at %s middle %s%s damping %.2f/%.2f iterations %d/%d"), *Body->BoneName.ToString(), *Show(RefPose(Mesh, Body->BoneName).GetLocation()), *Show(BodyMiddle(Mesh, Body)), *Shapes,
			Instance.LinearDamping, Instance.AngularDamping, Instance.GetPositionSolverIterationCount(), Instance.GetVelocitySolverIterationCount()));
	}
	for (const UPhysicsConstraintTemplate* Template : Ragdoll->ConstraintSetup)
	{
		const FConstraintInstance& Joint = Template->DefaultInstance;
		const FConstraintProfileProperties& Profile = Joint.ProfileInstance;
		const FTransform Frame = Joint.GetRefFrame(EConstraintFrame::Frame1) * RefPose(Mesh, Joint.ConstraintBone1);
		Lines.Add(FString::Printf(TEXT("joint %s on %s: swing1 %.0f%s swing2 %.0f%s twist %.0f%s, soft %d/%d, drive damping %.1f; twist axis %s swing2 axis %s"),
			*Joint.ConstraintBone1.ToString(), *Joint.ConstraintBone2.ToString(),
			Profile.ConeLimit.Swing1LimitDegrees, Profile.ConeLimit.Swing1Motion == ACM_Free ? TEXT(" free") : Profile.ConeLimit.Swing1Motion == ACM_Locked ? TEXT(" locked") : TEXT(""),
			Profile.ConeLimit.Swing2LimitDegrees, Profile.ConeLimit.Swing2Motion == ACM_Free ? TEXT(" free") : Profile.ConeLimit.Swing2Motion == ACM_Locked ? TEXT(" locked") : TEXT(""),
			Profile.TwistLimit.TwistLimitDegrees, Profile.TwistLimit.TwistMotion == ACM_Free ? TEXT(" free") : Profile.TwistLimit.TwistMotion == ACM_Locked ? TEXT(" locked") : TEXT(""),
			Profile.ConeLimit.bSoftConstraint ? 1 : 0, Profile.TwistLimit.bSoftConstraint ? 1 : 0, Profile.AngularDrive.SlerpDrive.Damping,
			*Turn(Frame.GetUnitAxis(EAxis::X)), *Turn(Frame.GetUnitAxis(EAxis::Y))));
	}
	for (int32 First = 0; First < Ragdoll->SkeletalBodySetups.Num(); ++First)
	{
		for (int32 Second = First + 1; Second < Ragdoll->SkeletalBodySetups.Num(); ++Second)
		{
			if (Ragdoll->IsCollisionEnabled(First, Second))
			{
				Lines.Add(FString::Printf(TEXT("collide %s-%s"), *Ragdoll->SkeletalBodySetups[First]->BoneName.ToString(), *Ragdoll->SkeletalBodySetups[Second]->BoneName.ToString()));
			}
		}
	}
	return Lines;
}
