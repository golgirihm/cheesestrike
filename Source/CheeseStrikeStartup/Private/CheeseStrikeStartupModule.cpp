// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCover.h"
#include "Modules/ModuleManager.h"
#include "MoviePlayer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogCheeseStrikeStartup, Log, All);

/**
 * Puts the loading cover up as the game starts. The engine makes the game's
 * window as its movie player starts, seconds before the game's module and
 * its assets are loaded and then the first level, and with nothing to play
 * it left the window black all that while (about 6 s in the editor's build,
 * up to 18 s just after a build; under a second packaged). This module is
 * loaded just before the engine plays its startup loading screen
 * (LoadingPhase PreLoadingScreen in the .uproject) and gives it the cover,
 * which the loading screen's own thread draws, the wedge turning, from the
 * window's first frame until the engine's start is done and the front end's
 * level has loaded; the cover in the view takes over from there
 * (UCheeseStrikeCurtainSubsystem). -NoStartupCover leaves it out.
 */
class FCheeseStrikeStartupModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (GIsEditor || IsRunningCommandlet() || IsRunningDedicatedServer() || !IsMoviePlayerEnabled() || GetMoviePlayer() == nullptr || FParse::Param(FCommandLine::Get(), TEXT("NoStartupCover")))
		{
			return;
		}
		FLoadingScreenAttributes Screen;
		Screen.bAutoCompleteWhenLoadingCompletes = true;
		Screen.bMoviesAreSkippable = false;
		Screen.bWaitForManualStop = false;
		Screen.MinimumLoadingScreenDisplayTime = -1.0f;
		Screen.WidgetLoadingScreen = CheeseStrikeCover::Make(FText::FromString(TEXT("Loading")), 1.0f, true, true);
		CheeseStrikeCover::ResetFramesDrawn();
		GetMoviePlayer()->SetupLoadingScreen(Screen);
		CheeseStrikeCover::NoteStartupCover();
		UE_LOG(LogCheeseStrikeStartup, Display, TEXT("The startup cover is given to the engine's loading screen"));
	}
};

IMPLEMENT_MODULE(FCheeseStrikeStartupModule, CheeseStrikeStartup)
