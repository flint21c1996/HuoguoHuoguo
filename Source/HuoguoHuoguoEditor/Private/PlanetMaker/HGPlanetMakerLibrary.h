#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "World/Planet/HGPlanetPreset.h"
#include "HGPlanetMakerLibrary.generated.h"

class UWorld;

/**
 * Planet Maker의 에디터 작업 (창 단추와 파이썬 스크립트가 같이 쓴다).
 *
 * 평지 행성: 분지 값(FHGBasinShape)으로 땅 높이를 계산해 지금 열린 맵에 랜드스케이프를 새로 만든다.
 *   - 맵에 있던 랜드스케이프는 지운다 (손으로 깎은 것도 사라지므로 창에서는 먼저 묻는다). Ctrl+Z로 되돌릴 수 있다.
 *   - 랜드스케이프 가운데 = 맵의 행성 생성기 위치 (없으면 원점). 1칸 = 1m, 높이 단위 약 0.8cm.
 *   - 맵의 KillZ(World Settings)를 땅에서 가장 낮은 곳보다 20m 아래로 둔다. 그 아래로 떨어지면 착륙 지점으로 돌아온다.
 *   - 행성 생성기를 평지 방식으로 두고, 재료와 바위를 놓을 원판 반지름을 둑 안쪽으로 맞춘다.
 *   - 땅 머티리얼은 M_HG_LandscapeBasin (평평한 곳 땅 색, 비탈 바위 색. 색 값은 게임에서 군계에 맞춰 바꾼다).
 */
UCLASS()
class UHGPlanetMakerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 지금 열린 에디터 맵에 분지 랜드스케이프를 만든다. 결과 한 줄(성공/실패 이유)을 돌려준다.
	UFUNCTION(BlueprintCallable, Category = "HG|Planet Maker")
	static FString ApplyBasinToLandscape(const FHGPlanetPreset& Preset);

	// 환경 종류별 처음 값 (HGPlanet::DefaultPreset). 파이썬에서 쓰기 위한 통로.
	UFUNCTION(BlueprintCallable, Category = "HG|Planet Maker")
	static FHGPlanetPreset GetDefaultPreset(EHGSiteType SiteType);

	// 지금 열린 에디터 맵에 랜드스케이프가 있는지 (덮어쓰기 전에 묻기 위해).
	static bool HasLandscape();
};
