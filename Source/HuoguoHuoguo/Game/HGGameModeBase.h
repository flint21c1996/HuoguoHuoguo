#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HGGameModeBase.generated.h"

/**
 * 경영 파트와 탐험 파트 게임 모드의 공통 부모.
 *
 * 두 파트는 맵 단위로 완전히 나뉜다. 맵의 World Settings > GameMode Override로 파트를 정한다.
 *   L_Station        -> BP_HG_ShopGameMode    (아이소 시점, 사장님이 가게 안을 걷는다)
 *   행성/환경 맵      -> BP_HG_ExploreGameMode_* (3인칭 시점, 환경별 중력)
 *
 * 사장님 스폰은 엔진 표준 흐름을 그대로 쓴다 (2026-11-28, Under One Umbrella와 같은 방식):
 *   접속 → 시작점 찾기(FindPlayerStart) → Default Pawn Class 스폰 → 빙의.
 *   어떤 사장님(캐릭터 BP)이 나올지는 게임 모드 BP의 Default Pawn Class가 정한다.
 *
 * 시작점은 엔진 PlayerStart다. Player Start Tag로 구분한다 (예: "Default", "FromPlanet").
 *   이동 발판이 맵을 열 때 주소 끝에 "#도착태그"(엔진의 Portal)를 붙이면, 엔진이 그 태그의 PlayerStart를 고른다.
 *   맞는 태그가 없거나 처음 시작이면 ChoosePlayerStart가 "Default" 태그를 먼저 고른다.
 */
UCLASS(Abstract)
class HUOGUOHUOGUO_API AHGGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHGGameModeBase();

	// 도착 태그(Portal)를 찾지 못하면 경고를 남긴다. 고르는 일 자체는 엔진과 같다.
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;

	// 태그 없이 시작할 때: "Default" 태그 PlayerStart > 아무 PlayerStart 순. 엔진 기본은 무작위라서 정거장에서 매번 다른 곳에 나올 수 있다.
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
