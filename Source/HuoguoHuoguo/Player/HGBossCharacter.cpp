#include "Player/HGBossCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Debug/HGVoxelShapes.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 정식 모델 비교 스위치. 다음에 사장님이 나타날 때 적용된다 (맵을 다시 열거나 PIE를 다시 시작).
	TAutoConsoleVariable<int32> CVarBossModel(
		TEXT("hg.BossModel"), 1,
		TEXT("사장님 외형. 1 = 블루프린트 Mesh에 넣은 정식 모델(기본, 모델이 없으면 자리 표시), 0 = 예전 자리 표시(복셀 직장인). 다음에 사장님이 나타날 때 바뀐다."),
		ECVF_Default);
}

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

	// 외형 축: 몸 가운데(VisualPivot) 아래 발 위치(VisualRoot). 부품은 VisualRoot에 붙이고 위치를 발 기준으로 옮긴다.
	VisualPivot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualPivot"));
	VisualPivot->SetupAttachment(GetCapsuleComponent());
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(VisualPivot);
	VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -FeetOffset));

	auto MakePart = [this](const TCHAR* Name, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(VisualRoot);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetRelativeLocation(Location + FVector(0.f, 0.f, FeetOffset));
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

	// 정식 모델 자리: 기본 스켈레탈 메시 컴포넌트(Mesh). 어떤 모델을 쓸지와 크기는 블루프린트(BP_HG_Explorer, BP_HG_ShopBoss)의
	// Mesh 컴포넌트에서 정한다. 여기서는 붙는 자리와 방향 규칙만 정한다.
	// 캡슐이 아니라 VisualRoot(발 위치) 아래에 붙여서, 공중제비/눌림 같은 외형 연출을 자리 표시 부품과 똑같이 받게 한다.
	// 모델 원점은 발로 맞춘다. 모델 앞이 +Y인 것(언리얼 마네킹과 같은 규칙)을 Yaw -90으로 캐릭터 앞(+X)에 맞춘다.
	// 보일지는 PostInitializeComponents에서 정한다 (모델이 들어 있고 hg.BossModel이 1이면). 일단 숨겨 둔다.
	GetMesh()->SetupAttachment(VisualRoot);
	GetMesh()->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetVisibility(false);
}

void AHGBossCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 게임 월드에서만 외형을 준비한다. 블루프린트 편집기 미리보기 같은 에디터 월드에서는 하지 않는다 (복셀 몸을 만들 이유가 없다).
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// 색은 런타임에 동적 머티리얼로 칠한다. 생성자에서는 머티리얼 인스턴스를 만들 수 없기 때문이다.
	HGPlaceholder::Paint(PlaceholderBody, FLinearColor(0.93f, 0.93f, 0.95f));   // 와이셔츠 흰색
	HGPlaceholder::Paint(PlaceholderHead, FLinearColor(0.95f, 0.72f, 0.55f));   // 피부색
	HGPlaceholder::Paint(PlaceholderHelmet, FLinearColor(0.55f, 0.85f, 0.95f)); // 헬멧 청록
	if (CVarBossModel.GetValueOnGameThread() != 0 && GetMesh()->GetSkeletalMeshAsset())
	{
		// 정식 모델이 있으면 그것만 보인다. 복셀 몸은 만들지 않는다 (만들고 숨기는 비용도 아낀다).
		GetMesh()->SetVisibility(true);
		PlaceholderBody->SetVisibility(false);
		PlaceholderHead->SetVisibility(false);
		PlaceholderHelmet->SetVisibility(false);
		bModelBody = true;
	}
	else if (HGVoxel::IsEnabled())
	{
		BuildVoxelBody();
	}
	SetSuitVisible(bWearSpaceSuit);
}

void AHGBossCharacter::BuildVoxelBody()
{
	// 주인공: 식당 경험이 없는 배불뚝이 직장인 아저씨 (기획서 "이야기"). 얼굴은 +X 쪽, 캡슐 가운데가 원점이라 발은 Z -92.
	// 실루엣의 중심은 직장인 차림이다: 흰 와이셔츠, 반쯤 풀린 빨간 넥타이, 회색 정장 바지, 검은 구두, 짧은 검은 머리, 네모 안경, 볼록한 배.
	// 같은 칸을 여러 도형이 차지하면 나중에 넣은 도형의 색이 남는다 (머리카락이 머리 위쪽을, 안경이 얼굴을 덮는다).
	// 위치는 캡슐 가운데 기준 값으로 적고, 부품을 붙이는 VisualRoot(발 위치) 기준으로 FeetOffset만큼 올려 넣는다.
	auto Add = [](TArray<FHGVoxelShape>& Shapes, const TCHAR* Shape, const FVector& Location, const FVector& Scale, const FLinearColor& Color)
	{
		FHGVoxelShape& S = Shapes.AddDefaulted_GetRef();
		S.Shape = Shape;
		S.Location = Location + FVector(0.f, 0.f, FeetOffset);
		S.Scale = Scale;
		S.Color = Color;
	};
	const FLinearColor Shirt(0.93f, 0.93f, 0.95f);
	const FLinearColor Skin(0.95f, 0.74f, 0.58f);
	const FLinearColor Slacks(0.32f, 0.33f, 0.37f);
	const FLinearColor Dark(0.08f, 0.08f, 0.09f);
	const FLinearColor Tie(0.80f, 0.12f, 0.12f);

	TArray<FHGVoxelShape> Body;
	for (const float Side : { -13.f, 13.f })
	{
		Add(Body, TEXT("Cube"), FVector(6.f, Side, -88.f), FVector(0.30f, 0.15f, 0.09f), Dark);          // 구두
		Add(Body, TEXT("Cylinder"), FVector(0.f, Side, -66.f), FVector(0.22f, 0.22f, 0.46f), Slacks);    // 정장 바지
		Add(Body, TEXT("Cylinder"), FVector(0.f, Side * 2.6f, 2.f), FVector(0.17f, 0.17f, 0.46f), Shirt); // 소매 (배 때문에 살짝 벌어짐)
		Add(Body, TEXT("Sphere"), FVector(2.f, Side * 2.75f, -26.f), FVector(0.15f), Skin);               // 손
	}
	Add(Body, TEXT("Sphere"), FVector(6.f, 0.f, -18.f), FVector(0.74f, 0.70f, 0.74f), Shirt);           // 볼록한 배
	Add(Body, TEXT("Cylinder"), FVector(0.f, 0.f, 18.f), FVector(0.60f, 0.62f, 0.36f), Shirt);          // 가슴
	Add(Body, TEXT("Cylinder"), FVector(2.f, 0.f, -44.f), FVector(0.62f, 0.60f, 0.06f), FLinearColor(0.25f, 0.16f, 0.10f)); // 허리띠
	Add(Body, TEXT("Cylinder"), FVector(0.f, 0.f, 38.f), FVector(0.22f, 0.22f, 0.10f), Skin);           // 목
	Add(Body, TEXT("Cube"), FVector(27.f, 0.f, 32.f), FVector(0.06f, 0.08f, 0.06f), Tie);               // 넥타이 매듭 (반쯤 풀려 내려옴)
	Add(Body, TEXT("Cube"), FVector(31.f, 2.f, 17.f), FVector(0.05f, 0.09f, 0.26f), Tie);               // 넥타이
	Add(Body, TEXT("Sphere"), FVector(2.f, 0.f, 60.f), FVector(0.46f, 0.46f, 0.48f), Skin);             // 머리
	Add(Body, TEXT("Sphere"), FVector(-3.f, 0.f, 68.f), FVector(0.47f, 0.48f, 0.36f), Dark);            // 짧은 머리카락 (위와 뒤)
	for (const float Side : { -9.f, 9.f })
	{
		Add(Body, TEXT("Cube"), FVector(24.f, Side, 60.f), FVector(0.03f, 0.11f, 0.08f), Dark);          // 네모 안경 알
	}
	Add(Body, TEXT("Cube"), FVector(25.f, 0.f, 61.f), FVector(0.02f, 0.06f, 0.02f), Dark);              // 안경 다리
	HGVoxel::Build(this, VisualRoot, Body);

	// 앞치마와 헬멧은 따로 복셀화한다. 같은 묶음에 넣으면 겹치는 칸을 덮어써서, 숨겼을 때 몸에 구멍이 난다.
	// 가게에서는 앞치마(배 앞, 허리 아래), 우주에서는 헬멧 (SetSuitVisible).
	TArray<FHGVoxelShape> Apron;
	Add(Apron, TEXT("Cube"), FVector(46.f, 0.f, -24.f), FVector(0.06f, 0.56f, 0.60f), FLinearColor(0.78f, 0.16f, 0.10f));
	for (const FHGVoxelGroup& Group : HGVoxel::Build(this, VisualRoot, Apron))
	{
		ApronVoxels.Add(Group.Component);
	}
	TArray<FHGVoxelShape> Helmet;
	Add(Helmet, TEXT("Sphere"), FVector(2.f, 0.f, 62.f), FVector(0.70f), FLinearColor(0.55f, 0.85f, 0.95f));
	for (const FHGVoxelGroup& Group : HGVoxel::Build(this, VisualRoot, Helmet))
	{
		HelmetVoxels.Add(Group.Component);
	}
	PlaceholderBody->SetVisibility(false);
	PlaceholderHead->SetVisibility(false);
	PlaceholderHelmet->SetVisibility(false);
	bVoxelBody = true;
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
	bSuitVisible = bVisible;
	if (bModelBody)
	{
		// 정식 모델은 아직 우주복 차림 하나뿐이다 (헬멧이 몸에 붙어 있다). 가게 차림(앞치마)은 옷을 따로 만들면 여기서 바꾼다.
		return;
	}
	if (bVoxelBody)
	{
		// 우주복 차림이면 헬멧, 가게 차림이면 앞치마.
		for (UInstancedStaticMeshComponent* Helmet : HelmetVoxels)
		{
			if (Helmet)
			{
				Helmet->SetVisibility(bVisible);
			}
		}
		for (UInstancedStaticMeshComponent* Apron : ApronVoxels)
		{
			if (Apron)
			{
				Apron->SetVisibility(!bVisible);
			}
		}
		return;
	}
	if (PlaceholderHelmet)
	{
		PlaceholderHelmet->SetVisibility(bVisible);
	}
}
