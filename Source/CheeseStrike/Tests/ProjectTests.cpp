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

#endif
