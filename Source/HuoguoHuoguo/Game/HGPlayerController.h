#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HGPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
class AHGShopCameraPawn;
class AHGExplorerCharacter;
class AHGTravelPad;
class AHGPlanetGenerator;

// 지금 플레이어가 어느 파트를 조작하고 있는지.
UENUM(BlueprintType)
enum class EHGPlayMode : uint8
{
	Shop,    // 경영: 아이소 카메라로 가게 전체를 내려다본다.
	Explore, // 탐험: 사장님을 3인칭으로 직접 움직인다.
};

/**
 * 두 모드를 오가는 중심 컨트롤러.
 *
 * 맵 초기화:
 *   BeginPlay에서 맵에 배치된 표식을 찾아 Pawn을 만든다.
 *   - AHGShopCameraAnchor가 있으면 경영 카메라를 만들고 경영 모드로 시작한다 (정거장 맵).
 *   - 없으면 경영 화면이 없는 장소로 보고 탐험 모드로만 시작한다 (파밍지 맵).
 *   - 사장님은 AHGExplorerStart 중 도착 태그가 맞는 곳에 나타난다. 다른 맵에서 발판을 타고 왔다면 탐험 모드로 시작한다.
 *
 * 설계:
 *   경영 카메라 Pawn과 탐험 캐릭터는 월드에 동시에 존재하고, 컨트롤러가 둘 중 하나에 빙의(Possess)한다.
 *   빙의를 바꿔도 카메라가 즉시 튀지 않도록 bAutoManageActiveCameraTarget을 끄고,
 *   뷰 타깃은 SetViewTargetWithBlend로 직접 옮긴다. 이 블렌드가 "위에서 내려다보던 카메라가 사장님 등 뒤로 내려오는" 연출이다.
 *
 * 입력:
 *   Enhanced Input의 입력 액션과 매핑 컨텍스트를 에셋 없이 코드에서 만든다.
 *   모드마다 매핑 컨텍스트를 따로 두고, 모드가 바뀌면 컨텍스트를 갈아 끼워서 그 모드의 키만 살아 있게 한다.
 *   (나중에 키 설정 화면이나 디자이너용 에셋이 필요해지면 같은 이름의 에셋으로 교체할 수 있다.)
 */
UCLASS()
class HUOGUOHUOGUO_API AHGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHGPlayerController();

	// 모드 전환. 블렌드 시간 동안은 다시 전환하지 않는다. 경영 화면이 없는 맵에서는 경영 모드로 바꾸지 않는다.
	void SetPlayMode(EHGPlayMode NewMode);

	EHGPlayMode GetPlayMode() const { return PlayMode; }

	// 이동 발판이 호출한다. 화면을 어둡게 한 뒤 도착 태그를 GameInstance에 맡기고 목적지 맵을 연다.
	void RequestTravel(AHGTravelPad* Pad);

	// 개발용 콘솔 명령 (~ 키로 콘솔을 열거나 실행 인자 -ExecCmds로 넣는다).
	// HGMode shop | explore : 모드 전환
	UFUNCTION(Exec)
	void HGMode(const FString& Mode);

	// HGTravel <맵 이름 일부> : 이 맵에 있는 발판 중 목적지 맵 경로에 그 글자가 들어간 것을 밟은 것처럼 이동
	// 예) HGTravel Asteroid, HGTravel Station. 인자가 없으면 첫 번째 발판을 쓴다.
	UFUNCTION(Exec)
	void HGTravel(const FString& Where);

	// HGSimKey W 1.5 : 키를 누른 것처럼 엔진 입력 처리에 넣고, 뗄 때 이동 결과를 로그로 남긴다.
	// OS 입력을 보내는 것이 아니라 PlayerController::InputKey에 직접 넣으므로 Enhanced Input 매핑부터 그대로 거친다.
	UFUNCTION(Exec)
	void HGSimKey(const FString& KeyName, float HoldSeconds = 1.f);

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

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/**
	 * 시점 회전 처리. 행성 위에서는 중력 기준 좌표계 안에서 처리한다.
	 *
	 * 기본 구현은 월드 Z축을 "위"로 보고 Pitch를 제한하고 Roll을 버린다. 행성 옆면에 서면 월드 Z가 옆쪽이라
	 * 시점이 엉뚱하게 꺾인다. 그래서 시점을 중력 좌표계로 옮겨 같은 처리를 한 뒤 다시 월드로 돌려놓는다.
	 * 또 행성 위를 걸으면 "위"가 조금씩 돌아가므로, 그만큼 시점도 함께 돌려서 지평선이 늘 수평으로 보이게 한다.
	 */
	virtual void UpdateRotation(float DeltaTime) override;

	// 경영 -> 탐험 / 탐험 -> 경영 카메라 블렌드 시간(초).
	UPROPERTY(EditAnywhere, Category = "Mode")
	float ModeBlendTime = 1.1f;

	// 블렌드 곡선의 가파름. 클수록 출발과 도착이 더 천천히, 가운데가 더 빠르다.
	UPROPERTY(EditAnywhere, Category = "Mode")
	float ModeBlendExp = 2.5f;

	// 이동 발판 암전 시간(초). 어두워지는 시간과 밝아지는 시간에 각각 쓴다.
	UPROPERTY(EditAnywhere, Category = "Mode")
	float TravelFadeTime = 0.35f;

private:
	// 입력 액션과 매핑 컨텍스트. UPROPERTY로 잡아 두어야 가비지 컬렉션에 지워지지 않는다.
	UPROPERTY() TObjectPtr<UInputMappingContext> CommonContext;
	UPROPERTY() TObjectPtr<UInputMappingContext> ShopContext;
	UPROPERTY() TObjectPtr<UInputMappingContext> ExploreContext;

	UPROPERTY() TObjectPtr<UInputAction> ToggleModeAction;
	UPROPERTY() TObjectPtr<UInputAction> ShopPanAction;
	UPROPERTY() TObjectPtr<UInputAction> ShopZoomAction;
	UPROPERTY() TObjectPtr<UInputAction> ShopRotateAction;
	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	UPROPERTY() TObjectPtr<UInputAction> JumpAction;
	UPROPERTY() TObjectPtr<UInputAction> SelectPlanetAction;

	UPROPERTY() TObjectPtr<AHGShopCameraPawn> ShopPawn;
	UPROPERTY() TObjectPtr<AHGExplorerCharacter> Explorer;

	// 행성 표면 맵이면 그 맵의 행성 생성기. 정거장에서는 nullptr.
	UPROPERTY() TObjectPtr<AHGPlanetGenerator> Planet;

	// 지난 프레임의 중력 기준 좌표계. 이번 프레임과의 차이만큼 시점을 같이 돌리는 데 쓴다.
	FQuat LastGravityFrame = FQuat::Identity;
	bool bHasLastGravityFrame = false;

	EHGPlayMode PlayMode = EHGPlayMode::Shop;
	bool bModesInitialized = false;
	bool bTraveling = false;
	double LastModeSwitchTime = -100.0;
	FTimerHandle TravelTimer;
	FTimerHandle SimKeyTimer;
	int32 MoveEventCount = 0;

	// 맵에 배치된 표식을 읽어 Pawn을 만들고 시작 모드를 정한다. 한 번만 동작한다.
	void InitializeFromLevel();

	void BuildInputObjects();
	void ApplyMappingContexts();

	// 화면 위쪽에 계속 떠 있는 조작 안내. 모드가 바뀔 때마다 내용을 바꾼다.
	void UpdateControlsHint() const;

	// 정거장 화면에 계속 떠 있는 "접근 중인 행성" 목록. 고른 행성 앞에 표시가 붙는다. 정거장이 아니면 지운다.
	void UpdatePlanetBoard() const;

	void OnSelectPlanet(const FInputActionValue& Value);

	void OnToggleMode(const FInputActionValue& Value);
	void OnShopPan(const FInputActionValue& Value);
	void OnShopZoom(const FInputActionValue& Value);
	void OnShopRotate(const FInputActionValue& Value);
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);

	void ShowMessage(const FString& Text, const FColor& Color = FColor(244, 178, 62)) const;
};
