// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

using UnrealBuildTool;

// The game's loading cover and spinner, plain Slate, in a module of their own that the engine loads before it first plays its
// loading screen (LoadingPhase PreLoadingScreen in the .uproject), so that the cover shows from the window's first frame.
public class CheeseStrikeStartup : ModuleRules
{
	public CheeseStrikeStartup(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "Slate", "SlateCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject", "ApplicationCore", "MoviePlayer" });
	}
}
