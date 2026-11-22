// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HuoguoHuoguo : ModuleRules
{
	public HuoguoHuoguo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 모듈 루트를 include 경로에 넣어서 "Game/HGPlayerController.h"처럼 하위 폴더 경로로 include할 수 있게 한다.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
