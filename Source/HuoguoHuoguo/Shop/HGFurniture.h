#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shop/HGFurnitureData.h"
#include "HGFurniture.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

// 배치 모드에서 가구가 어떻게 보이는지. None이 아니면 몸체 색을 덮어 칠한다.
UENUM()
enum class EHGFurniturePreview : uint8
{
	None,     // 원래 색
	Valid,    // 놓을 수 있는 자리 (초록)
	Invalid,  // 놓을 수 없는 자리 (빨강)
	Hover     // 커서가 가리키는 이미 놓인 가구 (주황, 클릭하면 집는다)
};

/**
 * 경영 파트에서 사장님이 배치하는 가구. 모든 가구가 이 클래스 하나다.
 *
 * 무슨 가구인지는 데이터가 정한다: FurnitureId = `Data/Furniture.csv`(DT_Furniture)의 행 이름.
 * OnConstruction(에디터에 놓을 때, 실행 중 스폰할 때)이 그 행을 읽어
 *   - 이름, 분류(식탁/의자/장식), 칸 크기, 보너스, 꼬리표를 액터에 담고
 *   - 분류 + Style(+ 식탁은 Pot)로 엔진 기본 도형을 조합해 생김새를 만든다 (BuildParts).
 * 그래서 새 가구는 CSV에 한 줄을 더하면 생긴다. 블루프린트 아트가 생기면 행의 ActorClass로 바꾼다.
 *
 * 가구 하나가 같은 클래스로 두 가지 역할을 한다.
 *   - 실제 가구: 충돌 부품이 BlockAll로 사장님을 막는다.
 *   - 유령(미리보기): 배치 모드에서 커서를 따라다니며 초록/빨강으로 칠해지는 사본.
 *     충돌은 "질의 전용 + 모든 채널 겹침"이라서 아무것도 막지 않지만, 충돌 모양은 남아 있어 겹침 검사에 쓸 수 있다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGFurniture : public AActor
{
	GENERATED_BODY()

public:
	AHGFurniture();

	// 가구 종류 (DT_Furniture 행 이름). 맵에 놓은 가구는 에디터 디테일 패널에서 목록으로 고른다.
	UPROPERTY(EditAnywhere, Category = "Furniture", meta = (GetOptions = "GetFurnitureIdOptions"))
	FName FurnitureId;

	UFUNCTION()
	TArray<FName> GetFurnitureIdOptions() const;

	// 데이터에서 읽은 값. 행을 찾지 못했으면 기본값(1x1 장식, 이름 = ID).
	bool HasData() const { return bHasData; }
	const FHGFurnitureRow& GetData() const { return Data; }
	FText GetDisplayName() const;
	EHGFurnitureCategory GetCategory() const { return Data.Category; }
	bool IsTable() const { return Data.Category == EHGFurnitureCategory::Table; }
	bool IsChair() const { return Data.Category == EHGFurnitureCategory::Chair; }
	FIntPoint GetFootprint() const { return Data.GetFootprint(); }
	const TArray<FName>& GetTags() const { return Tags; }

	// 놓을 때 바로 옆(상하좌우) 식탁 쪽으로 저절로 돌아앉는다 (의자).
	bool FacesTable() const { return IsChair(); }

	// 유령으로 바꾼다. SpawnActorDeferred 뒤 FinishSpawning 전에 부르면 부품이 처음부터 유령 충돌로 만들어진다.
	void MakeGhost();
	bool IsGhost() const { return bGhost; }

	// 미리보기 색 상태를 바꾼다. 같은 상태면 아무것도 하지 않는다.
	void SetPreviewState(EHGFurniturePreview State);

	// 충돌이 있는 부품들. 배치 검사가 이 부품들의 충돌 모양으로 겹침을 확인한다.
	void GetCollidingParts(TArray<UStaticMeshComponent*>& OutParts) const;

	// 냄비 위 김을 켜고 끈다 (식탁만 김 부품이 있다). 켤 때 색을 정한다.
	void SetSteam(bool bOn, const FLinearColor& Color = FLinearColor::White);

protected:
	// 행을 읽어 데이터를 담고 부품을 새로 만든다. 에디터에서 옮기거나 ID를 바꿀 때마다 다시 불린다.
	virtual void OnConstruction(const FTransform& Transform) override;

	// 분류와 Style로 기본 도형 부품을 만든다. 크기는 칸 수(Footprint)에 맞춘다.
	virtual void BuildParts();

	/**
	 * 부품 하나를 붙인다 (실행 중 생성, 맵에 저장되지 않음). Shape = "Cube", "Cylinder", "Sphere".
	 * 엔진 기본 도형은 1m(100유닛) 크기이고 중심이 원점이므로, Location은 부품 중심(액터 원점 = 바닥 위 가구 가운데), Scale 1 = 1m.
	 * bCollide가 false면 장식(냄비, 김 등)이라 충돌도 배치 검사도 하지 않는다.
	 */
	UStaticMeshComponent* AddPart(const TCHAR* Shape, const FVector& Location, const FVector& Scale, const FLinearColor& Color,
		bool bCollide = true, const FRotator& Rotation = FRotator::ZeroRotator);

	// 부품 하나의 원래 색을 바꾼다 (식탁 김 색 등). 미리보기 중이면 미리보기가 끝난 뒤에 보인다.
	void SetPartColor(UStaticMeshComponent* Part, const FLinearColor& Color);

	UPROPERTY(VisibleAnywhere, Category = "Furniture")
	TObjectPtr<USceneComponent> Root;

private:
	// 분류별 생김새.
	void BuildTable(const FLinearColor& A, const FLinearColor& B);
	void BuildPot(float PotSize, const FLinearColor& PotColor);
	void BuildChair(const FLinearColor& A, const FLinearColor& B);
	void BuildDecor(const FLinearColor& A, const FLinearColor& B);

	// 만든 부품을 모두 없앤다 (다시 만들기 전).
	void ClearParts();

	UPROPERTY(Transient)
	FHGFurnitureRow Data;

	bool bHasData = false;
	TArray<FName> Tags;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SteamParts;

	// 부품마다의 원래 색과 충돌 여부. Parts와 같은 순서다.
	TArray<FLinearColor> PartColors;
	TArray<bool> PartCollides;

	bool bGhost = false;
	EHGFurniturePreview Preview = EHGFurniturePreview::None;

	// 지금 상태(원래 색 / 미리보기 색)로 모든 부품을 다시 칠한다.
	void Repaint();

	// 부품 충돌을 지금 상태(실제 / 유령)에 맞춘다.
	void ApplyCollision(UStaticMeshComponent* Part, bool bCollide) const;
};

/**
 * 맵 에셋(L_Station)에 이미 놓여 있는 식탁/의자 액터의 클래스. 새로 쓰지 않는다.
 * 칸 배치 이전에 이 클래스로 맵에 놓았기 때문에 맵을 불러오려면 남아 있어야 한다. 기본 ID만 다르다.
 * 게임을 시작하면 경영 파트가 이 액터들을 ID와 자리만 읽고 AHGFurniture로 다시 만든다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGTable : public AHGFurniture
{
	GENERATED_BODY()

public:
	AHGTable();
};

UCLASS()
class HUOGUOHUOGUO_API AHGChair : public AHGFurniture
{
	GENERATED_BODY()

public:
	AHGChair();
};
