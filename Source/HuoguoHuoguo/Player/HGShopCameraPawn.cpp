#include "Player/HGShopCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"

AHGShopCameraPawn::AHGShopCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 초점: 카메라가 바라보는 바닥 위의 한 점. Pawn을 움직이면 이 점이 움직인다.
	FocusRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FocusRoot"));
	SetRootComponent(FocusRoot);

	// 스프링 암: 초점에서 카메라까지의 "막대". 길이 = 카메라 거리, 회전 = 내려다보는 방향.
	// 경영 화면에서는 벽 같은 물체에 카메라가 끌려 들어오면 안 되므로 충돌 검사를 끈다.
	// 벽이 시야를 가리는 문제는 나중에 벽을 낮추는 방식(심즈식 컷어웨이)으로 해결한다.
	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(FocusRoot);
	Boom->bDoCollisionTest = false;
	Boom->bUsePawnControlRotation = false;
	Boom->bInheritPitch = false;
	Boom->bInheritYaw = false;
	Boom->bInheritRoll = false;
	Boom->bEnableCameraLag = false;
	Boom->TargetArmLength = DefaultArmLength;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(FieldOfView);

	// 이 Pawn은 충돌하거나 중력을 받을 몸체가 없다. 회전도 컨트롤러가 아니라 스스로 관리한다.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	SetActorEnableCollision(false);

	TargetArmLength = DefaultArmLength;
	ApplyBoomRotation();
}

void AHGShopCameraPawn::SnapTo(const FVector& Focus, float Yaw, float InPanRadius)
{
	PanCenter = Focus;
	PanRadius = InPanRadius;
	FocusTarget = Focus;
	SetActorLocation(Focus);

	TargetYaw = Yaw;
	CurrentYaw = Yaw;
	TargetArmLength = FMath::Clamp(DefaultArmLength, MinArmLength, MaxArmLength);
	Boom->TargetArmLength = TargetArmLength;
	Camera->SetFieldOfView(FieldOfView);
	ApplyBoomRotation();
}

void AHGShopCameraPawn::FocusOn(const FVector& Focus)
{
	// 초점은 바닥 높이(PanCenter의 Z)에 고정한다. 캐릭터의 허리 높이 등을 그대로 쓰면 카메라가 들썩인다.
	FVector Flat = Focus;
	Flat.Z = PanCenter.Z;
	FocusTarget = Flat;
}

void AHGShopCameraPawn::AddPanInput(const FVector2D& Input)
{
	PendingPan += Input;
}

void AHGShopCameraPawn::AddZoomInput(float Input)
{
	// 거리에 비례한 배율로 바꾼다. 고정 값으로 바꾸면 가까울 때는 너무 크게, 멀 때는 너무 작게 변한다.
	const float Factor = 1.f - Input * ZoomStepRatio;
	TargetArmLength = FMath::Clamp(TargetArmLength * Factor, MinArmLength, MaxArmLength);
}

void AHGShopCameraPawn::RotateStep(int32 Direction)
{
	// 목표 각도를 90도 격자에 맞춘다. 연타로 회전 중간에 다시 눌러도 항상 격자 위에서 멈춘다.
	const float Snapped = FMath::RoundToFloat(TargetYaw / 90.f - 0.5f) * 90.f + 45.f;
	TargetYaw = Snapped + 90.f * FMath::Sign(static_cast<float>(Direction));
}

void AHGShopCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 1) 회전: 현재 각도를 목표 각도로 부드럽게 따라가게 한다.
	CurrentYaw = FMath::FInterpTo(CurrentYaw, TargetYaw, DeltaSeconds, InterpSpeed);
	ApplyBoomRotation();

	// 2) 확대/축소
	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, TargetArmLength, DeltaSeconds, InterpSpeed);

	// 3) 이동: 입력을 카메라의 수평 방향 기준 앞/오른쪽으로 바꾼다.
	//    카메라가 회전해 있어도 W를 누르면 항상 "화면 위쪽"으로 움직이게 하려는 것이다.
	if (!PendingPan.IsNearlyZero())
	{
		const FRotator YawOnly(0.f, CurrentYaw, 0.f);
		const FVector Forward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
		const FVector2D Dir = PendingPan.GetClampedToMaxSize(1.f);

		// 멀리 있을수록 화면에 넓은 영역이 보이므로 같은 화면 비율만큼 움직이려면 더 빨리 움직여야 한다.
		const float Speed = PanSpeed * (Boom->TargetArmLength / DefaultArmLength);
		FocusTarget += (Forward * Dir.Y + Right * Dir.X) * Speed * DeltaSeconds;
		PendingPan = FVector2D::ZeroVector;
	}

	// 초점이 가게 중심에서 너무 멀어지지 않게 원 안으로 제한한다.
	FVector Offset = FocusTarget - PanCenter;
	Offset.Z = 0.f;
	if (Offset.SizeSquared() > FMath::Square(PanRadius))
	{
		FocusTarget = PanCenter + Offset.GetSafeNormal() * PanRadius;
	}

	SetActorLocation(FMath::VInterpTo(GetActorLocation(), FocusTarget, DeltaSeconds, InterpSpeed));
}

void AHGShopCameraPawn::ApplyBoomRotation()
{
	// 스프링 암은 자기 -X 방향(뒤쪽)으로 카메라를 둔다. 따라서 Pitch가 음수면 카메라는 초점의 위쪽 뒤에 놓이고 아래를 본다.
	Boom->SetRelativeRotation(FRotator(Pitch, CurrentYaw, 0.f));
}
