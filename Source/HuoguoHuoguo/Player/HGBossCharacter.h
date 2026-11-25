#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HGBossCharacter.generated.h"

class UStaticMeshComponent;

/**
 * 사장님 캐릭터의 공통 몸체. 경영 파트(AHGShopBossCharacter)와 탐험 파트(AHGExplorerCharacter)가 상속한다.
 *
 * 공통으로 갖는 것: 충돌 캡슐, 걷기 이동, 자리 표시 외형, "카메라 기준" 이동 입력.
 * 파트마다 다른 것: 카메라(아이소 / 3인칭), 중력(평면 / 구형), 시점 조작. 이것들은 하위 클래스가 정한다.
 *
 * 카메라 기준 이동:
 *   이동 입력(X = 오른쪽, Y = 앞쪽)을 컨트롤러 회전(= 카메라 방향)의 수평 성분 기준으로 바꾼다.
 *   그래서 카메라를 어느 방향으로 돌려 놓아도 W는 언제나 "화면 위쪽(카메라가 보는 쪽)"으로 걷는다.
 *   "수평"의 기준은 GetMoveFrame()이다. 평면에서는 항등 회전이고, 행성 위에서는 중력 기준 좌표계다.
 */
UCLASS(Abstract)
class HUOGUOHUOGUO_API AHGBossCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHGBossCharacter();

	// 이동 입력. X = 오른쪽, Y = 앞쪽. 카메라(컨트롤러)의 수평 방향 기준이다.
	void Move(const FVector2D& Input);

	// 탐험 장비(헬멧)를 보이거나 숨긴다. 가게 안에서는 앞치마 차림, 우주에서는 우주복 차림이다.
	void SetSuitVisible(bool bVisible);

	/**
	 * 이동 입력의 "수평"을 정하는 좌표계. 이 회전을 적용하면 기준 좌표계의 위(Z)가 월드에서 "위"가 된다.
	 * 기본은 항등 회전(월드 Z가 위). 구형 중력을 쓰는 하위 클래스가 바꾼다.
	 */
	virtual FQuat GetMoveFrame() const { return FQuat::Identity; }

protected:
	virtual void BeginPlay() override;

	// 외형 축 둘. 보이는 부품(자리 표시 도형, 복셀)은 모두 VisualRoot 아래에 붙는다.
	//   VisualPivot: 캡슐 가운데(몸 가운데). 공중제비처럼 몸 전체를 돌릴 때 이 축의 회전을 바꾼다.
	//   VisualRoot: VisualPivot 아래 발 위치(캡슐 반높이만큼 아래). 눌림/늘어남은 이 축의 크기를 바꿔서 발이 땅에 붙은 채로 변한다.
	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<USceneComponent> VisualPivot;

	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<USceneComponent> VisualRoot;

	// 캡슐 가운데에서 발까지 거리 (VisualRoot의 높이). 부품 위치는 캡슐 가운데 기준 값에 이만큼 더해 VisualRoot 기준으로 놓는다.
	static constexpr float FeetOffset = 92.f;

	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderHead;

	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderHelmet;

private:
	// 복셀 그림체(hg.Voxel)일 때 자리 표시 도형 대신 쓰는 복셀 묶음. 헬멧은 따로 켜고 끈다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UInstancedStaticMeshComponent>> HelmetVoxels;

	// 가게 차림의 앞치마 (헬멧과 반대로 켜고 끈다).
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UInstancedStaticMeshComponent>> ApronVoxels;

	bool bVoxelBody = false;
	bool bSuitVisible = false;

	// 자리 표시 도형을 숨기고 같은 모양을 복셀로 만든다 (Debug/HGVoxelShapes).
	void BuildVoxelBody();
};
