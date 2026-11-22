#include "Explore/HGExplorerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Dev/HGPlaceholderPaint.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AHGExplorerCharacter::AHGExplorerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 캡슐: 언리얼 캐릭터의 실제 충돌 몸체. 반지름 40, 반높이 92면 약 1.84m 키다.
	GetCapsuleComponent()->InitCapsuleSize(40.f, 92.f);

	// 몸은 컨트롤러가 아니라 이동 방향을 따라 돈다 (카메라와 몸의 회전을 분리).
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->MaxWalkSpeed = 520.f;
	Move->JumpZVelocity = 620.f;
	// 공중 제어: 점프 중에도 방향을 어느 정도 틀 수 있게 한다. 저중력 행성 느낌은 GravityScale로 따로 조절한다.
	Move->AirControl = 0.45f;

	// 3인칭 카메라: 스프링 암이 컨트롤러 회전을 따라가므로 마우스로 캐릭터 주위를 돌 수 있다.
	// 충돌 검사를 켜 두어서 벽 뒤로 카메라가 들어가면 암이 줄어들어 캐릭터가 가려지지 않게 한다.
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

	// 자리 표시용 외형. 엔진 기본 도형은 1m(100유닛) 크기이므로 Scale로 크기를 맞춘다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetCapsuleComponent());
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderBody->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	PlaceholderBody->SetRelativeScale3D(FVector(0.85f, 0.85f, 1.3f));

	PlaceholderHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderHead"));
	PlaceholderHead->SetupAttachment(GetCapsuleComponent());
	PlaceholderHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderHead->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	PlaceholderHead->SetRelativeScale3D(FVector(0.55f));

	PlaceholderHelmet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderHelmet"));
	PlaceholderHelmet->SetupAttachment(GetCapsuleComponent());
	PlaceholderHelmet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderHelmet->SetRelativeLocation(FVector(0.f, 0.f, 72.f));
	PlaceholderHelmet->SetRelativeScale3D(FVector(0.78f));

	if (CylinderMesh.Succeeded())
	{
		PlaceholderBody->SetStaticMesh(CylinderMesh.Object);
	}
	if (SphereMesh.Succeeded())
	{
		PlaceholderHead->SetStaticMesh(SphereMesh.Object);
		PlaceholderHelmet->SetStaticMesh(SphereMesh.Object);
	}

	// 기본 스켈레탈 메시 컴포넌트는 아직 쓰지 않는다.
	GetMesh()->SetVisibility(false);
}

void AHGExplorerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 색은 런타임에 동적 머티리얼로 칠한다. 생성자에서는 머티리얼 인스턴스를 만들 수 없기 때문이다.
	HGPlaceholder::Paint(PlaceholderBody, FLinearColor(0.78f, 0.16f, 0.10f));   // 앞치마 빨강
	HGPlaceholder::Paint(PlaceholderHead, FLinearColor(0.95f, 0.72f, 0.55f));   // 피부색
	HGPlaceholder::Paint(PlaceholderHelmet, FLinearColor(0.55f, 0.85f, 0.95f)); // 헬멧 청록
	SetSuitVisible(false);
}

void AHGExplorerCharacter::Move(const FVector2D& Input)
{
	if (!Controller || Input.IsNearlyZero())
	{
		return;
	}

	// 카메라가 보는 수평 방향을 기준으로 앞/오른쪽을 구한다. 위아래 시선(Pitch)은 이동 방향에 섞지 않는다.
	const FRotator YawOnly(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void AHGExplorerCharacter::Look(const FVector2D& Input)
{
	// 마우스 원시 입력은 위로 올릴 때 Y가 양수이다. 엔진 3인칭 템플릿과 같은 방향(마우스를 올리면 위를 봄)이
	// 되도록 Pitch 입력에는 부호를 뒤집어 넣는다.
	AddControllerYawInput(Input.X * LookSensitivity);
	AddControllerPitchInput(-Input.Y * LookSensitivity);
}

void AHGExplorerCharacter::TeleportWithCamera(const FTransform& Destination)
{
	// 카메라 지연은 "이전 프레임 카메라 위치"에서 목표 위치로 조금씩 다가가는 방식이다.
	// 수만 유닛을 순간 이동하면 카메라가 그 거리를 날아오는 것이 보이므로, 이동하는 동안만 지연을 끈다.
	// 다음 틱에 다시 켜면 그 시점의 위치가 새 기준점이 된다.
	Boom->bEnableCameraLag = false;
	SetActorLocationAndRotation(Destination.GetLocation(), Destination.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();

	TWeakObjectPtr<USpringArmComponent> WeakBoom = Boom;
	GetWorldTimerManager().SetTimerForNextTick([WeakBoom]()
	{
		if (WeakBoom.IsValid())
		{
			WeakBoom->bEnableCameraLag = true;
		}
	});
}

void AHGExplorerCharacter::SetSuitVisible(bool bVisible)
{
	if (PlaceholderHelmet)
	{
		PlaceholderHelmet->SetVisibility(bVisible);
	}
}
