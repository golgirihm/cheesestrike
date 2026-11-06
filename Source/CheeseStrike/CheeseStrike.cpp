// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "CheeseStrike.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "GenericPlatform/GenericWindow.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Widgets/SWindow.h"
#include "Settings/CheeseStrikeSettings.h"
#include "Rules/CheeseStrikeShadows.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

/**
 * The game's module. A packaged game has the cheese icon built into its
 * program (Build/Windows/Application.ico); a game run through the editor's
 * program with -game would show Unreal's, so the game window is given the
 * cheese one from that file once the engine is up. It also holds virtual
 * shadow maps off for a run's first moments (Rules/CheeseStrikeShadows.h).
 */
class FCheeseStrikeModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddStatic(&FCheeseStrikeModule::UseOurIcon);
		HoldVirtualShadowsOff();
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		if (ShadowTicker.IsValid())
		{
			FTSTicker::RemoveTicker(ShadowTicker);
		}
	}

	virtual bool IsGameModule() const override { return true; }

private:
	/** Virtual shadow maps off until the run has drawn its first frames (CheeseStrikeShadows), in the game only. */
	void HoldVirtualShadowsOff()
	{
		IConsoleVariable* Virtual = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable"));
		if (GIsEditor || Virtual == nullptr || !CheeseStrikeShadows::WantedFromConfig())
		{
			return;
		}
		Virtual->Set(0, ECVF_SetByCode);
		UE_LOG(LogCheeseStrike, Display, TEXT("Shadows: virtual shadow maps off for the first %d frames and %.0f s"), CheeseStrikeShadows::Frames, CheeseStrikeShadows::Seconds);
		ShadowTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this, Virtual](float)
		{
			if (FirstFrame == 0)
			{
				FirstFrame = GFrameCounter;
				FirstSeconds = FPlatformTime::Seconds();
			}
			const int32 Frames = static_cast<int32>(GFrameCounter - FirstFrame);
			const float Since = static_cast<float>(FPlatformTime::Seconds() - FirstSeconds);
			if (!CheeseStrikeShadows::ShouldTurnOn(Frames, Since))
			{
				return true;
			}
			Virtual->Set(1, ECVF_SetByCode);
			UE_LOG(LogCheeseStrike, Display, TEXT("Shadows: virtual shadow maps on after %d frames, %.1f s"), Frames, Since);
			ShadowTicker.Reset();
			return false;
		}));
	}

	static void UseOurIcon()
	{
#if PLATFORM_WINDOWS
		if (GIsEditor || GEngine == nullptr)
		{
			return;
		}
		const UGameEngine* Game = Cast<UGameEngine>(GEngine);
		const TSharedPtr<SWindow> Window = Game != nullptr ? Game->GameViewportWindow.Pin() : nullptr;
		if (Window.IsValid())
		{
			// What the window came up as, for checking a fresh install opens borderless (docs/manual-tests.md).
			const FVector2D Size = Window->GetSizeInScreen();
			UE_LOG(LogCheeseStrike, Display, TEXT("Window: %s, %.0f x %.0f"), UCheeseStrikeSettings::WindowModeWord(Window->GetWindowMode()), Size.X, Size.Y);
		}
		const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
		const FString IconPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Build/Windows/Application.ico"));
		if (!Native.IsValid() || !FPaths::FileExists(IconPath))
		{
			UE_LOG(LogCheeseStrike, Display, TEXT("Window icon: not set (%s)"), !Native.IsValid() ? TEXT("no game window") : TEXT("no icon file"));
			return;
		}
		HWND Handle = static_cast<HWND>(Native->GetOSWindowHandle());
		HANDLE Big = ::LoadImageW(nullptr, *IconPath, IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE);
		HANDLE Small = ::LoadImageW(nullptr, *IconPath, IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
		if (Big != nullptr)
		{
			::SendMessageW(Handle, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(Big));
		}
		if (Small != nullptr)
		{
			::SendMessageW(Handle, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(Small));
		}
		UE_LOG(LogCheeseStrike, Display, TEXT("Window icon: %s"), Big != nullptr ? *IconPath : TEXT("couldn't load it"));
#endif
	}

	FDelegateHandle PostEngineInitHandle;
	FTSTicker::FDelegateHandle ShadowTicker;
	uint64 FirstFrame = 0;
	double FirstSeconds = 0.0;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FCheeseStrikeModule, CheeseStrike, "CheeseStrike");

DEFINE_LOG_CATEGORY(LogCheeseStrike)
