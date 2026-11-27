#include "Player/HGBossAnimInstance.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Player/HGBossCharacter.h"
#include "Player/HGExplorerCharacter.h"

void UHGBossAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwnerBoss = Cast<AHGBossCharacter>(TryGetPawnOwner());
}

void UHGBossAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!OwnerBoss)
	{
		// 블루프린트 편집기 미리보기처럼 사장님이 없을 때는 서 있는 값.
		OwnerBoss = Cast<AHGBossCharacter>(TryGetPawnOwner());
		if (!OwnerBoss)
		{
			Speed = VerticalSpeed = 0.f;
			IsMoving = IsInAir = IsFloating = false;
			GravityUp = FVector::UpVector;
			return;
		}
	}

	// 속도를 캐릭터의 위 성분과 발밑 평면 성분으로 나눈다.
	GravityUp = OwnerBoss->GetMoveFrame().GetUpVector();
	const FVector Velocity = OwnerBoss->GetVelocity();
	VerticalSpeed = FVector::DotProduct(Velocity, GravityUp);
	Speed = (Velocity - GravityUp * VerticalSpeed).Size();

	const UCharacterMovementComponent* Move = OwnerBoss->GetCharacterMovement();
	const bool bAccelerating = Move && !Move->GetCurrentAcceleration().IsNearlyZero();
	IsMoving = Speed > 3.f && bAccelerating;
	IsInAir = Move && Move->IsFalling();

	const AHGExplorerCharacter* Explorer = Cast<AHGExplorerCharacter>(OwnerBoss);
	IsFloating = Explorer && Explorer->IsFloating();
}
