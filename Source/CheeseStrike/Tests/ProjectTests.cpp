// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// That the project is put together as it should be: it opens on the shooter
// level with the game's own rules, and the template's Blueprints, saved against
// the template's own C++ classes, load against ours.

#include "Misc/AutomationTest.h"
#include "GameMapsSettings.h"
#include "Misc/PackageName.h"
#include "GameFramework/GameModeBase.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/CoreRedirects.h"
#include "CheeseStrikeCharacter.h"
#include "ShooterCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "UI/CheeseStrikeCredits.h"
#include "CheeseStrikeLoader.h"
#include "Settings/CheeseStrikeSettings.h"
#include "Sound/SoundClass.h"
#include "Rules/CheeseStrikeRules.h"
#include "Dev/CheeseStrikeLoadWatch.h"
#include "UObject/Linker.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectResource.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCoverWaitsForTheRendererToWarmUp, "CheeseStrike.Project.TheCoverWaitsForTheRendererToWarmUp", ProjectTests::Flags)

bool FTheCoverWaitsForTheRendererToWarmUp::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeLoadWatch;
	// A first load compiles the pipeline states the renderer precaches for the level's materials, at a few frames a
	// second: the loading cover stays over it, and the match waits for it, but never for ever.
	TestFalse(TEXT("pipeline states compiling just after a load: the cover stays"), IsWarm(120, 1.0f));
	TestFalse(TEXT("a few still compiling a while later: it stays"), IsWarm(3, MostWarmingSeconds - 1.0f));
	TestTrue(TEXT("none compiling: it lifts at once"), IsWarm(0, 0.0f));
	TestTrue(TEXT("still compiling after the longest wait: it lifts anyway"), IsWarm(40, MostWarmingSeconds));
	TestTrue(TEXT("the longest wait is well inside the cover's own"), MostWarmingSeconds > 5.0f && MostWarmingSeconds < 30.0f);
	TestEqual(TEXT("a second's frames as the log says them"), DescribeSecond({0.1f, 0.1f, 0.3f}), FString(TEXT("6 fps (mean 167 ms, worst 300 ms)")));
	bool bSlow = false;
	DescribeSecond({0.1f, 0.1f, 0.3f}, &bSlow);
	TestTrue(TEXT("and slow"), bSlow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpensOnTheShooterLevel, "CheeseStrike.Project.OpensOnTheShooterLevel", ProjectTests::Flags)

bool FOpensOnTheShooterLevel::RunTest(const FString& Parameters)
{
	const FString Map = UGameMapsSettings::GetGameDefaultMap();
	TestEqual(TEXT("the game opens on the shooter level"), Map, FString(TEXT("/Game/Variant_Shooter/Lvl_Shooter")));
	TestTrue(TEXT("and it is there"), FPackageName::DoesPackageExist(Map));

	const UClass* Mode = LoadClass<AGameModeBase>(nullptr, *UGameMapsSettings::GetGlobalDefaultGameMode());
	if (TestNotNull(TEXT("the default game mode loads"), Mode))
	{
		TestEqual(TEXT("and is the game's own"), Mode, static_cast<const UClass*>(ACheeseStrikeGameMode::StaticClass()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTemplateBlueprintsLoadAgainstOurClasses, "CheeseStrike.Project.TemplateBlueprintsLoadAgainstOurClasses", ProjectTests::Flags)

bool FTemplateBlueprintsLoadAgainstOurClasses::RunTest(const FString& Parameters)
{
	const UClass* FirstPerson = LoadClass<ACharacter>(nullptr, TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C"));
	if (TestNotNull(TEXT("the first-person character loads"), FirstPerson))
	{
		TestTrue(TEXT("as one of ours"), FirstPerson->IsChildOf(ACheeseStrikeCharacter::StaticClass()));
	}
	const UClass* Shooter = LoadClass<ACharacter>(nullptr, TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter.BP_ShooterCharacter_C"));
	if (TestNotNull(TEXT("the shooter character loads"), Shooter))
	{
		TestTrue(TEXT("as one of ours"), Shooter->IsChildOf(AShooterCharacter::StaticClass()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNothingIsMissing, "CheeseStrike.Project.NothingIsMissing", ProjectTests::Flags)

bool FNothingIsMissing::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	Registry.SearchAllAssets(true);
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(TEXT("/Game")), Assets, true);
	TestTrue(TEXT("the project has content"), Assets.Num() > 100);

	TSet<FName> Checked;
	int32 Missing = 0;
	for (const FAssetData& Asset : Assets)
	{
		TArray<FName> Dependencies;
		Registry.GetDependencies(Asset.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);
		for (const FName& Dependency : Dependencies)
		{
			const FString Name = Dependency.ToString();
			if (Name.StartsWith(TEXT("/Script/")))
			{
				continue;
			}
			// A reference that Config/DefaultEngine.ini redirects counts as where it now goes.
			const FName Resolved = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, Dependency)).PackageName;
			if (Checked.Contains(Resolved))
			{
				continue;
			}
			if (!FPackageName::DoesPackageExist(Resolved.ToString()))
			{
				AddError(FString::Printf(TEXT("%s needs %s, which isn't there"), *Asset.PackageName.ToString(), *Name));
				++Missing;
				continue;
			}
			Checked.Add(Resolved);
		}
	}
	TestEqual(TEXT("packages needed and missing"), Missing, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCreditsNameEveryone, "CheeseStrike.Project.TheCreditsNameEveryone", ProjectTests::Flags)

bool FTheCreditsNameEveryone::RunTest(const FString& Parameters)
{
	// Every pack or file in Content/LICENSES.md whose licence asks for credit (Creative Commons Attribution) is named in the credits.
	FString Licenses;
	if (!TestTrue(TEXT("Content/LICENSES.md is there"), FFileHelper::LoadFileToString(Licenses, *(FPaths::ProjectContentDir() / TEXT("LICENSES.md")))))
	{
		return false;
	}
	const FString Credits = CheeseStrikeCredits::AllText();
	TArray<FString> Rows;
	Licenses.ParseIntoArrayLines(Rows);
	int32 Asked = 0;
	for (const FString& Row : Rows)
	{
		if (!Row.StartsWith(TEXT("|")) || !(Row.Contains(TEXT("CC BY")) || Row.Contains(TEXT("CC-BY")) || Row.Contains(TEXT("Creative Commons Attribution"))))
		{
			continue;
		}
		// Each work the row links to, by its title, less any "(...)" after it: [Title (Free download)](https://...).
		int32 From = 0;
		while ((From = Row.Find(TEXT("["), ESearchCase::CaseSensitive, ESearchDir::FromStart, From)) != INDEX_NONE)
		{
			const int32 To = Row.Find(TEXT("]("), ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			if (To == INDEX_NONE)
			{
				break;
			}
			FString Title = Row.Mid(From + 1, To - From - 1);
			int32 Bracket = INDEX_NONE;
			if (Title.FindChar(TEXT('('), Bracket))
			{
				Title = Title.Left(Bracket).TrimEnd();
			}
			++Asked;
			TestTrue(FString::Printf(TEXT("the credits name \"%s\""), *Title), Credits.Contains(Title));
			From = To + 2;
		}
	}
	TestTrue(FString::Printf(TEXT("some works ask for credit (%d)"), Asked), Asked > 0);
	TestTrue(TEXT("the credits name their licences"), Credits.Contains(TEXT("creativecommons.org/licenses/by/4.0")));
	return true;
}

#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFreshInstallPlaysBorderless, "CheeseStrike.Project.AFreshInstallPlaysBorderless", ProjectTests::Flags)

bool FAFreshInstallPlaysBorderless::RunTest(const FString& Parameters)
{
	// Config/DefaultGameUserSettings.ini: what a machine with no saved settings plays in. The engine reads the window mode
	// and the resolution from it before it makes the window, but only under a Version of its own (UGameUserSettings::
	// PreloadResolutionSettings), so the version there must be the engine's.
	FConfigFile File;
	File.Read(FPaths::ProjectConfigDir() / TEXT("DefaultGameUserSettings.ini"));
	const FString Section = UCheeseStrikeSettings::StaticClass()->GetPathName();
	int32 Version = -1;
	TestTrue(TEXT("the game's settings have defaults in Config/DefaultGameUserSettings.ini"), File.GetInt(*Section, TEXT("Version"), Version));
	TestEqual(TEXT("under the version the engine reads them by"), Version, UCheeseStrikeSettings::VersionTheEngineReads());
	int32 Mode = -1;
	TestTrue(TEXT("the window mode is there"), File.GetInt(*Section, TEXT("FullscreenMode"), Mode));
	TestEqual(TEXT("borderless (the engine's windowed fullscreen)"), Mode, static_cast<int32>(EWindowMode::WindowedFullscreen));
	TestEqual(TEXT("as the settings' own default"), static_cast<int32>(CheeseStrikeSettings::DefaultWindowMode), Mode);
	int32 Confirmed = -1;
	int32 Preferred = -1;
	TestTrue(TEXT("confirmed as such"), File.GetInt(*Section, TEXT("LastConfirmedFullscreenMode"), Confirmed) && Confirmed == Mode);
	TestTrue(TEXT("and preferred when full screen is asked for"), File.GetInt(*Section, TEXT("PreferredFullscreenMode"), Preferred) && Preferred == 1);
	int32 Width = -1;
	int32 Height = -1;
	bool bDesktop = false;
	TestTrue(TEXT("at the screen's own size (a resolution of 0 is the desktop's)"), File.GetInt(*Section, TEXT("ResolutionSizeX"), Width) && File.GetInt(*Section, TEXT("ResolutionSizeY"), Height) && Width == 0 && Height == 0);
	TestTrue(TEXT("the desktop's, says the file"), File.GetBool(*Section, TEXT("bUseDesktopResolution"), bDesktop) && bDesktop);
	// Nothing in the file turns the saved settings of a player's machine over: it has no other keys of the player's own.
	for (const TCHAR* Key : {TEXT("MouseSensitivity"), TEXT("FieldOfView"), TEXT("Hero"), TEXT("MasterVolume")})
	{
		FString Value;
		TestFalse(FString::Printf(TEXT("%s is the player's own, not the file's"), Key), File.GetString(*Section, Key, Value));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheSoundPlaysOnWithoutFocus, "CheeseStrike.Project.TheSoundPlaysOnWithoutFocus", ProjectTests::Flags)

bool FTheSoundPlaysOnWithoutFocus::RunTest(const FString& Parameters)
{
	// The engine mutes an unfocused game (its [Audio] UnfocusedVolumeMultiplier is 0): the game's own ini says 1, so the music
	// and the sounds go on when the player clicks off the window, and the engine reads that value.
	FConfigFile File;
	File.Read(FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini"));
	float Multiplier = -1.0f;
	TestTrue(TEXT("Config/DefaultEngine.ini sets the unfocused volume"), File.GetFloat(TEXT("Audio"), TEXT("UnfocusedVolumeMultiplier"), Multiplier));
	TestEqual(TEXT("to full"), Multiplier, 1.0f);
	TestEqual(TEXT("and the engine has it so"), FApp::GetUnfocusedVolumeMultiplier(), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoAsyncCompute, "CheeseStrike.Project.NoAsyncCompute", ProjectTests::Flags)

bool FNoAsyncCompute::RunTest(const FString& Parameters)
{
	// The GPU faulted in the virtual shadow maps' passes while Lumen ran beside them on the async compute queue (DefaultEngine.ini
	// says how), so the game runs every pass on the graphics queue, and Nanite streams into ordinary buffers.
	FConfigFile File;
	File.Read(FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini"));
	int32 Async = -1;
	TestTrue(TEXT("Config/DefaultEngine.ini turns async compute off"), File.GetInt(TEXT("SystemSettings"), TEXT("r.RDG.AsyncCompute"), Async) && Async == 0);
	int32 Reserved = -1;
	TestTrue(TEXT("and Nanite's reserved resources"), File.GetInt(TEXT("SystemSettings"), TEXT("r.Nanite.Streaming.ReservedResources"), Reserved) && Reserved == 0);
	const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RDG.AsyncCompute"));
	TestTrue(TEXT("and the engine has it so"), Variable != nullptr && Variable->GetInt() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheSpinnerIsACheeseWedge, "CheeseStrike.Project.TheSpinnerIsACheeseWedge", ProjectTests::Flags)

bool FTheSpinnerIsACheeseWedge::RunTest(const FString& Parameters)
{
	// The spinner's frames (tools/art/spinner_frames.py): a loop of 72 PNGs of 64 x 64 in Content/UI/Spinner, read as files so
	// that the loading screen can draw them before any asset is loaded, shown at 36 a second, each blended into the next; and the same loop at 320 x 320
	// in Spinner/Large for the loading screen's big spinner, a fifth of the screen's height (of the cover's 1080 units).
	using namespace CheeseStrikeLoader;
	TestTrue(TEXT("the loading screen's spinner is about a fifth of the screen's height (of the cover's 1080 units)"), LargeRadius * 2.0f >= 1080.0f * 0.2f && LargeRadius * 2.0f <= 1080.0f * 0.25f);
	TestTrue(TEXT("drawn from frames at least as big as it is drawn at 1080p"), LargeFrameSize >= LargeRadius * 2.0f * DrawnOver);
	TestEqual(TEXT("a loop of 72 frames"), Frames, 72);
	TestEqual(TEXT("at 36 a second"), FramesPerSecond, 36);
	TestEqual(TEXT("the first frame at the start"), FrameAt(0.0), 0);
	TestEqual(TEXT("the next a 36th of a second on"), FrameAt(1.0 / 36.0 + 0.001), 1);
	TestEqual(TEXT("round in two seconds"), FrameAt(2.0 + 0.001), 0);
	TestEqual(TEXT("never past the loop"), FrameAt(1.999), 71);
	TestTrue(TEXT("never back"), FrameAt(-0.5) >= 0 && FrameAt(-0.5) < Frames);
	TestTrue(TEXT("halfway between two frames, the next is drawn half over it"), FMath::IsNearlyEqual(FrameBlendAt(0.5 / 36.0), 0.5f, 0.01f));
	// Smooth: drawn at 58 frames a second (what the loading screen's thread draws, by the log's count), the turn moves on by
	// the same small step every frame, about 3.1 degrees, never standing still for a frame and then jumping (48 frames at 24 a
	// second moved 0 or 7.5 degrees from one frame drawn to the next).
	double Least = 360.0, Most = 0.0;
	for (int32 Drawn = 1; Drawn < 200; ++Drawn)
	{
		const double Step = FMath::Fmod(TurnAt(Drawn / 58.0) - TurnAt((Drawn - 1) / 58.0) + 360.0, 360.0);
		Least = FMath::Min(Least, Step);
		Most = FMath::Max(Most, Step);
	}
	TestTrue(FString::Printf(TEXT("each frame drawn turns it by the same step (%.2f to %.2f degrees)"), Least, Most), Most - Least < 0.01 && Least > 3.0 && Most < 3.2);
	for (const bool bLarge : {false, true})
	{
		const int32 Size = bLarge ? LargeFrameSize : FrameSize;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			TArray<uint8> Bytes;
			const FString Path = FramePath(Frame, bLarge);
			if (!TestTrue(FString::Printf(TEXT("%s is there"), *Path), FFileHelper::LoadFileToArray(Bytes, *Path)) || Bytes.Num() < 24)
			{
				continue;
			}
			// A PNG's size is in its header: the width and height as big-endian 32-bit numbers at 16 and 20.
			const auto At = [&Bytes](int32 Offset) { return (Bytes[Offset] << 24) | (Bytes[Offset + 1] << 16) | (Bytes[Offset + 2] << 8) | Bytes[Offset + 3]; };
			TestTrue(TEXT("a PNG"), Bytes[1] == 'P' && Bytes[2] == 'N' && Bytes[3] == 'G');
			TestEqual(FString::Printf(TEXT("%d across"), Size), At(16), Size);
			TestEqual(FString::Printf(TEXT("and %d high"), Size), At(20), Size);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCoverIsUpFromTheWindowsFirstFrame, "CheeseStrike.Project.TheCoverIsUpFromTheWindowsFirstFrame", ProjectTests::Flags)

bool FTheCoverIsUpFromTheWindowsFirstFrame::RunTest(const FString& Parameters)
{
	// Wave 17: the engine makes the game's window as its movie player starts, seconds before the game's module is loaded, and
	// left it black until the first level's loading screen. The cover is in a module of its own, loaded before the engine
	// plays its startup loading screen (PreLoadingScreen), which gives it the cover.
	FString Text;
	TSharedPtr<FJsonObject> Project;
	if (!TestTrue(TEXT("the .uproject reads"), FFileHelper::LoadFileToString(Text, *FPaths::GetProjectFilePath())) || !TestTrue(TEXT("as JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Project) && Project.IsValid()))
	{
		return false;
	}
	bool bStartupModule = false;
	for (const TSharedPtr<FJsonValue>& Module : Project->GetArrayField(TEXT("Modules")))
	{
		const TSharedPtr<FJsonObject> Fields = Module->AsObject();
		bStartupModule |= Fields.IsValid() && Fields->GetStringField(TEXT("Name")) == TEXT("CheeseStrikeStartup") && Fields->GetStringField(TEXT("LoadingPhase")) == TEXT("PreLoadingScreen");
	}
	TestTrue(TEXT("the startup cover's module loads before the engine's startup loading screen plays"), bStartupModule);
	// The startup cover, made before there is a window, is drawn at the size the game's UI would have in the window it gets.
	TestTrue(TEXT("at 1920 x 1080, the cover's own size"), FMath::IsNearlyEqual(SCheeseStrikeSteadyScale::TargetForSize(FVector2D(1920.0, 1080.0), 1.0f), 1.0f));
	TestTrue(TEXT("at 3440 x 1440, by its height"), FMath::IsNearlyEqual(SCheeseStrikeSteadyScale::TargetForSize(FVector2D(3440.0, 1440.0), 1.0f), 1440.0f / 1080.0f));
	TestTrue(TEXT("at 1280 x 720, two thirds"), FMath::IsNearlyEqual(SCheeseStrikeSteadyScale::TargetForSize(FVector2D(1280.0, 720.0), 1.0f), 720.0f / 1080.0f));
	TestTrue(TEXT("times the window's DPI"), FMath::IsNearlyEqual(SCheeseStrikeSteadyScale::TargetForSize(FVector2D(1920.0, 1080.0), 1.5f), 1.5f));
	// NVIDIA's overlay reads its frame rate from Reflex's PC Latency stats: the engine's Reflex plugin is on (the game turns its
	// markers on as the engine starts, FCheeseStrikeModule::StartLatencyMarkers).
	bool bReflex = false;
	for (const TSharedPtr<FJsonValue>& Plugin : Project->GetArrayField(TEXT("Plugins")))
	{
		const TSharedPtr<FJsonObject> Fields = Plugin->AsObject();
		bReflex |= Fields.IsValid() && Fields->GetStringField(TEXT("Name")) == TEXT("Reflex") && Fields->GetBoolField(TEXT("Enabled"));
	}
	TestTrue(TEXT("the Reflex plugin is enabled"), bReflex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheVolumesKeepTheSoundClassesTheySet, "CheeseStrike.Project.TheVolumesKeepTheSoundClassesTheySet", ProjectTests::Flags)

bool FTheVolumesKeepTheSoundClassesTheySet::RunTest(const FString& Parameters)
{
	// A sound class that goes with a level, as the classes the effects volume set did: the audio device kept pointing at
	// them after they were unloaded, and the game crashed as the next match began.
	TWeakObjectPtr<USoundClass> Passing = NewObject<USoundClass>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), USoundClass::StaticClass(), TEXT("PassingSounds")));
	UCheeseStrikeSettings::ApplySound();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue(TEXT("a sound class the volumes were put on is still there after the level's things are let go"), Passing.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoMapKeepsAnUnbuiltNavigationMesh, "CheeseStrike.Project.NoMapKeepsAnUnbuiltNavigationMesh", ProjectTests::Flags)

bool FNoMapKeepsAnUnbuiltNavigationMesh::RunTest(const FString& Parameters)
{
	// The game builds each map's navigation mesh as the level starts (RuntimeGeneration=Dynamic) and rebuilds it where the
	// cheese goes; a mesh saved in the map is kept as built, so a map saved by a script with the editor's unbuilt one in it
	// (a tile grid and no tiles: the editor fills it over the frames a script never gives it) leaves the bots and mice
	// nowhere to walk, as every map was on 5 Nov 2026. The scripts save through map_kit.save_level, which leaves it out;
	// this reads each map's export table (nothing is loaded) for a RecastNavMesh.
	for (int32 Index = 0; Index < CheeseStrikeRules::NumMaps(); ++Index)
	{
		const FString Level = CheeseStrikeRules::Maps[Index].Level;
		FPackagePath Path;
		if (!TestTrue(FString::Printf(TEXT("%s is a package"), *Level), FPackagePath::TryFromPackageName(Level, Path)))
		{
			continue;
		}
		const FLinkerLoad* Linker = GetPackageLinker(nullptr, Path, LOAD_NoVerify | LOAD_Quiet, nullptr);
		if (!TestNotNull(FString::Printf(TEXT("%s can be read"), *Level), Linker))
		{
			continue;
		}
		int32 Saved = 0;
		for (int32 Export = 0; Export < Linker->ExportMap.Num(); ++Export)
		{
			Saved += Linker->GetExportClassName(Export) == FName(TEXT("RecastNavMesh")) ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("%s keeps no navigation mesh for the game to take as built"), *Level), Saved, 0);
	}
	return true;
}
