#pragma once

#include "CoreMinimal.h"
#include "Game/HGPlayerControllerBase.h"
#include "HGShopPlayerController.generated.h"

class AHGShopBossCharacter;
class UHGCustomerDirector;
class UHGStationLighting;
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
 *   T              시간 넘기기 (낮 -> 밤 -> 문 닫을 시간 -> 하루 마치기). 다이빙하고 돌아와도 한 칸 넘어간다.
 *   마우스 커서는 늘 보인다. 나중에 손님, 주방, UI를 클릭으로 다루기 위해서이다. 그래서 회전은 우클릭을 누르는 동안만 한다.
 *
 * 가구 배치 모드 (IMC_ShopBuild, 이동/카메라 조작은 그대로). 바닥에 배치 영역 칸 무늬가 깔린다:
 *   1 / 2 / 3      분류 고르기: 식탁 / 의자 / 장식 (가구 목록은 Data/Furniture.csv). 가구는 크기만큼 칸을 차지한다.
 *   Z / C          같은 분류 안에서 이전 / 다음 가구 (잠긴 가구도 보인다)
 *   U              손에 든 잠긴 가구를 돈이나 재료를 내고 열기 (손님 수로 열리는 가구는 저절로 열린다)
 *   새로 놓으면 가구 값을 내고, 치우면 절반을 돌려받는다. 옮기기는 공짜.
 *   좌클릭          유령 자리에 놓기. 이미 놓인 가구를 가리키고 누르면 집어 든다.
 *   R              손에 든 가구 90도 회전 (의자는 식탁 옆에 대면 저절로 식탁을 본다)
 *   X              옮기던 가구를 원래 자리로 되돌리기, 또는 가리킨 가구 치우기
 *   B              배치 모드 끝
 *   조작 안내 줄에 "이 자리에 놓으면 생길 콤보"가 나온다.
 *
 * 손님 (UHGCustomerDirector): 낮과 밤에 손님이 들어와 식탁을 바라보는 의자에 앉아 먹고, 식탁/의자 보너스와 콤보만큼 돈을 더 내고 나간다.
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

	// HGBuildSelect TableYinYang : 배치할 가구를 ID(Data/Furniture.csv의 Name)로 고른다.
	UFUNCTION(Exec)
	void HGBuildSelect(FName FurnitureId);

	// HGBuildCategory 2 : 분류를 고른다 (1 식탁, 2 의자, 3 장식. 배치 모드의 숫자 키와 같다).
	UFUNCTION(Exec)
	void HGBuildCategory(int32 Number);

	// HGVoxel 0 : 비교용. 가구를 매끈한 기본 도형으로 그린다. 1이면 복셀(기본)로 되돌린다. 손님/사장님은 새로 나타날 때 바뀐다.
	UFUNCTION(Exec)
	void HGVoxel(int32 On);

	// HGUnlock : 손에 든 잠긴 가구를 돈이나 재료를 내고 연다 (배치 모드의 U 키와 같다).
	UFUNCTION(Exec)
	void HGUnlock();

	// HGGiveMoney 500 : 시험용. 돈을 더한다 (음수면 뺀다).
	UFUNCTION(Exec)
	void HGGiveMoney(int32 Amount);

	// HGFurnitureList : 가구 목록 전체(분류, 이름, ID, 칸, 생김새, 값, 보너스, 꼬리표)를 로그로 낸다.
	UFUNCTION(Exec)
	void HGFurnitureList();

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

	// HGNextCycle : 궤도 주기를 넘겨 접근 행성 목록을 새로 뽑는다 (다음 날 낮이 된다).
	UFUNCTION(Exec)
	void HGNextCycle();

	// HGPassTime : 시간대를 한 칸 넘긴다 (T 키와 같다). 문 닫을 시간에 부르면 하루를 마치고 다음 날 낮이 된다.
	UFUNCTION(Exec)
	void HGPassTime();

	// HGRotate 90 : 카메라를 그 각도만큼 좌우로 돌린다 (우클릭 드래그를 대신하는 시험용).
	UFUNCTION(Exec)
	void HGRotate(float Degrees);

	// HGCustomers : 손님 상태, 가진 돈, 발견한 콤보 수를 로그로 낸다.
	UFUNCTION(Exec)
	void HGCustomers();

	// HGSpawnCustomer : 빈자리가 있으면 지금 바로 손님 한 명을 들인다.
	UFUNCTION(Exec)
	void HGSpawnCustomer();

	// HGCombos : 식탁마다 자리 수와 성립한 콤보를 로그로 낸다.
	UFUNCTION(Exec)
	void HGCombos();

	// HGGiveIngredient GlowCap 2 : 시험용. 가방에 재료를 넣는다 (손님이 먹는 꼬치 확인용).
	UFUNCTION(Exec)
	void HGGiveIngredient(FName IngredientId, int32 Count = 1);

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

	// 손님 흐름 (들어오기, 앉기, 먹기, 계산, 나가기).
	UPROPERTY(VisibleAnywhere, Category = "Customers")
	TObjectPtr<UHGCustomerDirector> CustomerDirector;

	// 정거장 조명을 낮/밤에 맞춘다 (밤: 해를 줄이고 가구 불빛을 켠다).
	UPROPERTY(VisibleAnywhere, Category = "Lighting")
	TObjectPtr<UHGStationLighting> StationLighting;

private:
	// 조작 안내에 마지막으로 쓴 콤보 미리보기. 바뀔 때만 안내 줄을 다시 쓴다.
	FString LastPreviewText;

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
	UPROPERTY() TObjectPtr<UInputAction> CycleFurnitureAction;
	UPROPERTY() TObjectPtr<UInputAction> UnlockFurnitureAction;
	UPROPERTY() TObjectPtr<UInputAction> PassTimeAction;

	UPROPERTY() TObjectPtr<AHGShopBossCharacter> ShopBoss;

	bool bRotateHeld = false;

	void InitializeFromLevel();

	// 지금 시간대가 밤(또는 문 닫을 시간)이면 밤 조명, 낮이면 낮 조명.
	void ApplyPhaseLighting();
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
	void OnCycleFurniture(const FInputActionValue& Value);
	void OnUnlockFurniture(const FInputActionValue& Value);
	void OnPassTime(const FInputActionValue& Value);
};
