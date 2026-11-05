// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// TEMPORARY: the UI theme sampler's tests (UI/CheeseStrikeThemes.h), to go with it.

#include "Misc/AutomationTest.h"
#include "Streaks/CheeseStrikeSynth.h"
#include "UI/CheeseStrikeMenu.h"
#include "UI/CheeseStrikeThemes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheThemeSamplerHasFiveThemesWithTheirOwnSounds, "CheeseStrike.Rules.TheThemeSamplerHasFiveThemesWithTheirOwnSounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTheThemeSamplerHasFiveThemesWithTheirOwnSounds::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeThemes;
	TestEqual(TEXT("five themes"), NumThemes, 5);
	TSet<FString> Names;
	TArray<TArray<int16>> Hovers;
	TArray<TArray<int16>> Clicks;
	for (int32 Index = 0; Index < NumThemes; ++Index)
	{
		const FTheme& T = Theme(Index);
		Names.Add(T.Name);
		// Each a shape, a type and a motion of its own, not only colours.
		for (int32 Other = 0; Other < Index; ++Other)
		{
			const FTheme& O = Theme(Other);
			TestTrue(FString::Printf(TEXT("%s and %s differ in more than colour"), T.Name, O.Name), T.Motion != O.Motion && (T.Shape != O.Shape || T.Rounding != O.Rounding) && (T.ButtonFace != O.ButtonFace || T.ButtonTracking != O.ButtonTracking || T.bUpper != O.bUpper || T.TextOutline != O.TextOutline));
		}
		// Its sounds: short, under full scale, and not the same as any other theme's.
		const TArray<int16> Hover = CheeseStrikeSynth::ThemeHover(Index);
		const TArray<int16> Click = CheeseStrikeSynth::ThemeClick(Index);
		TestTrue(FString::Printf(TEXT("%s's hover is a short sound (%d samples)"), T.Name, Hover.Num()), Hover.Num() > CheeseStrikeSynth::SampleRate / 100 && Hover.Num() <= CheeseStrikeSynth::SampleRate * 4 / 10);
		TestTrue(FString::Printf(TEXT("%s's click is a short sound (%d samples)"), T.Name, Click.Num()), Click.Num() > CheeseStrikeSynth::SampleRate / 100 && Click.Num() <= CheeseStrikeSynth::SampleRate * 4 / 10);
		int32 Loudest = 0;
		for (const int16 Value : Hover)
		{
			Loudest = FMath::Max(Loudest, FMath::Abs(static_cast<int32>(Value)));
		}
		TestTrue(FString::Printf(TEXT("%s's hover is softer than its click (%d)"), T.Name, Loudest), Loudest > 3000 && Loudest <= 32767 * 0.5f);
		Loudest = 0;
		for (const int16 Value : Click)
		{
			Loudest = FMath::Max(Loudest, FMath::Abs(static_cast<int32>(Value)));
		}
		TestTrue(FString::Printf(TEXT("%s's click peaks under full scale (%d)"), T.Name, Loudest), Loudest > 10000 && Loudest <= 32767 * 0.7f);
		TestFalse(FString::Printf(TEXT("%s's hover is not its click"), T.Name), Hover == Click);
		for (int32 Other = 0; Other < Hovers.Num(); ++Other)
		{
			TestFalse(FString::Printf(TEXT("%s's hover is its own"), T.Name), Hover == Hovers[Other]);
			TestFalse(FString::Printf(TEXT("%s's click is its own"), T.Name), Click == Clicks[Other]);
		}
		Hovers.Add(Hover);
		Clicks.Add(Click);
		TestEqual(FString::Printf(TEXT("%s sets its labels as it says"), T.Name), Label(Index, TEXT("Start the match")), T.bUpper ? FString(TEXT("START THE MATCH")) : FString(TEXT("Start the match")));
	}
	TestEqual(TEXT("every theme has a name of its own"), Names.Num(), NumThemes);
	TestTrue(TEXT("out of range, the first"), &Theme(-1) == &Theme(0) && &Theme(NumThemes) == &Theme(0));
	// The page is the front end's: shown when chosen there, never in a match or the lobby.
	using CheeseStrikeMenu::PageFor;
	TestTrue(TEXT("the themes page is a page of the front end's"), PageFor(false, false, false, false, EMenuPage::Themes) == EMenuPage::Themes);
	TestTrue(TEXT("but the lobby's where there is a lobby"), PageFor(false, false, true, false, EMenuPage::Themes) == EMenuPage::Lobby);
	TestTrue(TEXT("and nothing once a match is on"), PageFor(true, false, true, false, EMenuPage::Themes) == EMenuPage::None);
	return true;
}
