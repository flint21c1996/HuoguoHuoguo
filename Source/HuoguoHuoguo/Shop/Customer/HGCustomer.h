#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HGCustomer.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * 가게 손님 한 명의 몸 (자리 표시 도형: 원통 몸통 + 구 머리).
 *
 * 스스로 판단하지 않는다. 어디로 갈지, 언제 앉고 일어날지는 UHGCustomerDirector가 정하고,
 * 손님은 받은 길(칸 가운데 지점들)을 따라 걷거나 의자에 앉는 동작만 한다.
 * 충돌이 없다: 사장님, 가구, 발판과 부딪히지 않고 배치 검사와 커서 추적에도 걸리지 않는다.
 * 가구를 피해 가는 것은 Director가 칸 위에서 길을 찾아서 해결한다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGCustomer : public AActor
{
	GENERATED_BODY()

public:
	AHGCustomer();

	virtual void Tick(float DeltaSeconds) override;

	// 몸통 색 (종족 자리 표시).
	void SetBodyColor(const FLinearColor& Color);

	// 이 지점들을 차례로 걸어간다 (바닥 높이 지점). 걷던 길이 있으면 버린다.
	void WalkPath(const TArray<FVector>& Points);
	bool IsWalking() const { return !Path.IsEmpty(); }

	// 의자 위에 앉는다 (좌석 높이로 올라가고 Yaw 방향을 본다).
	void SitAt(const FVector& ChairLocation, float Yaw);

	// 앉아 있던 의자 칸 바닥으로 내려선다.
	void StandUp();

	// 걷는 속도 (cm/s).
	UPROPERTY(EditAnywhere, Category = "Customer")
	float WalkSpeed = 170.f;

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Torso;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;

	TArray<FVector> Path;
	bool bSeated = false;
	bool bVoxelBody = false;
	float WalkTime = 0.f;
	FVector SeatFloor = FVector::ZeroVector;
};

/**
 * 손님이 계산할 때 식탁 위로 떠오르는 "+42" 글자. 1.6초 동안 올라가다가 사라진다.
 * 엔진 기본 글꼴에는 한글이 없어서 숫자와 기호만 쓴다 (콤보 이름은 화면 메시지로 낸다).
 */
UCLASS()
class HUOGUOHUOGUO_API AHGMoneyPopup : public AActor
{
	GENERATED_BODY()

public:
	AHGMoneyPopup();

	virtual void Tick(float DeltaSeconds) override;

	void SetupText(const FString& Text, const FColor& Color);

private:
	UPROPERTY() TObjectPtr<UTextRenderComponent> Text;

	float Age = 0.f;
	static constexpr float LifeSeconds = 1.6f;
	static constexpr float RiseSpeed = 70.f;
};
