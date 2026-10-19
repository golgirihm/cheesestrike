// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CheeseStrike : ModuleRules
{
	public CheeseStrike(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "EngineSettings", "AssetRegistry" });

		// The tests that play the game do so in the editor.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicIncludePaths.AddRange(new string[] {
			"CheeseStrike",
			"CheeseStrike/Dev",
			"CheeseStrike/Input",
			"CheeseStrike/Movement",
			"CheeseStrike/Rules",
			"CheeseStrike/UI",
			"CheeseStrike/Weapons",
			"CheeseStrike/Variant_Shooter",
			"CheeseStrike/Variant_Shooter/AI",
			"CheeseStrike/Variant_Shooter/UI",
			"CheeseStrike/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
