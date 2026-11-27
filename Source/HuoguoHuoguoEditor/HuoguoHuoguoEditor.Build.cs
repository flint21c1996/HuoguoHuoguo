using UnrealBuildTool;

// 에디터 전용 모듈: 게임에는 들어가지 않는 개발 도구(Planet Maker 등).
// UnderOneUmbrella의 <프로젝트>Editor 모듈과 같은 구성이다.
public class HuoguoHuoguoEditor : ModuleRules
{
	public HuoguoHuoguoEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"PropertyEditor",
			"DesktopPlatform",
			"MainFrame",
			"Landscape",
			"HuoguoHuoguo"
		});
	}
}
