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
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"AnimGraphRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "EngineSettings", "AssetRegistry", "OnlineBase", "OnlineSubsystem", "OnlineSubsystemUtils", "AudioExtensions" });

		// The tests that play the game do so in the editor.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			// For the art scripts' tools (Dev/CheeseStrikeArtTools): making physics assets, from the vertices on each bone.
			PrivateDependencyModuleNames.Add("PhysicsUtilities");
			PrivateDependencyModuleNames.Add("MeshUtilitiesEngine");
		}

		PublicIncludePaths.AddRange(new string[] {
			"CheeseStrike",
			"CheeseStrike/Animation",
			"CheeseStrike/Dev",
			"CheeseStrike/Input",
			"CheeseStrike/Map",
			"CheeseStrike/Movement",
			"CheeseStrike/Rules",
			"CheeseStrike/Sessions",
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
