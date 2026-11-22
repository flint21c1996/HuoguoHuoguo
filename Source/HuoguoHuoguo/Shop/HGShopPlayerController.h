#pragma once

#include "CoreMinimal.h"
#include "Game/HGPlayerControllerBase.h"
#include "HGShopPlayerController.generated.h"

class AHGShopBossCharacter;
class UHGFurniturePlacer;

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
 *   B              가구 배치 모드 켜기/끄기
 *   마우스 커서는 늘 보인다. 나중에 손님, 주방, UI를 클릭으로 다루기 위해서이다. 그래서 회전은 우클릭을 누르는 동안만 한다.
 *
 * 가구 배치 모드 (IMC_ShopBuild, 이동/카메라 조작은 그대로):
 *   1 / 2          식탁 / 의자 고르기 (UHGFurniturePlacer::Catalog 순서)
 *   좌클릭          유령 자리에 놓기. 이미 놓인 가구를 가리키고 누르면 집어 든다.
 *   R              손에 든 가구 45도 회전
 *   X              옮기던 가구를 원래 자리로 되돌리기, 또는 가리킨 가구 치우기
 *   B              배치 모드 끝
 */
UCLASS()
class HUOGUOHUOGUO_API AHGShopPlayerController : public AHGPlayerControllerBase
{
	GENERATED_BODY()

public:
	AHGShopPlayerController();

	// HGBuild : 가구 배치 모드를 켜고 끈다 (B 키와 같다).
	UFUNCTION(Exec)
	void HGBuild();

	// HGBuildSelect 2 : 배치할 가구를 고른다 (1부터, 배치 모드의 숫자 키와 같다).
	UFUNCTION(Exec)
	void HGBuildSelect(int32 Number);

	// HGCursorAt -200 -100 : 시험용. 마우스 대신 이 바닥 지점을 커서로 쓴다. HGCursorClear로 실제 마우스로 돌아간다.
	UFUNCTION(Exec)
	void HGCursorAt(float X, float Y);

	UFUNCTION(Exec)
	void HGCursorClear();

	// HGFurniture : 가구 배치 기록과 지금 커서 상태를 로그로 낸다.
	UFUNCTION(Exec)
	void HGFurniture();

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

	// 정거장을 떠날 때 접근 행성 목록 메시지를 지운다 (화면 디버그 메시지는 엔진 전역이라 행성 맵까지 남는다).
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 배치 모드에서 매 프레임 커서 위치로 유령 가구를 옮긴다.
	virtual void PlayerTick(float DeltaTime) override;

	// 기본 시점 처리 뒤에 Pitch를 고정값으로, Roll을 0으로 되돌린다. 마우스 입력이 어떻게 들어와도 좌우 회전만 남는다.
	virtual void UpdateRotation(float DeltaTime) override;

	virtual FString DescribeForSimKey() const override;

	// 우클릭 드래그 회전 감도 배율.
	UPROPERTY(EditAnywhere, Category = "Shop Camera")
	float RotateSensitivity = 1.f;

	// Q/E 한 번에 도는 각도.
	UPROPERTY(EditAnywhere, Category = "Shop Camera")
	float RotateStepDegrees = 90.f;

	// 가구 배치 모드. 고를 수 있는 가구 목록, 격자 간격 등은 이 컴포넌트의 속성이다.
	UPROPERTY(VisibleAnywhere, Category = "Furniture")
	TObjectPtr<UHGFurniturePlacer> FurniturePlacer;

private:
	// 평소 경영 조작과 가구 배치 모드 조작. 배치 모드에 들어가면 컨텍스트를 통째로 바꾼다.
	UPROPERTY() TObjectPtr<UInputMappingContext> ShopContext;
	UPROPERTY() TObjectPtr<UInputMappingContext> BuildContext;
	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	UPROPERTY() TObjectPtr<UInputAction> RotateHoldAction;
	UPROPERTY() TObjectPtr<UInputAction> RotateStepAction;
	UPROPERTY() TObjectPtr<UInputAction> ZoomAction;
	UPROPERTY() TObjectPtr<UInputAction> SelectPlanetAction;
	UPROPERTY() TObjectPtr<UInputAction> ToggleBuildAction;
	UPROPERTY() TObjectPtr<UInputAction> PlaceAction;
	UPROPERTY() TObjectPtr<UInputAction> RotateFurnitureAction;
	UPROPERTY() TObjectPtr<UInputAction> RemoveFurnitureAction;
	UPROPERTY() TObjectPtr<UInputAction> SelectFurnitureAction;

	UPROPERTY() TObjectPtr<AHGShopBossCharacter> ShopBoss;

	bool bRotateHeld = false;

	void InitializeFromLevel();
	void BuildInputObjects();

	// 두 컨텍스트에 공통인 이동/카메라/배치 모드 전환 키.
	void MapCommonKeys(UInputMappingContext* Context);

	// 지금 모드(평소/배치)에 맞는 조작 안내를 띄운다.
	void UpdateControlsHint() const;

	// 정거장 화면에 계속 떠 있는 "접근 중인 행성" 목록. 고른 행성 앞에 >> 표시가 붙는다.
	void UpdatePlanetBoard() const;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnRotateHoldStarted(const FInputActionValue& Value);
	void OnRotateHoldCompleted(const FInputActionValue& Value);
	void OnRotateStep(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);
	void OnSelectPlanet(const FInputActionValue& Value);
	void OnToggleBuild(const FInputActionValue& Value);
	void OnPlace(const FInputActionValue& Value);
	void OnRotateFurniture(const FInputActionValue& Value);
	void OnRemoveFurniture(const FInputActionValue& Value);
	void OnSelectFurniture(const FInputActionValue& Value);
};
