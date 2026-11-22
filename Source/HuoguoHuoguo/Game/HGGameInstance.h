#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Planet/HGPlanetTypes.h"
#include "HGGameInstance.generated.h"

/**
 * 정거장 가게에 놓인 가구 하나의 기록.
 * 가구 클래스는 경영 파트(Shop/) 것이지만 공통 폴더가 경영 파트를 include하지 않도록 AActor 클래스로 들고 있는다.
 */
USTRUCT()
struct FHGPlacedFurniture
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<AActor> Class;

	// 바닥 위 가구 중심 위치.
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float Yaw = 0.f;
};

/**
 * 맵을 옮겨 다녀도 유지되는 게임 전체 상태.
 *
 * 맵을 열면(OpenLevel) 그 맵의 액터, 게임 모드, 플레이어 컨트롤러는 모두 새로 만들어진다.
 * GameInstance만 게임이 켜져 있는 동안 하나로 유지되므로, "다음 맵에서 어디에 나타날지",
 * "지금 어느 행성으로 내려가는지" 같은 맵 사이에 넘겨야 할 정보는 여기에 둔다.
 * 나중에 재료 인벤토리, 돈, 날짜도 여기(또는 서브시스템)로 온다.
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

	// 지금 궤도 주기에 정거장으로 접근 중인 행성 목록. 처음 부를 때 만든다.
	const TArray<FHGPlanetSeed>& GetApproachingPlanets();

	// 궤도 주기를 하나 넘긴다. 접근 행성 목록이 새로 바뀐다. (나중에 하루가 끝날 때 부른다.)
	void AdvanceOrbitCycle();

	int32 GetOrbitCycle() const { return OrbitCycle; }

	// 다이빙할 행성을 고른다. Index는 GetApproachingPlanets의 순서. 범위를 벗어나면 false.
	bool SelectPlanet(int32 Index);

	int32 GetSelectedPlanetIndex() const { return SelectedPlanetIndex; }

	// 고른 행성. 목록이 비어 있으면 nullptr.
	const FHGPlanetSeed* GetSelectedPlanet();

	// 행성 표면 맵이 읽는다. 정거장에서 다이빙해 왔다면 고른 행성, 아니면(표면 맵을 바로 열었으면) unset.
	const TOptional<FHGPlanetSeed>& GetDivePlanet() const { return DivePlanet; }

	// 다이빙 직전에 정거장 쪽 컨트롤러가 부른다. 고른 행성을 표면 맵으로 넘길 값으로 고정한다.
	void CommitDiveToSelectedPlanet();

	// 정거장 가구 배치 기록. 경영 파트가 가구를 놓거나 없앨 때마다 통째로 덮어쓰고, 정거장 맵을 다시 열 때 읽는다.
	// 아직 한 번도 기록하지 않았으면 HasShopLayout()이 false다 (그때는 맵에 배치된 가구가 처음 배치).
	bool HasShopLayout() const { return bShopLayoutSaved; }
	const TArray<FHGPlacedFurniture>& GetShopLayout() const { return ShopLayout; }
	void SetShopLayout(const TArray<FHGPlacedFurniture>& Layout)
	{
		ShopLayout = Layout;
		bShopLayoutSaved = true;
	}

	// 개발 명령 HGSeq가 아직 실행하지 않은 명령들. 맵을 옮겨도 남아서 새 맵의 컨트롤러가 이어서 실행한다.
	TArray<FString>& GetDevSequence() { return DevSequence; }

private:
	UPROPERTY()
	TArray<FHGPlacedFurniture> ShopLayout;

	TArray<FString> DevSequence;

	bool bShopLayoutSaved = false;

	FName PendingArrivalTag = NAME_None;

	// 세계 전체의 시드. 나중에 새 게임을 시작할 때 무작위로 정하고 저장 파일에 넣는다.
	int32 UniverseSeed = 20261123;
	int32 OrbitCycle = 0;
	int32 SelectedPlanetIndex = 0;
	bool bPlanetsBuilt = false;
	TArray<FHGPlanetSeed> ApproachingPlanets;
	TOptional<FHGPlanetSeed> DivePlanet;
};
