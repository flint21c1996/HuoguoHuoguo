#include "Dev/HGGreyboxBuilder.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Dev/HGPlaceholderPaint.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"
#include "World/HGTravelPad.h"

namespace HGGreybox
{
	// 색상 팔레트. 웹 프로토타입의 색을 그대로 가져왔다 (마라 빨강, 등불 금색, 산소 청록, 정거장 남색).
	const FLinearColor Broth(0.80f, 0.12f, 0.06f);
	const FLinearColor BrothWhite(0.92f, 0.86f, 0.72f);
	const FLinearColor Lantern(0.95f, 0.62f, 0.15f);
	const FLinearColor Oxygen(0.25f, 0.80f, 0.88f);
	const FLinearColor Hull(0.10f, 0.14f, 0.24f);
	const FLinearColor HullDark(0.05f, 0.07f, 0.13f);
	const FLinearColor Floor(0.70f, 0.58f, 0.45f);
	const FLinearColor Wood(0.38f, 0.14f, 0.09f);
	const FLinearColor Steel(0.55f, 0.58f, 0.62f);
	const FLinearColor Rock(0.36f, 0.33f, 0.30f);
	const FLinearColor Mushroom(0.55f, 0.40f, 0.95f);
	const FLinearColor Kelp(0.30f, 0.85f, 0.65f);
	const FLinearColor Chili(0.95f, 0.25f, 0.15f);

	// 가게 치수 (반폭). 12m x 10m.
	constexpr float HalfX = 600.f;
	constexpr float HalfY = 500.f;
	constexpr float WallHeight = 340.f;
}

AHGGreyboxBuilder::AHGGreyboxBuilder()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	SphereMesh = Sphere.Object;
}

FHGWorldLayout AHGGreyboxBuilder::Build()
{
	if (bSpawnLighting)
	{
		BuildLighting();
	}
	FHGWorldLayout Layout = BuildShop();
	BuildAsteroid();
	SpawnTravelPads(Layout);
	Layout.bValid = true;
	return Layout;
}

UStaticMeshComponent* AHGGreyboxBuilder::AddPiece(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
	const FLinearColor& Color, const FRotator& Rotation, bool bCollide)
{
	if (!Mesh)
	{
		return nullptr;
	}

	// 런타임에 컴포넌트를 만들 때의 순서: 생성 -> 부착 -> 속성 설정 -> 등록.
	// 등록(RegisterComponent)을 해야 렌더러와 물리 엔진에 실제로 올라간다.
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetMobility(EComponentMobility::Static);
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetStaticMesh(Mesh);
	Comp->SetRelativeTransform(FTransform(Rotation, Location, Scale));
	Comp->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Comp->RegisterComponent();
	AddInstanceComponent(Comp);

	HGPlaceholder::Paint(Comp, Color);
	return Comp;
}

void AHGGreyboxBuilder::BuildLighting()
{
	UWorld* World = GetWorld();

	// 조명 액터는 지연 생성(SpawnActorDeferred)으로 만든다. FinishSpawning 전에 이동성 같은 설정을 끝내야
	// 컴포넌트가 처음 등록될 때부터 그 설정으로 렌더러에 올라가기 때문이다.
	const FTransform SunTransform(FRotator(-48.f, 35.f, 0.f));
	if (ADirectionalLight* Sun = World->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), SunTransform))
	{
		if (UDirectionalLightComponent* SunComp = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			SunComp->SetMobility(EComponentMobility::Movable);
			SunComp->SetIntensity(8.f);
			SunComp->SetLightColor(FLinearColor(1.f, 0.95f, 0.88f));
			SunComp->SetAtmosphereSunLight(true);
		}
		Sun->FinishSpawning(SunTransform);
	}

	// 하늘 대기: 하늘 색과 태양 산란을 계산한다. 우주 배경으로 바꾸는 작업은 아트 단계에서 한다.
	World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity);

	// 스카이 라이트: 하늘 전체에서 오는 간접광. 실시간 캡처를 켜서 대기 색을 그대로 주변광으로 쓴다.
	// 이것이 없으면 해가 비추지 않는 면이 완전히 검게 나온다.
	if (ASkyLight* Sky = World->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity))
	{
		if (USkyLightComponent* SkyComp = Sky->GetLightComponent())
		{
			SkyComp->SetMobility(EComponentMobility::Movable);
			SkyComp->bRealTimeCapture = true;
			SkyComp->SetIntensity(1.2f);
		}
		Sky->FinishSpawning(FTransform::Identity);
	}
}

FHGWorldLayout AHGGreyboxBuilder::BuildShop()
{
	using namespace HGGreybox;

	// 바닥판과 그 아래 받침대. 받침대는 "정거장에서 잘라 낸 한 칸"을 디오라마처럼 보여 주는 장치이다.
	AddPiece(CubeMesh, FVector(0, 0, -15), FVector(HalfX * 2 / 100, HalfY * 2 / 100, 0.3f), Floor);
	AddPiece(CubeMesh, FVector(0, 0, -150), FVector(HalfX * 2 / 100 + 0.6f, HalfY * 2 / 100 + 0.6f, 2.4f), HullDark);

	// 벽: 기본 카메라 방향(45도)에서 먼 쪽 두 면(+X, +Y)은 높게, 카메라 쪽 두 면은 발목 높이로 낮게 둔다.
	// 카메라를 돌렸을 때 가리는 벽을 자동으로 낮추는 기능(컷어웨이)은 다음 단계에서 넣는다.
	AddPiece(CubeMesh, FVector(HalfX + 15, 0, WallHeight / 2), FVector(0.3f, HalfY * 2 / 100, WallHeight / 100), Hull);
	AddPiece(CubeMesh, FVector(0, HalfY + 15, WallHeight / 2), FVector(HalfX * 2 / 100 + 0.6f, 0.3f, WallHeight / 100), Hull);
	AddPiece(CubeMesh, FVector(-HalfX - 15, 0, 20), FVector(0.3f, HalfY * 2 / 100, 0.4f), Hull);
	AddPiece(CubeMesh, FVector(0, -HalfY - 15, 20), FVector(HalfX * 2 / 100 + 0.6f, 0.3f, 0.4f), Hull);
	// 벽 아래 청록 띠: 정거장 내부라는 느낌을 주는 조명 라인.
	AddPiece(CubeMesh, FVector(HalfX - 2, 0, 35), FVector(0.05f, HalfY * 2 / 100, 0.06f), Oxygen, FRotator::ZeroRotator, false);
	AddPiece(CubeMesh, FVector(0, HalfY - 2, 35), FVector(HalfX * 2 / 100, 0.05f, 0.06f), Oxygen, FRotator::ZeroRotator, false);

	// 주방 카운터와 화구 2개 (+Y 벽 앞). 냄비 위에는 마라탕(빨강)과 백탕(흰색) 원판을 얹는다.
	AddPiece(CubeMesh, FVector(100, 400, 45), FVector(4.8f, 1.f, 0.9f), Broth);
	AddPiece(CubeMesh, FVector(100, 400, 93), FVector(4.9f, 1.1f, 0.08f), Steel);
	for (const float X : {0.f, 220.f})
	{
		AddPiece(CylinderMesh, FVector(X, 400, 111), FVector(0.8f, 0.8f, 0.28f), Steel);
		AddPiece(CylinderMesh, FVector(X - 10, 400, 126), FVector(0.36f, 0.72f, 0.03f), Broth, FRotator::ZeroRotator, false);
		AddPiece(CylinderMesh, FVector(X + 10, 400, 126), FVector(0.36f, 0.72f, 0.03f), BrothWhite, FRotator::ZeroRotator, false);
	}

	// 손님 테이블 4개와 의자. 테이블 위의 작은 냄비는 "주문이 나간 자리"를 표시할 자리이다.
	for (const FVector2D& T : {FVector2D(-300.0, -250.0), FVector2D(100.0, -250.0), FVector2D(-300.0, 50.0), FVector2D(100.0, 50.0)})
	{
		AddPiece(CylinderMesh, FVector(T.X, T.Y, 78), FVector(1.56f, 1.56f, 0.08f), Wood);
		AddPiece(CylinderMesh, FVector(T.X, T.Y, 38), FVector(0.2f, 0.2f, 0.76f), Steel);
		AddPiece(CylinderMesh, FVector(T.X, T.Y, 92), FVector(0.6f, 0.6f, 0.2f), Steel, FRotator::ZeroRotator, false);
		AddPiece(CylinderMesh, FVector(T.X, T.Y - 105, 22), FVector(0.56f, 0.56f, 0.45f), BrothWhite);
	}

	// 재료 진열대 (+X 벽 앞)와 도착 전광판 (+X 벽면).
	AddPiece(CubeMesh, FVector(510, -330, 75), FVector(1.f, 1.3f, 1.5f), Steel);
	for (int32 i = 0; i < 4; ++i)
	{
		const FLinearColor Colors[] = {Mushroom, Kelp, Chili, Lantern};
		AddPiece(CubeMesh, FVector(455, -330, 40 + i * 28), FVector(0.1f, 0.9f, 0.2f), Colors[i], FRotator::ZeroRotator, false);
	}
	AddPiece(CubeMesh, FVector(HalfX - 4, 0, 235), FVector(0.05f, 3.2f, 1.8f), HullDark, FRotator::ZeroRotator, false);
	AddPiece(CubeMesh, FVector(HalfX - 6, 0, 310), FVector(0.05f, 3.0f, 0.12f), Lantern, FRotator::ZeroRotator, false);

	// 천장에 매단 등불.
	for (int32 i = 0; i < 4; ++i)
	{
		AddPiece(SphereMesh, FVector(-360 + i * 240, 250, 300), FVector(0.5f, 0.5f, 0.42f), Broth, FRotator::ZeroRotator, false);
	}

	// 에어록 문 (+Y 벽, 왼쪽 구석). 문 앞 바닥에 이동 발판을 놓는다.
	AddPiece(CubeMesh, FVector(-450, HalfY - 5, 125), FVector(1.7f, 0.2f, 2.5f), Steel, FRotator::ZeroRotator, false);
	AddPiece(CubeMesh, FVector(-450, HalfY - 18, 245), FVector(1.4f, 0.06f, 0.1f), Oxygen, FRotator::ZeroRotator, false);

	FHGWorldLayout Layout;
	Layout.ShopFocus = FVector(0.f, 0.f, 0.f);
	Layout.ShopYaw = 45.f;
	Layout.ShopPanRadius = 900.f;
	// 사장님은 테이블 사이 통로에 서서 에어록 쪽(+Y)을 바라보고 시작한다.
	Layout.ExplorerStart = FTransform(FRotator(0.f, 90.f, 0.f), FVector(-100.f, 220.f, 100.f));
	return Layout;
}

void AHGGreyboxBuilder::BuildAsteroid()
{
	using namespace HGGreybox;
	const FVector O = AsteroidOrigin;

	// 착륙지: 지름 40m의 납작한 원판. 구형 중력(작은 행성 위를 한 바퀴 도는 것)은 다음 단계에서 넣는다.
	AddPiece(CylinderMesh, O, FVector(40.f, 40.f, 1.f), Rock);
	AddPiece(CylinderMesh, O - FVector(0, 0, 300), FVector(34.f, 34.f, 5.f), Rock * 0.7f, FRotator::ZeroRotator, false);

	// 바위와 재료 표식. 난수 시드를 고정해서 매번 같은 배치가 나오게 한다 (테스트 재현성).
	FRandomStream Rng(7);
	const float Top = O.Z + 50.f;
	for (int32 i = 0; i < 26; ++i)
	{
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Radius = Rng.FRandRange(500.f, 1850.f);
		const FVector P(O.X + FMath::Cos(Angle) * Radius, O.Y + FMath::Sin(Angle) * Radius, Top);
		const float S = Rng.FRandRange(0.8f, 3.2f);
		const FRotator R(Rng.FRandRange(-25.f, 25.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-25.f, 25.f));
		AddPiece(i % 3 == 0 ? CubeMesh : SphereMesh, P + FVector(0, 0, S * 30.f), FVector(S, S * 0.9f, S * 0.7f), Rock * Rng.FRandRange(0.8f, 1.2f), R);
	}
	const FLinearColor Ingredients[] = {Mushroom, Kelp, Chili};
	for (int32 i = 0; i < 9; ++i)
	{
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Radius = Rng.FRandRange(400.f, 1700.f);
		const FVector P(O.X + FMath::Cos(Angle) * Radius, O.Y + FMath::Sin(Angle) * Radius, Top + 25.f);
		AddPiece(SphereMesh, P, FVector(0.4f), Ingredients[i % 3], FRotator::ZeroRotator, false);
	}

	// 멀리 보이는 거대 가스 행성. 충돌 없는 배경이다.
	AddPiece(SphereMesh, O + FVector(90000.f, 60000.f, 15000.f), FVector(600.f), FLinearColor(0.75f, 0.38f, 0.22f), FRotator::ZeroRotator, false);
}

FTransform AHGGreyboxBuilder::AsteroidArrival() const
{
	return FTransform(FRotator(0.f, 0.f, 0.f), AsteroidOrigin + FVector(-1300.f, 0.f, 160.f));
}

void AHGGreyboxBuilder::SpawnTravelPads(const FHGWorldLayout& Layout)
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 가게 -> 소행성: 에어록 앞 발판. 도착 지점은 소행성 쪽 귀환 발판에서 3m 떨어진 곳이다.
	if (AHGTravelPad* ToSpace = World->SpawnActor<AHGTravelPad>(AHGTravelPad::StaticClass(), FVector(-450.f, 400.f, 60.f), FRotator::ZeroRotator, Params))
	{
		ToSpace->bLeadsToSpace = true;
		ToSpace->DestinationName = FText::FromString(TEXT("크락-7 소행성"));
		ToSpace->Destination = AsteroidArrival();
		ToSpace->SetPadColor(HGGreybox::Oxygen);
	}

	// 소행성 -> 가게: 귀환 발판. 도착 지점은 가게 안 사장님 시작 위치이다.
	const FVector ReturnPadLocation = AsteroidOrigin + FVector(-1600.f, 0.f, 110.f);
	if (AHGTravelPad* ToShop = World->SpawnActor<AHGTravelPad>(AHGTravelPad::StaticClass(), ReturnPadLocation, FRotator::ZeroRotator, Params))
	{
		ToShop->bLeadsToSpace = false;
		ToShop->DestinationName = FText::FromString(TEXT("훠궈훠궈 (허브 정거장)"));
		ToShop->Destination = Layout.ExplorerStart;
		ToShop->SetPadColor(HGGreybox::Lantern);
	}
}
