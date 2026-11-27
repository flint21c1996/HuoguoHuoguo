#pragma once

#include "CoreMinimal.h"
#include "World/Planet/HGBasinShape.h"
#include "World/Planet/HGPlanetShape.h"
#include "World/Planet/HGPlanetTypes.h"
#include "HGPlanetPreset.generated.h"

/**
 * Planet Maker에서 만든 행성 하나 (JSON 파일 한 개).
 *
 * 하는 일:
 *   행성 기본 값(환경 종류, 시드, 군계, 반지름, 높낮이, 촘촘함, 중력)과 지형 모양 값(FHGPlanetShape)을 함께 담는다.
 *   Content/HG/Planets/<이름>.json으로 저장하고 불러온다. 사람이 읽고 고칠 수 있는 글자 파일이라 버전 관리에서 바뀐 값이 그대로 보인다.
 *
 * 쓰는 곳:
 *   지금은 Planet Maker 창(에디터)에서 만들고, 미리보기 맵의 생성기에 넣어 본다.
 *   나중에 "고정 행성"으로 게임에 내보낼 때도 이 파일을 읽는다 (기획서: 고정판과 생성판 섞기).
 *
 * 버전:
 *   값 구성이 바뀌면 CurrentVersion을 올린다. 읽을 때 파일 버전이 더 높으면 거절하고, 낮으면 없는 값은 기본값으로 둔다.
 */
USTRUCT(BlueprintType)
struct HUOGUOHUOGUO_API FHGPlanetPreset
{
	GENERATED_BODY()

	// 1: 처음. 2 (2026-11-27): 평지 분지 값(Basin) 추가. 옛 파일은 분지 값을 기본값으로 읽는다.
	static constexpr int32 CurrentVersion = 2;

	// 파일 형식 판 (CurrentVersion).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "행성", meta = (DisplayName = "파일 버전"))
	int32 Version = CurrentVersion;

	// 행성 이름. 파일 이름으로도 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "행성", meta = (DisplayName = "이름"))
	FString Name = TEXT("새 행성");

	// 환경 종류. Planet Maker 창 위쪽 탭이 정한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "행성", meta = (DisplayName = "환경 종류"))
	EHGSiteType SiteType = EHGSiteType::PlanetSmall;

	// 노이즈 위치와 크레이터 배치를 정하는 시드. 값이 같아도 시드가 다르면 다른 지형이 된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "행성", meta = (DisplayName = "시드"))
	int32 Seed = 7;

	// 생물 군계 (땅 색, 하늘 색, 나오는 재료).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "행성", meta = (DisplayName = "군계"))
	EHGPlanetBiome Biome = EHGPlanetBiome::Rock;

	// 평균 반지름(cm). 소형 20 ~ 60m, 중형 150 ~ 300m, 소행성 10 ~ 15m가 지금 게임의 범위다. 평지 행성은 분지 한 변 길이로 정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "행성", meta = (DisplayName = "반지름(cm)", EditCondition = "SiteType != EHGSiteType::PlanetLarge", EditConditionHides, ClampMin = "500", ClampMax = "200000", UIMin = "500", UIMax = "150000"))
	float Radius = 8000.f;

	// 지형 높낮이. 반지름에 대한 비율 (0.05 = 반지름의 5%만큼 오르내림).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "행성", meta = (DisplayName = "높낮이(반지름 비율)", EditCondition = "SiteType != EHGSiteType::PlanetLarge", EditConditionHides, ClampMin = "0", ClampMax = "0.3", UIMin = "0", UIMax = "0.2"))
	float TerrainAmplitude = 0.05f;

	// 큰 언덕의 촘촘함. 클수록 언덕이 작고 많아진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "행성", meta = (DisplayName = "언덕 촘촘함", EditCondition = "SiteType != EHGSiteType::PlanetLarge", EditConditionHides, ClampMin = "0.1", ClampMax = "20", UIMin = "0.5", UIMax = "12"))
	float TerrainFrequency = 2.5f;

	// 캐릭터 중력 배율 (1 = 지구).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "행성", meta = (DisplayName = "중력 배율", ClampMin = "0.1", ClampMax = "3"))
	float GravityScale = 0.6f;

	// 바위와 재료 개수 배율.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "행성", meta = (DisplayName = "자원 풍부함", ClampMin = "0", ClampMax = "3"))
	float Richness = 1.f;

	// 지형 모양 값 (언덕 겹, 능선, 잔 굴곡, 크레이터, 착륙지, 높이 곡선).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "지형 모양", meta = (ShowOnlyInnerProperties, EditCondition = "SiteType != EHGSiteType::PlanetLarge", EditConditionHides))
	FHGPlanetShape Shape;

	// 평지 행성(대형) 분지 모양. 평지 탭에서만 보인다. [랜드스케이프에 넣기]로 평지 맵의 땅을 만든다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "평지 분지", meta = (ShowOnlyInnerProperties, EditCondition = "SiteType == EHGSiteType::PlanetLarge", EditConditionHides))
	FHGBasinShape Basin;

	// 생성기에 넣을 행성 값으로 바꾼다. 이름, 남은 시간 등 프리셋에 없는 값은 시드로 채운다.
	FHGPlanetSeed ToPlanetSeed() const;
};

namespace HGPlanet
{
	// 프리셋 파일을 두는 폴더: <프로젝트>/Content/HG/Planets/
	HUOGUOHUOGUO_API FString PresetDirectory();

	// 환경 종류별 처음 값. 그 종류가 나오는 첫 시드의 행성 값 + 기본 지형 모양.
	// 평지 행성(대형)은 중형 행성 값을 평지로 바꾸고(HGPlanet::MakeFlat) 기본 분지 모양을 쓴다.
	HUOGUOHUOGUO_API FHGPlanetPreset DefaultPreset(EHGSiteType SiteType);

	// JSON 파일로 저장 / 불러오기. 실패하면 false와 이유(OutError).
	HUOGUOHUOGUO_API bool SavePresetJson(const FHGPlanetPreset& Preset, const FString& FilePath, FString& OutError);
	HUOGUOHUOGUO_API bool LoadPresetJson(const FString& FilePath, FHGPlanetPreset& OutPreset, FString& OutError);
}
