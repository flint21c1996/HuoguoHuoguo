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
 * 나중에 아트 메시가 생기면 이 클래스를 상속한 블루프린트(BP_Table 등)에서 메시만 바꾸면 되도록,
 * 배치 규칙(겹침 검사, 바닥 검사)은 부품의 실제 충돌 모양을 그대로 쓴다. 별도의 "발자국 크기" 값을 두지 않는다.
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

	// 엔진 기본 도형을 불러온다 (생성자 안에서만 부를 수 있다). Shape = "Cube", "Cylinder", "Sphere".
	static UStaticMesh* LoadBasicShape(const TCHAR* Shape);

	UPROPERTY(VisibleAnywhere, Category = "Furniture")
	TObjectPtr<USceneComponent> Root;

private:
	UPROPERTY()
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
 * 식탁: 둥근 상판, 가운데 다리, 바닥 받침, 상판 위 훠궈 냄비(장식).
 * 지름 1.56m, 상판 높이 0.78m. 의자를 상판 밑으로 밀어 넣을 수 있도록 다리와 받침만 바닥에 닿는다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGTable : public AHGFurniture
{
	GENERATED_BODY()

public:
	AHGTable();
};

/**
 * 의자: 둥근 좌석과 등받이. 앞쪽(액터의 +X)이 손님이 바라보는 방향이고 등받이는 -X 쪽에 있다.
 * 식탁을 바라보게 놓으려면 의자의 +X가 식탁 쪽을 향하도록 돌린다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGChair : public AHGFurniture
{
	GENERATED_BODY()

public:
	AHGChair();
};
