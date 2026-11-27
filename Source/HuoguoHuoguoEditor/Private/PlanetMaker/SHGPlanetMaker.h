#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "World/Planet/HGPlanetTypes.h"

class IStructureDetailsView;
class AHGPlanetGenerator;
struct FHGPlanetPreset;
struct FPropertyChangedEvent;
class FStructOnScope;

/**
 * Planet Maker 창.
 *
 * 생김새 (위에서 아래로):
 *   - 환경 종류 탭: 소형 | 중형 | 대형 | 소행성 지대 | 폐우주선. 탭마다 만지던 값을 따로 들고 있다 (탭을 오가도 안 사라짐).
 *   - 단추 줄: 주사위(시드만 새로), 기본값으로, 미리보기 맵 열기.
 *   - 상태 한 줄: 미리보기 생성기를 찾았는지, 마지막으로 한 일.
 *   - 값 패널: 행성 값(시드, 군계, 반지름, 높낮이 ...)과 지형 모양 값(언덕, 능선, 잔 굴곡, 크레이터, 착륙지, 높이 곡선).
 *   - 아래 단추 줄: 불러오기, 저장, 저장 폴더 열기.
 *
 * 값을 바꾸면 (슬라이더는 놓을 때) 지금 열린 맵의 행성 생성기(AHGPlanetGenerator)에 넣어 지형 미리보기를 다시 만든다.
 * 미리보기 맵은 Maps/Development/L_PlanetMaker. 다른 맵에 생성기가 있으면 그것도 쓴다.
 * 저장은 Content/HG/Planets/<이름>.json (HGPlanet::SavePresetJson).
 */
class SHGPlanetMaker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHGPlanetMaker) {}
	SLATE_END_ARGS()

	static const FName TabName;

	void Construct(const FArguments& InArgs);

private:
	// 탭(환경 종류)마다 만지는 값. 값 패널이 이 메모리를 직접 고친다.
	TMap<EHGSiteType, TSharedPtr<FStructOnScope>> Presets;
	EHGSiteType CurrentTab = EHGSiteType::PlanetSmall;

	TSharedPtr<IStructureDetailsView> DetailsView;
	FText StatusText;

	FHGPlanetPreset& CurrentPreset() const;
	TSharedPtr<FStructOnScope> GetOrCreatePreset(EHGSiteType SiteType);

	// 지금 탭 값을 값 패널에 다시 보여 준다 (코드에서 값을 바꾼 뒤).
	void RefreshDetails();

	// 지금 열린 에디터 맵의 행성 생성기. 없으면 nullptr.
	static AHGPlanetGenerator* FindPreviewGenerator();

	// 지금 탭 값을 생성기에 넣어 미리보기를 다시 만든다.
	void ApplyToPreview();

	void OnTabChanged(EHGSiteType NewTab);
	void OnPropertiesChanged(const FPropertyChangedEvent& Event);

	FReply OnRollSeed();
	FReply OnResetDefaults();
	FReply OnOpenPreviewMap();
	FReply OnFocusPlanet();

	// 평지 탭: 분지 값으로 지금 맵의 랜드스케이프를 새로 만든다 (있으면 먼저 묻는다).
	FReply OnApplyLandscape();
	FReply OnSave();
	FReply OnLoad();
	FReply OnOpenFolder();
};
