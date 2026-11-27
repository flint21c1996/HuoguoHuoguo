#include "PlanetMaker/SHGPlanetMaker.h"

#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "IDesktopPlatform.h"
#include "IStructureDetailsView.h"
#include "LevelEditorViewport.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PlanetMaker/HGPlanetMakerLibrary.h"
#include "PropertyEditorModule.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "World/Planet/HGPlanetGenerator.h"
#include "World/Planet/HGPlanetPreset.h"

#define LOCTEXT_NAMESPACE "HGPlanetMaker"

const FName SHGPlanetMaker::TabName(TEXT("HGPlanetMaker"));

namespace
{
	// 미리보기 맵 (Maps/Development). 지형만 보는 개발용 맵이다.
	const TCHAR* PreviewMapPackage = TEXT("/Game/HG/Maps/Development/L_PlanetMaker");

	// 평지 행성 맵 (게임용). 땅이 이 맵의 랜드스케이프다.
	const TCHAR* FlatMapPackage = TEXT("/Game/HG/Maps/L_Planet_Large");
}

void SHGPlanetMaker::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = true;
	ViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	FStructureDetailsViewArgs StructArgs;
	DetailsView = PropertyEditor.CreateStructureDetailView(ViewArgs, StructArgs, GetOrCreatePreset(CurrentTab));
	DetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SHGPlanetMaker::OnPropertiesChanged);

	StatusText = FindPreviewGenerator()
		? LOCTEXT("StatusReady", "미리보기 생성기를 찾았어요. 값을 바꾸면 지형이 다시 만들어져요.")
		: LOCTEXT("StatusNoGenerator", "지금 맵에 행성 생성기가 없어요. [미리보기 맵 열기]를 누르세요.");

	using FTabs = SSegmentedControl<EHGSiteType>;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(6.f)
		[
			SNew(SVerticalBox)

			// 환경 종류 탭
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(FTabs)
				.Value_Lambda([this]() { return CurrentTab; })
				.OnValueChanged(this, &SHGPlanetMaker::OnTabChanged)
				+ FTabs::Slot(EHGSiteType::PlanetSmall).Text(LOCTEXT("TabSmall", "소형"))
				+ FTabs::Slot(EHGSiteType::PlanetMedium).Text(LOCTEXT("TabMedium", "중형"))
				+ FTabs::Slot(EHGSiteType::PlanetLarge).Text(LOCTEXT("TabLarge", "평지 (대형)"))
				+ FTabs::Slot(EHGSiteType::AsteroidField).Text(LOCTEXT("TabField", "소행성 지대"))
				+ FTabs::Slot(EHGSiteType::Derelict).Text(LOCTEXT("TabDerelict", "폐우주선"))
			]

			// 단추 줄
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RollSeed", "주사위 (시드만 새로)"))
					.ToolTipText(LOCTEXT("RollSeedTip", "다른 값은 그대로 두고 시드만 바꾼다. 언덕 위치와 크레이터 자리가 바뀐다."))
					.OnClicked(this, &SHGPlanetMaker::OnRollSeed)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Reset", "기본값으로"))
					.ToolTipText(LOCTEXT("ResetTip", "이 탭의 값을 처음 값(그 환경이 나오는 첫 시드 + 기본 지형 모양)으로 되돌린다."))
					.OnClicked(this, &SHGPlanetMaker::OnResetDefaults)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("OpenMap", "미리보기 맵 열기"))
					.ToolTipText(LOCTEXT("OpenMapTip", "Maps/Development/L_PlanetMaker를 연다. 바뀐 맵이 있으면 저장할지 먼저 묻는다."))
					.OnClicked(this, &SHGPlanetMaker::OnOpenPreviewMap)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ApplyLandscape", "랜드스케이프에 넣기"))
					.ToolTipText(LOCTEXT("ApplyLandscapeTip", "분지 값으로 지금 맵의 랜드스케이프를 새로 만든다 (있던 랜드스케이프는 지운다, Ctrl+Z 가능). KillZ와 행성 생성기 평지 설정도 맞춘다."))
					.Visibility_Lambda([this]() { return CurrentTab == EHGSiteType::PlanetLarge ? EVisibility::Visible : EVisibility::Collapsed; })
					.OnClicked(this, &SHGPlanetMaker::OnApplyLandscape)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Focus", "행성으로 카메라"))
					.ToolTipText(LOCTEXT("FocusTip", "에디터 뷰포트 카메라를 행성 전체가 보이게 옮긴다."))
					.OnClicked(this, &SHGPlanetMaker::OnFocusPlanet)
				]
			]

			// 상태 한 줄
			+ SVerticalBox::Slot().AutoHeight().Padding(2.f, 2.f, 2.f, 6.f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text_Lambda([this]() { return StatusText; })
			]

			// 값 패널
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				DetailsView->GetWidget().ToSharedRef()
			]

			// 저장 / 불러오기
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Load", "불러오기..."))
					.OnClicked(this, &SHGPlanetMaker::OnLoad)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Save", "저장 (Content/HG/Planets/이름.json)"))
					.OnClicked(this, &SHGPlanetMaker::OnSave)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("OpenFolder", "저장 폴더 열기"))
					.OnClicked(this, &SHGPlanetMaker::OnOpenFolder)
				]
			]
		]
	];
}

TSharedPtr<FStructOnScope> SHGPlanetMaker::GetOrCreatePreset(EHGSiteType SiteType)
{
	if (const TSharedPtr<FStructOnScope>* Found = Presets.Find(SiteType))
	{
		return *Found;
	}
	TSharedPtr<FStructOnScope> Scope = MakeShared<FStructOnScope>(FHGPlanetPreset::StaticStruct());
	*reinterpret_cast<FHGPlanetPreset*>(Scope->GetStructMemory()) = HGPlanet::DefaultPreset(SiteType);
	Presets.Add(SiteType, Scope);
	return Scope;
}

FHGPlanetPreset& SHGPlanetMaker::CurrentPreset() const
{
	return *reinterpret_cast<FHGPlanetPreset*>(Presets.FindChecked(CurrentTab)->GetStructMemory());
}

void SHGPlanetMaker::RefreshDetails()
{
	DetailsView->SetStructureData(GetOrCreatePreset(CurrentTab));
}

AHGPlanetGenerator* SHGPlanetMaker::FindPreviewGenerator()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AHGPlanetGenerator> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void SHGPlanetMaker::ApplyToPreview()
{
	AHGPlanetGenerator* Generator = FindPreviewGenerator();
	if (!Generator)
	{
		StatusText = LOCTEXT("StatusNoGeneratorApply", "지금 맵에 행성 생성기가 없어서 미리보기를 못 만들었어요. [미리보기 맵 열기]를 누르세요.");
		return;
	}
	const FHGPlanetPreset& Preset = CurrentPreset();
	if (Preset.SiteType == EHGSiteType::PlanetLarge)
	{
		// 평지의 땅은 랜드스케이프라서 값을 바꿀 때마다 다시 만들지 않는다 (손으로 깎은 것이 사라지고 무겁다).
		StatusText = Generator->IsFlatSite()
			? LOCTEXT("StatusFlat", "평지 분지 값을 바꿨어요. [랜드스케이프에 넣기]를 누르면 땅에 반영돼요.")
			: LOCTEXT("StatusFlatNotMap", "평지 탭이에요. [미리보기 맵 열기]로 평지 맵(L_Planet_Large)을 열고 [랜드스케이프에 넣기]를 누르세요.");
		return;
	}
	if (Preset.SiteType == EHGSiteType::Derelict)
	{
		Generator->SetEditorPreview(Preset.ToPlanetSeed(), Preset.Shape);
		StatusText = LOCTEXT("StatusDerelict", "폐우주선은 땅이 없어서 지형 미리보기가 비어 있어요 (우주선은 고정형으로 만들 예정).");
		return;
	}

	const double StartTime = FPlatformTime::Seconds();
	Generator->SetEditorPreview(Preset.ToPlanetSeed(), Preset.Shape);
	StatusText = FText::Format(LOCTEXT("StatusApplied", "미리보기를 다시 만들었어요: {0} | 반지름 {1}m | 크레이터 {2}개 | {3}ms"),
		FText::FromString(Preset.Name),
		FText::AsNumber(FMath::RoundToInt(Preset.Radius / 100.f)),
		FText::AsNumber(Generator->GetCraterCount()),
		FText::AsNumber(FMath::RoundToInt((FPlatformTime::Seconds() - StartTime) * 1000.0)));
}

void SHGPlanetMaker::OnTabChanged(EHGSiteType NewTab)
{
	CurrentTab = NewTab;
	RefreshDetails();
	ApplyToPreview();
}

void SHGPlanetMaker::OnPropertiesChanged(const FPropertyChangedEvent& Event)
{
	// 슬라이더를 끄는 동안(Interactive)은 건너뛰고, 놓거나 값을 입력했을 때 한 번 만든다.
	if (Event.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}
	ApplyToPreview();
}

FReply SHGPlanetMaker::OnRollSeed()
{
	CurrentPreset().Seed = FMath::Rand() & 0x7fffffff;
	RefreshDetails();
	ApplyToPreview();
	return FReply::Handled();
}

FReply SHGPlanetMaker::OnResetDefaults()
{
	CurrentPreset() = HGPlanet::DefaultPreset(CurrentTab);
	RefreshDetails();
	ApplyToPreview();
	return FReply::Handled();
}

FReply SHGPlanetMaker::OnApplyLandscape()
{
	if (UHGPlanetMakerLibrary::HasLandscape())
	{
		const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo,
			LOCTEXT("ReplaceLandscape", "지금 맵의 랜드스케이프를 지우고 분지 값으로 새로 만들어요. 손으로 깎은 곳도 사라져요 (Ctrl+Z로 되돌릴 수 있음). 계속할까요?"));
		if (Answer != EAppReturnType::Yes)
		{
			return FReply::Handled();
		}
	}
	StatusText = FText::FromString(UHGPlanetMakerLibrary::ApplyBasinToLandscape(CurrentPreset()));
	OnFocusPlanet();
	return FReply::Handled();
}

FReply SHGPlanetMaker::OnOpenPreviewMap()
{
	// 평지 탭은 게임용 평지 맵 자체를 연다 (땅이 그 맵의 랜드스케이프라서). 나머지는 구 행성 미리보기 맵.
	const TCHAR* MapPackage = CurrentTab == EHGSiteType::PlanetLarge ? FlatMapPackage : PreviewMapPackage;
	const FString MapFile = FPackageName::LongPackageNameToFilename(MapPackage, FPackageName::GetMapPackageExtension());
	if (!FPaths::FileExists(MapFile))
	{
		StatusText = FText::Format(LOCTEXT("StatusNoMap", "미리보기 맵이 없어요: {0}"), FText::FromString(MapPackage));
		return FReply::Handled();
	}
	// 바뀐 맵이 있으면 저장할지 묻고(취소하면 열지 않는다) 연다.
	if (FEditorFileUtils::SaveDirtyPackages(true, true, false))
	{
		FEditorFileUtils::LoadMap(MapFile, false, true);
		ApplyToPreview();
		OnFocusPlanet();
	}
	return FReply::Handled();
}

FReply SHGPlanetMaker::OnFocusPlanet()
{
	AHGPlanetGenerator* Generator = FindPreviewGenerator();
	if (!Generator || !GEditor)
	{
		return FReply::Handled();
	}
	// 행성 반지름(+ 높낮이)이 화면에 다 들어오게, 중심에서 비스듬히 위쪽(착륙 지점 쪽)에서 바라본다.
	const FHGPlanetPreset& Preset = CurrentPreset();
	// 평지는 분지 전체(반폭)가 들어오게.
	const float Reach = Preset.SiteType == EHGSiteType::PlanetLarge
		? Preset.Basin.HalfExtentCm() * 1.7f
		: Preset.Radius * (1.f + Preset.TerrainAmplitude) * 2.6f;
	const FVector Center = Generator->GetActorLocation();
	const FVector Eye = Center + FVector(-0.55f, -0.35f, 0.75f).GetSafeNormal() * Reach;
	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (Client && Client->IsPerspective())
		{
			Client->SetViewLocation(Eye);
			Client->SetViewRotation((Center - Eye).Rotation());
			Client->Invalidate();
		}
	}
	return FReply::Handled();
}

FReply SHGPlanetMaker::OnSave()
{
	const FHGPlanetPreset& Preset = CurrentPreset();
	const FString Directory = HGPlanet::PresetDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString FileName = FPaths::MakeValidFileName(Preset.Name.TrimStartAndEnd(), TEXT('_'));
	if (FileName.IsEmpty())
	{
		StatusText = LOCTEXT("StatusNoName", "이름이 비어 있어서 저장하지 않았어요.");
		return FReply::Handled();
	}
	const FString FilePath = FPaths::Combine(Directory, FileName + TEXT(".json"));
	if (FPaths::FileExists(FilePath))
	{
		const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo,
			FText::Format(LOCTEXT("Overwrite", "{0} 파일이 이미 있어요. 덮어쓸까요?"), FText::FromString(FileName + TEXT(".json"))));
		if (Answer != EAppReturnType::Yes)
		{
			return FReply::Handled();
		}
	}
	FString Error;
	StatusText = HGPlanet::SavePresetJson(Preset, FilePath, Error)
		? FText::Format(LOCTEXT("StatusSaved", "저장했어요: {0}"), FText::FromString(FilePath))
		: FText::Format(LOCTEXT("StatusSaveFailed", "저장하지 못했어요: {0}"), FText::FromString(Error));
	return FReply::Handled();
}

FReply SHGPlanetMaker::OnLoad()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}
	TArray<FString> Files;
	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	const bool bPicked = DesktopPlatform->OpenFileDialog(ParentWindow,
		LOCTEXT("LoadTitle", "행성 프리셋 불러오기").ToString(), HGPlanet::PresetDirectory(), TEXT(""),
		TEXT("행성 프리셋 (*.json)|*.json"), EFileDialogFlags::None, Files);
	if (!bPicked || Files.Num() == 0)
	{
		return FReply::Handled();
	}

	FHGPlanetPreset Loaded;
	FString Error;
	if (!HGPlanet::LoadPresetJson(Files[0], Loaded, Error))
	{
		StatusText = FText::Format(LOCTEXT("StatusLoadFailed", "불러오지 못했어요: {0}"), FText::FromString(Error));
		return FReply::Handled();
	}
	// 파일의 환경 종류 탭으로 옮겨 그 탭 값을 바꾼다.
	CurrentTab = Loaded.SiteType;
	GetOrCreatePreset(CurrentTab);
	CurrentPreset() = Loaded;
	RefreshDetails();
	ApplyToPreview();
	StatusText = FText::Format(LOCTEXT("StatusLoaded", "불러왔어요: {0}"), FText::FromString(Files[0]));
	return FReply::Handled();
}

FReply SHGPlanetMaker::OnOpenFolder()
{
	const FString Directory = HGPlanet::PresetDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);
	FPlatformProcess::ExploreFolder(*FPaths::ConvertRelativePathToFull(Directory));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
