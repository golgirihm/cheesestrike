// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeHeroTurn.h"
#include "UI/CheeseStrikeHeroView.h"
#include "UI/CheeseStrikeMenu.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLoadoutHeroTurnsByHandAndThenByItself, "CheeseStrike.Rules.TheLoadoutHeroTurnsByHandAndThenByItself", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTheLoadoutHeroTurnsByHandAndThenByItself::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHeroTurn;
	// Never turned by hand, it turns by itself at its pace.
	TestEqual(TEXT("never turned by hand: its own pace"), AutoShare(-1.0), 1.0f);
	// Turned by hand: still until left alone for three seconds, then easing back up to its pace from where it was left.
	TestEqual(TEXT("just turned: still"), AutoShare(0.0), 0.0f);
	TestEqual(TEXT("2.9 s after: still"), AutoShare(2.9), 0.0f);
	TestTrue(TEXT("about 3 s idle"), IdleSeconds >= 2.5f && IdleSeconds <= 3.5f);
	const float Starting = AutoShare(IdleSeconds + ResumeSeconds * 0.5);
	TestTrue(FString::Printf(TEXT("then easing back up (%.2f halfway)"), Starting), Starting > 0.2f && Starting < 0.8f);
	TestEqual(TEXT("and at its pace once eased in"), AutoShare(IdleSeconds + ResumeSeconds + 0.01), 1.0f);
	float Last = 0.0f;
	bool bRising = true;
	for (double Since = 0.0; Since < IdleSeconds + ResumeSeconds + 1.0; Since += 0.05)
	{
		bRising &= AutoShare(Since) >= Last - 1e-6f;
		Last = AutoShare(Since);
	}
	TestTrue(TEXT("never slowing again as it eases in"), bRising);
	// A drag: across the whole picture about half a turn, each way.
	const float AcrossPicture = CheeseStrikeMenu::HeroViewWidth * DragDegreesPerUnit;
	TestTrue(FString::Printf(TEXT("a drag across the picture turns it about half a turn (%.0f degrees)"), AcrossPicture), AcrossPicture >= 120.0f && AcrossPicture <= 240.0f);
	// The right stick: nothing in its dead zone, its full rate at the edge, either way.
	TestEqual(TEXT("the stick at rest turns nothing"), StickTurn(0.1f, 1.0f), 0.0f);
	TestTrue(TEXT("held right, it turns right at the full rate"), FMath::IsNearlyEqual(StickTurn(1.0f, 0.5f), StickDegreesPerSecond * 0.5f, 0.01f));
	TestTrue(TEXT("held left, left"), FMath::IsNearlyEqual(StickTurn(-1.0f, 0.5f), -StickDegreesPerSecond * 0.5f, 0.01f));
	TestTrue(TEXT("half over, slower"), StickTurn(0.6f, 1.0f) > 0.0f && StickTurn(0.6f, 1.0f) < StickDegreesPerSecond);
	// Zoomed out so that a gun held out never leaves the picture: the hero takes less of its height than it did (0.8).
	TestTrue(TEXT("the hero takes about three fifths of the picture's height, room either side for a gun held across it"), CheeseStrikeHeroView::HeroShare <= 0.65f && CheeseStrikeHeroView::HeroShare >= 0.55f);
	return true;
}
