#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HGExplorerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

/**
 * 파밍 파트용 3인칭 캐릭터 (사장님).
 *
 * 카메라 구조:
 *   스프링 암이 컨트롤러 회전(마우스 시점)을 따라 돌고, 캐릭터 몸은 이동 방향으로 돈다.
 *   그래서 카메라를 돌려도 캐릭터가 따라 돌지 않고, 걷는 쪽으로만 몸을 튼다. 일반적인 액션 어드벤처식 3인칭이다.
 *   경영 카메라와 달리 시야각을 넓게(약 75도) 써서 가까운 거리감과 속도감을 살린다.
 *
 * 외형:
 *   아직 캐릭터 에셋이 없으므로 엔진 기본 도형(원기둥 몸통 + 구 머리)으로 자리만 표시한다.
 *   나중에 스켈레탈 메시가 생기면 기본 Mesh 컴포넌트에 넣고 PlaceholderBody/Head를 숨기면 된다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGExplorerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHGExplorerCharacter();

	// 이동 입력. X = 오른쪽, Y = 앞쪽. 카메라(컨트롤러)의 수평 방향 기준이다.
	void Move(const FVector2D& Input);

	// 시점 입력. X = 좌우 회전, Y = 상하 회전.
	void Look(const FVector2D& Input);

	// 탐험 장비(헬멧)를 보이거나 숨긴다. 가게 안에서는 앞치마 차림, 우주에서는 우주복 차림이다.
	void SetSuitVisible(bool bVisible);

	/**
	 * 행성 중력을 켠다. 이후 매 프레임 중력 방향을 "내 위치 -> Center"로 바꾼다.
	 * 캐릭터 이동 컴포넌트(UE 5.4부터)가 임의 중력 방향을 지원하므로, 방향만 바꿔 주면 걷기, 점프, 경사 판정이
	 * 그 방향 기준으로 동작한다. 몸이 기울어진 지면에 맞춰 서는 것도 이동 컴포넌트가 처리한다.
	 */
	void SetPlanetGravity(const FVector& Center, float GravityScale);

	bool HasPlanetGravity() const { return bPlanetGravity; }

	/**
	 * 중력 기준 좌표계. 이 회전을 적용하면 "중력 기준의 위(Z)"가 월드에서 지표 바깥 방향이 된다.
	 *
	 * 매 프레임 바뀐 만큼만 이어서 돌리기 때문에(평행 이동) 행성 어디를 걸어도 기준이 갑자기 뒤집히지 않는다.
	 * "아래 방향에서 바로 계산"하면 행성 반대편(남극)에서 기준 축이 180도 튀는 특이점이 생긴다.
	 * 중력이 없으면(정거장) 항등 회전이다.
	 */
	FQuat GetGravityFrame() const { return GravityFrame; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderHead;

	UPROPERTY(VisibleAnywhere, Category = "Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderHelmet;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float FieldOfView = 75.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float ArmLength = 420.f;

	// 마우스 감도 배율.
	UPROPERTY(EditAnywhere, Category = "Camera")
	float LookSensitivity = 1.f;

	virtual void BeginPlay() override;

private:
	bool bPlanetGravity = false;
	FVector PlanetCenter = FVector::ZeroVector;
	FQuat GravityFrame = FQuat::Identity;

	void UpdatePlanetGravity();
};
