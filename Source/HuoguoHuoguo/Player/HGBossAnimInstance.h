#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HGBossAnimInstance.generated.h"

class AHGBossCharacter;

/**
 * 사장님 애니메이션 블루프린트(ABP_HG_Boss_*)의 부모 클래스 (2026-11-28, Under One Umbrella의 UUOUPlayerAnimInstance와 같은 방식).
 *
 * 하는 일:
 *   매 프레임 캐릭터의 이동 상태를 "그 캐릭터의 위" 기준으로 계산해 변수로 내준다. 애님 그래프는 이 변수만 읽으면 된다.
 *   템플릿 애님 그래프는 월드 Z를 위로 보고 수평 속도를 구해서, 행성 옆면처럼 위가 월드 Z가 아닌 곳에서는 걷기/뛰기 판단이 틀린다.
 *   여기서는 캐릭터의 이동 기준 좌표계(AHGBossCharacter::GetMoveFrame, 행성 위에서는 중력 기준)로 나눠서 어디서나 맞다.
 *
 * 쓰는 법:
 *   환경별 애님 BP(ABP_HG_Boss_Shop, _PlanetSmall 등)의 부모가 이 클래스다. 그래프에서 Speed, VerticalSpeed, IsMoving, IsInAir, IsFloating을 읽는다.
 *   환경마다 다른 동작(무중력에서 떠 있는 자세 등)은 각 애님 BP의 그래프에서 이 값으로 고른다.
 */
UCLASS(BlueprintType, Blueprintable)
class HUOGUOHUOGUO_API UHGBossAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// 발밑 평면(캐릭터의 위에 수직인 면) 방향의 이동 속도 (cm/초). 걷기/뛰기 블렌드에 쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement", meta = (ToolTip = "발밑 평면 방향 이동 속도(cm/초)입니다. 행성 옆면에서도 캐릭터의 위 기준입니다."))
	float Speed = 0.f;

	// 캐릭터의 위 방향 속도 (cm/초, + = 올라감). 점프 오름/내림 구분에 쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement", meta = (ToolTip = "캐릭터의 위 방향 속도(cm/초)입니다. 양수면 올라갑니다."))
	float VerticalSpeed = 0.f;

	// 움직일 만큼 빠르고 입력(가속)이 있는가. 서기 -> 걷기 전환에 쓴다.
	// 이름을 템플릿 그래프의 변수(GroundSpeed, ShouldMove, IsFalling)와 겹치지 않게 했다. 부모를 바꿔도 그래프 변수와 충돌하지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement", meta = (ToolTip = "움직이는 중인지 여부입니다 (속도 3 이상 + 가속 입력)."))
	bool IsMoving = false;

	// 공중에 있는가 (점프, 낙하).
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement", meta = (ToolTip = "공중(점프/낙하)인지 여부입니다."))
	bool IsInAir = false;

	// 무중력 비행 중인가 (폐우주선 바깥). 떠 있는 자세를 고를 때 쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement", meta = (ToolTip = "무중력 비행(제트팩) 중인지 여부입니다."))
	bool IsFloating = false;

	// 캐릭터의 위 방향 (월드). 평면에서는 (0, 0, 1), 행성 위에서는 중력 반대 방향.
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	FVector GravityUp = FVector::UpVector;

	// 이 애니메이션을 가진 사장님.
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Animation|References")
	TObjectPtr<AHGBossCharacter> OwnerBoss = nullptr;
};
