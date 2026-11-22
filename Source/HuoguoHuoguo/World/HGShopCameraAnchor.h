#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HGShopCameraAnchor.generated.h"

class UArrowComponent;
class UBillboardComponent;

/**
 * 경영 카메라의 처음 방향. 정거장 맵에 하나만 배치한다.
 *
 * - 액터의 Yaw = 경영 카메라의 처음 수평 방향 (화살표가 카메라가 바라보는 방향)
 *   가게를 디자인한 각도(낮은 벽이 카메라 쪽에 오는 방향)로 시작하게 하려는 것이다.
 * - 카메라는 사장님을 따라가므로 위치는 쓰지 않는다. 가게 가운데에 두면 에디터에서 찾기 쉽다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGShopCameraAnchor : public AActor
{
	GENERATED_BODY()

public:
	AHGShopCameraAnchor();

private:
	// 에디터에서만 보이는 표시. 게임 중에는 숨겨진다.
	UPROPERTY(VisibleAnywhere, Category = "Shop Camera")
	TObjectPtr<UArrowComponent> Arrow;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
