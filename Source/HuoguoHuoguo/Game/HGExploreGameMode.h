#pragma once

#include "CoreMinimal.h"
#include "Game/HGGameModeBase.h"
#include "HGExploreGameMode.generated.h"

/**
 * 탐험 파트 게임 모드. 환경 맵마다 이것의 BP(BP_HG_ExploreGameMode_소형 등)를 World Settings에서 지정한다.
 * 맵에 게임 모드 지정이 없을 때 쓰는 프로젝트 기본 게임 모드이기도 하다 (시험용 맵을 열면 바로 걸어 다닐 수 있게).
 *
 * 시작 위치:
 *   행성 생성기(AHGPlanetGenerator)가 있는 맵은 지형이 시드마다 달라서 PlayerStart를 미리 둘 수 없다.
 *   그래서 생성기가 지형을 먼저 만들고 알려 준 착륙 지점에 엔진 표준 스폰(RestartPlayerAtTransform)으로 만든다.
 *   생성기가 없는 맵은 엔진 표준대로 PlayerStart에 만든다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGExploreGameMode : public AHGGameModeBase
{
	GENERATED_BODY()

public:
	AHGExploreGameMode();

	// 행성 맵이면 생성기를 시작 기준으로 돌려준다 (PlayerStart가 없어도 경고가 나지 않게).
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;

	virtual void RestartPlayer(AController* NewPlayer) override;
};
