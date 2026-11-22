#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HGShopCameraPawn.generated.h"

class USceneComponent;
class USpringArmComponent;
class UCameraComponent;

/**
 * 경영 파트용 카메라 Pawn. 몸체 없이 "초점"과 그 초점을 내려다보는 카메라만 있다.
 *
 * 가짜 아이소메트릭 원리:
 *   진짜 직교 투영 대신 원근 카메라를 아주 멀리(스프링 암 수천 유닛) 두고 시야각을 좁게(약 15도) 잡는다.
 *   멀리서 좁게 보면 화면 안 물체들 사이의 거리 차이가 카메라 거리에 비해 작아져서
 *   원근에 의한 크기 차이와 소실점 효과가 거의 사라지고, 결과적으로 평행 투영처럼 보인다.
 *   원근 카메라이므로 Lumen, 그림자, 포스트 프로세스가 그대로 동작하고,
 *   3인칭 카메라로 전환할 때도 위치와 시야각만 보간하면 되어 블렌드가 끊기지 않는다.
 *
 * 조작:
 *   WASD 이동은 카메라가 보는 수평 방향 기준으로 초점을 옮긴다.
 *   Q/E는 90도 단위 회전, 마우스 휠은 확대/축소. 회전과 확대는 목표값을 바꾸고 Tick에서 부드럽게 따라간다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGShopCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AHGShopCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	// 화면 기준 이동 입력. X = 오른쪽, Y = 위쪽(화면 안쪽).
	void AddPanInput(const FVector2D& Input);

	// 확대/축소 입력. 양수면 가까이, 음수면 멀리.
	void AddZoomInput(float Input);

	// 90도 단위 회전. Direction은 -1 또는 +1.
	void RotateStep(int32 Direction);

	// 초점 위치와 방향을 즉시 맞춘다 (보간 없이). 월드 생성 직후 한 번 쓴다.
	void SnapTo(const FVector& Focus, float Yaw, float PanRadius);

	// 다른 위치를 비춰야 할 때 초점을 부드럽게 옮긴다 (예: 탐험에서 돌아올 때 사장님 위치).
	void FocusOn(const FVector& Focus);

	// 지금 카메라가 바라보는 수평 방향(도). 탐험 카메라로 넘어갈 때 같은 방향을 이어받는 데 쓴다.
	float GetViewYaw() const { return CurrentYaw; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USceneComponent> FocusRoot;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	// 시야각(도). 작을수록 평행 투영에 가까워지고, 같은 화면 크기를 얻으려면 더 멀리 떨어져야 한다.
	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float FieldOfView = 15.f;

	// 내려다보는 각도(도). 음수가 아래를 보는 방향이다. 고전 아이소 게임은 대략 -30 ~ -35도이다.
	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float Pitch = -35.f;

	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float DefaultArmLength = 6000.f;

	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float MinArmLength = 3000.f;

	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float MaxArmLength = 11000.f;

	// 휠 한 칸당 거리 변화 비율. 거리에 비례해서 바꿔야 멀리서도 가까이서도 같은 느낌으로 확대된다.
	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float ZoomStepRatio = 0.12f;

	// 기본 거리에서의 이동 속도(유닛/초). 실제 속도는 현재 거리에 비례해서 커진다.
	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float PanSpeed = 1200.f;

	// 회전, 확대, 초점 이동이 목표값을 따라가는 빠르기. 클수록 빨리 붙는다.
	UPROPERTY(EditAnywhere, Category = "Camera|Iso")
	float InterpSpeed = 8.f;

private:
	FVector2D PendingPan = FVector2D::ZeroVector;
	float TargetYaw = 45.f;
	float CurrentYaw = 45.f;
	float TargetArmLength = 6000.f;
	FVector PanCenter = FVector::ZeroVector;
	float PanRadius = 1500.f;
	FVector FocusTarget = FVector::ZeroVector;

	void ApplyBoomRotation();
};
