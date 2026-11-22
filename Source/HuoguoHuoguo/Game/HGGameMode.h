#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HGGameMode.generated.h"

/**
 * 훠궈훠궈 기본 게임 모드. 정거장 맵과 파밍지 맵 모두 이 게임 모드를 쓴다.
 *
 * 기본 Pawn을 두지 않는다(DefaultPawnClass = null). 맵마다 필요한 Pawn이 다르기 때문에
 * (가게가 있으면 경영 카메라 + 사장님, 파밍지면 사장님만) 플레이어 컨트롤러가 맵에 배치된
 * 표식(AHGShopCameraAnchor, AHGExplorerStart)을 보고 직접 만든다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHGGameMode();

	// 엔진 기본 동작은 접속한 플레이어에게 DefaultPawnClass를 스폰해 빙의시키는 것이다.
	// 이 게임은 컨트롤러가 Pawn을 직접 만들므로 아무것도 하지 않는다 (클래스 없이 스폰하려다 나는 오류 로그 방지).
	virtual void RestartPlayer(AController* NewPlayer) override {}
};
