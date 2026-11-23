#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Game/HGDayTypes.h"
#include "Planet/HGPlanetTypes.h"
#include "HGGameInstance.generated.h"

class UDataTable;
struct FHGIngredientRow;

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
	UHGGameInstance();

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

	// 궤도 주기를 하나 넘긴다. 접근 행성 목록이 새로 바뀌고 다음 날 낮이 된다. 하루를 마칠 때 부른다.
	void AdvanceOrbitCycle();

	int32 GetOrbitCycle() const { return OrbitCycle; }

	// ---- 하루와 시간대 ----

	// 지금 시간대. 행성 표면 맵도 이 값을 읽는다 (다이빙 중에는 내려간 시간대 그대로이고, 돌아와야 넘어간다).
	EHGDayPhase GetDayPhase() const { return DayPhase; }

	// 몇째 날인지 (1부터). 하루 = 궤도 주기 하나.
	int32 GetDayNumber() const { return OrbitCycle + 1; }

	// "3일째 밤" 처럼 한 줄로.
	FString DescribeTime() const;

	// 문 닫을 시간에는 다이빙하지 않는다.
	bool CanDive() const { return DayPhase != EHGDayPhase::Closing; }

	// 시간대를 한 칸 넘긴다. 낮 -> 밤 -> 문 닫을 시간 -> (하루를 마치고) 다음 날 낮.
	// 다음 날로 넘어갔으면 true (접근 행성 목록이 바뀌었다).
	bool PassTime();

	// 정거장에 돌아왔을 때 경영 파트가 부른다. 다이빙하고 돌아온 것이면 시간대를 한 칸 넘기고 true.
	// 다이빙 기록이 없으면(다른 발판으로 왔거나 맵을 바로 열었으면) 아무것도 하지 않는다.
	bool FinishDive();

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

	// ---- 재료 ----

	// 재료 DataTable (처음 부를 때 불러온다). 에셋이 없으면 nullptr이고 에러 로그를 한 번 남긴다.
	const UDataTable* GetIngredientTable();

	// 재료 ID(DataTable 행 이름)로 한 줄을 찾는다. 없으면 nullptr.
	const FHGIngredientRow* FindIngredient(FName IngredientId);

	// 화면에 쓸 재료 이름. 표에 없으면 ID 그대로.
	FString GetIngredientName(FName IngredientId);

	// 가방: 재료 ID -> 개수. 맵을 옮겨도 남는다 (행성에서 캐서 정거장으로 가져간다).
	void AddToBag(FName IngredientId, int32 Count = 1);
	int32 GetBagCount(FName IngredientId) const;
	const TMap<FName, int32>& GetBag() const { return Bag; }

	// "포자버섯 3, 서리다시마 1" 처럼 한 줄로. 비었으면 "비어 있음". 순서는 DataTable 행 순서.
	FString DescribeBag();

	/**
	 * 캔 재료 표식 기록. 행성 시드별로 표식 번호(생성기가 시드로 만든 순서)를 모은다.
	 * 같은 행성에 다시 내려가면 생성기가 이 번호의 표식을 숨긴다. 행성은 시드로 매번 같은 자리에 같은 표식을 만들기 때문에
	 * "무엇을 캤는지"만 남기면 된다. 행성이 정거장을 떠나면(궤도 주기가 넘어가 목록에서 빠지면) 그 행성 기록은 지운다.
	 */
	bool IsMarkerHarvested(int32 PlanetSeed, int32 MarkerId) const;
	void MarkHarvested(int32 PlanetSeed, int32 MarkerId);
	int32 GetHarvestedCount(int32 PlanetSeed) const;

protected:
	// 재료 DataTable 경로. 기본값은 /Game/HG/Data/DT_Ingredients (Tools/GenerateGreyboxMaps.py가 Data/Ingredients.csv로 만든다).
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> IngredientTable;

private:
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedIngredientTable;

	bool bIngredientTableMissingLogged = false;

	UPROPERTY()
	TMap<FName, int32> Bag;

	TMap<int32, TSet<int32>> HarvestedByPlanet;

	// 지금 접근 목록에 없는 행성의 캔 재료 기록을 지운다.
	void ForgetDepartedPlanets();

	UPROPERTY()
	TArray<FHGPlacedFurniture> ShopLayout;

	TArray<FString> DevSequence;

	bool bShopLayoutSaved = false;

	FName PendingArrivalTag = NAME_None;

	// 세계 전체의 시드. 나중에 새 게임을 시작할 때 무작위로 정하고 저장 파일에 넣는다.
	int32 UniverseSeed = 20261123;
	int32 OrbitCycle = 0;
	EHGDayPhase DayPhase = EHGDayPhase::Day;

	// 다이빙 발판을 밟았고 아직 정거장에 돌아오지 않았다. 돌아오면 FinishDive가 시간대를 넘기고 끈다.
	bool bDiveInProgress = false;

	int32 SelectedPlanetIndex = 0;
	bool bPlanetsBuilt = false;
	TArray<FHGPlanetSeed> ApproachingPlanets;
	TOptional<FHGPlanetSeed> DivePlanet;
};
