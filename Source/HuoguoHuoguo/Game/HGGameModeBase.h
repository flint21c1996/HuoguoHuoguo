#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HGGameModeBase.generated.h"

/**
 * 경영 파트와 탐험 파트 게임 모드의 공통 부모.
 *
 * 두 파트는 맵 단위로 완전히 나뉜다. 맵의 World Settings > GameMode Override로 파트를 정한다.
 *   L_Station        -> AHGShopGameMode    (아이소 시점, 사장님이 가게 안을 걷는다)
 *   L_PlanetSurface  -> AHGExploreGameMode (3인칭 시점, 구형 중력)
 *
 * 기본 Pawn을 두지 않는다(DefaultPawnClass = null). 맵에 배치된 표식(시작점, 행성 생성기)을 보고
 * 각 파트의 플레이어 컨트롤러가 사장님을 직접 만든다.
 */
UCLASS(Abstract)
class HUOGUOHUOGUO_API AHGGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHGGameModeBase();

	// 엔진 기본 동작은 접속한 플레이어에게 DefaultPawnClass를 스폰해 빙의시키는 것이다.
	// 이 게임은 컨트롤러가 Pawn을 직접 만들므로 아무것도 하지 않는다 (클래스 없이 스폰하려다 나는 오류 로그 방지).
	virtual void RestartPlayer(AController* NewPlayer) override {}
};
