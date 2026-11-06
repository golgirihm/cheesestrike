// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

/**
 * The game's spinner, as plain rules: a loop of pictures of a 3D cheese
 * wedge with rounded corners turning about its vertical axis, rendered in
 * Blender by tools/art/spinner_frames.py into Content/UI/Spinner as PNG
 * files (read as files, not assets, so that the loading screen can draw
 * them on its own thread before any asset is loaded), shown in turn from
 * the clock.
 */
namespace CheeseStrikeLoader
{
	/**
	 * How many frames the loop has, how many are shown a second (one turn in two seconds), and each frame's size in pixels
	 * (square): the small set's, for the menu's spinners, and the large set's (Content/UI/Spinner/Large), for the loading
	 * screen's big one, drawn at a fifth of the screen's height (LargeRadius) and sharp up to 1440p. Wave 17: 72 frames, 5
	 * degrees apart, each blended into the next by the clock (FrameBlendAt), where 48 at 24 a second stepped 7.5 degrees
	 * every second or third frame the loading screen drew (it draws about 58 a second), which read as choppy.
	 */
	constexpr int32 Frames = 72;
	constexpr int32 FramesPerSecond = 36;
	constexpr int32 FrameSize = 64;
	constexpr int32 LargeFrameSize = 320;
	/**
	 * The loading screen's spinner's half-size, in the cover's units (the screen 1080 of them high, by the DPI curve, at
	 * any resolution): the wedge, which is drawn a little bigger than the box (DrawnOver) and fills most of its frame
	 * across but less of it up and down, about a fifth of the screen's height.
	 */
	constexpr float LargeRadius = 130.0f;
	/** How much bigger than its box a frame is drawn: the wedge fills about 85% of its frame, so drawn so it is the box's size. */
	constexpr float DrawnOver = 1.18f;
	/** Which frame shows `Seconds` into the loop (any time: it goes round, and round again). */
	CHEESESTRIKESTARTUP_API int32 FrameAt(double Seconds);
	/** How far `Seconds` is from FrameAt's frame to the next, 0 to 1: the next frame is drawn over it at this opacity, so that the turn moves on every frame drawn. */
	CHEESESTRIKESTARTUP_API float FrameBlendAt(double Seconds);
	/** The turn, in degrees, the spinner shows at `Seconds` (with the blend), for measuring how far it moves between two frames drawn. */
	CHEESESTRIKESTARTUP_API double TurnAt(double Seconds);
	/** The file frame `Frame` is in (Content/UI/Spinner/spinner_NN.png; the large set's in Spinner/Large). */
	CHEESESTRIKESTARTUP_API FString FramePath(int32 Frame, bool bLarge = false);
}

/**
 * The game's spinner: the cheese wedge turning (CheeseStrikeLoader), drawn
 * from the clock, so that it turns anywhere Slate draws, the loading
 * screen's own thread included; its frames are loaded as files once, the
 * first time one is made (on the game's thread). `Radius` is half its size.
 * In the CheeseStrikeStartup module, which is loaded before the engine's
 * loading screen first plays, so that the startup cover can draw it.
 */
class CHEESESTRIKESTARTUP_API SCheeseStrikeLoader : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCheeseStrikeLoader) : _Radius(12.0f), _Large(false) {}
		SLATE_ARGUMENT(float, Radius)
		/** Drawn from the large frames (the loading screen's big spinner). */
		SLATE_ARGUMENT(bool, Large)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float Radius = 12.0f;
	bool bLarge = false;
};

/**
 * Draws its content at one fixed scale (Target: the window's scale times
 * the game's UI scale) however much is put on it by the widgets above it, so
 * that the loading cover is the same size on the engine's loading screen
 * (which has no UI scale) and on the game's viewport (which has one, and
 * whose changes as a level loads would show as the cover growing and
 * shrinking).
 */
class CHEESESTRIKESTARTUP_API SCheeseStrikeSteadyScale : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCheeseStrikeSteadyScale) : _Target(1.0f), _TargetFromSize(false) {}
		SLATE_ARGUMENT(float, Target)
		/**
		 * The target taken from the size it is given rather than Target: the game's UI scale at that size in pixels (the
		 * engine's DPI curve, the short side over 1080, as the game's viewport has it) times the window's own DPI scale. For
		 * the startup cover, made before there is a window or a viewport to measure (TargetForSize); it should then be given
		 * the whole window.
		 */
		SLATE_ARGUMENT(bool, TargetFromSize)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual float GetRelativeLayoutScale(const int32 ChildIndex, float LayoutScaleMultiplier) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	// The sizes, as plain rules (CheeseStrike.HUD.TheCoverFitsAtAnyResolution): `Accumulated` is the scale the widgets above give,
	// `Target` the scale wanted, `Content` the content's own size.
	/** The scale the content is laid out at relative to what is given: Target over Accumulated. */
	static float RelativeScale(float Target, float Accumulated);
	/** The size this widget asks for: the content's, grown by the relative scale, since it is drawn that much larger. */
	static FVector2D DesiredFor(const FVector2D& Content, float Target, float Accumulated);
	/** The size the content is given to lay out in, in its own units, out of the `Allotted` size: the allotted size over the relative scale. */
	static FVector2D ContentArea(const FVector2D& Allotted, float Target, float Accumulated);
	/** The target for a window `Pixels` big at `WindowDpi`: the engine's default DPI curve (the short side over 1080, as the game's UI scale is) times the window's DPI. */
	static float TargetForSize(const FVector2D& Pixels, float WindowDpi);

private:
	/** The target now, given `AllottedGeometry` (Target, or from its size). */
	float TargetFor(const FGeometry& AllottedGeometry) const;

	float Target = 1.0f;
	bool bTargetFromSize = false;
};
