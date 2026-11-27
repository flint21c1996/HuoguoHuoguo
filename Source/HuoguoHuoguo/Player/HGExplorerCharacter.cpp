#include "Player/HGExplorerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "HuoguoHuoguo.h"
#include "Player/HGExplorePlayerController.h"
#include "UObject/ConstructorHelpers.h"

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

	// 플랫폼 게임식 점프: 누르고 있는 동안(최대 0.22초) 점프 속도를 유지해 더 높이 뜬다. 공중 조종을 세게 둔다.
	JumpMaxHoldTime = 0.22f;
	// 탐험 캐릭터는 우주복 차림으로 시작한다 (캐릭터 BP에서 끌 수 있다).
	bWearSpaceSuit = true;
	GetCharacterMovement()->JumpZVelocity = BaseJumpVelocity;
	GetCharacterMovement()->AirControl = 0.8f;

	// 착지 먼지: 인스턴스 좌표를 월드 좌표 그대로 쓰도록 컴포넌트를 월드 원점에 고정한다 (캐릭터를 따라 움직이지 않는다).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Dust = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Dust"));
	Dust->SetupAttachment(GetCapsuleComponent());
	Dust->SetUsingAbsoluteLocation(true);
	Dust->SetUsingAbsoluteRotation(true);
	Dust->SetUsingAbsoluteScale(true);
	Dust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dust->SetCastShadow(false);
	Dust->SetCanEverAffectNavigation(false);
	if (CubeMesh.Succeeded())
	{
		Dust->SetStaticMesh(CubeMesh.Object);
	}
}

void AHGExplorerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	DesiredArmLength = ArmLength;
	WalkAcceleration = GetCharacterMovement()->MaxAcceleration;
	Dust->SetWorldTransform(FTransform::Identity);
	HGPlaceholder::Paint(Dust, FLinearColor(0.92f, 0.88f, 0.82f));
}

void AHGExplorerCharacter::Look(const FVector2D& Input)
{
	// 마우스 원시 입력은 위로 올릴 때 Y가 양수이다. 엔진 3인칭 템플릿과 같은 방향(마우스를 올리면 위를 봄)이
	// 되도록 Pitch 입력에는 부호를 뒤집어 넣는다.
	AddControllerYawInput(Input.X * LookSensitivity);
	AddControllerPitchInput(-Input.Y * LookSensitivity);
}

void AHGExplorerCharacter::AddZoomInput(float Value)
{
	DesiredArmLength = FMath::Clamp(DesiredArmLength * (1.f - ZoomStep * Value), MinArmLength, MaxArmLength);
}

void AHGExplorerCharacter::SetFloating(const FVector& Up)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	bPlanetGravity = true;
	bFixedUp = true;
	FixedUp = Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	bFloating = true;
	Bodies.Reset();
	// 기준 좌표계는 바뀐 만큼만 돌린다 (안에서 나올 때 시점이 튀지 않게).
	GravityFrame = (FQuat::FindBetweenNormals(GravityFrame.GetUpVector(), FixedUp) * GravityFrame).GetNormalized();

	// 날기 상태: 중력 없음, 분사 가속은 약하게, 손을 떼면 아주 천천히 멈춘다(관성).
	Move->GravityScale = 0.f;
	Move->SetMovementMode(MOVE_Flying);
	Move->MaxFlySpeed = FloatMaxSpeed;
	Move->BrakingDecelerationFlying = FloatDrift;
	Move->MaxAcceleration = FloatAcceleration;
	UE_LOG(LogHuoguo, Log, TEXT("[Float] 무중력 비행 시작"));
}

void AHGExplorerCharacter::SetFixedGravity(const FVector& Up, float Scale)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	bPlanetGravity = true;
	bFixedUp = true;
	FixedUp = Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	bFloating = false;
	Bodies.Reset();
	PlanetGravityScale = Scale;
	GravityFrame = (FQuat::FindBetweenNormals(GravityFrame.GetUpVector(), FixedUp) * GravityFrame).GetNormalized();
	Move->MaxAcceleration = WalkAcceleration;
	Move->GravityScale = Scale;
	Move->SetGravityDirection(-FixedUp);
	Move->SetMovementMode(MOVE_Falling);
	UE_LOG(LogHuoguo, Log, TEXT("[Float] 인공 중력 %.1f"), Scale);
}

void AHGExplorerCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	if (!bHasFallRespawn)
	{
		Super::FellOutOfWorld(DamageType);
		return;
	}
	// 평지 행성 가장자리 밖으로 떨어졌다: 사장님을 없애지 않고 착륙 지점으로 순간 이동한다. 떨어지던 속도는 버린다.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->StopMovementImmediately();
	SetActorLocationAndRotation(FallRespawn.GetLocation(), FallRespawn.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	Move->SetMovementMode(MOVE_Falling);
	UE_LOG(LogHuoguo, Log, TEXT("[Flat] 분지 밖으로 떨어져서 착륙 지점으로 돌아왔다 (%s)"), *FallRespawn.GetLocation().ToString());
	if (const AHGExplorePlayerController* PC = Cast<AHGExplorePlayerController>(GetController()))
	{
		PC->NotifyFellOffAndReturned();
	}
}

void AHGExplorerCharacter::FloatMove(const FVector2D& Input)
{
	if (!Controller || Input.IsNearlyZero())
	{
		return;
	}
	// 무중력에서는 카메라가 보는 방향 그대로(위아래 포함) 분사한다.
	const FRotator View = Controller->GetControlRotation();
	AddMovementInput(View.Vector(), Input.Y);
	AddMovementInput(FRotationMatrix(View).GetUnitAxis(EAxis::Y), Input.X);
	bThrusting = true;
}

void AHGExplorerCharacter::SetPlanetGravity(const FVector& Center, float GravityScale)
{
	bPlanetGravity = true;
	bFixedUp = false;
	bFloating = false;
	PlanetCenter = Center;
	PlanetGravityScale = GravityScale;
	Bodies.Reset();
	GetCharacterMovement()->GravityScale = GravityScale;

	// 처음 한 번은 현재 위치의 "위"에 맞춘 기준을 바로 만든다. 이후로는 Tick에서 변화량만 이어 붙인다.
	const FVector Up = (GetActorLocation() - PlanetCenter).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	GravityFrame = FQuat::FindBetweenNormals(FVector::UpVector, Up);
	UpdatePlanetGravity(0.f);
}

void AHGExplorerCharacter::SetGravityBodies(const TArray<FHGGravityBody>& InBodies)
{
	if (InBodies.IsEmpty())
	{
		return;
	}
	SetPlanetGravity(InBodies[0].Center, InBodies[0].SurfaceGravity);
	Bodies = InBodies;
	CurrentBody = 0;
	Oxygen = 100.f;
}

void AHGExplorerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPlanetGravity)
	{
		UpdatePlanetGravity(DeltaSeconds);
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();

	// 무중력 비행: Space/Ctrl로 위/아래 분사, Shift로 브레이크, 분사하는 동안 산소 소모. 걷기/점프 처리는 하지 않는다.
	if (bFloating)
	{
		const float Vertical = (bJumpHeld ? 1.f : 0.f) - (bDownHeld ? 1.f : 0.f);
		if (Vertical != 0.f)
		{
			AddMovementInput(GravityFrame.GetUpVector(), Vertical);
			bThrusting = true;
		}
		if (bBrakeHeld)
		{
			Move->Velocity *= FMath::Exp(-BrakeStrength * DeltaSeconds);
		}
		if (bThrusting)
		{
			Oxygen = FMath::Max(0.f, Oxygen - ThrustOxygenPerSecond * DeltaSeconds);
		}
		bThrusting = false;
		UpdateJuice(DeltaSeconds);
		return;
	}

	const bool bGrounded = Move->IsMovingOnGround();
	if (bGrounded)
	{
		LastGroundedTime = GetWorld()->GetTimeSeconds();
		// 착지 직전에 미리 받아 둔 점프를 지금 낸다.
		// (bJumpedSinceGround는 여기서 지우지 않고 Landed에서 지운다. 점프를 누른 프레임에는 이동 컴포넌트가 아직 뛰기 전이라
		//  여기서는 땅에 서 있는 것으로 보이기 때문이다.)
		if (bBufferedJump)
		{
			bBufferedJump = false;
			DoGroundJump(false);
		}
	}
	else
	{
		LastFallSpeed = FMath::Max(0.f, static_cast<float>(-(Move->Velocity | GravityFrame.GetUpVector())));
	}
	UpdateJuice(DeltaSeconds);

	if (!IsHopMode())
	{
		return;
	}

	// 놓친 점프: 뛴 뒤 오래 떠 있거나 너무 멀리 나가면 한 번만 산소를 잃는다 (되돌리기는 PickBody가 한다).
	if (bHopAirborne && !bGrounded)
	{
		AirTime += DeltaSeconds;
		float Scale = 0.f;
		float SurfaceDistance = 0.f;
		PickBody(GetActorLocation(), false, Scale, SurfaceDistance);
		if (!bMissCounted && (AirTime > MissSeconds || SurfaceDistance > MissDistance))
		{
			bMissCounted = true;
			Oxygen = FMath::Max(0.f, Oxygen - MissOxygen);
			HopMessage = FString::Printf(TEXT("궤도 이탈! 가까운 소행성이 끌어당긴다 (산소 -%.0f)"), MissOxygen);
		}
	}
}

void AHGExplorerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (bHopAirborne && IsHopMode())
	{
		UE_LOG(LogHuoguo, Log, TEXT("[Hop] %d번 소행성에 착지 (공중 %.1f초, 점프 %d단, 낙하 속도 %.0f, 산소 %.0f)"), CurrentBody, AirTime, JumpStage, LastFallSpeed, Oxygen);
	}

	// 착지 눌림: 떨어진 속도가 클수록 많이 눌린다 (초속 9m에서 최대). 옆으로는 부피를 지키듯 조금 퍼진다.
	const float K = FMath::Clamp(LastFallSpeed / 900.f, 0.12f, 0.4f);
	Squash = FVector(1.f + K * 0.6f, 1.f + K * 0.6f, 1.f - K);

	// 착지 먼지: 발 위치에서 지표를 따라 퍼진다.
	DustTime = 0.f;
	DustFrame = GravityFrame;
	DustCenter = GetActorLocation() - GravityFrame.GetUpVector() * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// 공중제비 중이었으면 바로 세운다.
	FlipTime = -1.f;
	VisualPivot->SetRelativeRotation(FRotator::ZeroRotator);

	LastLandTime = GetWorld()->GetTimeSeconds();
	bJumpedSinceGround = false;
	bHopAirborne = false;
	bJetpackReady = false;
	AirTime = 0.f;
}

int32 AHGExplorerCharacter::PickBody(const FVector& Pos, bool bGrounded, float& OutScale, float& OutSurfaceDistance) const
{
	int32 Best = 0;
	int32 Nearest = 0;
	float BestPull = -1.f;
	float BestSurface = TNumericLimits<float>::Max();
	float NearestSurface = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Bodies.Num(); ++i)
	{
		const float Distance = FVector::Dist(Pos, Bodies[i].Center);
		const float Surface = Distance - Bodies[i].Radius;
		const float Pull = Bodies[i].PullAt(Distance);
		// 더 세게 끄는 쪽. 끄는 힘이 같으면(둘 다 중력 껍질 안) 지표가 더 가까운 쪽.
		const bool bStronger = Pull > BestPull + KINDA_SMALL_NUMBER;
		const bool bTieCloser = FMath::IsNearlyEqual(Pull, BestPull) && Surface < BestSurface;
		if (bStronger || bTieCloser)
		{
			BestPull = Pull;
			BestSurface = Surface;
			Best = i;
		}
		if (Surface < NearestSurface)
		{
			NearestSurface = Surface;
			Nearest = i;
		}
	}
	OutSurfaceDistance = NearestSurface;
	if (bGrounded)
	{
		OutScale = Bodies[Nearest].SurfaceGravity;
		return Nearest;
	}
	OutScale = FMath::Max(BestPull, MinPull);
	// 너무 멀리 나가면 가장 가까운 소행성이 지표 중력으로 끌어 되돌린다.
	if (NearestSurface > RescueDistance)
	{
		OutScale = FMath::Max(OutScale, Bodies[Nearest].SurfaceGravity);
		return Nearest;
	}
	return Best;
}

float AHGExplorerCharacter::JumpGravityMultiplier() const
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Move->IsMovingOnGround())
	{
		return 1.f;
	}
	const float UpSpeed = static_cast<float>(Move->Velocity | GravityFrame.GetUpVector());
	if (UpSpeed < 0.f)
	{
		return FallGravityScale;
	}
	// 점프로 오르는 중에 버튼을 떼면 빨리 꺾여 짧은 점프가 된다.
	return (bJumpedSinceGround && !bJumpHeld) ? ReleaseGravityScale : 1.f;
}

void AHGExplorerCharacter::UpdatePlanetGravity(float DeltaSeconds)
{
	FVector Up;
	const float JumpMultiplier = JumpGravityMultiplier();
	if (IsHopMode())
	{
		const bool bGrounded = GetCharacterMovement()->IsMovingOnGround();
		float Scale = 0.f;
		float SurfaceDistance = 0.f;
		CurrentBody = PickBody(GetActorLocation(), bGrounded, Scale, SurfaceDistance);
		GetCharacterMovement()->GravityScale = Scale * JumpMultiplier;

		// 목표 "위" 방향으로 초당 GravityTurnRate도까지만 돌린다. 땅에 서 있거나 처음(DeltaSeconds 0)이면 바로 맞춘다.
		const FVector Target = (GetActorLocation() - Bodies[CurrentBody].Center).GetSafeNormal(UE_SMALL_NUMBER, GravityFrame.GetUpVector());
		const FVector Current = GravityFrame.GetUpVector();
		const float Angle = FMath::Acos(FMath::Clamp(static_cast<float>(Current | Target), -1.f, 1.f));
		const float MaxStep = (bGrounded || DeltaSeconds <= 0.f) ? UE_PI : FMath::DegreesToRadians(GravityTurnRate) * DeltaSeconds;
		if (Angle <= MaxStep)
		{
			Up = Target;
		}
		else
		{
			// 정반대(180도)면 돌릴 축이 정해지지 않으므로 기준 좌표계의 앞 방향을 축으로 쓴다.
			FVector Axis = FVector::CrossProduct(Current, Target);
			Axis = Axis.IsNearlyZero() ? GravityFrame.GetForwardVector() : Axis.GetSafeNormal();
			Up = FQuat(Axis, MaxStep).RotateVector(Current).GetSafeNormal();
		}
	}
	else if (bFixedUp)
	{
		// 우주선 안(인공 중력) 또는 밖(무중력): "위"가 고정이다.
		GetCharacterMovement()->GravityScale = bFloating ? 0.f : PlanetGravityScale * JumpMultiplier;
		Up = FixedUp;
	}
	else
	{
		GetCharacterMovement()->GravityScale = PlanetGravityScale * JumpMultiplier;
		Up = (GetActorLocation() - PlanetCenter).GetSafeNormal(UE_SMALL_NUMBER, GravityFrame.GetUpVector());
	}

	// 지난 프레임의 위 방향에서 이번 위 방향으로 돌린 만큼만 기준 좌표계를 돌린다 (평행 이동).
	// 한 프레임 동안의 변화는 아주 작으므로 FindBetweenNormals가 불안정해지는 반대 방향(180도) 경우가 생기지 않는다.
	const FQuat Delta = FQuat::FindBetweenNormals(GravityFrame.GetUpVector(), Up);
	GravityFrame = (Delta * GravityFrame).GetNormalized();

	// 이동 컴포넌트는 같은 프레임 뒤에 틱하므로(캐릭터가 선행 조건) 바뀐 방향이 이번 이동에 바로 쓰인다.
	GetCharacterMovement()->SetGravityDirection(-Up);
}

void AHGExplorerCharacter::DoGroundJump(bool bCoyote)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const FVector Up = GravityFrame.GetUpVector();

	// 연속 점프: 착지 직후(ChainWindow 안)에 달리면서 뛰면 다음 단계, 아니면 1단부터.
	const float Now = GetWorld()->GetTimeSeconds();
	const float Speed = FVector::VectorPlaneProject(Move->Velocity, Up).Size();
	JumpStage = (Now - LastLandTime < ChainWindow && Speed > ChainMinSpeed) ? FMath::Min(JumpStage + 1, 3) : 1;
	const float Velocity = BaseJumpVelocity * static_cast<float>(JumpStageScale[JumpStage - 1]);

	if (bCoyote)
	{
		// 이미 떨어지는 중이라 엔진 점프(땅에서만 됨)를 쓸 수 없다. 수평 속도는 두고 위로 점프 속도를 넣는다.
		LaunchCharacter(FVector::VectorPlaneProject(Move->Velocity, Up) + Up * Velocity, true, true);
	}
	else
	{
		Move->JumpZVelocity = Velocity;
		Jump();
	}
	bJumpedSinceGround = true;
	MarkAirborne();

	// 뛰는 순간 위로 늘어난다. 단계가 높을수록 더.
	const float Stretch = 0.2f + 0.08f * JumpStage;
	Squash = FVector(1.f - Stretch * 0.5f, 1.f - Stretch * 0.5f, 1.f + Stretch);
	if (JumpStage == 3)
	{
		FlipTime = 0.f;
		HopMessage = TEXT("3단 점프!");
	}
}

bool AHGExplorerCharacter::IsGroundWithin(float Height) const
{
	const FVector Start = GetActorLocation();
	const FVector End = Start - GravityFrame.GetUpVector() * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + Height);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HGGroundProbe), false, this);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
}

void AHGExplorerCharacter::JumpPressed()
{
	bJumpHeld = true;
	if (bFloating)
	{
		return; // 무중력에서는 Space = 위로 분사 (Tick에서 누르고 있는 동안)
	}
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Move->IsMovingOnGround())
	{
		DoGroundJump(false);
		return;
	}

	// 코요테 타임: 발이 막 떨어졌으면(뛰지 않고 걸어서 떨어짐) 땅 점프로 받아 준다.
	if (!bJumpedSinceGround && GetWorld()->GetTimeSeconds() - LastGroundedTime < CoyoteTime)
	{
		DoGroundJump(true);
		return;
	}

	// 입력 미리 받기: 내려오는 중이고 발밑이 가까우면 착지하자마자 뛴다.
	const float UpSpeed = static_cast<float>(Move->Velocity | GravityFrame.GetUpVector());
	if (UpSpeed < 0.f && IsGroundWithin(BufferHeight))
	{
		bBufferedJump = true;
		return;
	}

	// 공중(소행성 지대): 제트팩 짧은 분사 (점프 한 번에 한 번). 카메라가 보는 쪽으로 속도를 더해 궤적을 고친다.
	if (IsHopMode() && bJetpackReady)
	{
		bJetpackReady = false;
		LaunchCharacter(GetControlRotation().Vector() * JetpackSpeed, false, false);
		Oxygen = FMath::Max(0.f, Oxygen - JetpackOxygen);
		HopMessage = FString::Printf(TEXT("제트팩 분사 (산소 -%.0f)"), JetpackOxygen);
	}
}

void AHGExplorerCharacter::JumpReleased()
{
	// 떼면 점프 유지(더 높이 뜨기)가 끝나고, 오르는 중이면 중력이 세져 짧게 꺾인다.
	bJumpHeld = false;
	StopJumping();
}

void AHGExplorerCharacter::MarkAirborne()
{
	bHopAirborne = true;
	bJetpackReady = true;
	bMissCounted = false;
	AirTime = 0.f;
}

void AHGExplorerCharacter::UpdateJuice(float DeltaSeconds)
{
	// 카메라 거리: 휠로 정한 목표 거리로 부드럽게 따라간다.
	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, DesiredArmLength, DeltaSeconds, ZoomSpeed);

	// 눌림/늘어남은 스프링처럼 원래 크기로 빠르게 돌아온다 (초당 약 14배 속도로 차이가 줄어든다).
	Squash = FMath::Lerp(Squash, FVector::OneVector, 1.f - FMath::Exp(-14.f * DeltaSeconds));
	VisualRoot->SetRelativeScale3D(Squash);

	// 3단 점프 공중제비: 앞으로 한 바퀴. 처음이 빠르고 끝이 느리게 돌고, 도는 동안 시야를 잠깐 넓힌다.
	float Fov = FieldOfView;
	if (FlipTime >= 0.f)
	{
		FlipTime += DeltaSeconds;
		const float T = FMath::Clamp(FlipTime / FlipSeconds, 0.f, 1.f);
		const float Eased = 1.f - FMath::Square(1.f - T);
		VisualPivot->SetRelativeRotation(FRotator(-360.f * Eased, 0.f, 0.f));
		Fov += FlipFovKick * FMath::Sin(UE_PI * T);
		if (T >= 1.f)
		{
			FlipTime = -1.f;
			VisualPivot->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
	Camera->SetFieldOfView(Fov);

	// 착지 먼지: 0.45초 동안 칸 10개가 지표를 따라 바깥으로 퍼지며 살짝 떴다가 작아져 사라진다.
	if (DustTime >= 0.f)
	{
		DustTime += DeltaSeconds;
		const float T = DustTime / 0.45f;
		Dust->ClearInstances();
		if (T >= 1.f)
		{
			DustTime = -1.f;
			return;
		}
		const float Spread = 1.f - FMath::Square(1.f - T);
		TArray<FTransform> Puffs;
		for (int32 k = 0; k < 10; ++k)
		{
			const float Angle = UE_TWO_PI * k / 10.f + 0.3f * (k % 2);
			const float Rad = FMath::Lerp(25.f, 110.f, Spread);
			const FVector Local(FMath::Cos(Angle) * Rad, FMath::Sin(Angle) * Rad, 6.f + 22.f * FMath::Sin(UE_PI * T));
			Puffs.Add(FTransform(DustFrame, DustCenter + DustFrame.RotateVector(Local), FVector(0.14f * (1.f - T))));
		}
		Dust->AddInstances(Puffs, false, true);
	}
}

FString AHGExplorerCharacter::ConsumeHopMessage()
{
	FString Out = MoveTemp(HopMessage);
	HopMessage.Reset();
	return Out;
}
