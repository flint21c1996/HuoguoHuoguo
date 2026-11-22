#pragma once

#include "CoreMinimal.h"
#include "Game/HGGameModeBase.h"
#include "HGExploreGameMode.generated.h"

/**
 * 탐험 파트 게임 모드. 행성 표면 맵(L_PlanetSurface)의 World Settings에서 지정한다.
 * 맵에 게임 모드 지정이 없을 때 쓰는 프로젝트 기본 게임 모드이기도 하다 (시험용 맵을 열면 바로 걸어 다닐 수 있게).
 */
UCLASS()
class HUOGUOHUOGUO_API AHGExploreGameMode : public AHGGameModeBase
{
	GENERATED_BODY()

public:
	AHGExploreGameMode();
};
