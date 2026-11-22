#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HGTravelPad.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * 밟으면 다른 장소로 이동시키는 발판. 가게의 에어록과 소행성의 귀환 지점에 쓴다.
 *
 * 발판 자체는 "어디로 보내는지"만 알고, 실제 이동 연출(화면 암전, 순간 이동, 장비 교체)은
 * 플레이어 컨트롤러가 맡는다. 연출 방식이 바뀌어도 발판 코드는 손댈 필요가 없게 하려는 분리이다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGTravelPad : public AActor
{
	GENERATED_BODY()

public:
	AHGTravelPad();

	// 도착 위치와 방향. 도착 지점은 반대편 발판과 겹치지 않게 조금 떨어뜨려 둔다 (도착하자마자 되돌아가는 것 방지).
	UPROPERTY(EditAnywhere, Category = "Travel")
	FTransform Destination;

	// true면 우주(탐험 지역)로 나가는 발판, false면 가게로 돌아오는 발판.
	UPROPERTY(EditAnywhere, Category = "Travel")
	bool bLeadsToSpace = true;

	// 화면에 띄울 장소 이름.
	UPROPERTY(EditAnywhere, Category = "Travel")
	FText DestinationName;

	// 발판 색 (에어록은 청록, 귀환 지점은 금색 등).
	void SetPadColor(const FLinearColor& Color);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Travel")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Travel")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
