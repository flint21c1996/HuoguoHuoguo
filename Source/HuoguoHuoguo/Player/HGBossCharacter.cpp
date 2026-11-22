#include "Player/HGBossCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AHGBossCharacter::AHGBossCharacter()
{
	// 캡슐: 언리얼 캐릭터의 실제 충돌 몸체. 반지름 40, 반높이 92면 약 1.84m 키다.
	GetCapsuleComponent()->InitCapsuleSize(40.f, 92.f);

	// 몸은 컨트롤러가 아니라 이동 방향을 따라 돈다 (카메라와 몸의 회전을 분리).
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	// Pitch/Roll 회전 속도도 준다. 구형 중력에서 몸을 새 "위"에 맞춰 세워야 하기 때문이다. 평면에서는 영향이 없다.
	Move->RotationRate = FRotator(720.f, 720.f, 720.f);
	Move->MaxWalkSpeed = 520.f;
	Move->JumpZVelocity = 620.f;
	// 공중 제어: 점프 중에도 방향을 어느 정도 틀 수 있게 한다. 저중력 행성 느낌은 GravityScale로 따로 조절한다.
	Move->AirControl = 0.45f;

	// 자리 표시용 외형. 엔진 기본 도형은 1m(100유닛) 크기이므로 Scale로 크기를 맞춘다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	auto MakePart = [this](const TCHAR* Name, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(GetCapsuleComponent());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		return Part;
	};
	PlaceholderBody = MakePart(TEXT("PlaceholderBody"), FVector(0.f, 0.f, -20.f), FVector(0.85f, 0.85f, 1.3f));
	PlaceholderHead = MakePart(TEXT("PlaceholderHead"), FVector(0.f, 0.f, 70.f), FVector(0.55f));
	PlaceholderHelmet = MakePart(TEXT("PlaceholderHelmet"), FVector(0.f, 0.f, 72.f), FVector(0.78f));

	if (CylinderMesh.Succeeded())
	{
		PlaceholderBody->SetStaticMesh(CylinderMesh.Object);
	}
	if (SphereMesh.Succeeded())
	{
		PlaceholderHead->SetStaticMesh(SphereMesh.Object);
		PlaceholderHelmet->SetStaticMesh(SphereMesh.Object);
	}

	// 기본 스켈레탈 메시 컴포넌트는 아직 쓰지 않는다. 캐릭터 에셋이 생기면 여기에 넣고 자리 표시를 숨긴다.
	GetMesh()->SetVisibility(false);
}

void AHGBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 색은 런타임에 동적 머티리얼로 칠한다. 생성자에서는 머티리얼 인스턴스를 만들 수 없기 때문이다.
	HGPlaceholder::Paint(PlaceholderBody, FLinearColor(0.78f, 0.16f, 0.10f));   // 앞치마 빨강
	HGPlaceholder::Paint(PlaceholderHead, FLinearColor(0.95f, 0.72f, 0.55f));   // 피부색
	HGPlaceholder::Paint(PlaceholderHelmet, FLinearColor(0.55f, 0.85f, 0.95f)); // 헬멧 청록
	SetSuitVisible(false);
}

void AHGBossCharacter::Move(const FVector2D& Input)
{
	if (!Controller || Input.IsNearlyZero())
	{
		return;
	}

	// 카메라가 보는 방향에서 수평 성분(Yaw)만 남긴다. 위아래 시선(Pitch)은 이동 방향에 섞지 않는다.
	// 아이소 카메라처럼 아래를 비스듬히 내려다봐도 W는 바닥을 따라 "화면 위쪽"으로 간다.
	// "수평"은 GetMoveFrame() 기준이다. 시점 회전을 그 좌표계로 옮겨서 Yaw만 남긴 뒤 다시 월드로 돌려놓는다.
	const FQuat Frame = GetMoveFrame();
	const FQuat LocalView = Frame.Inverse() * Controller->GetControlRotation().Quaternion();
	const FRotator YawOnly(0.f, LocalView.Rotator().Yaw, 0.f);
	const FVector Forward = Frame.RotateVector(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X));
	const FVector Right = Frame.RotateVector(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y));

	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void AHGBossCharacter::SetSuitVisible(bool bVisible)
{
	if (PlaceholderHelmet)
	{
		PlaceholderHelmet->SetVisibility(bVisible);
	}
}
