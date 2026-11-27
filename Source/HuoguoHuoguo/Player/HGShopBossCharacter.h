#pragma once

#include "CoreMinimal.h"
#include "Player/HGBossCharacter.h"
#include "HGShopBossCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * 경영 파트의 사장님. 가게 안을 걸어 다니고, 카메라는 가짜 아이소메트릭 시점으로 사장님을 따라간다.
 *
 * 가짜 아이소메트릭 원리:
 *   진짜 직교 투영 대신 원근 카메라를 아주 멀리(스프링 암 수천 유닛) 두고 시야각을 좁게(약 15도) 잡는다.
 *   멀리서 좁게 보면 화면 안 물체들 사이의 거리 차이가 카메라 거리에 비해 작아져서
 *   원근에 의한 크기 차이와 소실점 효과가 거의 사라지고, 결과적으로 평행 투영처럼 보인다.
 *   원근 카메라이므로 Lumen, 그림자, 포스트 프로세스가 그대로 동작한다.
 *
 * 카메라 회전:
 *   스프링 암이 컨트롤러 회전을 따른다. 컨트롤러는 Yaw(Z축 기준 좌우)만 바꾸고 Pitch는 CameraPitch로 고정한다.
 *   위아래로 기울지 않아야 아이소 시점의 "가게를 내려다보는 판" 느낌과 가림 규칙이 유지된다.
 *   카메라 회전 지연(rotation lag)을 켜 두어서 마우스 회전과 Q/E 90도 회전이 모두 부드럽게 따라간다.
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

private:
	float TargetArmLength = 5000.f;
};
