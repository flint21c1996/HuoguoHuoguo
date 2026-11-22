#include "Player/HGExplorerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "UObject/ConstructorHelpers.h"

AHGExplorerCharacter::AHGExplorerCharacter()
{
	// 행성 위에서는 매 프레임 중력 방향을 갱신해야 한다. 정거장에서는 Tick이 바로 끝난다.
	PrimaryActorTick.bCanEverTick = true;

	// 캡슐: 언리얼 캐릭터의 실제 충돌 몸체. 반지름 40, 반높이 92면 약 1.84m 키다.
	GetCapsuleComponent()->InitCapsuleSize(40.f, 92.f);

	// 몸은 컨트롤러가 아니라 이동 방향을 따라 돈다 (카메라와 몸의 회전을 분리).
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	// Pitch/Roll 회전 속도도 준다. 행성 위에서 중력 방향이 바뀌면 몸을 새 "위"에 맞춰 세워야 하기 때문이다.
	Move->RotationRate = FRotator(720.f, 720.f, 720.f);
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
	// "수평"은 중력 기준이다. 시점 회전을 중력 좌표계로 옮겨서 Yaw만 남긴 뒤 다시 월드로 돌려놓는다.
	// 정거장에서는 GravityFrame이 항등 회전이라 월드 Yaw를 그대로 쓰는 것과 같다.
	const FQuat LocalView = GravityFrame.Inverse() * Controller->GetControlRotation().Quaternion();
	const FRotator YawOnly(0.f, LocalView.Rotator().Yaw, 0.f);
	const FVector Forward = GravityFrame.RotateVector(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X));
	const FVector Right = GravityFrame.RotateVector(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y));

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

void AHGExplorerCharacter::SetSuitVisible(bool bVisible)
{
	if (PlaceholderHelmet)
	{
		PlaceholderHelmet->SetVisibility(bVisible);
	}
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
