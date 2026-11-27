#pragma once

#include "CoreMinimal.h"
#include "Game/HGGameModeBase.h"
#include "HGShopGameMode.generated.h"

/**
 * 경영 파트 게임 모드. 정거장 맵(L_Station)의 World Settings에서 BP_HG_ShopGameMode로 지정한다.
 * 플레이어 컨트롤러는 경영용(AHGShopPlayerController), 기본 캐릭터는 경영용 사장님(AHGShopBossCharacter).
 * 사장님은 PlayerStart에 나타난다 (행성에서 돌아오면 "FromPlanet" 태그).
 */
UCLASS()
class HUOGUOHUOGUO_API AHGShopGameMode : public AHGGameModeBase
{
	GENERATED_BODY()

public:
	AHGShopGameMode();
};
