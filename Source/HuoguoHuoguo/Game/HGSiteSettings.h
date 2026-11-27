#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "World/Planet/HGPlanetTypes.h"
#include "HGSiteSettings.generated.h"

/**
 * 환경 종류별로 다이빙할 맵. 프로젝트 설정 → Game → HG 환경 맵에 나온다 (값은 Config/DefaultGame.ini에 저장).
 *
 * 다이빙 발판(bDiveToSelectedPlanet)을 밟으면, 고른 행성의 환경 종류(HGPlanet::SiteTypeOf)를 보고 여기서 맵을 찾아 연다.
 * 표에 없거나 비어 있는 환경은 발판의 DestinationLevel(기본 목적지)로 간다.
 * 맵마다 규칙(조명, 하늘, 생성기 값)은 그 맵 안에서 직접 정한다. 이 설정은 "어느 맵으로 갈지"만 가진다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "HG 환경 맵"))
class HUOGUOHUOGUO_API UHGSiteSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 환경 종류 → 그 환경의 맵.
	UPROPERTY(config, EditAnywhere, Category = "다이빙", meta = (DisplayName = "환경별 맵"))
	TMap<EHGSiteType, TSoftObjectPtr<UWorld>> SiteMaps;

	// 접근 행성 목록에서 셋째부터 각 행성이 평지 행성(대형, 랜드스케이프 분지)이 될 확률. 0이면 평지 행성이 나오지 않는다.
	// 첫째(소형)와 둘째(중형)는 늘 구 행성이다.
	UPROPERTY(config, EditAnywhere, Category = "다이빙", meta = (DisplayName = "평지 행성 확률", ClampMin = "0", ClampMax = "1"))
	float FlatPlanetChance = 0.35f;

	// 환경에 맞는 맵. 없으면 빈 값(호출하는 쪽이 기본 목적지를 쓴다).
	static TSoftObjectPtr<UWorld> FindMap(EHGSiteType Site)
	{
		const TSoftObjectPtr<UWorld>* Found = GetDefault<UHGSiteSettings>()->SiteMaps.Find(Site);
		return Found ? *Found : TSoftObjectPtr<UWorld>();
	}
};
