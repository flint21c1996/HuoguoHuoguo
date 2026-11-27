#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HGExplorerStart.generated.h"

class UArrowComponent;
class UCapsuleComponent;

/**
 * 탐험 캐릭터(사장님)가 나타나는 위치. 맵마다 여러 개 둘 수 있고 StartTag로 구분한다.
 *
 * 예) 정거장 맵: "Default"(가게 안), "FromPlanet"(에어록 앞, 행성에서 돌아왔을 때)
 *     행성 표면 맵에는 두지 않는다. 지형이 시드마다 달라서 AHGPlanetGenerator가 착륙 지점을 정한다.
 *
 * 이동 발판의 ArrivalTag와 같은 태그를 가진 시작점에 도착한다.
 * 에디터에서 캡슐이 보이므로 캐릭터 크기 그대로 바닥에 맞춰 배치하면 된다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGExplorerStart : public AActor
{
	GENERATED_BODY()

public:
	AHGExplorerStart();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explorer Start")
	FName StartTag = TEXT("Default");

	// true면 우주복(헬멧) 차림으로 나타난다. 우주 공간의 맵에서 켠다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explorer Start")
	bool bSpaceSuit = false;

private:
	UPROPERTY(VisibleAnywhere, Category = "Explorer Start")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Explorer Start")
	TObjectPtr<UArrowComponent> Arrow;
};
