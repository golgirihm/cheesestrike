// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMinimap.h"
#include "CheeseStrikeHUDDraw.h"
#include "CheeseStrike.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Map/CheeseStrikeCheese.h"
#include "Map/CheeseStrikeDoor.h"
#include "PhysicsEngine/BodySetup.h"
#include "Weapons/CheeseStrikeWeapon.h"

namespace
{
	using CheeseStrikeHUD::ELayer;

	/** Adds the piece whose corners (world) are `Corners`, if it has any area from above. */
	void AddMinimapPiece(const TArray<FVector>& Corners, bool bRamp, TArray<FCheeseStrikeMinimap::FPiece>& Out)
	{
		FCheeseStrikeMinimap::FPiece Piece;
		Piece.Bottom = TNumericLimits<float>::Max();
		Piece.Top = TNumericLimits<float>::Lowest();
		TArray<FVector2D> Flat;
		Flat.Reserve(Corners.Num());
		for (const FVector& Corner : Corners)
		{
			Piece.Bottom = FMath::Min(Piece.Bottom, static_cast<float>(Corner.Z));
			Piece.Top = FMath::Max(Piece.Top, static_cast<float>(Corner.Z));
			Flat.Add(FVector2D(Corner.X, Corner.Y));
		}
		Piece.Outline = CheeseStrikeHUD::ConvexHull(MoveTemp(Flat));
		if (Piece.Outline.Num() < 3)
		{
			return;
		}
		for (const FVector2D& Point : Piece.Outline)
		{
			Piece.Middle += Point / Piece.Outline.Num();
		}
		for (const FVector2D& Point : Piece.Outline)
		{
			Piece.Radius = FMath::Max(Piece.Radius, static_cast<float>(FVector2D::Distance(Point, Piece.Middle)));
		}
		Piece.bRamp = bRamp;
		Piece.bWide = CheeseStrikeHUD::NarrowestWidth(Piece.Outline) >= CheeseStrikeHUD::WideFrom;
		Out.Add(MoveTemp(Piece));
	}

	/** Whether a box `Size` big placed at `Box` is a ramp: a slab no thicker than a step, tilted between a little and steeply. */
	bool IsMinimapRamp(const FTransform& Box, const FVector& Size)
	{
		const FVector Axes[3] = {Box.TransformVector(FVector(Size.X, 0.0f, 0.0f)), Box.TransformVector(FVector(0.0f, Size.Y, 0.0f)), Box.TransformVector(FVector(0.0f, 0.0f, Size.Z))};
		int32 Thin = 0;
		for (int32 Axis = 1; Axis < 3; ++Axis)
		{
			if (Axes[Axis].SizeSquared() < Axes[Thin].SizeSquared())
			{
				Thin = Axis;
			}
		}
		const float Thickness = Axes[Thin].Size();
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (Axis != Thin && Axes[Axis].Size() < 3.0f * Thickness)
			{
				return false;
			}
		}
		const float Up = FMath::Abs(Axes[Thin].GetSafeNormal().Z);
		return Thickness <= 60.0f && Up > 0.5f && Up < 0.985f;
	}

	void AddMinimapBox(const FTransform& Box, const FVector& Size, TArray<FCheeseStrikeMinimap::FPiece>& Out)
	{
		TArray<FVector> Corners;
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Local((Corner & 1 ? 0.5f : -0.5f) * Size.X, (Corner & 2 ? 0.5f : -0.5f) * Size.Y, (Corner & 4 ? 0.5f : -0.5f) * Size.Z);
			Corners.Add(Box.TransformPosition(Local));
		}
		AddMinimapPiece(Corners, IsMinimapRamp(Box, Size), Out);
	}

	/** A round shape: rings of eight round its middle at the given heights (local Z), `Radius` wide. */
	void AddMinimapRound(const FTransform& Shape, float Radius, std::initializer_list<float> Heights, TArray<FCheeseStrikeMinimap::FPiece>& Out)
	{
		TArray<FVector> Corners;
		for (const float Height : Heights)
		{
			for (int32 Step = 0; Step < 8; ++Step)
			{
				const float Angle = UE_PI * Step / 4.0f;
				Corners.Add(Shape.TransformPosition(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Height)));
			}
		}
		AddMinimapPiece(Corners, false, Out);
	}

	FLinearColor MinimapColour(ELayer Layer)
	{
		switch (Layer)
		{
		case ELayer::Below: return FLinearColor(0.018f, 0.019f, 0.021f, 0.9f);
		case ELayer::Floor: return FLinearColor(0.05f, 0.052f, 0.056f, 0.92f);
		case ELayer::Raised: return FLinearColor(0.13f, 0.132f, 0.137f, 0.94f);
		case ELayer::Low: return FLinearColor(0.27f, 0.272f, 0.28f, 0.96f);
		default: return FLinearColor(0.6f, 0.6f, 0.61f, 0.98f);
		}
	}

	FLinearColor MinimapCheeseColour(ELayer Layer, int32 Stage)
	{
		const float Light = Layer == ELayer::Below ? 0.35f : Layer == ELayer::Floor ? 0.6f : Layer == ELayer::Low || Layer == ELayer::Raised ? 0.82f : 1.0f;
		return FLinearColor(0.98f * Light, 0.74f * Light, 0.2f * Light, 0.97f - 0.14f * Stage);
	}
}

void FCheeseStrikeMinimap::PiecesOf(const UStaticMesh* Mesh, const FTransform& ToWorld, TArray<FPiece>& Out)
{
	if (Mesh == nullptr)
	{
		return;
	}
	const int32 Before = Out.Num();
	if (const UBodySetup* Body = Mesh->GetBodySetup())
	{
		const FKAggregateGeom& Geometry = Body->AggGeom;
		for (const FKBoxElem& Box : Geometry.BoxElems)
		{
			AddMinimapBox(Box.GetTransform() * ToWorld, FVector(Box.X, Box.Y, Box.Z), Out);
		}
		for (const FKConvexElem& Convex : Geometry.ConvexElems)
		{
			const FTransform Shape = Convex.GetTransform() * ToWorld;
			TArray<FVector> Corners;
			Corners.Reserve(Convex.VertexData.Num());
			for (const FVector& Vertex : Convex.VertexData)
			{
				Corners.Add(Shape.TransformPosition(Vertex));
			}
			AddMinimapPiece(Corners, false, Out);
		}
		for (const FKSphereElem& Sphere : Geometry.SphereElems)
		{
			AddMinimapRound(FTransform(Sphere.Center) * ToWorld, Sphere.Radius, {-Sphere.Radius, 0.0f, Sphere.Radius}, Out);
		}
		for (const FKSphylElem& Capsule : Geometry.SphylElems)
		{
			const float Half = Capsule.Length / 2.0f;
			AddMinimapRound(Capsule.GetTransform() * ToWorld, Capsule.Radius, {-Half - Capsule.Radius, -Half, Half, Half + Capsule.Radius}, Out);
		}
	}
	// No simple collision (it collides by its triangles): its bounds.
	if (Out.Num() == Before)
	{
		const FBox Bounds = Mesh->GetBoundingBox();
		AddMinimapBox(FTransform(Bounds.GetCenter()) * ToWorld, Bounds.GetSize(), Out);
	}
}

void FCheeseStrikeMinimap::Gather(UWorld* World)
{
	if (World == nullptr || IsGatheredFor(World))
	{
		return;
	}
	GatheredFor = World;
	Pieces.Reset();
	Cheese.Reset();
	Doors.Reset();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->IsHidden() || Actor->IsA<APawn>() || Actor->IsA<ACheeseStrikeWeapon>() || Cast<APawn>(Actor->GetAttachParentActor()) != nullptr)
		{
			continue;
		}
		if (ACheeseStrikeCheese* Block = Cast<ACheeseStrikeCheese>(Actor))
		{
			Cheese.Add({Block, {}, FTransform(FVector(1e9f))});
			continue;
		}
		if (Actor->IsA<ACheeseStrikeDoor>())
		{
			Doors.Add(Actor);
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
		for (const UStaticMeshComponent* Component : Components)
		{
			if (Component->GetStaticMesh() == nullptr || !Component->IsVisible() || Component->bHiddenInGame || Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision
				|| Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
			{
				continue;
			}
			if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Component))
			{
				for (int32 Instance = 0; Instance < Instanced->GetInstanceCount(); ++Instance)
				{
					FTransform Placed;
					if (Instanced->GetInstanceTransform(Instance, Placed, true))
					{
						PiecesOf(Component->GetStaticMesh(), Placed, Pieces);
					}
				}
			}
			else
			{
				PiecesOf(Component->GetStaticMesh(), Component->GetComponentTransform(), Pieces);
			}
		}
	}
	Pieces.Sort([](const FPiece& A, const FPiece& B) { return A.Top < B.Top; });
	Bounds = FBox2D(ForceInit);
	// By the pieces' own outlines, not the circles round them, which reach far past a long wall's ends and sides and left the
	// strike map's level small in its square.
	for (const FPiece& Piece : Pieces)
	{
		for (const FVector2D& Point : Piece.Outline)
		{
			Bounds += Point;
		}
		if (Piece.Outline.Num() == 0)
		{
			Bounds += FBox2D(Piece.Middle - FVector2D(Piece.Radius, Piece.Radius), Piece.Middle + FVector2D(Piece.Radius, Piece.Radius));
		}
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Minimap: %d pieces and %d blocks of cheese"), Pieces.Num(), Cheese.Num());
}

int32 FCheeseStrikeMinimap::NumCheese() const
{
	int32 Count = 0;
	for (const FCheese& Block : Cheese)
	{
		Count += Block.Block.IsValid() && !Block.Block->IsActorBeingDestroyed() ? 1 : 0;
	}
	return Count;
}

void FCheeseStrikeMinimap::DrawPiece(CheeseStrikeHUDDraw::FShapes& Shapes, const FPiece& Piece, const FLinearColor& Colour, const FVector2D& Min, const FVector2D& Max, const FVector2D& Middle, float ViewYaw, float Scale) const
{
	const FVector2D Centre = (Min + Max) / 2.0f;
	TArray<FVector2D, TInlineAllocator<32>> Corners;
	for (const FVector2D& Point : Piece.Outline)
	{
		Corners.Add(Centre + CheeseStrikeHUD::MapPoint(Point - Middle, ViewYaw, Scale));
	}
	// Wholly inside the circle the square holds: nothing to cut.
	const float Out = (FVector2D::Distance(Piece.Middle, Middle) + Piece.Radius) * Scale;
	if (Out <= FMath::Min(Max.X - Min.X, Max.Y - Min.Y) / 2.0f)
	{
		Shapes.Polygon(Corners, Colour);
		return;
	}
	// Cut to the square, and then, only if some of it is left in a corner beyond the rounding, to the rounded edge.
	TArray<FVector2D> Cut = CheeseStrikeHUD::ClipToRect(TArray<FVector2D>(Corners), Min, Max);
	if (Edge.Num() > 4 && Cut.ContainsByPredicate([&](const FVector2D& Point) { return CheeseStrikeHUD::InsideRoundedRect(Point, Min, Max, Corner) < 0.0f; }))
	{
		Cut = CheeseStrikeHUD::ClipToConvex(Cut, Edge);
	}
	Shapes.Polygon(Cut, Colour);
	// Where it was cut to the square's edge, a soft edge outward, so that the rounded corners are smooth (the pieces' own
	// edges stay hard, where they meet each other).
	float Area = 0.0f;
	for (int32 Index = 0; Index < Cut.Num(); ++Index)
	{
		Area += Cut[Index].X * Cut[(Index + 1) % Cut.Num()].Y - Cut[(Index + 1) % Cut.Num()].X * Cut[Index].Y;
	}
	const auto OnEdge = [&](const FVector2D& Point) { return FMath::Abs(CheeseStrikeHUD::InsideRoundedRect(Point, Min, Max, Corner)) < 0.15f; };
	for (int32 Index = 0; Index < Cut.Num(); ++Index)
	{
		const FVector2D& A = Cut[Index];
		const FVector2D& B = Cut[(Index + 1) % Cut.Num()];
		if (OnEdge(A) && OnEdge(B) && OnEdge((A + B) / 2.0f) && !A.Equals(B, 0.01f))
		{
			const FVector2D Along = (B - A).GetSafeNormal();
			Shapes.Fringe(A, B, FVector2D(Along.Y, -Along.X) * (Area >= 0.0f ? 1.0f : -1.0f) * CheeseStrikeHUD::SoftEdge, Colour);
		}
	}
}

void FCheeseStrikeMinimap::Draw(CheeseStrikeHUDDraw::FShapes& Shapes, const FVector2D& Min, const FVector2D& Max, const FVector2D& Middle, float ViewYaw, float Feet, float Scale, float InCorner)
{
	if (Scale <= 0.0f)
	{
		return;
	}
	// Pieces wholly inside the circle the square holds need no cutting; the rest are cut to its rounded edge.
	Corner = InCorner;
	Edge = CheeseStrikeHUD::RoundedRect(Min, Max, Corner);
	// As far as the square's corners reach.
	const float Reach = FVector2D::Distance(Min, Max) / 2.0f / Scale;
	for (const FPiece& Piece : Pieces)
	{
		if (FVector2D::Distance(Piece.Middle, Middle) - Piece.Radius > Reach)
		{
			continue;
		}
		const ELayer Layer = CheeseStrikeHUD::LayerFor(Piece.Bottom, Piece.Top, Feet, Piece.bRamp, Piece.bWide);
		if (Layer != ELayer::Hidden)
		{
			DrawPiece(Shapes, Piece, MinimapColour(Layer), Min, Max, Middle, ViewYaw, Scale);
		}
	}
	// The doors' leaves where they are now.
	for (const TWeakObjectPtr<AActor>& Door : Doors)
	{
		const AActor* Live = Door.Get();
		if (Live == nullptr || FVector2D::Distance(FVector2D(Live->GetActorLocation()), Middle) > Reach + 1000.0f)
		{
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Leaves(Live);
		for (const UStaticMeshComponent* Leaf : Leaves)
		{
			if (Leaf->GetStaticMesh() == nullptr || !Leaf->IsVisible() || Leaf->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}
			TArray<FPiece> Found;
			PiecesOf(Leaf->GetStaticMesh(), Leaf->GetComponentTransform(), Found);
			for (const FPiece& Piece : Found)
			{
				const ELayer Layer = CheeseStrikeHUD::LayerFor(Piece.Bottom, Piece.Top, Feet, false);
				if (Layer != ELayer::Hidden)
				{
					DrawPiece(Shapes, Piece, MinimapColour(Layer), Min, Max, Middle, ViewYaw, Scale);
				}
			}
		}
	}
	for (FCheese& Block : Cheese)
	{
		const ACheeseStrikeCheese* Live = Block.Block.Get();
		if (Live == nullptr || Live->IsActorBeingDestroyed() || Live->GetMesh() == nullptr || !Live->GetMesh()->IsVisible())
		{
			continue;
		}
		// Read again only when it has moved (a block falling when what held it is eaten).
		const FTransform Now = Live->GetMesh()->GetComponentTransform();
		if (!Now.Equals(Block.At, 0.5f))
		{
			Block.At = Now;
			Block.Pieces.Reset();
			PiecesOf(Live->GetMesh()->GetStaticMesh(), Now, Block.Pieces);
		}
		for (const FPiece& Piece : Block.Pieces)
		{
			if (FVector2D::Distance(Piece.Middle, Middle) - Piece.Radius > Reach)
			{
				continue;
			}
			const ELayer Layer = CheeseStrikeHUD::LayerFor(Piece.Bottom, Piece.Top, Feet, false);
			if (Layer != ELayer::Hidden)
			{
				DrawPiece(Shapes, Piece, MinimapCheeseColour(Layer, Live->GetStage()), Min, Max, Middle, ViewYaw, Scale);
			}
		}
	}
}
