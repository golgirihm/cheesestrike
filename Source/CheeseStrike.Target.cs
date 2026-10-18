// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class CheeseStrikeTarget : TargetRules
{
	public CheeseStrikeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("CheeseStrike");

		// Unreal 5.8 builds with the 14.50 compiler; 14.51, which a Visual Studio
		// update brings in, breaks the engine's headers (a removed hash_map).
		WindowsPlatform.CompilerVersion = "14.50.35717";
	}
}
