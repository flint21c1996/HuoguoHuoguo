#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shop/Core/HGShopGrid.h"
#include "HGCustomerDirector.generated.h"

class AHGCustomer;
class AHGFurniture;
class UHGFurniturePlacer;

/**
 * 가게 손님의 흐름을 정하는 컴포넌트 (경영 컨트롤러에 붙는다). 첫 시제품이다.
 *
 * 한 손님의 방문
 *   1. 들어오기: 가게가 열려 있으면(낮, 밤. 문 닫을 시간은 아님) ArrivalInterval 간격으로 입구 칸에 손님이 나타난다.
 *      빈자리 = 식탁을 바라보는 의자(의자 바로 앞 칸이 식탁) 중 다른 손님이 잡지 않은 것. 빈자리가 없으면 오지 않는다.
 *      식탁 두 개 사이에 낀 의자는 바라보는 쪽 식탁의 자리다.
 *   2. 자리로 걷기: 칸 위에서 너비 우선 탐색으로 길을 찾는다. 가구, 카운터, 발판 칸은 피한다.
 *   3. 먹기: 의자에 앉아 식탁을 보고 EatSeconds 동안 먹는다. 그동안 식탁 냄비 위에 김이 오른다.
 *      김 색이 식탁의 콤보 수를 보여 준다: 없음 = 흰색, 1개 = 연한 금색, 2개 이상 = 진한 주황.
 *   4. 계산: 꼬치 SkewersPerCustomer개를 먹은 값을 낸다. 꼬치는 가방(지금은 가게 창고 역할)의 재료를 하나씩 쓰고,
 *      가방이 비면 기본 꼬치(BasicSkewerPrice)로 친다. 식탁 콤보 보너스(%)를 모두 더해 곱한다.
 *      식탁 위로 "+금액" 글자가 떠오르고, 화면 메시지에 꼬치와 콤보 내역이 나온다.
 *   5. 나가기: 입구 칸으로 걸어가서 사라진다.
 *   앉을 의자나 식탁이 사라지면(배치 모드에서 집거나 치우면) 계산하지 않고 바로 나간다.
 *
 * 손님은 충돌이 없다. 길은 칸 단위로 찾으므로 가구를 뚫고 지나가지 않는다 (사장님과는 겹칠 수 있다).
 */
UCLASS(ClassGroup = (Huoguo))
class HUOGUOHUOGUO_API UHGCustomerDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UHGCustomerDirector();

	// 맵 시작에 한 번 부른다 (가구 배치를 되돌린 뒤). 고정 장애물 칸을 조사하고 손님 흐름을 시작한다.
	void Start(UHGFurniturePlacer* InPlacer);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 개발용: 빈자리가 있으면 지금 바로 손님 한 명을 들인다. 결과 문장.
	FString SpawnCustomerNow();

	// 개발용: 손님들 상태, 가진 돈, 대접한 손님 수.
	FString DescribeCustomers() const;

	// 개발용: 식탁마다 자리 수와 콤보.
	FString DescribeTables() const;

	// 화면 메시지와 돈이 바뀐 것을 컨트롤러에 알린다 (컨트롤러가 정한다).
	TFunction<void(const FString&)> OnMessage;
	TFunction<void()> OnMoneyChanged;

	// 손님이 오는 평균 간격(초). 실제 간격은 이것의 0.6 ~ 1.4배에서 무작위.
	UPROPERTY(EditAnywhere, Category = "Customers")
	float ArrivalInterval = 7.f;

	// 앉아서 먹는 시간(초).
	UPROPERTY(EditAnywhere, Category = "Customers")
	float EatSeconds = 10.f;

	// 손님 한 명이 먹는 꼬치 수.
	UPROPERTY(EditAnywhere, Category = "Customers")
	int32 SkewersPerCustomer = 3;

	// 가방이 비었을 때 내는 기본 꼬치 값.
	UPROPERTY(EditAnywhere, Category = "Customers")
	int32 BasicSkewerPrice = 5;

	// 들어올 손님 클래스 (2026-11-28). 손님 BP(BP_HG_Customer)를 넣으면 거기서 정한 값으로 나타난다. 비어 있으면 C++ 기본 클래스.
	UPROPERTY(EditAnywhere, Category = "Customers|Classes")
	TSubclassOf<AHGCustomer> CustomerClass;

	// 계산할 때 식탁 위로 떠오르는 "+금액" 글자 클래스. 비어 있으면 C++ 기본 클래스.
	UPROPERTY(EditAnywhere, Category = "Customers|Classes")
	TSubclassOf<class AHGMoneyPopup> MoneyPopupClass;

private:
	enum class EVisitState : uint8
	{
		ToSeat,   // 자리로 걷는 중
		Eating,   // 앉아서 먹는 중
		Leaving   // 입구로 걷는 중
	};

	struct FVisit
	{
		TWeakObjectPtr<AHGCustomer> Customer;
		TWeakObjectPtr<AHGFurniture> Chair;
		TWeakObjectPtr<AHGFurniture> Table;
		EVisitState State = EVisitState::ToSeat;
		float Timer = 0.f;
	};

	TArray<FVisit> Visits;
	TWeakObjectPtr<UHGFurniturePlacer> Placer;

	// 카운터, 진열대, 발판처럼 가구가 아닌 것이 차지한 바닥 칸. 맵 시작에 한 번 조사한다.
	TSet<FIntPoint> StaticBlocked;

	bool bStarted = false;
	float ArrivalTimer = 2.f;
	float SteamTimer = 0.f;
	int32 ServedCount = 0;
	int32 SpawnedCount = 0;
	FRandomStream Random;

	const FHGShopGrid& GetGrid() const;
	bool IsOpen() const;

	void ComputeStaticBlocked();

	// 가구가 차지한 칸 전부.
	static void GatherOccupied(const TArray<FHGGridItem>& Items, TSet<FIntPoint>& OutOccupied);

	// Start 칸에서 Goal 칸까지 상하좌우로 걷는 가장 짧은 길. 지나갈 칸의 가운데 지점들(Start 제외, Goal 포함). 없으면 false.
	// Start와 Goal은 가구가 있어도 지나갈 수 있다 (의자 칸에 앉고, 의자 칸에서 일어나 나가므로).
	bool FindPath(const FIntPoint& Start, const FIntPoint& Goal, const TSet<FIntPoint>& Occupied, TArray<FVector>& OutPoints) const;

	bool TrySpawnCustomer(FString& OutResult);
	void SendHome(FVisit& Visit);
	void Pay(FVisit& Visit);

	// 먹는 손님이 있는 식탁에 김을 켜고, 없는 식탁은 끈다. 김 색 = 콤보 수.
	void RefreshSteam();

	void Message(const FString& Text) const;
};
