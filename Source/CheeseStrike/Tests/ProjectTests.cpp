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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UI/CheeseStrikeCredits.h"
#include "Settings/CheeseStrikeSettings.h"
#include "Sound/SoundClass.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ProjectTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
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
