// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OneCloudTarget : TargetRules
{
	public OneCloudTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OneCloud");
	}
}
