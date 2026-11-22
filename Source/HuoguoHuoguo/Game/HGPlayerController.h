#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/HGWorldLayout.h"
#include "HGPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
class AHGShopCameraPawn;
class AHGExplorerCharacter;
class AHGTravelPad;

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

	// 월드 생성이 끝난 뒤 게임 모드가 호출한다. 탐험 캐릭터를 만들고 경영 모드로 시작한다. 여러 번 불려도 한 번만 동작한다.
	void InitializeModes(const FHGWorldLayout& Layout);

	// 모드 전환. 블렌드 시간 동안은 다시 전환하지 않는다.
	void SetPlayMode(EHGPlayMode NewMode);

	EHGPlayMode GetPlayMode() const { return PlayMode; }

	// 이동 발판이 호출한다. 화면을 어둡게 했다가 목적지로 옮긴 뒤 다시 밝힌다.
	void RequestTravel(AHGTravelPad* Pad);

	// 개발용 콘솔 명령 (~ 키로 콘솔을 열거나 실행 인자 -ExecCmds로 넣는다).
	// HGMode shop | explore : 모드 전환
	UFUNCTION(Exec)
	void HGMode(const FString& Mode);

	// HGTravel space | shop : 발판을 밟은 것처럼 이동 (필요하면 탐험 모드로 먼저 바꾼다)
	UFUNCTION(Exec)
	void HGTravel(const FString& Where);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

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

	UPROPERTY() TObjectPtr<AHGShopCameraPawn> ShopPawn;
	UPROPERTY() TObjectPtr<AHGExplorerCharacter> Explorer;

	EHGPlayMode PlayMode = EHGPlayMode::Shop;
	bool bModesInitialized = false;
	bool bExplorerInSpace = false;
	bool bTraveling = false;
	double LastModeSwitchTime = -100.0;
	FHGWorldLayout CachedLayout;
	FTimerHandle TravelTimer;

	void BuildInputObjects();
	void ApplyMappingContexts();

	void OnToggleMode(const FInputActionValue& Value);
	void OnShopPan(const FInputActionValue& Value);
	void OnShopZoom(const FInputActionValue& Value);
	void OnShopRotate(const FInputActionValue& Value);
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpCompleted(const FInputActionValue& Value);

	void FinishTravel(FTransform Destination, bool bToSpace);
	void ShowMessage(const FString& Text, const FColor& Color = FColor(244, 178, 62)) const;
};
