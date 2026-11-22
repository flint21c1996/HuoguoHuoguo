#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "HGGameInstance.generated.h"

/**
 * 맵을 옮겨 다녀도 유지되는 게임 전체 상태.
 *
 * 맵을 열면(OpenLevel) 그 맵의 액터, 게임 모드, 플레이어 컨트롤러는 모두 새로 만들어진다.
 * GameInstance만 게임이 켜져 있는 동안 하나로 유지되므로, "다음 맵에서 어디에 나타날지" 같은
 * 맵 사이에 넘겨야 할 정보는 여기에 둔다. 나중에 재료 인벤토리, 돈, 날짜도 여기(또는 서브시스템)로 온다.
 */
UCLASS()
class HUOGUOHUOGUO_API UHGGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// 이동 발판이 맵을 열기 직전에 도착 태그를 맡겨 둔다.
	void SetPendingArrival(FName ArrivalTag) { PendingArrivalTag = ArrivalTag; }

	// 새 맵의 컨트롤러가 한 번 꺼내 쓴다. 꺼내면 비워진다. 맡겨 둔 것이 없으면 NAME_None.
	FName ConsumePendingArrival()
	{
		const FName Tag = PendingArrivalTag;
		PendingArrivalTag = NAME_None;
		return Tag;
	}

private:
	FName PendingArrivalTag = NAME_None;
};
