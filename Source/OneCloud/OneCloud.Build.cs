// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OneCloud : ModuleRules
{
	public OneCloud(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Niagara", "UMG" });

		PrivateDependencyModuleNames.AddRange(new string[] { "PhysicsCore", "Slate", "SlateCore", "AIModule", "NavigationSystem" });
	}
}
