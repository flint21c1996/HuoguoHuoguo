#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
 * 경영 파트에서 사장님이 배치하는 가구의 공통 부모.
 *
 * 외형은 엔진 기본 도형을 조합한 "부품"으로 만든다. 하위 클래스가 생성자에서 AddPart로 부품을 붙인다.
 * 나중에 아트 메시가 생기면 이 클래스를 상속한 블루프린트(BP_Table 등)에서 메시만 바꾸면 된다.
 *
 * 배치는 칸(그리드) 단위다 (FHGShopGrid). 가구마다 차지하는 칸 수 Footprint가 있고, 가구끼리는 칸이 겹치면 안 된다.
 * 벽, 카운터, 사장님과의 겹침은 부품의 실제 충돌 모양으로 따로 검사한다.
 * 가구 종류 ID(FurnitureId)는 콤보 규칙(Data/Combos.csv의 Needs 열)이 가리키는 이름이다.
 *
 * 가구 하나가 같은 클래스로 두 가지 역할을 한다.
 *   - 실제 가구: 부품이 BlockAll로 사장님과 손님을 막는다.
 *   - 유령(미리보기): 배치 모드에서 커서를 따라다니며 초록/빨강으로 칠해지는 사본.
 *     충돌은 "질의 전용 + 모든 채널 겹침"이라서 아무것도 막지 않지만, 충돌 모양은 남아 있어 겹침 검사에 쓸 수 있다.
 */
UCLASS(Abstract)
class HUOGUOHUOGUO_API AHGFurniture : public AActor
{
	GENERATED_BODY()

public:
	AHGFurniture();

	// 화면 안내와 메시지에 쓰는 이름 ("식탁", "의자").
	UPROPERTY(EditDefaultsOnly, Category = "Furniture")
	FText DisplayName;

	// 가구 종류 ID ("Table", "Chair", "Planter", "Lantern"). 콤보 규칙이 이 이름으로 가구를 가리킨다.
	UPROPERTY(EditDefaultsOnly, Category = "Furniture")
	FName FurnitureId;

	// 차지하는 칸 수 (회전 0도 기준, X = 액터 앞쪽 방향). 가구 원점(가운데)은 이 칸 사각형의 가운데에 온다.
	UPROPERTY(EditDefaultsOnly, Category = "Furniture")
	FIntPoint Footprint = FIntPoint(1, 1);

	// 놓을 때 바로 옆(상하좌우) 식탁 쪽으로 저절로 돌아앉는다 (의자).
	UPROPERTY(EditDefaultsOnly, Category = "Furniture")
	bool bFacesTable = false;

	// 유령으로 바꾼다. 스폰 직후(FinishSpawning 전)에 불러야 한 프레임도 사장님을 막지 않는다.
	void MakeGhost();
	bool IsGhost() const { return bGhost; }

	// 미리보기 색 상태를 바꾼다. 같은 상태면 아무것도 하지 않는다.
	void SetPreviewState(EHGFurniturePreview State);

	// 충돌이 있는 부품들. 배치 검사가 이 부품들의 충돌 모양으로 겹침을 확인한다.
	void GetCollidingParts(TArray<UStaticMeshComponent*>& OutParts) const;

protected:
	// 에디터에 배치했을 때와 실행 중 스폰했을 때 모두 불린다. 여기서 부품 색을 칠한다 (생성자에서는 동적 머티리얼을 만들 수 없다).
	virtual void OnConstruction(const FTransform& Transform) override;

	/**
	 * 생성자에서 부품 하나를 붙인다. 엔진 기본 도형은 1m(100유닛) 크기이고 중심이 원점이므로
	 * Location은 부품 중심의 위치(액터 원점 = 바닥 위 가구 중심), Scale 1 = 1m다.
	 * bCollide가 false면 장식(냄비 등)이라 충돌도 배치 검사도 하지 않는다.
	 */
	UStaticMeshComponent* AddPart(FName Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color, bool bCollide = true);

	// 부품 하나의 원래 색을 바꾼다 (식탁 김 색 등). 미리보기 중이면 미리보기가 끝난 뒤에 보인다.
	void SetPartColor(UStaticMeshComponent* Part, const FLinearColor& Color);

	// 엔진 기본 도형을 불러온다 (생성자 안에서만 부를 수 있다). Shape = "Cube", "Cylinder", "Sphere".
	static UStaticMesh* LoadBasicShape(const TCHAR* Shape);

	UPROPERTY(VisibleAnywhere, Category = "Furniture")
	TObjectPtr<USceneComponent> Root;

private:
	// Transient: 맵에 저장하지 않는다. 맵에 저장된 가구를 불러오면 생성자가 만든 부품 목록이 맵에 저장된 옛 목록으로
	// 덮어써져서, 나중에 추가한 부품(식탁 김 등)이 목록에서 빠지고 색이 칠해지지 않은 적이 있다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	// 부품마다의 원래 색과 충돌 여부. Parts와 같은 순서다.
	TArray<FLinearColor> PartColors;
	TArray<bool> PartCollides;

	bool bGhost = false;
	EHGFurniturePreview Preview = EHGFurniturePreview::None;

	// 지금 상태(원래 색 / 미리보기 색)로 모든 부품을 다시 칠한다.
	void Repaint();
};

/**
 * 식탁: 둥근 상판, 가운데 다리, 바닥 받침, 상판 위 훠궈 냄비(장식). 2x2칸.
 * 지름 1.56m, 상판 높이 0.78m. 이 식탁을 바라보는 바로 옆 칸의 의자가 이 식탁의 자리다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGTable : public AHGFurniture
{
	GENERATED_BODY()

public:
	AHGTable();
};

/**
 * 의자: 둥근 좌석과 등받이. 1x1칸. 앞쪽(액터의 +X)이 손님이 바라보는 방향이고 등받이는 -X 쪽에 있다.
 * 식탁 바로 옆 칸에 놓으면 저절로 식탁을 보게 돌아간다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGChair : public AHGFurniture
{
	GENERATED_BODY()

public:
	AHGChair();
};

/** 화분 (장식, 1x1칸). 식탁 둘레에 두면 콤보 재료가 된다. */
UCLASS()
class HUOGUOHUOGUO_API AHGPlanter : public AHGFurniture
{
	GENERATED_BODY()

public:
	AHGPlanter();
};

/**
 * 소스 바 (2x1칸). 훠궈 손님이 찍어 먹을 소스 그릇 셋을 올린 긴 바. 식탁 둘레에 두면 콤보 재료가 된다.
 * 회전 0도에서 액터 앞쪽(X)으로 2칸, 옆(Y)으로 1칸. R로 돌리면 세로로 2칸이 된다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGSauceBar : public AHGFurniture
{
	GENERATED_BODY()

public:
	AHGSauceBar();
};

/** 등불 (장식, 1x1칸). 식탁 둘레에 두면 콤보 재료가 된다. */
UCLASS()
class HUOGUOHUOGUO_API AHGLantern : public AHGFurniture
{
	GENERATED_BODY()

public:
	AHGLantern();
};
