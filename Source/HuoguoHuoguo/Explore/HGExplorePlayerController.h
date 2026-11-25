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
	void HGWarpToIngredient(int32 Index = -1);

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

	// HGWarpToRock <번호> [바라볼 번호] : 시험용. 소행성 지대의 그 소행성 위로 옮기고, 바라볼 소행성(없으면 가장 가까운 것) 쪽을 보게 한다.
	// 그대로 W + Space면 그 소행성으로 뛰어 건너가 본다. 0번 = 본 소행성.
	UFUNCTION(Exec)
	void HGWarpToRock(int32 Index, int32 Target = -1);

	// HGAutoRun <초> : 시험용. 그 시간 동안 앞으로(W) 달린다. HGSimKey는 키를 하나씩만 누르므로,
	// 달리면서 점프하는 시험은 HGAutoRun과 HGSimKey SpaceBar를 같이 쓴다.
	UFUNCTION(Exec)
	void HGAutoRun(float Seconds);

	// HGWarpToInteract <번호> : 시험용. 폐우주선의 상호작용 지점(설계도 단말들, 바깥 문, 안쪽 문 순서) 바로 앞으로 옮긴다.
	// 순서와 위치는 로그 [Derelict]. 옮긴 뒤 E(HGSimKey E)를 누르면 그 지점을 쓴다.
	UFUNCTION(Exec)
	void HGWarpToInteract(int32 Index);

	/**
	 * HGTune [이름] [값] : 손맛 맞추기용. 게임 중에 설정값(소수 UPROPERTY)을 이름으로 보거나 바꾼다.
	 *   HGTune                        -> 바꿀 수 있는 값 목록과 지금 값
	 *   HGTune CameraUpFollowSpeed    -> 그 값 보기
	 *   HGTune CameraUpFollowSpeed 2  -> 그 값 바꾸기 (게임을 다시 시작하면 원래 값으로 돌아간다)
	 * 찾는 곳: 이 컨트롤러(카메라 따라가기), 사장님(점프, 중력, 카메라 거리), 사장님 이동 컴포넌트(AirControl, MaxWalkSpeed 등).
	 * 이름의 대소문자는 가리지 않는다.
	 */
	UFUNCTION(Exec)
	void HGTune(const FString& Name = TEXT(""), const FString& Value = TEXT(""));

protected:
	virtual void BeginPlay() override;

	// 소행성 지대: 산소/제트팩 한 줄 표시, 캐릭터 안내 문구 띄우기, 산소가 바닥나면 비상 귀환.
	void UpdateHopHud();
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
	UPROPERTY() TObjectPtr<UInputAction> ZoomAction;
	UPROPERTY() TObjectPtr<UInputAction> DownAction;
	UPROPERTY() TObjectPtr<UInputAction> ScanAction;
	UPROPERTY() TObjectPtr<UInputAction> CodexAction;

	// 도감 화면 (J로 켜고 끈다, 처음 켤 때 만든다).
	UPROPERTY() TObjectPtr<class UHGCodexWidget> Codex;

	// 마지막 스캔 시각 (월드 시간).
	float LastScanTime = -100.f;

protected:
	// 스캐너: 다시 쓸 수 있을 때까지 시간(초), 퍼지는 표면 거리(cm, 작은 행성이면 표면의 약 35%), 퍼지는 데 걸리는 시간(초).
	// HGTune으로 게임 중에 바꿀 수 있다.
	UPROPERTY(EditAnywhere, Category = "Scan")
	float ScanCooldown = 6.f;

	UPROPERTY(EditAnywhere, Category = "Scan")
	float ScanRange = 8000.f;

	UPROPERTY(EditAnywhere, Category = "Scan")
	float ScanDuration = 2.2f;

private:
	UPROPERTY() TObjectPtr<UInputAction> BrakeAction;

	// 폐우주선 문/단말 (가까운 것 번호)과 상호작용 거리(cm).
	int32 NearestInteract = INDEX_NONE;
	float InteractRadius = 260.f;

	// 오른쪽 위 미니맵. 행성 맵에서만 만든다.
	UPROPERTY() TObjectPtr<UHGPlanetMinimap> Minimap;

	// 시험용 자동 달리기 남은 시간 (HGAutoRun).
	float AutoRunSeconds = 0.f;

	void CreateMinimap();
	void OnToggleMap(const FInputActionValue& Value);

	// 마우스 휠: 카메라 거리.
	void OnZoom(const FInputActionValue& Value);

	// F: 스캐너. 지금 서 있는 천체(행성 또는 밟은 소행성)의 지표를 따라 파동을 퍼뜨린다 (AHGScanWave).
	void OnScan(const FInputActionValue& Value);

	// J: 도감 켜기/끄기.
	void OnCodex(const FInputActionValue& Value);

	// 무중력 비행: Ctrl(아래 분사), Shift(브레이크) 누름/뗌.
	void OnDown(const FInputActionValue& Value);
	void OnBrake(const FInputActionValue& Value);

	// 폐우주선 문(들어가기/나가기)이나 설계도 단말을 쓴다.
	void UseInteractable(int32 Index);

	// 사장님에서 Target까지 벽에 막히지 않았는가 (폐우주선에서 벽 너머 상호작용 막기).
	bool HasLineOfSight(const FVector& Target) const;

	// 지금 캘 수 있는 가장 가까운 표식 번호 (PlayerTick에서 갱신). 없으면 INDEX_NONE.
	int32 NearestMarker = INDEX_NONE;

	// 가장 가까운 표식을 다시 찾고 안내 문구를 맞춘다.
	void UpdateHarvestTarget();

	UPROPERTY() TObjectPtr<AHGExplorerCharacter> Explorer;

	// 행성 표면 맵이면 그 맵의 행성 생성기. 평면 맵에서는 nullptr.
	UPROPERTY() TObjectPtr<AHGPlanetGenerator> Planet;

	// 지난 프레임의 카메라 기준 좌표계 (사장님 중력 기준을 부드럽게 따라간 것). 이번 프레임과의 차이만큼 시점을 같이 돌리는 데 쓴다.
	FQuat LastGravityFrame = FQuat::Identity;

	// 카메라가 사장님의 중력 기준("위")을 따라가는 빠르기 (1/초, 클수록 빨리). 3이면 약 0.33초에 차이가 63% 줄어든다.
	UPROPERTY(EditAnywhere, Category = "Camera")
	float CameraUpFollowSpeed = 3.f;

	// 확인용: 공중에서 카메라 기준이 가장 빨리 돈 속도(도/초). 착지할 때 로그 [Cam]으로 남기고 지운다.
	float MaxCameraTurnRate = 0.f;
	bool bCameraWasAirborne = false;
	bool bHasLastGravityFrame = false;

	void InitializeFromLevel();
	void BuildInputObjects();

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);
	void OnInteract(const FInputActionValue& Value);
};
