// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CheeseStrikeArtTools.generated.h"

class UPhysicsAsset;
class USkeletalMesh;

/**
 * Editor work the art scripts in tools/art/ ask for that the editor's Python
 * can't do itself (unreal.CheeseStrikeArtTools). Editor only: in a game they
 * do nothing.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeArtTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Makes a ragdoll for `Mesh`, saved as the asset `AssetPath`, and makes
	 * it the mesh's physics asset: a body for each of `Bones`, a capsule
	 * fitted to the vertices on that bone and on the bones under it that
	 * have no body (twist bones, fingers, the face, armour), except those
	 * under the bones in `Leave` (a character's own weapons, hidden in the
	 * game); and a joint from each body to the nearest one above it, free
	 * to turn 45 degrees every way until LimitRagdollJoint says how a
	 * person's turns. Bones under a body that are not bodies follow it as
	 * posed: with the head the last body, the face and jaw stay on it.
	 */
	UFUNCTION(BlueprintCallable, Category = "CheeseStrike|Art")
	static UPhysicsAsset* MakeRagdoll(USkeletalMesh* Mesh, const FString& AssetPath, const TArray<FName>& Bones, const TArray<FName>& Leave);

	/** The bones of a ragdoll's bodies. */
	UFUNCTION(BlueprintCallable, Category = "CheeseStrike|Art")
	static TArray<FName> RagdollBodies(UPhysicsAsset* Ragdoll);

	/**
	 * Limits the joint that holds `Bone`'s body to the body above it as a
	 * person's joint is limited, in degrees: it bends towards `BendToward`
	 * (a direction in the mesh's space as its reference pose stands, such
	 * as forward for a hip, backward for a knee) from `BendFrom` to
	 * `BendTo` (negative is the other way), measured from the reference
	 * pose or, with `bFromStraight`, from in line with the body above
	 * (for an elbow or a knee); it leans to either side of that by `Side`,
	 * and twists about the bone by `Twist` either way. A hinge is a joint
	 * with little side or twist. False if no joint holds that body.
	 */
	UFUNCTION(BlueprintCallable, Category = "CheeseStrike|Art")
	static bool LimitRagdollJoint(UPhysicsAsset* Ragdoll, USkeletalMesh* Mesh, FName Bone, FVector BendToward, float BendFrom, float BendTo, float Side, float Twist, bool bFromStraight);

	/**
	 * Makes a ragdoll settle as a body does rather than wobble: every body
	 * damped (`LinearDamping`, `AngularDamping`) and solved with so many
	 * iterations, and every joint resisting being turned quickly
	 * (`JointDamping`, a velocity drive towards no turning) and pulled
	 * back inside its limits when the solver leaves it outside them.
	 */
	UFUNCTION(BlueprintCallable, Category = "CheeseStrike|Art")
	static void SettleRagdoll(UPhysicsAsset* Ragdoll, float LinearDamping, float AngularDamping, float JointDamping, int32 PositionIterations, int32 VelocityIterations);

	/**
	 * Lets each body of `Bones` collide with each of `Others` (the two legs,
	 * say, so that they can't pass through each other), unless they are
	 * joined, directly or through one body, or are within `Margin` cm of
	 * each other as the reference pose has them. The pairs it let collide.
	 */
	UFUNCTION(BlueprintCallable, Category = "CheeseStrike|Art")
	static TArray<FString> LetRagdollBodiesCollide(UPhysicsAsset* Ragdoll, USkeletalMesh* Mesh, const TArray<FName>& Bones, const TArray<FName>& Others, float Margin);

	/** Each body (its bone, shape and place) and joint (its bones, limits and axes) of a ragdoll, a line each, for a script's log. */
	UFUNCTION(BlueprintCallable, Category = "CheeseStrike|Art")
	static TArray<FString> DescribeRagdoll(UPhysicsAsset* Ragdoll, USkeletalMesh* Mesh);
};
