// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HuoguoHuoguo : ModuleRules
{
	public HuoguoHuoguo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 모듈 루트를 include 경로에 넣어서 "Game/HGPlayerControllerBase.h"처럼 하위 폴더 경로로 include할 수 있게 한다.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		// 행성 지형을 런타임에 메시로 만들기 위한 엔진 플러그인 모듈.
		PrivateDependencyModuleNames.Add("ProceduralMeshComponent");

		// 화면 UI(미니맵 등). UMG 위젯을 C++에서 만들고 Slate로 직접 그린다.
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore" });

		// 프로젝트 설정 화면에 나오는 설정 클래스(UDeveloperSettings, 예: 그림체 조절 값 UHGLookSettings).
		PublicDependencyModuleNames.Add("DeveloperSettings");
	}
}
