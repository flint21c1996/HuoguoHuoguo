#pragma once

#include "CoreMinimal.h"
#include "Game/HGPlayerControllerBase.h"
#include "HGExplorePlayerController.generated.h"

class AHGExplorerCharacter;
class AHGPlanetGenerator;
class UHGPlanetMinimap;

/**
 * 탐험 파트 컨트롤러 (행성 표면 맵 등).
 *
 * 맵 시작:
 *   AHGPlanetGenerator가 있으면 행성 맵이다. 지형을 먼저 만들게 한 뒤 착륙 지점(북극)에 사장님을 만들고 구형 중력을 켠다.
 *   없으면 평면 맵으로 보고 도착 태그에 맞는 AHGExplorerStart에 만든다.
 *
 * 조작: WASD 이동(카메라 기준), 마우스 시점, Space 점프, E 채집, M 미니맵 범위(근거리/행성 전체). 커서는 숨긴다.
 * 행성 맵이면 오른쪽 위에 미니맵(UHGPlanetMinimap)을 띄운다.
 * 재료 표식 가까이(HarvestRadius 안) 가면 "E 채집: <재료>" 안내가 뜨고, E를 누르면 가장 가까운 표식을 캐서 가방에 넣는다.
 * 정거장으로 돌아가는 길은 착륙 지점 옆의 금색 귀환 발판뿐이다. 경영 화면으로 바로 전환하는 키는 없다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGExplorePlayerController : public AHGPlayerControllerBase
{
	GENERATED_BODY()

public:
	// HGWarpToIngredient : 시험용. 가장 가까운 캐지 않은 재료 표식 바로 옆으로 사장님을 옮긴다.
	UFUNCTION(Exec)
	void HGWarpToIngredient();

	// HGIngredients : 이 행성의 남은 재료 표식 수와 가방 내용을 로그로 낸다.
	UFUNCTION(Exec)
	void HGIngredients();

	// HGMapInfo : 미니맵 기준 귀환 발판의 방향(지도 위쪽 = 0도, 오른쪽 = +)과 표면 거리를 로그로 낸다.
	UFUNCTION(Exec)
	void HGMapInfo();

	// HGFacePad : 시험용. 미니맵이 가리키는 귀환 발판 방향으로 카메라를 돌린다. 그 뒤 W로 걸으면 발판에 가까워져야 한다.
	UFUNCTION(Exec)
	void HGFacePad();

	// HGLook <좌우 도> <위아래 도> : 시험용. 카메라를 "위"(행성 바깥) 축으로 좌우, 카메라 오른쪽 축으로 위아래(+ = 올려다봄) 돌린다.
	// 하늘(고리 행성, 정거장, 별)을 캡처할 때 쓴다.
	UFUNCTION(Exec)
	void HGLook(float YawDegrees, float PitchDegrees);

	// HGWarpToLandmark <번호> : 시험용. 지형 볼거리(탁상 바위/아치) 하나에서 25m 떨어진 땅 위로 옮기고 그쪽을 보게 한다.
	UFUNCTION(Exec)
	void HGWarpToLandmark(int32 Index);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// 매 프레임 가까운 재료 표식을 찾아 채집 안내를 갱신한다.
	virtual void PlayerTick(float DeltaTime) override;

	// 재료를 캘 수 있는 거리(cm, 사장님 캡슐 중심에서 표식 중심까지). 캡슐 중심이 바닥에서 약 92 위라서 바로 옆에 서면 100 안팎이다.
	UPROPERTY(EditAnywhere, Category = "Harvest")
	float HarvestRadius = 220.f;

	/**
	 * 시점 회전 처리. 행성 위에서는 중력 기준 좌표계 안에서 처리한다.
	 *
	 * 기본 구현은 월드 Z축을 "위"로 보고 Pitch를 제한하고 Roll을 버린다. 행성 옆면에 서면 월드 Z가 옆쪽이라
	 * 시점이 엉뚱하게 꺾인다. 그래서 시점을 중력 좌표계로 옮겨 같은 처리를 한 뒤 다시 월드로 돌려놓는다.
	 * 또 행성 위를 걸으면 "위"가 조금씩 돌아가므로, 그만큼 시점도 함께 돌려서 지평선이 늘 수평으로 보이게 한다.
	 */
	virtual void UpdateRotation(float DeltaTime) override;

	virtual FString DescribeForSimKey() const override;

private:
	UPROPERTY() TObjectPtr<UInputMappingContext> ExploreContext;
	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	UPROPERTY() TObjectPtr<UInputAction> JumpAction;
	UPROPERTY() TObjectPtr<UInputAction> InteractAction;
	UPROPERTY() TObjectPtr<UInputAction> MapAction;

	// 오른쪽 위 미니맵. 행성 맵에서만 만든다.
	UPROPERTY() TObjectPtr<UHGPlanetMinimap> Minimap;

	void CreateMinimap();
	void OnToggleMap(const FInputActionValue& Value);

	// 지금 캘 수 있는 가장 가까운 표식 번호 (PlayerTick에서 갱신). 없으면 INDEX_NONE.
	int32 NearestMarker = INDEX_NONE;

	// 가장 가까운 표식을 다시 찾고 안내 문구를 맞춘다.
	void UpdateHarvestTarget();

	UPROPERTY() TObjectPtr<AHGExplorerCharacter> Explorer;

	// 행성 표면 맵이면 그 맵의 행성 생성기. 평면 맵에서는 nullptr.
	UPROPERTY() TObjectPtr<AHGPlanetGenerator> Planet;

	// 지난 프레임의 중력 기준 좌표계. 이번 프레임과의 차이만큼 시점을 같이 돌리는 데 쓴다.
	FQuat LastGravityFrame = FQuat::Identity;
	bool bHasLastGravityFrame = false;

	void InitializeFromLevel();
	void BuildInputObjects();

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);
	void OnInteract(const FInputActionValue& Value);
};
