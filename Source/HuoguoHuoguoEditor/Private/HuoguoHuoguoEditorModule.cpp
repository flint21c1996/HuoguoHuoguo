#include "Framework/Docking/TabManager.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"
#include "PlanetMaker/SHGPlanetMaker.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "HuoguoHuoguoEditor"

/**
 * 훠궈훠궈 에디터 모듈. 에디터에서만 켜지는 도구를 등록한다.
 *   - Planet Maker 창: 메뉴 Tools → 훠궈훠궈 → Planet Maker, 또는 콘솔 명령 hg.PlanetMaker
 */
class FHuoguoHuoguoEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// 창(탭) 만드는 법을 등록한다. 메뉴에는 아래 RegisterMenus가 따로 넣는다.
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SHGPlanetMaker::TabName,
			FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
			{
				return SNew(SDockTab)
					.TabRole(ETabRole::NomadTab)
					[
						SNew(SHGPlanetMaker)
					];
			}))
			.SetDisplayName(LOCTEXT("PlanetMakerTab", "Planet Maker"))
			.SetTooltipText(LOCTEXT("PlanetMakerTabTip", "절차적 행성의 크기, 높이, 지형 모양을 바꿔 보며 미리보고 JSON으로 저장한다."))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FHuoguoHuoguoEditorModule::RegisterMenus));

		// 콘솔 명령. 실행 인자(-ExecCmds="hg.PlanetMaker")로 부르면 에디터 메인 창이 뜨기 전이라 탭을 붙일 곳이 없다.
		// 그래서 메인 창이 준비될 때까지 0.5초마다 기다렸다가 연다.
		OpenCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("hg.PlanetMaker"),
			TEXT("Planet Maker 창을 연다 (에디터 메인 창이 아직 없으면 뜰 때까지 기다린다)."),
			FConsoleCommandDelegate::CreateLambda([]()
			{
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
				{
					const IMainFrameModule* MainFrame = FModuleManager::GetModulePtr<IMainFrameModule>("MainFrame");
					if (!MainFrame || !MainFrame->IsWindowInitialized())
					{
						return true; // 계속 기다린다
					}
					FGlobalTabmanager::Get()->TryInvokeTab(SHGPlanetMaker::TabName);
					return false;
				}), 0.5f);
			}));
	}

	virtual void ShutdownModule() override
	{
		OpenCommand.Reset();
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SHGPlanetMaker::TabName);
		}
	}

private:
	TUniquePtr<FAutoConsoleCommand> OpenCommand;

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->FindOrAddSection("HuoguoHuoguo", LOCTEXT("HGToolsSection", "훠궈훠궈"));
		Section.AddMenuEntry(
			"HGPlanetMaker",
			LOCTEXT("PlanetMakerMenu", "Planet Maker"),
			LOCTEXT("PlanetMakerMenuTip", "절차적 행성 만들기 창을 연다."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(SHGPlanetMaker::TabName);
			})));
	}
};

IMPLEMENT_MODULE(FHuoguoHuoguoEditorModule, HuoguoHuoguoEditor)

#undef LOCTEXT_NAMESPACE
