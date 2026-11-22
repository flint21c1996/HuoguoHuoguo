#pragma once

#include "CoreMinimal.h"
#include "Game/HGPlayerControllerBase.h"
#include "HGShopPlayerController.generated.h"

class AHGShopBossCharacter;

/**
 * 경영 파트 컨트롤러 (정거장 맵).
 *
 * 맵 시작:
 *   도착 태그(행성에서 돌아왔으면 FromPlanet)에 맞는 AHGExplorerStart에 경영용 사장님을 만들어 빙의한다.
 *   카메라의 처음 방향은 AHGShopCameraAnchor의 Yaw(없으면 시작점 Yaw)다.
 *
 * 조작:
 *   WASD           사장님 이동 (카메라가 보는 방향 기준, W = 화면 위쪽)
 *   우클릭 드래그   카메라 좌우 자유 회전 (Z축 기준 Yaw만, Pitch는 고정)
 *   Q / E          카메라 90도 회전 (키보드용)
 *   마우스 휠       확대/축소
 *   1 ~ 4          다이빙할 접근 행성 선택. 에어록의 청록 다이빙 발판을 밟으면 그 행성으로 내려간다.
 *   마우스 커서는 늘 보인다. 나중에 손님, 주방, UI를 클릭으로 다루기 위해서이다. 그래서 회전은 우클릭을 누르는 동안만 한다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGShopPlayerController : public AHGPlayerControllerBase
{
	GENERATED_BODY()

public:
	// HGPlanets : 접근 중인 행성 목록을 로그와 화면에 낸다.
	UFUNCTION(Exec)
	void HGPlanets();

	// HGSelectPlanet 2 : 목록의 2번째 행성을 다이빙 목적지로 고른다 (1부터 센다, 숫자 키 1~4와 같다).
	UFUNCTION(Exec)
	void HGSelectPlanet(int32 Number);

	// HGDive [번호] : 번호를 주면 그 행성을 고르고, 다이빙 발판을 밟은 것처럼 바로 내려간다.
	UFUNCTION(Exec)
	void HGDive(int32 Number = 0);

	// HGNextCycle : 궤도 주기를 넘겨 접근 행성 목록을 새로 뽑는다.
	UFUNCTION(Exec)
	void HGNextCycle();

	// HGRotate 90 : 카메라를 그 각도만큼 좌우로 돌린다 (우클릭 드래그를 대신하는 시험용).
	UFUNCTION(Exec)
	void HGRotate(float Degrees);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// 기본 시점 처리 뒤에 Pitch를 고정값으로, Roll을 0으로 되돌린다. 마우스 입력이 어떻게 들어와도 좌우 회전만 남는다.
	virtual void UpdateRotation(float DeltaTime) override;

	virtual FString DescribeForSimKey() const override;

	// 우클릭 드래그 회전 감도 배율.
	UPROPERTY(EditAnywhere, Category = "Shop Camera")
	float RotateSensitivity = 1.f;

	// Q/E 한 번에 도는 각도.
	UPROPERTY(EditAnywhere, Category = "Shop Camera")
	float RotateStepDegrees = 90.f;

private:
	UPROPERTY() TObjectPtr<UInputMappingContext> ShopContext;
	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	UPROPERTY() TObjectPtr<UInputAction> RotateHoldAction;
	UPROPERTY() TObjectPtr<UInputAction> RotateStepAction;
	UPROPERTY() TObjectPtr<UInputAction> ZoomAction;
	UPROPERTY() TObjectPtr<UInputAction> SelectPlanetAction;

	UPROPERTY() TObjectPtr<AHGShopBossCharacter> ShopBoss;

	bool bRotateHeld = false;

	void InitializeFromLevel();
	void BuildInputObjects();

	// 정거장 화면에 계속 떠 있는 "접근 중인 행성" 목록. 고른 행성 앞에 >> 표시가 붙는다.
	void UpdatePlanetBoard() const;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnRotateHoldStarted(const FInputActionValue& Value);
	void OnRotateHoldCompleted(const FInputActionValue& Value);
	void OnRotateStep(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);
	void OnSelectPlanet(const FInputActionValue& Value);
};
