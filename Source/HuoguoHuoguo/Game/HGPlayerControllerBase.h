#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "HGPlayerControllerBase.generated.h"

class UInputAction;
class UInputMappingContext;
class AHGBossCharacter;
class AHGExplorerStart;
class AHGTravelPad;

/**
 * 경영/탐험 파트 플레이어 컨트롤러의 공통 부모.
 *
 * 공통으로 맡는 것:
 *   - 맵 이동: 이동 발판을 밟으면 화면을 어둡게 하고 도착 태그를 GameInstance에 맡긴 뒤 목적지 맵을 연다.
 *     다이빙 발판이면 고른 행성을 표면 맵으로 넘길 값으로 고정한다.
 *   - 맵 시작: 도착 태그 꺼내기, 시작점 찾기, 이동해 왔으면 화면 다시 밝히기.
 *   - 입력 도우미: Enhanced Input 액션과 매핑 컨텍스트를 에셋 없이 코드에서 만드는 함수.
 *   - 임시 화면 안내(조작 안내, 메시지)와 개발용 콘솔 명령.
 *
 * 파트별로 다른 것(사장님 종류, 카메라, 입력 키, 시점 처리)은 하위 클래스가 정한다.
 * 두 파트는 서로를 모른다. 둘 사이를 잇는 정보는 GameInstance에만 둔다.
 */
UCLASS(Abstract)
class HUOGUOHUOGUO_API AHGPlayerControllerBase : public APlayerController
{
	GENERATED_BODY()

public:
	// 이동 발판이 호출한다. 화면을 어둡게 한 뒤 도착 태그를 GameInstance에 맡기고 목적지 맵을 연다.
	void RequestTravel(AHGTravelPad* Pad);

	// 개발용 콘솔 명령 (~ 키로 콘솔을 열거나 실행 인자 -ExecCmds로 넣는다).
	// HGTravel <맵 이름 일부> : 이 맵에 있는 발판 중 목적지 맵 경로에 그 글자가 들어간 것을 밟은 것처럼 이동
	UFUNCTION(Exec)
	void HGTravel(const FString& Where);

	// HGSimKey W 1.5 : 키를 누른 것처럼 엔진 입력 처리에 넣고, 뗄 때 이동 결과를 로그로 남긴다.
	// OS 입력을 보내는 것이 아니라 PlayerController::InputKey에 직접 넣으므로 Enhanced Input 매핑부터 그대로 거친다.
	UFUNCTION(Exec)
	void HGSimKey(const FString& KeyName, float HoldSeconds = 1.f);

protected:
	// 이동 발판 암전 시간(초). 어두워지는 시간과 밝아지는 시간에 각각 쓴다.
	UPROPERTY(EditAnywhere, Category = "Travel")
	float TravelFadeTime = 0.35f;

	UPROPERTY() TObjectPtr<AHGBossCharacter> Boss;

	bool bTraveling = false;

	// 이동 이벤트 횟수. HGSimKey가 입력이 실제로 들어갔는지 확인하는 데 쓴다. 하위 클래스의 이동 처리에서 올린다.
	int32 MoveEventCount = 0;

	// GameInstance에 맡겨 둔 도착 태그를 꺼낸다. 이동해 온 것이 아니면 NAME_None.
	FName ConsumeArrivalTag() const;

	// 태그가 맞는 시작점 > "Default" > 아무거나 순으로 고른다. 맵에 시작점이 없으면 nullptr.
	AHGExplorerStart* FindStart(FName WantedTag) const;

	// 이동해 와서 검게 유지된 화면을 다시 밝힌다.
	void FadeInAfterTravel();

	// 매핑 컨텍스트를 이것 하나만 남기고 바꾼다.
	void ApplyMappingContext(UInputMappingContext* Context);

	// 화면 왼쪽 위 임시 메시지 (정식 UI 전까지). 로그에도 남긴다.
	void ShowMessage(const FString& Text, const FColor& Color = FColor(244, 178, 62)) const;

	// 화면 위쪽에 계속 떠 있는 조작 안내 한 줄.
	void SetControlsHint(const FString& Hint) const;

	// HGSimKey 결과 로그에 덧붙일 파트별 정보 (카메라 방향, 행성 중심 거리 등).
	virtual FString DescribeForSimKey() const { return FString(); }

	// 입력 도우미
	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType Type);

	/**
	 * 키 하나를 2D 입력의 한 방향으로 매핑한다.
	 *
	 * Enhanced Input에서 키보드 키는 1D 값(눌림 = 1)만 낸다. 이것을 2D 벡터의 한 방향으로 만들려면 수정자를 붙인다.
	 *   D: 그대로 -> (1, 0)            오른쪽
	 *   A: Negate -> (-1, 0)           왼쪽
	 *   W: Swizzle(YXZ) -> (0, 1)      앞쪽. 축을 바꿔서 X에 들어온 1을 Y로 옮긴다.
	 *   S: Swizzle 후 Negate -> (0, -1) 뒤쪽
	 * 같은 액션에 여러 키가 매핑되면 값이 합쳐지므로 W+D를 같이 누르면 (1, 1) 대각선이 된다.
	 */
	void MapDirectionalKey(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate);
	void MapWASD(UInputMappingContext* Context, const UInputAction* Action);

	// 키 하나를 Scalar 배율 수정자와 함께 매핑한다. 액션 하나에 키 여러 개를 매핑하고 키마다 다른 값을 내게 할 때 쓴다.
	void MapScaledKey(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key, float Scale);

private:
	FTimerHandle TravelTimer;
	FTimerHandle SimKeyTimer;
};
