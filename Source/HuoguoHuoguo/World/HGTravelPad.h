#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HGTravelPad.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * 밟으면 다른 맵으로 이동시키는 발판. 정거장의 에어록, 소행성의 귀환 지점에 배치한다.
 *
 * 발판은 "어느 맵의 어느 시작점으로 보내는지"만 갖고 있고, 화면 암전과 맵 열기는 플레이어 컨트롤러가 맡는다.
 * 연출 방식이 바뀌어도 발판과 맵 배치는 손댈 필요가 없게 하려는 분리이다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGTravelPad : public AActor
{
	GENERATED_BODY()

public:
	AHGTravelPad();

	// 도착할 맵. 소프트 참조라서 이 맵을 열 때 도착 맵까지 함께 메모리에 올라가지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Travel")
	TSoftObjectPtr<UWorld> DestinationLevel;

	// 도착 맵에서 이 태그를 가진 AHGExplorerStart 위치에 나타난다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Travel")
	FName ArrivalTag = TEXT("Default");

	// 이동할 때 화면에 띄울 장소 이름.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Travel")
	FText DestinationName;

	// 발판 색 (에어록은 청록, 귀환 지점은 금색 등).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Travel")
	FLinearColor PadColor = FLinearColor(0.25f, 0.80f, 0.88f);

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Travel")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Travel")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
