#include "Explore/HGExplorerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

AHGExplorerCharacter::AHGExplorerCharacter()
{
	// 행성 위에서는 매 프레임 중력 방향을 갱신해야 한다. 중력이 없으면 Tick이 바로 끝난다.
	PrimaryActorTick.bCanEverTick = true;

	// 3인칭 카메라: 스프링 암이 컨트롤러 회전을 따라가므로 마우스로 캐릭터 주위를 돌 수 있다.
	// 충돌 검사를 켜 두어서 바위 뒤로 카메라가 들어가면 암이 줄어들어 캐릭터가 가려지지 않게 한다.
	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(GetCapsuleComponent());
	Boom->TargetArmLength = ArmLength;
	Boom->bUsePawnControlRotation = true;
	Boom->bDoCollisionTest = true;
	Boom->SocketOffset = FVector(0.f, 0.f, 60.f);
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 12.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(FieldOfView);
}

void AHGExplorerCharacter::Look(const FVector2D& Input)
{
	// 마우스 원시 입력은 위로 올릴 때 Y가 양수이다. 엔진 3인칭 템플릿과 같은 방향(마우스를 올리면 위를 봄)이
	// 되도록 Pitch 입력에는 부호를 뒤집어 넣는다.
	AddControllerYawInput(Input.X * LookSensitivity);
	AddControllerPitchInput(-Input.Y * LookSensitivity);
}

void AHGExplorerCharacter::SetPlanetGravity(const FVector& Center, float GravityScale)
{
	bPlanetGravity = true;
	PlanetCenter = Center;
	GetCharacterMovement()->GravityScale = GravityScale;

	// 처음 한 번은 현재 위치의 "위"에 맞춘 기준을 바로 만든다. 이후로는 Tick에서 변화량만 이어 붙인다.
	const FVector Up = (GetActorLocation() - PlanetCenter).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	GravityFrame = FQuat::FindBetweenNormals(FVector::UpVector, Up);
	UpdatePlanetGravity();
}

void AHGExplorerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPlanetGravity)
	{
		UpdatePlanetGravity();
	}
}

void AHGExplorerCharacter::UpdatePlanetGravity()
{
	const FVector Up = (GetActorLocation() - PlanetCenter).GetSafeNormal(UE_SMALL_NUMBER, GravityFrame.GetUpVector());

	// 지난 프레임의 위 방향에서 이번 위 방향으로 돌린 만큼만 기준 좌표계를 돌린다 (평행 이동).
	// 한 프레임 동안의 변화는 아주 작으므로 FindBetweenNormals가 불안정해지는 반대 방향(180도) 경우가 생기지 않는다.
	const FQuat Delta = FQuat::FindBetweenNormals(GravityFrame.GetUpVector(), Up);
	GravityFrame = (Delta * GravityFrame).GetNormalized();

	// 이동 컴포넌트는 같은 프레임 뒤에 틱하므로(캐릭터가 선행 조건) 바뀐 방향이 이번 이동에 바로 쓰인다.
	GetCharacterMovement()->SetGravityDirection(-Up);
}
