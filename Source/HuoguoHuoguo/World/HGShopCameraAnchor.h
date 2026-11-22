#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HGShopCameraAnchor.generated.h"

class UArrowComponent;
class UBillboardComponent;

/**
 * 경영 카메라가 바라볼 기준점. 가게가 있는 맵에 하나만 배치한다.
 *
 * - 액터 위치 = 경영 카메라의 초점(가게 바닥 가운데)
 * - 액터의 Yaw = 경영 카메라의 처음 수평 방향 (화살표가 카메라가 바라보는 방향)
 * - PanRadius = 초점이 기준점에서 벗어날 수 있는 거리
 *
 * 이 액터가 없는 맵(소행성 등)은 경영 화면이 없는 장소로 취급하고, Tab 전환을 막는다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGShopCameraAnchor : public AActor
{
	GENERATED_BODY()

public:
	AHGShopCameraAnchor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop Camera", meta = (ClampMin = "0"))
	float PanRadius = 900.f;

private:
	// 에디터에서만 보이는 표시. 게임 중에는 숨겨진다.
	UPROPERTY(VisibleAnywhere, Category = "Shop Camera")
	TObjectPtr<UArrowComponent> Arrow;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
