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
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
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
};

IMPLEMENT_PRIMARY_GAME_MODULE(FCheeseStrikeModule, CheeseStrike, "CheeseStrike");

DEFINE_LOG_CATEGORY(LogCheeseStrike)
