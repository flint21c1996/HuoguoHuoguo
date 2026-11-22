#include "Shop/HGShopBossCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

AHGShopBossCharacter::AHGShopBossCharacter()
{
	// 확대/축소 보간에만 Tick을 쓴다.
	PrimaryActorTick.bCanEverTick = true;

	// 가게 안에서는 조금 천천히 걷는다. 넓은 행성보다 좁은 공간이라 같은 속도면 너무 빨라 보인다.
	GetCharacterMovement()->MaxWalkSpeed = 420.f;

	// 스프링 암: 사장님에서 카메라까지의 "막대". 길이 = 카메라 거리, 회전 = 컨트롤러 회전(Yaw 자유, Pitch 고정).
	// 벽 같은 물체에 카메라가 끌려 들어오면 아이소 시점이 깨지므로 충돌 검사를 끈다.
	// 벽이 사장님을 가리는 문제는 나중에 벽을 낮추는 방식(심즈식 컷어웨이)으로 해결한다.
	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(GetCapsuleComponent());
	Boom->bUsePawnControlRotation = true;
	Boom->bDoCollisionTest = false;
	Boom->TargetArmLength = DefaultArmLength;

	// 위치 지연: 사장님이 걷기 시작하거나 멈출 때 카메라가 살짝 늦게 따라와서 화면이 덜 흔들린다.
	// 회전 지연: 마우스 회전과 Q/E 90도 회전이 뚝 끊기지 않고 미끄러지듯 돈다.
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 6.f;
	Boom->bEnableCameraRotationLag = true;
	Boom->CameraRotationLagSpeed = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(FieldOfView);

	TargetArmLength = DefaultArmLength;
}

void AHGShopBossCharacter::AddZoomInput(float Input)
{
	// 거리에 비례한 배율로 바꾼다. 고정 값으로 바꾸면 가까울 때는 너무 크게, 멀 때는 너무 작게 변한다.
	const float Factor = 1.f - Input * ZoomStepRatio;
	TargetArmLength = FMath::Clamp(TargetArmLength * Factor, MinArmLength, MaxArmLength);
}

void AHGShopBossCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, TargetArmLength, DeltaSeconds, ZoomInterpSpeed);
}
