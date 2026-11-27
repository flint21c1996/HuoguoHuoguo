#pragma once

#include "CoreMinimal.h"
#include "Player/HGBossCharacter.h"
#include "HGShopBossCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * 경영 파트의 사장님. 가게 안을 걸어 다니고, 카메라는 아이소메트릭 시점으로 사장님을 따라간다.
 *
 * 투영 방식 (2026-11-28, 사용자 결정: 경영 = 직교, 탐험 = 원근):
 *   카메라 컴포넌트의 Projection Mode로 고른다. 기본은 직교(Orthographic). BP_HG_ShopBoss의 Camera 디테일에서 바꿀 수 있다.
 *   - 직교: 진짜 평행 투영. 멀고 가까운 물체가 같은 크기로 보인다. 확대/축소는 화면 폭(Ortho Width)을 바꾼다.
 *   - 원근: 예전 "가짜 아이소". 원근 카메라를 멀리(스프링 암 수천 유닛) 두고 시야각을 좁게(15도) 잡아 평행 투영처럼 보이게 한다.
 *     확대/축소는 카메라 거리를 바꾼다.
 *   두 방식 모두 스프링 암 길이(카메라가 가게 밖에 있는 거리)는 같다. 직교에서는 거리가 화면 크기를 바꾸지 않지만,
 *   카메라가 벽 안쪽으로 들어오면 앞쪽이 잘리므로 멀리 둔다.
 *
 * 카메라 회전:
 *   스프링 암이 컨트롤러 회전을 따른다. 컨트롤러는 Yaw(Z축 기준 좌우)만 바꾸고 Pitch는 CameraPitch로 고정한다.
 *   위아래로 기울지 않아야 아이소 시점의 "가게를 내려다보는 판" 느낌과 가림 규칙이 유지된다.
 *   카메라 회전 지연(rotation lag)을 켜 두어서 마우스 회전과 Q/E 45도 회전이 모두 부드럽게 따라간다.
 *
 * 이동:
 *   AHGBossCharacter::Move가 카메라 Yaw 기준으로 바꾸므로 W는 언제나 화면 위쪽(카메라가 보는 쪽)이다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGShopBossCharacter : public AHGBossCharacter
{
	GENERATED_BODY()

public:
	AHGShopBossCharacter();

	virtual void Tick(float DeltaSeconds) override;

	// 확대/축소 입력. 양수면 가까이, 음수면 멀리.
	void AddZoomInput(float Input);

	// 내려다보는 각도(도). 컨트롤러가 이 값으로 Pitch를 고정한다. 음수가 아래를 보는 방향이다.
	float GetCameraPitch() const { return CameraPitch; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	// 시야각(도). 작을수록 평행 투영에 가까워지고, 같은 화면 크기를 얻으려면 더 멀리 떨어져야 한다.
	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float FieldOfView = 15.f;

	// 고전 아이소 게임은 대략 -30 ~ -35도이다.
	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float CameraPitch = -35.f;

	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float DefaultArmLength = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float MinArmLength = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float MaxArmLength = 9000.f;

	// 휠 한 칸당 거리 변화 비율. 거리에 비례해서 바꿔야 멀리서도 가까이서도 같은 느낌으로 확대된다.
	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float ZoomStepRatio = 0.12f;

	// 확대/축소가 목표 거리를 따라가는 빠르기.
	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float ZoomInterpSpeed = 8.f;

	// 직교 투영일 때 화면 가로 폭(cm). 원근 15도 + 거리 5000일 때 보이던 폭(약 1300)에 맞췄다. 작을수록 확대.
	UPROPERTY(EditAnywhere, Category = "Camera|Ortho", meta = (DisplayName = "직교 화면 폭(cm)", ClampMin = "100"))
	float DefaultOrthoWidth = 1300.f;

	UPROPERTY(EditAnywhere, Category = "Camera|Ortho", meta = (DisplayName = "직교 최소 폭(cm)", ClampMin = "100"))
	float MinOrthoWidth = 650.f;

	UPROPERTY(EditAnywhere, Category = "Camera|Ortho", meta = (DisplayName = "직교 최대 폭(cm)", ClampMin = "100"))
	float MaxOrthoWidth = 2400.f;

	// 스폰할 때 확대/축소 목표를 지금 투영 방식에 맞춘다 (BP에서 Projection Mode를 바꿔도 맞게).
	virtual void PostInitializeComponents() override;

private:
	float TargetArmLength = 5000.f;
	float TargetOrthoWidth = 1300.f;

	bool IsOrthographic() const;
};
