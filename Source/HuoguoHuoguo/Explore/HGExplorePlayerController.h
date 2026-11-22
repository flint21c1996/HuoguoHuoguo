#pragma once

#include "CoreMinimal.h"
#include "Game/HGPlayerControllerBase.h"
#include "HGExplorePlayerController.generated.h"

class AHGExplorerCharacter;
class AHGPlanetGenerator;

/**
 * 탐험 파트 컨트롤러 (행성 표면 맵 등).
 *
 * 맵 시작:
 *   AHGPlanetGenerator가 있으면 행성 맵이다. 지형을 먼저 만들게 한 뒤 착륙 지점(북극)에 사장님을 만들고 구형 중력을 켠다.
 *   없으면 평면 맵으로 보고 도착 태그에 맞는 AHGExplorerStart에 만든다.
 *
 * 조작: WASD 이동(카메라 기준), 마우스 시점, Space 점프. 커서는 숨긴다.
 * 정거장으로 돌아가는 길은 착륙 지점 옆의 금색 귀환 발판뿐이다. 경영 화면으로 바로 전환하는 키는 없다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGExplorePlayerController : public AHGPlayerControllerBase
{
	GENERATED_BODY()

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

	virtual FString DescribeForSimKey() const override;

private:
	UPROPERTY() TObjectPtr<UInputMappingContext> ExploreContext;
	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	UPROPERTY() TObjectPtr<UInputAction> JumpAction;

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
};
