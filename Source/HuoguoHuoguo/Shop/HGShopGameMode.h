#pragma once

#include "CoreMinimal.h"
#include "Game/HGGameModeBase.h"
#include "HGShopGameMode.generated.h"

/**
 * 경영 파트 게임 모드. 정거장 맵(L_Station)의 World Settings에서 지정한다.
 * 플레이어 컨트롤러만 경영용(AHGShopPlayerController)으로 정한다. 나머지는 공통 부모와 같다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGShopGameMode : public AHGGameModeBase
{
	GENERATED_BODY()

public:
	AHGShopGameMode();
};
