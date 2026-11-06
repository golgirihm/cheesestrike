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
#include "Features/IModularFeatures.h"
#include "Performance/LatencyMarkerModule.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

/**
 * The game's module. A packaged game has the cheese icon built into its
 * program (Build/Windows/Application.ico); a game run through the editor's
 * program with -game would show Unreal's, so the game window is given the
 * cheese one from that file once the engine is up.
 */
class FCheeseStrikeModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddStatic(&FCheeseStrikeModule::UseOurIcon);
		LatencyHandle = FCoreDelegates::GetOnPostEngineInit().AddStatic(&FCheeseStrikeModule::StartLatencyMarkers);
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		FCoreDelegates::GetOnPostEngineInit().Remove(LatencyHandle);
	}

	virtual bool IsGameModule() const override { return true; }

private:
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

	/**
	 * Turns on the latency markers of the engine's Reflex plugin (enabled in the .uproject): NVIDIA's own (NvAPI) and its PC
	 * Latency stats (PCLStats), which the NVIDIA app's statistics overlay reads its frame rate and latencies from; without
	 * them it showed FPS, PCL and RLAT as N/A (wave 17). Only the markers: Reflex's low-latency mode, which changes how frames
	 * are paced, stays off. The plugin keeps them to an NVIDIA GPU under DirectX (its own GetAvailable), and the stats to a
	 * window drawn on screen. Not in the editor.
	 */
	static void StartLatencyMarkers()
	{
		if (GIsEditor || IsRunningCommandlet())
		{
			return;
		}
		int32 Started = 0;
		const TArray<ILatencyMarkerModule*> Markers = IModularFeatures::Get().GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());
		for (ILatencyMarkerModule* Marker : Markers)
		{
			if (Marker != nullptr && Marker->GetAvailable())
			{
				Marker->SetEnabled(true);
				Started += Marker->GetEnabled() ? 1 : 0;
			}
		}
		UE_LOG(LogCheeseStrike, Display, TEXT("Latency markers: %d of %d on (Reflex's and PCLStats', for NVIDIA's overlay)"), Started, Markers.Num());
	}

	FDelegateHandle PostEngineInitHandle;
	FDelegateHandle LatencyHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FCheeseStrikeModule, CheeseStrike, "CheeseStrike");

DEFINE_LOG_CATEGORY(LogCheeseStrike)
