#pragma once

#include "CoreMinimal.h"
#include "HGWorldLayout.generated.h"

/**
 * 월드 안에서 각 모드가 시작하는 위치를 모아 둔 구조체.
 *
 * 지금은 회색 박스 생성기(AHGGreyboxBuilder)가 이 값을 채운다.
 * 나중에 정식 레벨이 생기면 레벨에 배치한 마커 액터에서 같은 값을 읽어 오도록 바꾸면 되고,
 * 플레이어 컨트롤러는 값의 출처를 몰라도 되도록 이 구조체만 받는다.
 */
USTRUCT(BlueprintType)
struct FHGWorldLayout
{
	GENERATED_BODY()

	// 경영 카메라가 처음 바라보는 지점 (가게 바닥 가운데).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FVector ShopFocus = FVector::ZeroVector;

	// 경영 카메라의 초기 수평 방향(도). 45도면 가게를 대각선으로 내려다보는 아이소 구도가 된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float ShopYaw = 45.f;

	// 경영 카메라 초점이 가게 중심에서 벗어날 수 있는 최대 거리. 카메라가 우주 멀리 흘러가지 않게 막는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float ShopPanRadius = 1500.f;

	// 탐험 캐릭터(사장님)가 가게 안에서 서 있는 위치와 방향.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FTransform ExplorerStart;

	// 생성이 끝났는지 여부. 컨트롤러가 초기화를 미룰지 판단할 때 쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Layout")
	bool bValid = false;
};
