#include "World/Planet/HGPlanetGenerator.h"

#include "Components/BillboardComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Debug/HGVoxelShapes.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/HGGameInstance.h"
#include "Game/HGIngredientTypes.h"
#include "Game/HGLookSettings.h"
#include "HuoguoHuoguo.h"
#include "HAL/PlatformTime.h"
#include "LandscapeProxy.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/Planet/HGNoise.h"
#include "World/Travel/HGTravelPad.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPlanetMood(
		TEXT("hg.PlanetMood"), 1,
		TEXT("행성 그림체 (복셀 + 노 맨즈 스카이 색): 각진 색 지형, 하늘 돔, 복셀 바위/나무/풀꽃, 각진 바위섬. 기본 1. 0이면 다음에 만드는 행성을 예전 매끈한 단색 모습으로 만든다 (비교와 성능 확인용)."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarAsteroidField(
		TEXT("hg.AsteroidField"), 0,
		TEXT("시험용: 1이면 다음에 내려가는 행성을 소행성 지대로 바꾼다 (본 소행성 + 뛰어 건널 둘레 소행성). 0이면 시드가 정한 대로."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarDerelict(
		TEXT("hg.Derelict"), 0,
		TEXT("시험용: 1이면 다음에 내려가는 행성을 폐우주선으로 바꾼다 (무중력 바깥 + 인공 중력 안쪽 방). 0이면 시드가 정한 대로."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarPlanetFaceted(
		TEXT("hg.PlanetFaceted"), 0,
		TEXT("행성 그림체 지형 모양: 0 = 매끈한 땅(부드러운 색 얼룩, 지층 줄무늬, 비탈은 바위색), 1 = 각진 로우폴리 땅(면마다 한 색). 다음에 만드는 행성에 적용한다."),
		ECVF_Default);

	/**
	 * 여러 겹 노이즈(fBm). 큰 언덕 위에 작은 굴곡을 겹쳐 자연스러운 지형을 만든다.
	 * 한 겹 올라갈 때마다 주파수는 약 2배(2.03), 세기는 절반이다. 결과는 대략 -1 ~ 1.
	 * 계산은 정수(고정소수점) 노이즈 HGNoise로 한다. 어느 컴퓨터에서든 같은 시드면 같은 지형이 나오게 하려는 것이다 (2026-11-27).
	 */
	float Fbm(const FVector& P, int32 Octaves)
	{
		return HGNoise::Fbm(P, Octaves, 203);
	}

	/**
	 * 정육면체 표면의 점을 구 표면으로 옮긴다.
	 * 그냥 정규화하면 면 가운데 격자는 크고 모서리 격자는 작아진다. 이 식은 각 축을 서로의 값으로 보정해서
	 * 격자 크기를 고르게 만든다 (Philip Nowell의 정육면체-구 사상).
	 */
	FVector CubeToSphere(const FVector& C)
	{
		const double X2 = C.X * C.X, Y2 = C.Y * C.Y, Z2 = C.Z * C.Z;
		return FVector(
			C.X * FMath::Sqrt(FMath::Max(0.0, 1.0 - Y2 / 2.0 - Z2 / 2.0 + Y2 * Z2 / 3.0)),
			C.Y * FMath::Sqrt(FMath::Max(0.0, 1.0 - Z2 / 2.0 - X2 / 2.0 + Z2 * X2 / 3.0)),
			C.Z * FMath::Sqrt(FMath::Max(0.0, 1.0 - X2 / 2.0 - Y2 / 2.0 + X2 * Y2 / 3.0))).GetSafeNormal();
	}

	// Dir에 수직인 두 접선. 지표 법선을 유한 차분으로 구할 때 쓴다.
	void TangentBasis(const FVector& Dir, FVector& OutT1, FVector& OutT2)
	{
		const FVector Ref = FMath::Abs(Dir.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector;
		OutT1 = FVector::CrossProduct(Ref, Dir).GetSafeNormal();
		OutT2 = FVector::CrossProduct(Dir, OutT1);
	}
}

AHGPlanetGenerator::AHGPlanetGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	SetRootComponent(Terrain);
	// 지형 메시 자체를 충돌 모양으로 쓴다. 울퉁불퉁한 지면을 그대로 밟아야 하기 때문이다.
	Terrain->bUseComplexAsSimpleCollision = true;
	// 충돌을 같은 프레임에 만든다. 비동기로 만들면 사장님이 충돌이 생기기 전에 떨어질 수 있다.
	Terrain->bUseAsyncCooking = false;
	Terrain->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// 같은 모양을 수백 개 놓을 때는 인스턴스 메시 하나로 그린다. 그리기 호출이 하나로 묶여 훨씬 가볍다.
	auto MakeIsm = [this](const TCHAR* Name, UStaticMesh* Mesh, bool bCollide)
	{
		UInstancedStaticMeshComponent* Ism = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Ism->SetupAttachment(Terrain);
		Ism->SetStaticMesh(Mesh);
		Ism->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		return Ism;
	};
	RoundRocks = MakeIsm(TEXT("RoundRocks"), SphereMesh.Object, true);
	BlockRocks = MakeIsm(TEXT("BlockRocks"), CubeMesh.Object, true);

	// 재료 표식 인스턴스 메시는 재료 DataTable을 읽은 뒤 종류 수만큼 실행 중에 만든다 (ScatterProps).
	MarkerMesh = SphereMesh.Object;

#if WITH_EDITORONLY_DATA
	EditorIcon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorIcon"));
	if (EditorIcon)
	{
		EditorIcon->SetupAttachment(Terrain);
	}
	// 액터를 끌거나 슬라이더를 끄는 동안에는 미리보기를 다시 만들지 않고, 놓았을 때 한 번만 만든다 (지형 한 번에 수십 ms).
	bRunConstructionScriptOnDrag = false;
#endif
}

void AHGPlanetGenerator::BeginPlay()
{
	Super::BeginPlay();
	EnsureGenerated();
}

#if WITH_EDITOR
void AHGPlanetGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (bPreviewInEditor)
	{
		RebuildEditorPreview();
	}
	else
	{
		ClearEditorPreview();
	}
}

void AHGPlanetGenerator::RebuildEditorPreview()
{
	// 게임 월드(플레이 중)나 클래스 기본 객체에서는 만들지 않는다. 게임은 BeginPlay에서 진짜 지형을 만든다.
	UWorld* World = GetWorld();
	if (IsTemplate() || !World || World->IsGameWorld())
	{
		return;
	}

	const double StartTime = FPlatformTime::Seconds();
	ChoosePlanet();

	// 미리보기 메시는 처음 한 번 만든다. 임시(저장 안 함) + 복제 안 함이라 맵 파일에도, 플레이용 월드 복사본에도 남지 않는다.
	if (!EditorPreviewTerrain)
	{
		EditorPreviewTerrain = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		EditorPreviewTerrain->SetupAttachment(Terrain);
		EditorPreviewTerrain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EditorPreviewTerrain->RegisterComponent();
	}

	if (Planet.bDerelict || bFlatSite)
	{
		// 폐우주선은 땅이 없고, 평지 행성의 땅은 맵의 랜드스케이프 자체다 (따로 미리볼 지형 메시가 없다).
		EditorPreviewTerrain->ClearAllMeshSections();
		UE_LOG(LogHuoguo, Log, TEXT("[Preview] %s: 구 지형 미리보기 없음 (%s)"), bFlatSite ? TEXT("평지 행성") : TEXT("폐우주선"), *Planet.Describe());
		return;
	}

	BuildTerrain(EditorPreviewTerrain, false);
	UE_LOG(LogHuoguo, Log, TEXT("[Preview] 지형 미리보기: %s | 크레이터 %d | %.0f ms"),
		*Planet.Describe(), Craters.Num(), (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

void AHGPlanetGenerator::SetEditorPreview(const FHGPlanetSeed& InPlanet, const FHGPlanetShape& InShape)
{
	// Modify: 에디터 되돌리기(Ctrl+Z) 기록에 남기고 맵을 "바뀜"으로 표시한다.
	Modify();
	PreviewPlanet = InPlanet;
	bPreviewFromSeedOnly = false;
	PreviewShape = InShape;
	bUsePreviewShape = true;
	bPreviewInEditor = true;
	RebuildEditorPreview();
}

void AHGPlanetGenerator::SetFlatSite(const FHGPlanetSeed& InPlanet, float HalfExtent)
{
	Modify();
	PreviewPlanet = InPlanet;
	bPreviewFromSeedOnly = false;
	bFlatSite = true;
	FlatHalfExtent = HalfExtent;
	ClearEditorPreview();
}

void AHGPlanetGenerator::ClearEditorPreview()
{
	if (EditorPreviewTerrain)
	{
		EditorPreviewTerrain->ClearAllMeshSections();
	}
}
#endif

void AHGPlanetGenerator::EnsureGenerated()
{
	if (bGenerated)
	{
		return;
	}
	bGenerated = true;

	const double StartTime = FPlatformTime::Seconds();
	ChoosePlanet();
	if (Planet.bDerelict)
	{
		// 폐우주선: 땅이 없다. ScatterProps는 재료 종류(인스턴스 메시)만 준비하고, 우주선과 귀환 포드는 BuildDerelict가 만든다.
		ScatterProps();
		BuildDerelict();
	}
	else if (bFlatSite)
	{
		// 평지 행성: 땅은 맵의 랜드스케이프다. 그 위에 바위, 재료, 귀환 발판을 놓고 땅 색을 군계에 맞춘다.
		ScatterProps();
		SpawnReturnPad();
		PaintFlatGround();
	}
	else
	{
		BuildTerrain(Terrain, true);
		ScatterProps();
		SpawnReturnPad();
	}
	BuildAsteroidField();
	if (CVarPlanetMood.GetValueOnGameThread() != 0)
	{
		ApplyMoodPreview();
	}

	UE_LOG(LogHuoguo, Log, TEXT("행성 생성 완료: %s (시드 %d), %.0f ms"),
		*Planet.Describe(), Planet.Seed, (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

// ---------------------------------------------------------------------------
// 행성 그림체 (hg.PlanetMood): 모양은 복셀, 색과 하늘은 노 맨즈 스카이에서 배운다 (기획서 "그림체")
// ---------------------------------------------------------------------------

namespace
{
	// 생물 군계별 색. 땅과 하늘, 풀과 꽃, 나무를 서로 대비되는 쨍한 색으로 고른다.
	struct FMoodPalette
	{
		FLinearColor Ground;    // 땅 주 색
		FLinearColor GroundB;   // 땅 얼룩 색 (노이즈로 섞는다)
		FLinearColor Rock;
		FLinearColor Zenith;    // 하늘 천정 색
		FLinearColor Horizon;   // 하늘 지평선 색
		FLinearColor Grass[2];
		FLinearColor Flower[2];
		FLinearColor Shrub;
		FLinearColor Trunk;     // 나무 줄기 / 버섯 대 / 수정 아랫부분
		FLinearColor Canopy;    // 나무 잎 / 버섯 갓 / 수정 윗부분
	};

	FMoodPalette MoodPalette(EHGPlanetBiome Biome)
	{
		FMoodPalette P;
		switch (Biome)
		{
		case EHGPlanetBiome::Ice:     // 청록 땅, 보랏빛 하늘과 분홍 지평선, 하늘색 풀, 파란 덤불, 수정 기둥
			P.Ground = FLinearColor(0.20f, 0.62f, 0.58f); P.GroundB = FLinearColor(0.55f, 0.85f, 0.85f); P.Rock = FLinearColor(0.50f, 0.42f, 0.72f);
			P.Zenith = FLinearColor(0.40f, 0.28f, 0.85f); P.Horizon = FLinearColor(1.0f, 0.70f, 0.85f);
			P.Grass[0] = FLinearColor(0.45f, 0.90f, 0.90f); P.Grass[1] = FLinearColor(0.25f, 0.75f, 0.85f);
			P.Flower[0] = FLinearColor(1.0f, 1.0f, 1.0f); P.Flower[1] = FLinearColor(1.0f, 0.45f, 0.75f);
			P.Shrub = FLinearColor(0.20f, 0.35f, 0.95f); P.Trunk = FLinearColor(0.35f, 0.55f, 0.95f); P.Canopy = FLinearColor(0.75f, 0.95f, 1.0f);
			break;
		case EHGPlanetBiome::Fungal:  // 보라 땅, 청록 하늘과 민트 지평선, 자주 풀, 노랑/주황 꽃, 거대 버섯
			P.Ground = FLinearColor(0.45f, 0.20f, 0.55f); P.GroundB = FLinearColor(0.65f, 0.30f, 0.50f); P.Rock = FLinearColor(0.22f, 0.18f, 0.32f);
			P.Zenith = FLinearColor(0.05f, 0.50f, 0.55f); P.Horizon = FLinearColor(0.60f, 1.0f, 0.80f);
			P.Grass[0] = FLinearColor(0.80f, 0.30f, 0.80f); P.Grass[1] = FLinearColor(0.55f, 0.25f, 0.85f);
			P.Flower[0] = FLinearColor(1.0f, 0.85f, 0.15f); P.Flower[1] = FLinearColor(1.0f, 0.45f, 0.10f);
			P.Shrub = FLinearColor(0.95f, 0.55f, 0.15f); P.Trunk = FLinearColor(0.95f, 0.90f, 0.80f); P.Canopy = FLinearColor(1.0f, 0.50f, 0.12f);
			break;
		default:                      // 바위: 주황 땅, 파란 하늘, 노란 풀, 흰/빨강 꽃, 자홍 침엽수
			P.Ground = FLinearColor(0.80f, 0.40f, 0.20f); P.GroundB = FLinearColor(0.62f, 0.26f, 0.15f); P.Rock = FLinearColor(0.42f, 0.28f, 0.22f);
			P.Zenith = FLinearColor(0.10f, 0.38f, 0.95f); P.Horizon = FLinearColor(0.70f, 0.90f, 1.0f);
			P.Grass[0] = FLinearColor(0.95f, 0.75f, 0.20f); P.Grass[1] = FLinearColor(0.85f, 0.55f, 0.15f);
			P.Flower[0] = FLinearColor(1.0f, 1.0f, 1.0f); P.Flower[1] = FLinearColor(0.95f, 0.15f, 0.20f);
			P.Shrub = FLinearColor(0.55f, 0.12f, 0.35f); P.Trunk = FLinearColor(0.30f, 0.15f, 0.12f); P.Canopy = FLinearColor(0.85f, 0.15f, 0.40f);
			break;
		}
		return P;
	}

	bool IsMoodEnabled()
	{
		return CVarPlanetMood.GetValueOnGameThread() != 0;
	}

	FLinearColor ShadeOf(const FLinearColor& C, uint32 Hash)
	{
		static const float Scale[] = { 0.88f, 1.0f, 1.12f };
		FLinearColor Out = C * Scale[Hash % 3u];
		Out.A = 1.f;
		return Out;
	}
}

void AHGPlanetGenerator::PaintFlatGround()
{
	// 평지 땅(랜드스케이프) 머티리얼 M_HG_LandscapeBasin의 색 값을 군계 색으로 바꾼다 (평평한 곳 = 땅 색, 비탈 = 바위 색).
	// 랜드스케이프 머티리얼은 맵에서 사람이 바꿀 수 있다. 그 머티리얼에 같은 이름 값이 없으면 아무 일도 일어나지 않는다.
	const FMoodPalette Pal = MoodPalette(Planet.Biome);
	int32 Count = 0;
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		// 랜드스케이프 디테일의 "Use Dynamic Material Instance"가 꺼져 있으면 엔진이 색 값 바꾸기를 무시한다.
		if (!It->bUseDynamicMaterialInstance)
		{
			UE_LOG(LogHuoguo, Warning, TEXT("[Flat] %s: Use Dynamic Material Instance가 꺼져 있어서 땅 색을 군계에 맞출 수 없다. 랜드스케이프 디테일에서 켤 것"), *It->GetName());
			continue;
		}
		It->SetLandscapeMaterialVectorParameterValue(TEXT("Ground"), Pal.Ground);
		It->SetLandscapeMaterialVectorParameterValue(TEXT("GroundB"), Pal.GroundB);
		It->SetLandscapeMaterialVectorParameterValue(TEXT("Rock"), Pal.Rock);
		++Count;
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Flat] 평지 행성: 반폭 %.0fm, 랜드스케이프 %d개 색칠, 착륙 지점 땅 높이 %.1fm"),
		FlatHalfExtent / 100.f, Count, FlatGroundZ(FVector2D::ZeroVector) / 100.0);
}

FTransform AHGPlanetGenerator::SurfaceFrame(const FVector& Dir, float Spin, float Lift) const
{
	const FVector Up = SurfaceUpOf(Dir);
	FVector T1, T2;
	TangentBasis(Up, T1, T2);
	const FVector Forward = T1 * FMath::Cos(Spin) + T2 * FMath::Sin(Spin);
	if (bFlatSite)
	{
		return FTransform(FRotationMatrix::MakeFromZX(Up, Forward).ToQuat(), SurfacePointOf(Dir) + Up * Lift);
	}
	return FTransform(FRotationMatrix::MakeFromZX(Dir, Forward).ToQuat(), Dir * (SurfaceRadiusAt(Dir) + Lift));
}

FVector AHGPlanetGenerator::SurfacePointOf(const FVector& Dir) const
{
	if (bFlatSite)
	{
		const FVector2D XY = FlatPointOf(Dir);
		return FVector(XY.X, XY.Y, FlatGroundZ(XY));
	}
	return Dir * SurfaceRadiusAt(Dir);
}

FVector2D AHGPlanetGenerator::FlatPointOf(const FVector& Dir) const
{
	// 램버트 정적 방위 투영: 북극에서 각도 θ인 점을 가운데에서 반폭 x sin(θ/2) 떨어진 곳에 둔다.
	// |Dir - 북극| = 2 sin(θ/2) 이므로 거리 = 반폭 x 0.5 x |Dir - 북극|. 남극(θ = 180도)이 원판 가장자리(반폭)가 된다.
	const FVector2D Horizontal(Dir.X, Dir.Y);
	const double Length = Horizontal.Size();
	const double Distance = FlatHalfExtent * 0.5 * (Dir - FVector::UpVector).Size();
	return Length > UE_KINDA_SMALL_NUMBER ? Horizontal / Length * Distance : FVector2D(Distance, 0.0);
}

double AHGPlanetGenerator::FlatGroundZ(const FVector2D& XY) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	// 위 1km에서 아래 1km까지 쏴서 랜드스케이프에 맞은 첫 점을 쓴다. 이 생성기가 만든 바위, 하늘 섬 등은 건너뛴다.
	const FVector Base = GetActorLocation();
	const FVector Start(Base.X + XY.X, Base.Y + XY.Y, Base.Z + 100000.0);
	const FVector End(Base.X + XY.X, Base.Y + XY.Y, Base.Z - 100000.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HGFlatGround), false, this);
	TArray<FHitResult> Hits;
	World->LineTraceMultiByObjectType(Hits, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	for (const FHitResult& Hit : Hits)
	{
		if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
		{
			return Hit.ImpactPoint.Z - Base.Z;
		}
	}
	return Hits.IsEmpty() ? 0.0 : Hits[0].ImpactPoint.Z - Base.Z;
}

void AHGPlanetGenerator::ApplyMoodPreview()
{
	const FMoodPalette Pal = MoodPalette(Planet.Biome);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));

	// 색마다 복셀 인스턴스 메시 하나. 변환을 모았다가 한꺼번에 넣는다 (그리기 호출을 색 수만큼으로 묶는다).
	TMap<uint32, TArray<FTransform>> Batches;
	TMap<uint32, FLinearColor> BatchColor;
	TMap<uint32, bool> BatchShadow;
	TMap<uint32, bool> BatchGlow;
	// 그림체 비교(hg.Look)의 윤곽선에서 빼는 묶음 (외계 식물). 커스텀 스텐실로 표시해 두면 화면 효과가 그 점에는 선을 긋지 않는다.
	TMap<uint32, bool> BatchNoOutline;
	auto AddVoxel = [&](const FTransform& Frame, const FVector& Local, float Size, const FLinearColor& Color, bool bShadow, bool bGlow = false, bool bNoOutline = false)
	{
		const uint32 Key = HashCombine(HashCombine(HashCombine(HashCombine(Color.ToFColor(false).DWColor(), GetTypeHash(FMath::RoundToInt(Size))), GetTypeHash(bShadow)), GetTypeHash(bGlow)), GetTypeHash(bNoOutline));
		BatchGlow.Add(Key, bGlow);
		BatchNoOutline.Add(Key, bNoOutline);
		Batches.FindOrAdd(Key).Add(FTransform(Frame.GetRotation(), Frame.TransformPosition(Local), FVector(Size / 100.f)));
		BatchColor.Add(Key, Color);
		BatchShadow.Add(Key, bShadow);
	};

	// 모양 하나를 복셀 껍데기로 채운다: 칸 가운데가 모양 안이고 여섯 이웃 중 하나라도 밖이면 놓는다 (속은 비워 칸 수를 줄인다).
	// Inside(P) = 모양 안인가, ColorAt(P, Hash) = 그 칸 색. P는 Frame 기준 좌표(cm).
	auto FillShell = [&](const FTransform& Frame, const FVector& Min, const FVector& Max, float Size, bool bShadow, bool bGlow,
		TFunctionRef<bool(const FVector&)> Inside, TFunctionRef<FLinearColor(const FVector&, uint32)> ColorAt)
	{
		int32 Count = 0;
		const FIntVector Lo(FMath::FloorToInt(Min.X / Size), FMath::FloorToInt(Min.Y / Size), FMath::FloorToInt(Min.Z / Size));
		const FIntVector Hi(FMath::CeilToInt(Max.X / Size), FMath::CeilToInt(Max.Y / Size), FMath::CeilToInt(Max.Z / Size));
		auto In = [&](int32 X, int32 Y, int32 Z) { return Inside(FVector((X + 0.5f) * Size, (Y + 0.5f) * Size, (Z + 0.5f) * Size)); };
		for (int32 X = Lo.X; X < Hi.X; ++X)
		{
			for (int32 Y = Lo.Y; Y < Hi.Y; ++Y)
			{
				for (int32 Z = Lo.Z; Z < Hi.Z; ++Z)
				{
					if (!In(X, Y, Z))
					{
						continue;
					}
					if (In(X + 1, Y, Z) && In(X - 1, Y, Z) && In(X, Y + 1, Z) && In(X, Y - 1, Z) && In(X, Y, Z + 1) && In(X, Y, Z - 1))
					{
						continue;
					}
					const FVector P((X + 0.5f) * Size, (Y + 0.5f) * Size, (Z + 0.5f) * Size);
					AddVoxel(Frame, P, Size, ColorAt(P, GetTypeHash(FIntVector(X, Y, Z))), bShadow, bGlow);
					++Count;
				}
			}
		}
		return Count;
	};

	// 1. 복셀 바위: 매끈한 바위 인스턴스는 숨기고(크기 그대로 두어 충돌은 남긴다) 같은 모양을 15cm 껍데기로 채운다.
	int32 RockVoxels = 0;
	for (UInstancedStaticMeshComponent* Rocks : { RoundRocks.Get(), BlockRocks.Get() })
	{
		const bool bRound = Rocks == RoundRocks;
		const FLinearColor RockColor = bRound ? Pal.Rock : Pal.Rock * 0.8f;
		for (int32 i = 0; i < Rocks->GetInstanceCount(); ++i)
		{
			FTransform T;
			Rocks->GetInstanceTransform(i, T, false);
			const FVector Half = T.GetScale3D() * 50.f;
			const FTransform Frame(T.GetRotation(), T.GetLocation());
			// 아래 절반은 땅에 묻히므로 만들지 않는다 (Z 아래 한계를 -15로).
			RockVoxels += FillShell(Frame, FVector(-Half.X, -Half.Y, -15.f), Half, 15.f, true, false,
				[&](const FVector& P)
				{
					return bRound ? FMath::Square(P.X / Half.X) + FMath::Square(P.Y / Half.Y) + FMath::Square(P.Z / Half.Z) <= 1.f
						: FMath::Abs(P.X) <= Half.X && FMath::Abs(P.Y) <= Half.Y && FMath::Abs(P.Z) <= Half.Z;
				},
				[&](const FVector&, uint32 H) { return ShadeOf(RockColor, H ^ static_cast<uint32>(i)); });
		}
		Rocks->SetVisibility(false);
	}

	// 2. 나무 (군계마다 다른 큰 볼거리): 바위 행성 = 자홍 침엽수, 버섯숲 = 거대 버섯, 얼음 = 수정 기둥.
	FRandomStream Rng(static_cast<int32>(HashCombine(GetTypeHash(Planet.Seed), 0xf10a)));
	const double R = Planet.Radius;
	const double AreaM2 = 4.0 * UE_PI * R * R / 10000.0;
	const double MinPole = 600.0 / R;   // 착륙 지점 6m 안에는 큰 것을 두지 않는다
	auto RandomDirNearPole = [&](double MaxAngle)
	{
		const double Theta = MinPole + (MaxAngle - MinPole) * FMath::Sqrt(Rng.FRand());
		const double Phi = Rng.FRandRange(0.f, UE_TWO_PI);
		return FVector(FMath::Sin(Theta) * FMath::Cos(Phi), FMath::Sin(Theta) * FMath::Sin(Phi), FMath::Cos(Theta));
	};
	auto RandomDirAway = [&]()
	{
		for (;;)
		{
			const FVector Dir = Rng.VRand();
			if (FMath::Acos(FMath::Clamp(Dir.Z, -1.0, 1.0)) > MinPole)
			{
				return Dir;
			}
		}
	};
	int32 TreeVoxels = 0;
	auto PlaceTree = [&](const FVector& Dir)
	{
		const FTransform Frame = SurfaceFrame(Dir, Rng.FRandRange(0.f, UE_TWO_PI), -20.f);
		const float Scale = Rng.FRandRange(0.8f, 1.4f);
		const uint32 Seed = Rng.GetCurrentSeed();
		if (Planet.Biome == EHGPlanetBiome::Fungal)
		{
			// 거대 버섯: 굵은 대(반지름 35) 높이 3~4.5m, 위에 납작한 반구 갓(반지름 1.6m), 갓 밑면은 어둡게.
			const float H = 350.f * Scale;
			const float CapR = 160.f * Scale;
			TreeVoxels += FillShell(Frame, FVector(-40.f, -40.f, 0.f), FVector(40.f, 40.f, H), 20.f, true, false,
				[&](const FVector& P) { return P.X * P.X + P.Y * P.Y <= 35.f * 35.f; },
				[&](const FVector&, uint32 Hh) { return ShadeOf(Pal.Trunk, Hh ^ Seed); });
			TreeVoxels += FillShell(Frame, FVector(-CapR, -CapR, H - 20.f), FVector(CapR, CapR, H + CapR * 0.6f), 20.f, true, false,
				[&](const FVector& P) { const float Z = (P.Z - H) / (CapR * 0.6f); return Z >= 0.f && (P.X * P.X + P.Y * P.Y) / (CapR * CapR) + Z * Z <= 1.f; },
				[&](const FVector& P, uint32 Hh) { return P.Z < H + 20.f ? ShadeOf(Pal.Trunk * 0.6f, Hh) : ShadeOf(Pal.Canopy, Hh ^ Seed); });
		}
		else if (Planet.Biome == EHGPlanetBiome::Ice)
		{
			// 수정 기둥: 3~5m 높이의 가늘어지는 기둥 하나 + 짧은 곁기둥 둘, 아래는 진하고 위로 갈수록 밝다.
			auto Spire = [&](const FVector& Base, float Height, float Radius)
			{
				TreeVoxels += FillShell(Frame, Base + FVector(-Radius, -Radius, 0.f), Base + FVector(Radius, Radius, Height), 20.f, true, false,
					[&](const FVector& P) { const float T = (P.Z - Base.Z) / Height; const FVector2D D(P.X - Base.X, P.Y - Base.Y); return T >= 0.f && T <= 1.f && FMath::Abs(D.X) + FMath::Abs(D.Y) <= Radius * (1.f - T); },
					[&](const FVector& P, uint32 Hh) { const float T = FMath::FloorToFloat(FMath::Clamp((P.Z - Base.Z) / Height, 0.f, 0.999f) * 4.f) / 3.f; return ShadeOf(FMath::Lerp(Pal.Trunk, Pal.Canopy, T), Hh); });
			};
			Spire(FVector::ZeroVector, 450.f * Scale, 80.f * Scale);
			Spire(FVector(90.f, 30.f, 0.f), 220.f * Scale, 45.f * Scale);
			Spire(FVector(-60.f, -80.f, 0.f), 180.f * Scale, 40.f * Scale);
		}
		else
		{
			// 자홍 침엽수: 줄기 높이 1.5m 위에 층층이 작아지는 잎 원판 넷 (노 맨즈 스카이의 외계 침엽수 느낌).
			const float TrunkH = 150.f * Scale;
			TreeVoxels += FillShell(Frame, FVector(-25.f, -25.f, 0.f), FVector(25.f, 25.f, TrunkH + 300.f * Scale), 20.f, true, false,
				[&](const FVector& P) { return FMath::Abs(P.X) <= 20.f && FMath::Abs(P.Y) <= 20.f; },
				[&](const FVector&, uint32 Hh) { return ShadeOf(Pal.Trunk, Hh); });
			for (int32 Layer = 0; Layer < 4; ++Layer)
			{
				const float Z0 = TrunkH + Layer * 75.f * Scale;
				const float Rad = (170.f - Layer * 35.f) * Scale;
				TreeVoxels += FillShell(Frame, FVector(-Rad, -Rad, Z0), FVector(Rad, Rad, Z0 + 60.f * Scale), 20.f, true, false,
					[&](const FVector& P) { const float T = (P.Z - Z0) / (60.f * Scale); return T >= 0.f && T <= 1.f && P.X * P.X + P.Y * P.Y <= FMath::Square(Rad * (1.f - 0.5f * T)); },
					[&](const FVector&, uint32 Hh) { return ShadeOf(Pal.Canopy, Hh ^ Seed); });
			}
		}
	};
	// 소행성 지대의 본 소행성은 작아서 나무를 몇 그루만 둔다.
	const int32 NearTrees = Planet.bDerelict ? 0 : Planet.bAsteroidField ? 2 : 14;
	const int32 FarTrees = Planet.bDerelict ? 0 : Planet.bAsteroidField ? 1 : FMath::Clamp(FMath::RoundToInt(AreaM2 / 900.0), 10, 120);
	for (int32 i = 0; i < NearTrees; ++i)
	{
		PlaceTree(RandomDirNearPole(3500.0 / R));
	}
	for (int32 i = 0; i < FarTrees; ++i)
	{
		PlaceTree(RandomDirAway());
	}

	// 3. 외계 식물: 지구 풀잎/데이지 대신 낯선 모양 다섯 가지 (사용자 의견: "풀이 너무 지구스럽다").
	//    끝이 스스로 빛나는 것(발광 머티리얼)이 많아서 낮에도 "다른 별의 생물" 느낌을 준다.
	//    한 포기가 예전 풀보다 커서(칸이 많아서) 밀도는 낮춘다: 착륙 지점 둘레 25m는 1m²당 1.2포기, 25~45m는 0.4포기, 나머지는 25m²당 1포기.
	const float FloraVoxel = 8.f;
	auto CapArea = [R](double Angle) { return 2.0 * UE_PI * R * R * (1.0 - FMath::Cos(Angle)) / 10000.0; };
	const double Inner = 2500.0 / R;
	const double Outer = 4500.0 / R;
	// 폐우주선은 땅이 없어 식물도 없다.
	const int32 FloraScale = Planet.bDerelict ? 0 : 1;
	const int32 InnerCount = FloraScale * FMath::Clamp(FMath::RoundToInt(CapArea(Inner) * 1.2), 0, 4000);
	const int32 RingCount = FloraScale * FMath::Clamp(FMath::RoundToInt((CapArea(Outer) - CapArea(Inner)) * 0.4), 0, 3000);
	const int32 FarCount = FloraScale * FMath::Clamp(FMath::RoundToInt((AreaM2 - CapArea(Outer)) / 25.0), 0, 6000);
	auto PlaceTuft = [&](const FVector& Dir)
	{
		const FTransform Frame = SurfaceFrame(Dir, Rng.FRandRange(0.f, UE_TWO_PI), -2.f);
		const float Kind = Rng.FRand();
		const uint32 H = Rng.GetCurrentSeed();
		// 네 포기 중 하나는 칸을 1.5배로 키워 크기를 섞는다.
		const float V = Rng.FRand() < 0.25f ? FloraVoxel * 1.5f : FloraVoxel;
		const FLinearColor Stem = Pal.Grass[Rng.RandRange(0, 1)];
		const FLinearColor Tip = Pal.Flower[Rng.RandRange(0, 1)];
		// 칸 좌표(X, Y, Z)에 칸 하나. Z = 0이 땅에 닿는 칸.
		auto Put = [&](int32 X, int32 Y, int32 Z, const FLinearColor& Color, bool bGlow = false)
		{
			// 외계 식물은 칸이 잘고 촘촘해서 윤곽선을 그으면 지저분하다 (사용자 요청: 꽃은 외곽선을 뺀다).
			AddVoxel(Frame, FVector(X * V, Y * V, (Z + 0.5f) * V), V, Color, false, bGlow, true);
		};

		if (Kind < 0.3f)
		{
			// 등불 식물: 휘어 오르는 가는 줄기(3~7칸) 끝에 빛나는 둥근 구슬 (3x3x3에서 모서리 8칸을 뺀 모양).
			const int32 Height = Rng.RandRange(3, 7);
			const float Curve = Rng.FRandRange(-2.f, 2.f);
			int32 X = 0;
			for (int32 z = 0; z < Height; ++z)
			{
				X = FMath::RoundToInt(Curve * FMath::Square(static_cast<float>(z) / Height));
				Put(X, 0, z, ShadeOf(Stem, H + z));
			}
			for (int32 dx = -1; dx <= 1; ++dx)
			{
				for (int32 dy = -1; dy <= 1; ++dy)
				{
					for (int32 dz = -1; dz <= 1; ++dz)
					{
						if (FMath::Abs(dx) + FMath::Abs(dy) + FMath::Abs(dz) < 3)
						{
							Put(X + dx, dy, Height + 1 + dz, Tip, true);
						}
					}
				}
			}
		}
		else if (Kind < 0.55f)
		{
			// 관 산호: 높이가 다른 관 3~5개가 모여 서고, 관 끝마다 빛나는 칸 하나.
			const int32 Tubes = Rng.RandRange(3, 5);
			for (int32 t = 0; t < Tubes; ++t)
			{
				const int32 OX = Rng.RandRange(-2, 2);
				const int32 OY = Rng.RandRange(-2, 2);
				const int32 Height = Rng.RandRange(2, 6);
				for (int32 z = 0; z < Height; ++z)
				{
					Put(OX, OY, z, ShadeOf(Stem, H + t * 7 + z));
				}
				Put(OX, OY, Height, Tip, true);
			}
		}
		else if (Kind < 0.75f)
		{
			// 나선 덩굴: 위로 갈수록 좁아지는 나선을 그리며 오르고, 위 절반은 끝 색에 가깝게, 맨 끝은 빛난다.
			const int32 Height = Rng.RandRange(6, 10);
			const float Phase = Rng.FRandRange(0.f, UE_TWO_PI);
			const float Turn = Rng.FRand() < 0.5f ? 0.7f : -0.7f;
			const FLinearColor Upper = FMath::Lerp(Stem, Tip, 0.5f);
			for (int32 z = 0; z <= Height; ++z)
			{
				const float A = Phase + Turn * z;
				const float Rad = 1.6f * (1.f - 0.5f * z / Height);
				const int32 X = FMath::RoundToInt(FMath::Cos(A) * Rad);
				const int32 Y = FMath::RoundToInt(FMath::Sin(A) * Rad);
				if (z == Height)
				{
					Put(X, Y, z, Tip, true);
				}
				else
				{
					Put(X, Y, z, ShadeOf(z < Height / 2 ? Stem : Upper, H + z));
				}
			}
		}
		else if (Kind < 0.9f)
		{
			// 가시 꼬투리: 둥근 몸통(3x3x3, 모서리 뺌)에 사방과 위로 가시가 돋는다. 그림자를 드리운다.
			for (int32 dx = -1; dx <= 1; ++dx)
			{
				for (int32 dy = -1; dy <= 1; ++dy)
				{
					for (int32 dz = 0; dz <= 2; ++dz)
					{
						if (FMath::Abs(dx) + FMath::Abs(dy) + FMath::Abs(dz - 1) < 3)
						{
							AddVoxel(Frame, FVector(dx * V, dy * V, (dz + 0.5f) * V), V, ShadeOf(Pal.Shrub, H + dx * 7 + dy * 13 + dz), true);
						}
					}
				}
			}
			const int32 Spike = Rng.RandRange(1, 2);
			for (int32 s = 1; s <= Spike; ++s)
			{
				Put(1 + s, 0, 1, Tip);
				Put(-1 - s, 0, 1, Tip);
				Put(0, 1 + s, 1, Tip);
				Put(0, -1 - s, 1, Tip);
				Put(0, 0, 2 + s, Tip);
			}
		}
		else
		{
			// 갈래 식물: 짧은 줄기에서 양쪽과 가운데로 갈라져 비스듬히 오르고, 가지 끝 셋이 빛난다 (사슴뿔 / 부채 모양).
			Put(0, 0, 0, ShadeOf(Stem, H));
			Put(0, 0, 1, ShadeOf(Stem, H + 1));
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				for (int32 Step = 1; Step <= 3; ++Step)
				{
					Put(Side * Step, 0, 1 + Step, ShadeOf(Stem, H + Step));
				}
				Put(Side * 4, 0, 5, Tip, true);
			}
			for (int32 z = 2; z <= 4; ++z)
			{
				Put(0, 0, z, ShadeOf(Stem, H + z));
			}
			Put(0, 0, 5, Tip, true);
		}
	};
	for (int32 i = 0; i < InnerCount; ++i)
	{
		const FVector Dir = RandomDirNearPole(Inner);
		// 착륙 발판과 사장님 바로 밑(3m)은 비운다.
		if (FMath::Acos(FMath::Clamp(Dir.Z, -1.0, 1.0)) * R > 300.0)
		{
			PlaceTuft(Dir);
		}
	}
	for (int32 i = 0; i < RingCount; ++i)
	{
		const double Theta = FMath::Acos(FMath::Lerp(FMath::Cos(Inner), FMath::Cos(Outer), static_cast<double>(Rng.FRand())));
		const double Phi = Rng.FRandRange(0.f, UE_TWO_PI);
		PlaceTuft(FVector(FMath::Sin(Theta) * FMath::Cos(Phi), FMath::Sin(Theta) * FMath::Sin(Phi), FMath::Cos(Theta)));
	}
	for (int32 i = 0; i < FarCount; ++i)
	{
		PlaceTuft(RandomDirAway());
	}

	// 4. 하늘의 볼거리: 떠 있는 바위섬과 복셀 구름. 둘 다 행성을 둘러 떠 있다.
	//    바위섬은 복셀이 아니라 땅과 같은 방식의 메시로 만든다 (땅이 매끈하면 매끈하게, hg.PlanetFaceted 1이면 각지게): 윗면은 땅 색으로 살짝 울퉁불퉁하고,
	//    아래는 고리 몇 겹이 점점 좁아지며 비뚤게 내려가 뾰족한 바위 끝으로 모인다. 땅 조각이 떨어져 떠 있는 느낌.
	// 소행성 지대에는 떠 있는 바위섬 대신 뛰어 건널 소행성들이 있다 (BuildAsteroidField).
	const int32 Islands = (Planet.bAsteroidField || Planet.bDerelict) ? 0 : 9;
	const bool bFacetedIslands = CVarPlanetFaceted.GetValueOnGameThread() != 0;
	TArray<FVector> IslandVertices;
	TArray<FVector> IslandNormals;
	TArray<FVector2D> IslandUVs;
	TArray<FColor> IslandColors;
	TArray<int32> IslandTriangles;
	for (int32 i = 0; i < Islands; ++i)
	{
		const FVector Dir = i < 5 ? RandomDirNearPole(FMath::Min(1.2, 9000.0 / R)) : RandomDirAway();
		const float Size = Rng.FRandRange(250.f, 600.f);
		const FTransform Frame = SurfaceFrame(Dir, Rng.FRandRange(0.f, UE_TWO_PI), Rng.FRandRange(1800.f, 4000.f));

		// 섬 모양은 섬마다 따로 뽑는다 (공용 Rng를 쓰면 뒤의 구름 자리까지 바뀌므로 섬 번호로 만든 별도 수열을 쓴다).
		FRandomStream Shape(static_cast<int32>(HashCombine(GetTypeHash(Planet.Seed), GetTypeHash(i * 7919 + 17))));
		const int32 Sides = Shape.RandRange(8, 11);
		const float Depth = Size * Shape.FRandRange(1.1f, 1.6f);

		// 고리: 0 = 윗면 가운데 둘레(반지름 절반), 1 = 윗면 가장자리, 2 = 가장자리 바로 밑(흙 띠), 3~5 = 아래로 좁아지는 바위.
		// 각 고리는 (높이, 반지름 비율, 꼭짓점 흔들기 정도)로 정하고, 꼭짓점마다 반지름과 높이를 흔들어 각진 바위 느낌을 낸다.
		struct FRing { float Z; float Radius; float Jitter; };
		const FRing Rings[] = {
			{ Size * 0.10f, 0.50f, 0.15f },
			{ 0.f,          1.00f, 0.12f },
			{ -Size * 0.18f, 0.95f, 0.10f },
			{ -Depth * 0.40f, 0.70f, 0.18f },
			{ -Depth * 0.68f, 0.42f, 0.22f },
			{ -Depth * 0.88f, 0.18f, 0.30f },
		};
		const int32 RingNum = UE_ARRAY_COUNT(Rings);
		TArray<FVector> Points; // 고리 r의 꼭짓점 k = Points[r * Sides + k]
		Points.Reserve(RingNum * Sides);
		for (int32 r = 0; r < RingNum; ++r)
		{
			// 고리마다 조금씩 비틀어 아래 바위면이 반듯한 기둥처럼 보이지 않게 한다.
			const float Twist = r * Shape.FRandRange(0.f, 0.35f);
			for (int32 k = 0; k < Sides; ++k)
			{
				const float A = (k + Shape.FRandRange(-0.2f, 0.2f)) / Sides * UE_TWO_PI + Twist;
				const float Rad = Size * Rings[r].Radius * (1.f + Shape.FRandRange(-Rings[r].Jitter, Rings[r].Jitter));
				const float Z = Rings[r].Z + Size * Shape.FRandRange(-0.06f, 0.06f) * (r <= 1 ? 0.5f : 1.f);
				Points.Add(FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, Z));
			}
		}
		const FVector TopCenter(0.f, 0.f, Size * 0.14f);
		const FVector Tip(Shape.FRandRange(-0.15f, 0.15f) * Size, Shape.FRandRange(-0.15f, 0.15f) * Size, -Depth);
		const FVector Middle(0.f, 0.f, -Depth * 0.4f); // 바깥쪽 판단 기준 (모양의 대략 가운데)

		// 섬의 꼭짓점 목록: 0 = 윗면 가운데, 1 ~ = 고리 꼭짓점(고리 r의 k번째 = 1 + r * Sides + k), 마지막 = 바위 끝.
		// 꼭짓점마다 색: 윗면은 땅 두 색을 섞고, 가장자리 바로 밑은 어두운 흙, 그 아래 바위는 끝으로 갈수록 어둡다.
		TArray<FVector> Local;
		TArray<FLinearColor> LocalColor;
		Local.Reserve(RingNum * Sides + 2);
		LocalColor.Reserve(RingNum * Sides + 2);
		auto TopColor = [&]() { return Shape.FRand() < 0.65f ? Pal.Ground : Pal.GroundB; };
		Local.Add(TopCenter);
		LocalColor.Add(TopColor());
		for (int32 r = 0; r < RingNum; ++r)
		{
			for (int32 k = 0; k < Sides; ++k)
			{
				Local.Add(Points[r * Sides + k]);
				LocalColor.Add(r <= 1 ? TopColor() : r == 2 ? Pal.Ground * 0.7f : Pal.Rock * FMath::Lerp(1.f, 0.7f, (r - 3) / 2.f));
			}
		}
		const int32 TipIndex = Local.Num();
		Local.Add(Tip);
		LocalColor.Add(Pal.Rock * 0.6f);

		// 삼각형 하나 (꼭짓점 번호 셋): 면 법선이 바깥을 향하도록 감는 순서를 맞춘다. 앞면 법선 = (B - C) x (A - C) (지형과 같은 식).
		// 매끈한 땅(기본)이면 꼭짓점을 이웃 삼각형과 나눠 쓰고 법선을 면 법선의 합으로 부드럽게 만든다.
		// 각진 땅(hg.PlanetFaceted 1)이면 꼭짓점을 삼각형마다 따로 두고 면 법선, 면 색(세 꼭짓점 중 가장 높은 것의 색)으로 칠한다.
		const int32 Base = IslandVertices.Num();
		TArray<FVector> NormalSum;
		NormalSum.SetNumZeroed(Local.Num());
		int32 Face = 0;
		auto AddTri = [&](int32 IA, int32 IB, int32 IC)
		{
			FVector N = FVector::CrossProduct(Local[IB] - Local[IC], Local[IA] - Local[IC]);
			if ((N | ((Local[IA] + Local[IB] + Local[IC]) / 3.0 - Middle)) < 0.0)
			{
				Swap(IA, IB);
				N = -N;
			}
			if (bFacetedIslands)
			{
				int32 Top = IA;
				for (const int32 I : { IB, IC })
				{
					if (Local[I].Z > Local[Top].Z)
					{
						Top = I;
					}
				}
				const FColor Packed = ShadeOf(LocalColor[Top], static_cast<uint32>(i * 1000 + Face++) * 2654435761u).ToFColor(false);
				const FVector WorldN = Frame.TransformVectorNoScale(N.GetSafeNormal());
				for (const int32 I : { IA, IB, IC })
				{
					IslandTriangles.Add(IslandVertices.Num());
					IslandVertices.Add(Frame.TransformPosition(Local[I]));
					IslandNormals.Add(WorldN);
					IslandUVs.Add(FVector2D::ZeroVector);
					IslandColors.Add(Packed);
				}
			}
			else
			{
				// 넓은 삼각형일수록 법선에 크게 반영되도록 정규화하지 않은 외적을 더한다.
				for (const int32 I : { IA, IB, IC })
				{
					NormalSum[I] += N;
					IslandTriangles.Add(Base + I);
				}
			}
		};
		auto At = [&](int32 Ring, int32 K) { return 1 + Ring * Sides + (K % Sides); };

		// 윗면: 가운데 → 고리 0 → 고리 1.
		for (int32 k = 0; k < Sides; ++k)
		{
			AddTri(0, At(0, k), At(0, k + 1));
			AddTri(At(0, k), At(1, k), At(1, k + 1));
			AddTri(At(0, k), At(1, k + 1), At(0, k + 1));
		}
		// 옆과 아래: 고리 r → r + 1 띠, 마지막 고리 → 바위 끝.
		for (int32 r = 1; r + 1 < RingNum; ++r)
		{
			for (int32 k = 0; k < Sides; ++k)
			{
				AddTri(At(r, k), At(r + 1, k), At(r + 1, k + 1));
				AddTri(At(r, k), At(r + 1, k + 1), At(r, k + 1));
			}
		}
		for (int32 k = 0; k < Sides; ++k)
		{
			AddTri(At(RingNum - 1, k), TipIndex, At(RingNum - 1, k + 1));
		}
		if (!bFacetedIslands)
		{
			for (int32 v = 0; v < Local.Num(); ++v)
			{
				IslandVertices.Add(Frame.TransformPosition(Local[v]));
				IslandNormals.Add(Frame.TransformVectorNoScale(NormalSum[v].GetSafeNormal()));
				IslandUVs.Add(FVector2D::ZeroVector);
				FLinearColor Color = LocalColor[v] * (1.f + 0.06f * Shape.FRandRange(-1.f, 1.f));
				Color.A = 1.f;
				IslandColors.Add(Color.ToFColor(false));
			}
		}
	}
	if (IslandVertices.Num() > 0)
	{
		UProceduralMeshComponent* IslandMesh = NewObject<UProceduralMeshComponent>(this, TEXT("SkyIslands"), RF_Transient);
		IslandMesh->SetupAttachment(Terrain);
		IslandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		IslandMesh->SetCanEverAffectNavigation(false);
		IslandMesh->RegisterComponent();
		IslandMesh->CreateMeshSection(0, IslandVertices, IslandTriangles, IslandNormals, IslandUVs, IslandColors, TArray<FProcMeshTangent>(), false);
		IslandMesh->SetMaterial(0, Terrain->GetMaterial(0));
	}
	// 구름도 바위섬처럼 각진(로우폴리) 메시로 만든다. 구름 하나 = 낮은 해상도 구 덩어리 3~5개를 길게 겹친 것,
	// 밑면은 납작하게 눌러 평평한 구름 바닥을 만든다. 겹친 덩어리의 안쪽 면은 가려지므로 따로 지우지 않는다.
	// 기본은 구름 없음 (bSkyClouds): 우주에 떠 있는 작은 행성에 구름이 있으니 어색하다는 사용자 의견.
	const int32 Clouds = bSkyClouds ? FMath::Clamp(FMath::RoundToInt(AreaM2 / 2500.0), 12, 60) : 0;
	TArray<FVector> CloudVertices;
	TArray<FVector> CloudNormals;
	TArray<FVector2D> CloudUVs;
	TArray<int32> CloudTriangles;
	for (int32 i = 0; i < Clouds; ++i)
	{
		const FVector Dir = i < 8 ? RandomDirNearPole(FMath::Min(1.4, 12000.0 / R)) : RandomDirAway();
		const FTransform Frame = SurfaceFrame(Dir, Rng.FRandRange(0.f, UE_TWO_PI), Rng.FRandRange(4500.f, 7000.f));
		const FVector Ext(Rng.FRandRange(600.f, 1400.f), Rng.FRandRange(400.f, 800.f), Rng.FRandRange(150.f, 300.f));

		// 덩어리 배치는 구름 번호로 만든 따로 수열에서 뽑는다 (섬과 같은 이유).
		FRandomStream Shape(static_cast<int32>(HashCombine(GetTypeHash(Planet.Seed), GetTypeHash(i * 104729 + 31))));
		const int32 Lumps = Shape.RandRange(3, 5);
		const float Floor = -Ext.Z * 0.45f; // 이보다 아래 꼭짓점은 이 높이로 눌러 납작한 바닥을 만든다
		for (int32 l = 0; l < Lumps; ++l)
		{
			// 덩어리는 구름의 긴 축(X)을 따라 늘어서고, 가운데 것이 가장 크고 높다.
			const float T = Lumps == 1 ? 0.f : static_cast<float>(l) / (Lumps - 1) * 2.f - 1.f; // -1 ~ 1
			const float Big = 1.f - 0.45f * FMath::Abs(T);
			const FVector Center(T * Ext.X * 0.6f, Shape.FRandRange(-0.25f, 0.25f) * Ext.Y, Ext.Z * 0.15f * Big);
			const FVector Radius(Ext.X * 0.45f * Big, Ext.Y * Shape.FRandRange(0.6f, 0.9f) * Big, Ext.Z * Shape.FRandRange(0.8f, 1.1f) * Big);

			// 낮은 해상도 구: 둘레 8칸, 위아래 5칸. 꼭짓점을 조금씩 흔들어 반듯한 구가 아니게 한다.
			const int32 Seg = 8;
			const int32 Stack = 5;
			TArray<FVector> Grid;
			Grid.Reserve((Stack + 1) * Seg);
			for (int32 s = 0; s <= Stack; ++s)
			{
				const float Phi = UE_PI * s / Stack; // 0 = 꼭대기, PI = 바닥
				for (int32 k = 0; k < Seg; ++k)
				{
					const float Theta = UE_TWO_PI * (k + (s % 2) * 0.5f) / Seg;
					const float J = (s == 0 || s == Stack) ? 1.f : Shape.FRandRange(0.85f, 1.12f);
					FVector P = Center + FVector(FMath::Sin(Phi) * FMath::Cos(Theta) * Radius.X, FMath::Sin(Phi) * FMath::Sin(Theta) * Radius.Y, FMath::Cos(Phi) * Radius.Z) * J;
					P.Z = FMath::Max(P.Z, Floor);
					Grid.Add(P);
				}
			}
			auto G = [&](int32 S, int32 K) { return Grid[S * Seg + ((K % Seg) + Seg) % Seg]; };
			auto AddCloudTri = [&](FVector A, FVector B, FVector C)
			{
				FVector N = FVector::CrossProduct(B - C, A - C).GetSafeNormal();
				if (N.IsNearlyZero())
				{
					return; // 꼭대기/바닥에 모인 점끼리 생긴 납작한 삼각형
				}
				if ((N | ((A + B + C) / 3.0 - Center)) < 0.0)
				{
					Swap(A, B);
					N = -N;
				}
				const FVector WorldN = Frame.TransformVectorNoScale(N);
				for (const FVector& P : { A, B, C })
				{
					CloudTriangles.Add(CloudVertices.Num());
					CloudVertices.Add(Frame.TransformPosition(P));
					CloudNormals.Add(WorldN);
					CloudUVs.Add(FVector2D::ZeroVector);
				}
			};
			for (int32 s = 0; s < Stack; ++s)
			{
				// 홀수 줄은 반 칸 어긋나 있어서 위아래 줄을 지그재그 삼각형으로 잇는다.
				const int32 Shift = s % 2;
				for (int32 k = 0; k < Seg; ++k)
				{
					AddCloudTri(G(s, k), G(s + 1, k - 1 + Shift), G(s + 1, k + Shift));
					AddCloudTri(G(s, k), G(s + 1, k + Shift), G(s, k + 1));
				}
			}
		}
	}
	if (CloudVertices.Num() > 0)
	{
		UProceduralMeshComponent* CloudMesh = NewObject<UProceduralMeshComponent>(this, TEXT("SkyClouds"), RF_Transient);
		CloudMesh->SetupAttachment(Terrain);
		CloudMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CloudMesh->SetCanEverAffectNavigation(false);
		CloudMesh->SetCastShadow(false);
		CloudMesh->RegisterComponent();
		CloudMesh->CreateMeshSection(0, CloudVertices, CloudTriangles, CloudNormals, CloudUVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		// 구름은 살짝 스스로 빛나게 칠해서(발광 머티리얼) 아래에서 올려다봐도 검게 보이지 않게 한다.
		HGVoxel::PaintGlow(CloudMesh, FLinearColor(0.95f, 0.96f, 1.f), CloudGlow);
	}

	int32 Instances = 0;
	for (const TPair<uint32, TArray<FTransform>>& Batch : Batches)
	{
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Ism->SetupAttachment(Terrain);
		Ism->SetStaticMesh(Cube);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetCanEverAffectNavigation(false);
		Ism->SetCastShadow(BatchShadow[Batch.Key]);
		if (BatchNoOutline[Batch.Key])
		{
			// 스텐실 1을 찍어 둔다. 그림체 비교 머티리얼(M_HG_LookToon/Pixel)이 스텐실 1인 점은 윤곽선을 건너뛴다.
			// 커스텀 깊이 패스에는 셀/픽셀 그림체가 켜져 있을 때만 그린다 (식물 수천 포기를 한 번 더 그리는 비용).
			// 그림체를 바꾸면 컨트롤러(AHGPlayerControllerBase::ApplyLook)가 켜고 끈다.
			// 스텐실을 쓰려면 프로젝트 설정 r.CustomDepth=3(스텐실 포함)이 필요하다 (Config/DefaultEngine.ini).
			Ism->SetCustomDepthStencilValue(HGLook::NoOutlineStencil);
			Ism->SetRenderCustomDepth(HGLook::NeedsStencil());
		}
		Ism->RegisterComponent();
		Ism->AddInstances(Batch.Value, false, false);
		if (BatchGlow[Batch.Key])
		{
			HGVoxel::PaintGlow(Ism, BatchColor[Batch.Key], FloraGlow);
		}
		else
		{
			HGPlaceholder::Paint(Ism, BatchColor[Batch.Key]);
		}
		Instances += Batch.Value.Num();
	}

	// 5. 하늘 돔: 행성을 감싼 큰 구 (안에서 보는 빛 안 받는 하늘 머티리얼). "위"를 행성 중심에서 계산하므로 행성 어디서든 지평선이 맞다.
	//    해(첫 방향광) 방향을 넘겨 해 둘레가 밝게 빛나게 하고, 보조광은 하늘 색으로 물들여 그늘이 하늘빛을 받게 한다.
	FVector SunDir = FVector::UpVector;
	bool bSun = true;
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
		{
			if (bSun)
			{
				SunDir = -It->GetActorForwardVector();
				Light->SetLightColor(FMath::Lerp(FLinearColor::White, Pal.Horizon, 0.25f));
				bSun = false;
			}
			else
			{
				Light->SetLightColor(Pal.Zenith);
				// 소행성 지대는 하늘이 거의 검어서 하늘빛 간접광이 약하다. 해 반대쪽 면이 새까만 실루엣이 되지 않게 보조광을 더 세게 둔다.
				Light->SetIntensity(Light->Intensity * ((Planet.bAsteroidField || Planet.bDerelict) ? 4.f : 1.6f));
			}
		}
	}
	if (UMaterialInterface* SkyMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/HG/Materials/M_HG_PlanetSky.M_HG_PlanetSky")))
	{
		UStaticMeshComponent* Dome = NewObject<UStaticMeshComponent>(this, TEXT("SkyDome"), RF_Transient);
		Dome->SetupAttachment(Terrain);
		Dome->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Dome->SetRelativeScale3D(FVector(SkyDomeRadius / 50.f));
		Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Dome->SetCastShadow(false);
		Dome->SetCanEverAffectNavigation(false);
		Dome->bAffectDistanceFieldLighting = false;
		Dome->RegisterComponent();
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(SkyMaterial, Dome);
		// 하늘 머티리얼은 "Center에서 보는 사람 쪽 = 위"로 하늘 색을 나눈다. 평지 행성이면 땅 아래 먼 중력 중심을 넣어 어디서나 위 = +Z가 되게 한다.
		Mid->SetVectorParameterValue(TEXT("Center"), FLinearColor(GetGravityCenter()));
		// 하늘 꼭대기는 우주색 쪽으로 섞는다 (대기가 얇아 우주가 비쳐 보이고 별이 잘 보인다). 지평선 색은 그대로.
		// 소행성 지대는 대기가 없는 우주 한가운데라 하늘을 거의 우주색으로 칠한다 (지평선도 어둡게).
		const FLinearColor Space(0.02f, 0.02f, 0.07f);
		const bool bOpenSpace = Planet.bAsteroidField || Planet.bDerelict;
		Mid->SetVectorParameterValue(TEXT("Zenith"), FMath::Lerp(Pal.Zenith, Space, bOpenSpace ? 0.92f : SpaceTint));
		Mid->SetVectorParameterValue(TEXT("Horizon"), bOpenSpace ? FMath::Lerp(Pal.Horizon, Space, 0.93f) : Pal.Horizon);
		Mid->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(SunDir));
		Mid->SetScalarParameterValue(TEXT("Brightness"), SkyBrightness);
		Dome->SetMaterial(0, Mid);
	}
	else
	{
		UE_LOG(LogHuoguo, Error, TEXT("[Mood] 하늘 머티리얼 M_HG_PlanetSky가 없다. Tools/Invoke-HGMapGen.ps1을 실행할 것"));
	}

	// 하늘빛 간접광: 하늘 돔 머티리얼이 하늘(Is Sky)로 표시되어 있어서, 실시간 캡처 스카이 라이트가 돔 색을 그늘에 비춘다.
	// 이게 없으면 구름 아래, 바위섬 밑, 나무 그늘이 거의 검게 보였다.
	FActorSpawnParameters LightParams;
	LightParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ASkyLight* SkyLight = GetWorld()->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform(GetActorLocation()), LightParams))
	{
		USkyLightComponent* SkyComp = SkyLight->GetLightComponent();
		SkyComp->SetMobility(EComponentMobility::Movable);
		SkyComp->bRealTimeCapture = true;
		SkyComp->SetIntensity(SkyLightIntensity);
		SkyComp->RecaptureSky();
	}

	// 6. 우주가 보이는 하늘(별, 고리 행성, 위성, 정거장)과 지형 볼거리(탁상 바위, 아치).
	BuildSkyBodies();
	const int32 Landmarks = (Planet.bAsteroidField || Planet.bDerelict) ? 0 : BuildLandmarks();
	UE_LOG(LogHuoguo, Log, TEXT("[Mood] 하늘: 별 %d개, 고리 행성/위성/정거장 %s. 지형 볼거리 %d개, 크레이터 %d개"),
		StarCount, bSkyBodies ? TEXT("있음") : TEXT("없음"), Landmarks, Craters.Num());

	UE_LOG(LogHuoguo, Log, TEXT("[Mood] 행성 그림체: 군계 %d, 바위 칸 %d, 나무 %d그루(칸 %d), 풀과 꽃 %d포기, 바위섬 %d개(삼각형 %d), 구름 %d개(삼각형 %d), 복셀 인스턴스 %d, 색 묶음 %d"),
		static_cast<int32>(Planet.Biome), RockVoxels, NearTrees + FarTrees, TreeVoxels, InnerCount + RingCount + FarCount,
		Islands, IslandTriangles.Num() / 3, Clouds, CloudTriangles.Num() / 3, Instances, Batches.Num());
}

void AHGPlanetGenerator::BuildSkyBodies()
{
	const FMoodPalette Pal = MoodPalette(Planet.Biome);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// 하늘은 따로 수열에서 뽑는다 (지형/소품의 자리가 바뀌지 않게).
	FRandomStream Sky(static_cast<int32>(HashCombine(GetTypeHash(Planet.Seed), 0x5c7u)));

	// 하늘 물체 공통: 충돌, 그림자, 거리장 조명 없음. 그림자를 끄지 않으면 거대한 행성이 해를 가려 땅이 통째로 어두워질 수 있다.
	auto Prepare = [this](UPrimitiveComponent* Comp)
	{
		Comp->SetupAttachment(Terrain);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetCastShadow(false);
		Comp->bAffectDistanceFieldLighting = false;
		Comp->RegisterComponent();
	};
	auto MakeIsm = [&](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this, Name, RF_Transient);
		Ism->SetStaticMesh(Cube);
		Prepare(Ism);
		return Ism;
	};
	auto MakeBall = [&](const TCHAR* Name, const FVector& Location, float Radius, const FLinearColor& Color)
	{
		UStaticMeshComponent* Ball = NewObject<UStaticMeshComponent>(this, Name, RF_Transient);
		Ball->SetStaticMesh(Sphere);
		Ball->SetRelativeLocation(Location);
		Ball->SetRelativeScale3D(FVector(Radius / 50.f)); // 엔진 구는 지름 1m
		Prepare(Ball);
		HGPlaceholder::Paint(Ball, Color);
		return Ball;
	};

	// 1) 별: 하늘 돔 바로 안쪽 구면에 작은 발광 정육면체를 흩는다 (4 ~ 12m, 착륙 지점에서 1 ~ 3픽셀 크기).
	//    하늘 꼭대기를 우주색 쪽으로 어둡게 한 것(SpaceTint)과 같이 낮에도 별이 보여 "우주 한가운데" 느낌을 준다.
	if (StarCount > 0)
	{
		UInstancedStaticMeshComponent* Stars = MakeIsm(TEXT("SkyStars"));
		TArray<FTransform> Transforms;
		Transforms.Reserve(StarCount);
		const float Dist = SkyDomeRadius * 0.95f;
		for (int32 i = 0; i < StarCount; ++i)
		{
			const FVector Dir = Sky.VRand();
			const FRotator Spin(Sky.FRandRange(0.f, 360.f), Sky.FRandRange(0.f, 360.f), 0.f);
			Transforms.Add(FTransform(Spin.Quaternion(), Dir * Dist, FVector(Sky.FRandRange(4.f, 12.f))));
		}
		Stars->AddInstances(Transforms, false, false);
		HGVoxel::PaintGlow(Stars, FLinearColor(1.f, 0.96f, 0.88f), StarGlow);
	}
	if (!bSkyBodies)
	{
		return;
	}

	// 착륙 지점(북극)에서 올려다보기 좋은 방향: 천정에서 Tilt도 기울이고 둘레 Azimuth도 돌린 방향.
	auto SkyDir = [](float TiltDeg, float AzimuthDeg)
	{
		const float T = FMath::DegreesToRadians(TiltDeg);
		const float A = FMath::DegreesToRadians(AzimuthDeg);
		return FVector(FMath::Sin(T) * FMath::Cos(A), FMath::Sin(T) * FMath::Sin(A), FMath::Cos(T));
	};
	const float BaseAzimuth = Sky.FRandRange(0.f, 360.f);

	// 2) 고리 행성: 천정에서 35 ~ 50도, 돔 반지름의 0.75배 거리에 돔 0.2배 반지름 (착륙 지점에서 시야각 약 30도).
	//    색은 군계 꽃 색을 밝힌 것이라 땅/하늘과 대비된다. 해 쪽 반만 밝게 보여 초승달처럼 명암이 생긴다.
	//    고리는 기울인 평면에 작은 알갱이 2600개를 두 띠로 흩는다 (가운데가 비어 진짜 고리처럼 보이고, 알갱이가 작아 먼지 띠처럼 읽힌다).
	const FVector GiantDir = SkyDir(Sky.FRandRange(35.f, 50.f), BaseAzimuth);
	const FVector GiantPos = GiantDir * (SkyDomeRadius * 0.75f);
	const float GiantR = SkyDomeRadius * 0.2f;
	const FLinearColor GiantColor = FMath::Lerp(Pal.Flower[1], FLinearColor::White, 0.3f);
	MakeBall(TEXT("SkyGiant"), GiantPos, GiantR, GiantColor);
	{
		const FQuat RingRot = FRotator(Sky.FRandRange(15.f, 35.f), Sky.FRandRange(0.f, 360.f), 0.f).Quaternion();
		UInstancedStaticMeshComponent* Inner = MakeIsm(TEXT("SkyRingInner"));
		UInstancedStaticMeshComponent* Outer = MakeIsm(TEXT("SkyRingOuter"));
		TArray<FTransform> InnerT, OuterT;
		for (int32 i = 0; i < 2600; ++i)
		{
			const float T = Sky.FRand();
			const float RingRad = GiantR * FMath::Lerp(1.45f, 2.1f, T);
			const float Angle = Sky.FRandRange(0.f, UE_TWO_PI);
			const FVector Local(FMath::Cos(Angle) * RingRad, FMath::Sin(Angle) * RingRad, Sky.FRandRange(-0.01f, 0.01f) * GiantR);
			const FRotator Spin(Sky.FRandRange(0.f, 360.f), Sky.FRandRange(0.f, 360.f), 0.f);
			const FTransform Piece(Spin.Quaternion(), GiantPos + RingRot.RotateVector(Local), FVector(GiantR * Sky.FRandRange(0.008f, 0.02f) / 100.f));
			(T < 0.45f ? InnerT : OuterT).Add(Piece);
		}
		Inner->AddInstances(InnerT, false, false);
		Outer->AddInstances(OuterT, false, false);
		HGPlaceholder::Paint(Inner, FMath::Lerp(GiantColor, FLinearColor::White, 0.5f));
		HGPlaceholder::Paint(Outer, GiantColor * 0.75f);
	}

	// 3) 위성 둘: 지평선 가까이(55 ~ 75도), 고리 행성과 다른 쪽.
	for (int32 m = 0; m < 2; ++m)
	{
		const FVector Dir = SkyDir(Sky.FRandRange(55.f, 75.f), BaseAzimuth + 140.f + m * 90.f + Sky.FRandRange(-20.f, 20.f));
		const FLinearColor Color = m == 0 ? FMath::Lerp(Pal.Rock, FLinearColor::White, 0.45f) : FMath::Lerp(Pal.GroundB, FLinearColor::White, 0.25f);
		MakeBall(m == 0 ? TEXT("SkyMoon0") : TEXT("SkyMoon1"), Dir * (SkyDomeRadius * 0.8f), SkyDomeRadius * Sky.FRandRange(0.03f, 0.06f), Color);
	}

	// 4) 정거장: 천정 가까이(15 ~ 25도) 고리 행성 반대쪽에 떠 있는 우리 가게. 고리 + 가운데 축 + 바퀴살 넷 + 고리 둘레 불빛.
	//    "정거장에서 다이빙해 내려왔다"는 이야기를 하늘에서 보여 준다. 고리 면을 행성 쪽으로 35도 기울여 둥근 모양이 보이게 한다.
	{
		const FVector StationDir = SkyDir(Sky.FRandRange(15.f, 25.f), BaseAzimuth + 180.f);
		const FQuat StationRot = FRotationMatrix::MakeFromZ(-StationDir).ToQuat() * FQuat(FVector::XAxisVector, FMath::DegreesToRadians(35.f));
		const FTransform Station(StationRot, StationDir * (SkyDomeRadius * 0.4f));
		const float S = SkyDomeRadius * 0.02f; // 고리 반지름
		TArray<FTransform> Body, LightsA, LightsB;
		auto Box = [&](TArray<FTransform>& Out, const FVector& Pos, const FVector& Size, float Yaw)
		{
			Out.Add(FTransform(FRotator(0.f, Yaw, 0.f).Quaternion(), Pos, Size / 100.f) * Station);
		};
		const int32 Segments = 28;
		for (int32 k = 0; k < Segments; ++k)
		{
			const float Angle = UE_TWO_PI * k / Segments;
			const FVector P(FMath::Cos(Angle) * S, FMath::Sin(Angle) * S, 0.f);
			Box(Body, P, FVector(0.25f * S, UE_TWO_PI * S / Segments * 1.1f, 0.18f * S), FMath::RadiansToDegrees(Angle));
			if (k % 4 == 0)
			{
				Box((k / 4) % 2 == 0 ? LightsA : LightsB, P * 1.14f, FVector(0.08f * S), 0.f);
			}
		}
		Box(Body, FVector::ZeroVector, FVector(0.35f * S, 0.35f * S, 0.6f * S), 0.f);
		for (int32 k = 0; k < 4; ++k)
		{
			const float Angle = UE_HALF_PI * k;
			Box(Body, FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (0.5f * S), FVector(S, 0.06f * S, 0.06f * S), FMath::RadiansToDegrees(Angle));
		}
		UInstancedStaticMeshComponent* BodyIsm = MakeIsm(TEXT("SkyStation"));
		UInstancedStaticMeshComponent* LightAIsm = MakeIsm(TEXT("SkyStationLightA"));
		UInstancedStaticMeshComponent* LightBIsm = MakeIsm(TEXT("SkyStationLightB"));
		BodyIsm->AddInstances(Body, false, false);
		LightAIsm->AddInstances(LightsA, false, false);
		LightBIsm->AddInstances(LightsB, false, false);
		HGPlaceholder::Paint(BodyIsm, FLinearColor(0.62f, 0.66f, 0.74f));
		HGVoxel::PaintGlow(LightAIsm, FLinearColor(1.f, 0.25f, 0.2f), 8.f);   // 마라 빨강 등불
		HGVoxel::PaintGlow(LightBIsm, FLinearColor(0.2f, 0.95f, 0.9f), 8.f);  // 산소 청록
	}
}

int32 AHGPlanetGenerator::BuildLandmarks()
{
	if (LandmarkCount <= 0)
	{
		return 0;
	}
	const FMoodPalette Pal = MoodPalette(Planet.Biome);
	FRandomStream L(static_cast<int32>(HashCombine(GetTypeHash(Planet.Seed), 0x1a2bu)));
	const double R = Planet.Radius;

	// 모든 볼거리를 메시 하나에 모은다. 볼거리 하나는 지역 좌표(Frame 기준)로 꼭짓점을 만들고 Flush로 옮겨 담는다.
	// 꼭짓점은 이웃 삼각형과 나눠 쓰고, 법선은 면 법선의 합(넓은 면일수록 크게)으로 부드럽게 만든다 (매끈한 땅과 같은 방식).
	TArray<FVector> Verts, Norms;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<int32> Tris;
	TArray<FVector> V, NSum;
	TArray<FLinearColor> Col;
	TArray<int32> LocalTris;
	auto AddVert = [&](const FVector& P, const FLinearColor& C)
	{
		V.Add(P);
		NSum.Add(FVector::ZeroVector);
		Col.Add(C);
		return V.Num() - 1;
	};
	// Inside = 이 삼각형 안쪽의 한 점. 면 법선이 그 반대(바깥)를 향하도록 감는 순서를 맞춘다.
	auto AddTri = [&](int32 A, int32 B, int32 C, const FVector& Inside)
	{
		FVector N = FVector::CrossProduct(V[B] - V[C], V[A] - V[C]);
		if ((N | ((V[A] + V[B] + V[C]) / 3.0 - Inside)) < 0.0)
		{
			Swap(A, B);
			N = -N;
		}
		NSum[A] += N;
		NSum[B] += N;
		NSum[C] += N;
		LocalTris.Append({ A, B, C });
	};
	auto Flush = [&](const FTransform& Frame)
	{
		const int32 Base = Verts.Num();
		for (int32 i = 0; i < V.Num(); ++i)
		{
			Verts.Add(Frame.TransformPosition(V[i]));
			Norms.Add(Frame.TransformVectorNoScale(NSum[i].GetSafeNormal()));
			UVs.Add(FVector2D::ZeroVector);
			FLinearColor C = Col[i] * (1.f + 0.06f * L.FRandRange(-1.f, 1.f));
			C.A = 1.f;
			Colors.Add(C.ToFColor(false));
		}
		for (const int32 T : LocalTris)
		{
			Tris.Add(Base + T);
		}
		V.Reset();
		NSum.Reset();
		Col.Reset();
		LocalTris.Reset();
	};
	// 지층 색: 높이 1.5m마다 바위색과 (바위 + 땅 얼룩) 색이 번갈아 나온다. 외계 사암 절벽 같은 줄무늬.
	auto StrataColor = [&](float Z)
	{
		return (FMath::FloorToInt(Z / 150.f) & 1) ? FMath::Lerp(Pal.Rock, Pal.GroundB, 0.5f) : Pal.Rock * 1.08f;
	};

	// 탁상 바위(메사): 고리 일곱 겹이 조금씩 좁아지며 오르다 꼭대기 바로 밑에서 살짝 튀어나오고(처마), 평평한 윗면은 땅색.
	// 맨 아래 고리는 땅속(높이의 -15%)이라 비탈에 놓여도 바닥이 뜨지 않는다.
	auto BuildMesa = [&](float Base, float Height)
	{
		const int32 Sides = L.RandRange(8, 11);
		struct FRing { float Z; float Rad; float Jit; };
		const FRing Rings[] = {
			{ -0.15f * Height, 1.15f, 0.10f }, { 0.f, 1.10f, 0.10f }, { 0.30f * Height, 0.95f, 0.12f }, { 0.55f * Height, 0.88f, 0.12f },
			{ 0.80f * Height, 0.84f, 0.10f }, { 0.96f * Height, 0.92f, 0.08f }, { Height, 0.84f, 0.05f },
		};
		const int32 RingNum = UE_ARRAY_COUNT(Rings);
		const float Twist = L.FRandRange(0.f, 0.3f);
		for (int32 r = 0; r < RingNum; ++r)
		{
			for (int32 k = 0; k < Sides; ++k)
			{
				const float A = (k + L.FRandRange(-0.15f, 0.15f)) / Sides * UE_TWO_PI + Twist * r;
				const float Rad = Base * Rings[r].Rad * (1.f + L.FRandRange(-Rings[r].Jit, Rings[r].Jit));
				const float Z = Rings[r].Z + (r == RingNum - 1 ? 0.f : L.FRandRange(-30.f, 30.f));
				AddVert(FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, Z), r == RingNum - 1 ? Pal.Ground : StrataColor(Z));
			}
		}
		const int32 Cap = AddVert(FVector(0.f, 0.f, Height * 1.03f), Pal.Ground);
		auto At = [&](int32 Ring, int32 K) { return Ring * Sides + (K % Sides); };
		for (int32 r = 0; r + 1 < RingNum; ++r)
		{
			for (int32 k = 0; k < Sides; ++k)
			{
				const FVector Inside(0.f, 0.f, (Rings[r].Z + Rings[r + 1].Z) * 0.5f);
				AddTri(At(r, k), At(r + 1, k), At(r + 1, k + 1), Inside);
				AddTri(At(r, k), At(r + 1, k + 1), At(r, k + 1), Inside);
			}
		}
		for (int32 k = 0; k < Sides; ++k)
		{
			AddTri(Cap, At(RingNum - 1, k), At(RingNum - 1, k + 1), FVector(0.f, 0.f, Height * 0.9f));
		}
	};

	// 아치: 반원 길을 따라 굵은 관을 쓸어 만든다. 발(양 끝)은 1m 땅속이고, 발 쪽이 굵고 꼭대기가 가늘다.
	// 관의 단면 꼭짓점을 조금씩 흔들어 바위처럼 울퉁불퉁하게 한다.
	auto BuildArch = [&](float Span, float Height, float Thick)
	{
		const int32 Along = 16;
		const int32 Around = 8;
		TArray<FVector> Centers;
		for (int32 s = 0; s <= Along; ++s)
		{
			const float Angle = UE_PI * s / Along;
			const FVector C(-0.5f * Span * FMath::Cos(Angle), 0.f, Height * FMath::Sin(Angle) - 100.f);
			const FVector Tangent = FVector(0.5f * Span * FMath::Sin(Angle), 0.f, Height * FMath::Cos(Angle)).GetSafeNormal();
			const FVector U = FVector::YAxisVector;
			const FVector W = FVector::CrossProduct(Tangent, U).GetSafeNormal();
			const float Rad = Thick * (1.35f - 0.45f * FMath::Sin(Angle));
			Centers.Add(C);
			for (int32 a = 0; a < Around; ++a)
			{
				const float B = UE_TWO_PI * a / Around;
				const FVector P = C + (U * FMath::Cos(B) + W * FMath::Sin(B)) * Rad * (1.f + L.FRandRange(-0.08f, 0.08f));
				AddVert(P, StrataColor(P.Z));
			}
		}
		for (int32 s = 0; s < Along; ++s)
		{
			const FVector Inside = (Centers[s] + Centers[s + 1]) * 0.5f;
			for (int32 a = 0; a < Around; ++a)
			{
				const int32 A0 = s * Around + a;
				const int32 A1 = s * Around + (a + 1) % Around;
				const int32 B0 = (s + 1) * Around + a;
				const int32 B1 = (s + 1) * Around + (a + 1) % Around;
				AddTri(A0, B0, B1, Inside);
				AddTri(A0, B1, A1, Inside);
			}
		}
	};

	// 자리 고르기: 처음 둘은 착륙 지점에서 25 ~ 60m(걸어가 볼 만한 거리, 착륙하자마자 보인다), 나머지는 행성 아무 데나.
	// 착륙 지점, 귀환 발판, 재료 표식 위에는 세우지 않는다 (재료가 바위 속에 묻혀 못 캐는 일을 막는다).
	int32 Built = 0;
	for (int32 i = 0; i < LandmarkCount; ++i)
	{
		for (int32 Try = 0; Try < 12; ++Try)
		{
			const bool bArch = L.FRand() < 0.4f;
			const float Base = L.FRandRange(300.f, 650.f);
			const float Height = bArch ? L.FRandRange(600.f, 1200.f) : L.FRandRange(600.f, 1600.f);
			const float Span = L.FRandRange(800.f, 1600.f);
			const float Thick = L.FRandRange(120.f, 200.f);
			const float Footprint = bArch ? Span * 0.5f + Thick * 1.4f + 150.f : Base * 1.2f + 150.f;

			FVector Dir;
			if (i < 2)
			{
				const double Theta = FMath::Min(1.2, FMath::Lerp(2500.0, 6000.0, static_cast<double>(L.FRand())) / R);
				const double Phi = L.FRandRange(0.f, UE_TWO_PI);
				Dir = FVector(FMath::Sin(Theta) * FMath::Cos(Phi), FMath::Sin(Theta) * FMath::Sin(Phi), FMath::Cos(Theta));
			}
			else
			{
				Dir = L.VRand();
			}
			if (FMath::Acos(FMath::Clamp(Dir.Z, -1.0, 1.0)) * R < 1500.0 + Footprint)
			{
				continue;
			}
			const FTransform Frame = SurfaceFrame(Dir, L.FRandRange(0.f, UE_TWO_PI), bArch ? -120.f : -60.f);
			const FVector WorldPos = GetActorLocation() + Frame.GetLocation();
			if (bHasReturnPad && FVector::Dist(WorldPos, ReturnPadLocation) < Footprint + 600.f)
			{
				continue;
			}
			bool bBlocked = false;
			for (const FHGIngredientMarker& Marker : Markers)
			{
				if (FVector::Dist(WorldPos, Marker.Location) < Footprint)
				{
					bBlocked = true;
					break;
				}
			}
			if (bBlocked)
			{
				continue;
			}

			if (bArch)
			{
				BuildArch(Span, Height, Thick);
			}
			else
			{
				BuildMesa(Base, Height);
			}
			Flush(Frame);
			LandmarkLocations.Add(WorldPos);
			UE_LOG(LogHuoguo, Log, TEXT("[Landmark] %d: %s, 착륙 지점에서 %.0fm"), Built, bArch ? TEXT("아치") : TEXT("탁상 바위"),
				FMath::Acos(FMath::Clamp(Dir.Z, -1.0, 1.0)) * R / 100.0);
			++Built;
			break;
		}
	}

	if (Verts.Num() > 0)
	{
		// 지형과 같은 충돌 설정이라 올라서고 부딪힐 수 있다 (사장님이 탁상 위로 점프해 올라가거나 아치 밑을 지나간다).
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, TEXT("Landmarks"), RF_Transient);
		Mesh->SetupAttachment(Terrain);
		Mesh->SetCollisionProfileName(Terrain->GetCollisionProfileName());
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->RegisterComponent();
		Mesh->CreateMeshSection(0, Verts, Tris, Norms, UVs, Colors, TArray<FProcMeshTangent>(), true);
		Mesh->SetMaterial(0, Terrain->GetMaterial(0));
	}
	return Built;
}

void AHGPlanetGenerator::BuildAsteroidField()
{
	// 0번 천체는 늘 이 행성(소행성 지대면 착륙하는 본 소행성).
	GravityBodies.Reset();
	FHGGravityBody Main;
	// 평지 행성은 땅 아래 먼 곳을 중심으로 둔 아주 큰 구처럼 다룬다 (GetGravityCenter).
	Main.Center = GetGravityCenter();
	Main.Radius = GetGravityRadius();
	Main.SurfaceGravity = Planet.GravityScale;
	GravityBodies.Add(Main);
	if (!Planet.bAsteroidField)
	{
		return;
	}

	FRandomStream Rng(static_cast<int32>(HashCombine(GetTypeHash(Planet.Seed), 0xa5f1e1du)));
	const FMoodPalette Pal = MoodPalette(Planet.Biome);

	// 둘레 소행성 하나: 중심(행성 중심 기준), 평균 반지름, 축마다 늘인 비율(찌그러진 감자 모양), 노이즈 이동값.
	struct FRock
	{
		FVector Center;
		float Radius;
		FVector Squash;
		FVector Offset;
	};
	TArray<FRock> Rocks;

	// 배치 (플랫폼 게임식, 정해진 길 없음): 이미 놓인 소행성 하나에서 지표 사이 2.5 ~ 9m에 새 소행성을 붙여 나간다.
	// 가까운 것(2.5 ~ 5m)은 1단 점프로, 먼 것(~9m)은 2단/3단 점프나 제트팩으로 건너간다. 어느 소행성이든 이웃에서 뛰어 닿을 수 있다.
	// 발사대 같은 정해진 길은 두지 않는다 (사용자 결정: 너무 선형적이라 자유를 해친다).
	// Centers/Radii의 0번은 본 소행성이고, i번(1 ~)은 Rocks[i - 1]이다. 중력 천체 번호도 같은 순서다.
	TArray<FVector> Centers = { FVector::ZeroVector };
	TArray<float> Radii = { Planet.Radius };

	// Parent에서 지표 사이 GapMin ~ GapMax 떨어진 곳에 소행성 하나를 놓아 본다. Bias 쪽으로 치우친다. 실패하면 INDEX_NONE.
	auto TryPlace = [&](int32 Parent, float GapMin, float GapMax, const FVector& Bias)
	{
		for (int32 Try = 0; Try < 40; ++Try)
		{
			const float Radius = Rng.FRand() < 0.15f ? Rng.FRandRange(700.f, 1000.f) : Rng.FRandRange(300.f, 650.f);
			const float Gap = Rng.FRandRange(GapMin, GapMax);
			const FVector Dir = (Rng.VRand() + Bias * 0.8f).GetSafeNormal();
			const FVector Center = Centers[Parent] + Dir * (Radii[Parent] + Gap + Radius);
			if (Center.Size() > 15000.f)
			{
				continue;
			}
			// 다른 소행성과 겹치지 않게 (늘인 모양과 노이즈를 생각해 반지름을 15% 크게 잡는다).
			bool bClear = true;
			for (int32 j = 0; j < Centers.Num() && bClear; ++j)
			{
				bClear = j == Parent || FVector::Dist(Center, Centers[j]) > (Radius + Radii[j]) * 1.15f + 200.f;
			}
			if (!bClear)
			{
				continue;
			}
			FRock Rock;
			Rock.Center = Center;
			Rock.Radius = Radius;
			Rock.Squash = FVector(Rng.FRandRange(0.85f, 1.15f), Rng.FRandRange(0.85f, 1.15f), Rng.FRandRange(0.8f, 1.05f));
			Rock.Offset = FVector(Rng.FRandRange(-500.f, 500.f), Rng.FRandRange(-500.f, 500.f), Rng.FRandRange(-500.f, 500.f));
			Rocks.Add(Rock);
			Centers.Add(Center);
			Radii.Add(Radius);
			return Centers.Num() - 1;
		}
		return static_cast<int32>(INDEX_NONE);
	};

	// 부모는 최근에 놓은 몇 개 중에서 고르고 본 소행성에서 멀어지는 쪽으로 치우쳐, 사방으로 뻗은 나뭇가지처럼 퍼지게 한다.
	// 셋에 하나는 먼 간격(5 ~ 9m)이라 단계 점프나 제트팩을 써야 한다.
	const int32 Count = Rng.RandRange(16, 22);
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Parent = FMath::Max(0, Centers.Num() - 1 - Rng.RandRange(0, 4));
		const bool bFar = Rng.FRand() < 0.33f;
		TryPlace(Parent, bFar ? 500.f : 250.f, bFar ? 900.f : 500.f, Centers[Parent].GetSafeNormal());
	}

	// 소행성 표면 한 점 (행성 중심 기준): 방향 Dir로 노이즈 반지름만큼 나간 뒤 축마다 늘인다.
	auto SurfacePoint = [](const FRock& Rock, const FVector& Dir)
	{
		const float Rad = Rock.Radius * (1.f + 0.22f * Fbm(Dir * 1.6 + Rock.Offset, 3));
		return Rock.Center + Dir * Rad * Rock.Squash;
	};

	// 메시: 소행성마다 정육면체 여섯 면을 10칸 격자로 나눠 부풀린 구(큐브스피어). 매끈한 땅과 같은 색 규칙.
	// 법선은 옆 두 방향으로 조금 옮긴 표면점과의 차이로 구한다 (면 경계에서도 같은 식이라 이음매가 보이지 않는다).
	TArray<FVector> Verts, Norms;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<int32> Tris;
	struct FFace { FVector N, U, V; };
	const FFace Faces[6] = {
		{ FVector(1, 0, 0),  FVector(0, 1, 0),  FVector(0, 0, 1) },
		{ FVector(-1, 0, 0), FVector(0, -1, 0), FVector(0, 0, 1) },
		{ FVector(0, 1, 0),  FVector(-1, 0, 0), FVector(0, 0, 1) },
		{ FVector(0, -1, 0), FVector(1, 0, 0),  FVector(0, 0, 1) },
		{ FVector(0, 0, 1),  FVector(0, 1, 0),  FVector(-1, 0, 0) },
		{ FVector(0, 0, -1), FVector(0, 1, 0),  FVector(1, 0, 0) },
	};
	const int32 Grid = 10;
	const int32 Row = Grid + 1;
	for (const FRock& Rock : Rocks)
	{
		for (const FFace& Face : Faces)
		{
			const int32 Base = Verts.Num();
			for (int32 j = 0; j < Row; ++j)
			{
				for (int32 i = 0; i < Row; ++i)
				{
					const FVector Dir = CubeToSphere(Face.N + (2.0 * i / Grid - 1.0) * Face.U + (2.0 * j / Grid - 1.0) * Face.V);
					const FVector P = SurfacePoint(Rock, Dir);
					FVector T1, T2;
					TangentBasis(Dir, T1, T2);
					FVector N = FVector::CrossProduct(SurfacePoint(Rock, (Dir + T1 * 0.02).GetSafeNormal()) - P,
						SurfacePoint(Rock, (Dir + T2 * 0.02).GetSafeNormal()) - P).GetSafeNormal();
					if ((N | (P - Rock.Center)) < 0.0)
					{
						N = -N;
					}
					const float Patch = FMath::Clamp(0.5f + 1.6f * HGNoise::Perlin(P / 500.0 + Rock.Offset), 0.f, 1.f);
					const float Slope = 1.f - static_cast<float>(FMath::Abs(N | (P - Rock.Center).GetSafeNormal()));
					const float Moss = FMath::SmoothStep(0.15f, 0.35f, HGNoise::Perlin(P / 300.0 - Rock.Offset));
					FLinearColor Color = FMath::Lerp(Pal.Ground, Pal.GroundB, FMath::SmoothStep(0.25f, 0.75f, Patch));
					Color *= 1.f + 0.15f * HGNoise::Perlin(P / 200.0);
					Color = FMath::Lerp(Color, Pal.Grass[1] * 0.8f, Moss * 0.6f);
					Color = FMath::Lerp(Color, Pal.Rock, FMath::SmoothStep(0.15f, 0.4f, Slope));
					Color.A = 1.f;
					Verts.Add(P);
					Norms.Add(N);
					UVs.Add(FVector2D::ZeroVector);
					Colors.Add(Color.ToFColor(false));
				}
			}
			for (int32 j = 0; j < Grid; ++j)
			{
				for (int32 i = 0; i < Grid; ++i)
				{
					const int32 V00 = Base + j * Row + i;
					const int32 Quad[2][3] = { { V00, V00 + Row, V00 + 1 }, { V00 + 1, V00 + Row, V00 + Row + 1 } };
					for (const auto& T : Quad)
					{
						// 앞면 법선 = (B - C) x (A - C)가 바깥(소행성 중심 반대)을 향하게 감는다.
						int32 A = T[0], B = T[1], C = T[2];
						const FVector FaceN = FVector::CrossProduct(Verts[B] - Verts[C], Verts[A] - Verts[C]);
						if ((FaceN | ((Verts[A] + Verts[B] + Verts[C]) / 3.0 - Rock.Center)) < 0.0)
						{
							Swap(A, B);
						}
						Tris.Append({ A, B, C });
					}
				}
			}
		}

		FHGGravityBody Body;
		Body.Center = GetActorLocation() + Rock.Center;
		Body.Radius = Rock.Radius * (Rock.Squash.X + Rock.Squash.Y + Rock.Squash.Z) / 3.f;
		Body.SurfaceGravity = Planet.GravityScale;
		GravityBodies.Add(Body);
	}
	if (Verts.Num() > 0)
	{
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, TEXT("Asteroids"), RF_Transient);
		Mesh->SetupAttachment(Terrain);
		Mesh->SetCollisionProfileName(Terrain->GetCollisionProfileName());
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->RegisterComponent();
		Mesh->CreateMeshSection(0, Verts, Tris, Norms, UVs, Colors, TArray<FProcMeshTangent>(), true);
		if (UMaterialInterface* VertexColorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/HG/Materials/M_HG_VertexColor.M_HG_VertexColor")))
		{
			Mesh->SetMaterial(0, VertexColorMaterial);
		}
	}

	// 재료: 소행성마다 1 ~ 2개. 본 소행성 재료(ScatterProps) 뒤에 번호를 이어 붙인다. 같은 시드면 같은 번호에 같은 재료와 자리다.
	// 캔 기록과 시간대(낮/밤 재료) 규칙은 본 소행성과 같다.
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	const UDataTable* Table = GI ? GI->GetIngredientTable() : nullptr;
	int32 FieldMarkers = 0;
	if (Table && !IngredientIds.IsEmpty() && IngredientMarkers.Num() == IngredientIds.Num())
	{
		const EHGDayPhase Phase = GI->GetDayPhase();
		const FTransform ToWorld = Terrain->GetComponentTransform();
		for (const FRock& Rock : Rocks)
		{
			const int32 PerRock = Rng.RandRange(1, 2);
			for (int32 k = 0; k < PerRock; ++k)
			{
				const int32 Kind = Rng.RandRange(0, IngredientIds.Num() - 1);
				const FVector Dir = Rng.VRand();
				const FVector Surface = SurfacePoint(Rock, Dir);
				const FVector Up = (Surface - Rock.Center).GetSafeNormal();
				const FTransform Local(FRotationMatrix::MakeFromZ(Up).ToQuat(), Surface + Up * 22.f, FVector(MarkerScale));

				const int32 Index = Markers.Num();
				FHGIngredientMarker& Marker = Markers.AddDefaulted_GetRef();
				Marker.Kind = Kind;
				Marker.Instance = IngredientMarkers[Kind]->AddInstance(Local);
				Marker.Location = ToWorld.TransformPosition(Local.GetLocation());
				const FHGIngredientRow* RowData = Table->FindRow<FHGIngredientRow>(IngredientIds[Kind], TEXT("BuildAsteroidField"));
				if (GI->IsMarkerHarvested(Planet.Seed, Index))
				{
					Marker.bHarvested = true;
					HideInstance(Index);
				}
				else if (RowData && !HGDay::IsAvailable(RowData->AvailableAt, Phase))
				{
					Marker.bOtherTime = true;
					HideInstance(Index);
				}
				MaybeHideForScan(Index);
				++FieldMarkers;
			}
		}
	}

	UE_LOG(LogHuoguo, Log, TEXT("[Field] 소행성 지대: 둘레 소행성 %d개 (본 소행성 반지름 %.0fm), 재료 표식 %d개 추가, 지금 캘 수 있는 재료 %d"),
		Rocks.Num(), Planet.Radius / 100.f, FieldMarkers, GetRemainingIngredientCount());
}

void AHGPlanetGenerator::ChoosePlanet()
{
	const UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	const bool bFromDive = GI && GI->GetDivePlanet().IsSet();
	if (bFromDive)
	{
		Planet = GI->GetDivePlanet().GetValue();
	}
	else
	{
		Planet = bPreviewFromSeedOnly ? HGPlanet::MakePlanet(PreviewPlanet.Seed) : PreviewPlanet;
	}
	// 시험용: hg.AsteroidField 1이면 어느 행성이든 소행성 지대로 바꿔 내려간다 (같은 시드면 같은 지대).
	if (CVarAsteroidField.GetValueOnGameThread() != 0 && !Planet.bAsteroidField)
	{
		HGPlanet::MakeAsteroidField(Planet);
	}
	// 시험용: hg.Derelict 1이면 폐우주선 (소행성 지대 강제보다 우선).
	if (CVarDerelict.GetValueOnGameThread() != 0 && !Planet.bDerelict)
	{
		HGPlanet::MakeDerelict(Planet);
	}
	// 평지 행성 맵: 어떤 행성 값이 오든 평지로 다룬다 (맵이 규칙을 정한다). 재료와 바위 개수가 원판 넓이에 맞게
	// 반지름을 반폭의 절반으로 둔다 (구 겉넓이 4πr² = 원판 넓이 π(2r)²).
	if (bFlatSite)
	{
		if (!Planet.bFlat)
		{
			HGPlanet::MakeFlat(Planet);
		}
		Planet.Radius = FlatHalfExtent * 0.5f;
	}

	// 지형 모양 값: 맵을 바로 열었고 PreviewShape를 쓰라고 했으면 그 값을, 아니면 행성 값(군계, 크기)에 맞는 기본 모양.
	// 군계와 크기가 정해진 뒤(위의 시험용 강제 변경 포함)에 정해야 기본 모양이 맞는다.
	TerrainShape = (!bFromDive && bUsePreviewShape) ? PreviewShape : HGPlanet::DefaultShape(Planet);

	// 노이즈 이동값도 시드에서 뽑는다. 큰 값으로 옮겨서 행성마다 노이즈 공간의 전혀 다른 곳을 읽게 한다.
	FRandomStream Rng(Planet.Seed);
	NoiseOffset = FVector(Rng.FRandRange(-1000.f, 1000.f), Rng.FRandRange(-1000.f, 1000.f), Rng.FRandRange(-1000.f, 1000.f));

	// 크레이터: 행성마다 따로 수열에서 뽑는다 (위 값들을 뽑는 순서를 바꾸지 않아 기존 지형이 그대로 유지된다).
	// 개수, 반지름, 깊이 범위는 지형 모양 값(TerrainShape)에서 읽는다 (기본: 소형 3 ~ 6개, 중형 6 ~ 12개, 반지름 2.5 ~ 7m).
	// 반지름은 지형 격자가 그릇 모양을 담을 수 있게 격자 4칸 이상으로 둔다 (격자 1m면 4m).
	// 착륙 지점의 평평한 곳과 겹치지 않게 북극에서 떨어진 곳에만 둔다.
	Craters.Reset();
	FRandomStream CraterRng(static_cast<int32>(HashCombine(GetTypeHash(Planet.Seed), 0xc4a7u)));
	const int32 Grid = FMath::Clamp(FMath::CeilToInt(Planet.Radius * UE_HALF_PI / TargetEdgeLength), 8, MaxGridPerFace);
	const float CellLength = Planet.Radius * UE_HALF_PI / Grid;
	const int32 CraterCount = CraterRng.RandRange(TerrainShape.CraterCountMin, FMath::Max(TerrainShape.CraterCountMin, TerrainShape.CraterCountMax));
	for (int32 i = 0; i < CraterCount; ++i)
	{
		for (int32 Try = 0; Try < 20; ++Try)
		{
			FCrater Crater;
			Crater.Dir = CraterRng.VRand();
			Crater.Radius = FMath::Max(CraterRng.FRandRange(TerrainShape.CraterRadiusMin, TerrainShape.CraterRadiusMax), CellLength * 4.f);
			Crater.Depth = Crater.Radius * CraterRng.FRandRange(TerrainShape.CraterDepthRatioMin, TerrainShape.CraterDepthRatioMax);
			if (FMath::Acos(FMath::Clamp(Crater.Dir.Z, -1.0, 1.0)) * Planet.Radius > FlatRadius() * 2.5f + Crater.Radius)
			{
				Craters.Add(Crater);
				break;
			}
		}
	}
}

float AHGPlanetGenerator::SurfaceRadiusAt(const FVector& Dir) const
{
	// 큰 언덕: 여러 겹 노이즈. 겹 수, 겹 배율, 겹 감쇠는 지형 모양 값(TerrainShape)에서 읽는다.
	float Height = HGNoise::Fbm(Dir * Planet.TerrainFrequency + NoiseOffset, TerrainShape.HillOctaves, TerrainShape.HillLacunarityPercent, TerrainShape.HillPersistencePercent);

	// 솟은 곳과 꺼진 곳에 따로 배율을 곱한다 (기본: 얼음은 둘 다 0.6배로 매끈하게, 버섯숲은 솟은 곳 1.4배 / 꺼진 곳 0.7배).
	Height = Height > 0.f ? Height * TerrainShape.HighlandScale : Height * TerrainShape.LowlandScale;

	// 높이 곡선: 키가 있을 때만 언덕 높이를 곡선으로 바꾼다 (고원, 단구 등).
	if (const FRichCurve* Curve = TerrainShape.HeightCurve.GetRichCurveConst(); Curve && Curve->GetNumKeys() > 0)
	{
		Height = Curve->Eval(Height);
	}

	// 들쭉날쭉함 (TerrainRuggedness). 위의 큰 언덕만 있으면 작은 행성 한 바퀴에 언덕이 서너 개뿐이라 완만해 보였다.
	// 그 위에 두 겹을 더 얹는다. 둘 다 같은 Dir이면 언제나 같은 값이라 메시, 배치, 착륙이 모두 그대로 맞는다.
	// 세기(능선 세기, 둔덕 높이)는 지형 모양 값에서 읽는다 (기본: 바위는 능선이 날카롭고, 얼음은 얌전하다).
	float Bumps = 0.f; // cm. 행성 크기와 상관없이 일정한 높이의 잔 굴곡
	if (TerrainRuggedness > 0.f)
	{
		// 1) 능선(ridged noise): 노이즈 값이 0을 지나는 줄을 따라 1 - |노이즈|가 뾰족하게 솟는다. 제곱해서 봉우리를 더 날카롭게 한다.
		//    큰 언덕보다 촘촘하게(기본 2.7배) 깔고 여러 겹(기본 3) 겹친다. 평균이 약 0.5라 빼서 행성이 통째로 부풀지 않게 한다.
		const FVector P = Dir * (Planet.TerrainFrequency * TerrainShape.RidgeFrequencyScale) + NoiseOffset * 1.37;
		const float Ridge = HGNoise::Ridged(P, TerrainShape.RidgeOctaves, TerrainShape.RidgeLacunarityPercent);
		Height += TerrainShape.RidgeWeight * TerrainRuggedness * (Ridge - 0.5f);

		// 2) 잔 굴곡: 기본 9m 폭의 둔덕. 지형 격자 한 칸이 크면(중형 행성) 격자가 담을 수 있게 폭을 격자 4칸 이상으로 늘리고,
		//    폭이 늘어난 만큼 높이도 키워 기울기가 비슷하게 보이게 한다.
		const int32 Grid = FMath::Clamp(FMath::CeilToInt(Planet.Radius * UE_HALF_PI / TargetEdgeLength), 8, MaxGridPerFace);
		const float CellLength = Planet.Radius * UE_HALF_PI / Grid;
		const float Wavelength = FMath::Max(TerrainShape.BumpWavelength, CellLength * 4.f);
		Bumps = TerrainShape.BumpHeight * TerrainRuggedness * (Wavelength / TerrainShape.BumpWavelength)
			* Fbm(Dir * (Planet.Radius / Wavelength) - NoiseOffset * 0.71, TerrainShape.BumpOctaves) * 2.f;
	}

	// 착륙 지점(북극, +Z) 주변은 평평하게 다진다. 북극에서 잰 표면 거리가 평탄 반경 안이면 높이를 0으로,
	// 그 바깥 한 배 거리 동안은 부드럽게 원래 높이로 되돌린다.
	const float AngleFromPole = FMath::Acos(FMath::Clamp(Dir.Z, -1.0, 1.0));
	const float ArcDistance = AngleFromPole * Planet.Radius;
	const float Flatten = FMath::SmoothStep(FlatRadius(), FlatRadius() * 2.f, ArcDistance);

	// 크레이터: 가운데가 파인 그릇(깊이 x (1 - d²)) + 가장자리를 따라 솟은 둑(기본 깊이의 0.35배, 폭 반지름의 0.25배).
	// d = 크레이터 중심에서 반지름 단위 거리. 짧은 호는 현 길이로 근사한다 (크레이터는 행성보다 훨씬 작다).
	// 둑은 가장자리에서 둑 폭의 3.2배 떨어지면 깊이의 0.004% 이하라 계산을 건너뛴다 (기본 둑 폭이면 d < 1.8).
	const float RimReach = 1.f + TerrainShape.CraterRimWidth * 3.2f;
	float CraterHeight = 0.f;
	for (const FCrater& Crater : Craters)
	{
		const float D = static_cast<float>((Dir - Crater.Dir).Size() * Planet.Radius) / Crater.Radius;
		if (D < RimReach)
		{
			if (D < 1.f)
			{
				CraterHeight -= Crater.Depth * (1.f - D * D);
			}
			CraterHeight += Crater.Depth * TerrainShape.CraterRimHeight * FMath::Exp(-FMath::Square((D - 1.f) / TerrainShape.CraterRimWidth));
		}
	}

	return Planet.Radius * (1.f + Planet.TerrainAmplitude * Height * Flatten) + Bumps * Flatten + CraterHeight;
}

void AHGPlanetGenerator::BuildTerrain(UProceduralMeshComponent* Target, bool bCreateCollision)
{
	if (!Target)
	{
		return;
	}

	// 면 하나의 격자 수: 목표 격자 길이에 맞추되 상한을 둔다. 정육면체 한 면은 구에서 약 90도(반지름 * pi/2) 호에 해당한다.
	const int32 Grid = FMath::Clamp(FMath::CeilToInt(Planet.Radius * UE_HALF_PI / TargetEdgeLength), 8, MaxGridPerFace);
	const int32 Row = Grid + 1;

	// 정육면체 여섯 면. 각 면은 바깥 방향 N과 면 위의 두 축 U, V로 정한다.
	struct FFace { FVector N, U, V; };
	const FFace Faces[6] = {
		{ FVector(1, 0, 0),  FVector(0, 1, 0),  FVector(0, 0, 1) },
		{ FVector(-1, 0, 0), FVector(0, -1, 0), FVector(0, 0, 1) },
		{ FVector(0, 1, 0),  FVector(-1, 0, 0), FVector(0, 0, 1) },
		{ FVector(0, -1, 0), FVector(1, 0, 0),  FVector(0, 0, 1) },
		{ FVector(0, 0, 1),  FVector(0, 1, 0),  FVector(-1, 0, 0) },
		{ FVector(0, 0, -1), FVector(0, 1, 0),  FVector(1, 0, 0) },
	};

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;
	Vertices.Reserve(6 * Row * Row);
	Normals.Reserve(6 * Row * Row);
	UVs.Reserve(6 * Row * Row);
	Triangles.Reserve(6 * Grid * Grid * 6);

	// 법선 계산용 미소 각도. 격자 반 칸 정도로 잡아 너무 잘게 흔들리지 않게 한다.
	const float Eps = 0.5f * TargetEdgeLength / Planet.Radius;

	for (const FFace& Face : Faces)
	{
		const int32 Base = Vertices.Num();
		for (int32 j = 0; j < Row; ++j)
		{
			for (int32 i = 0; i < Row; ++i)
			{
				const double A = 2.0 * i / Grid - 1.0;
				const double B = 2.0 * j / Grid - 1.0;
				const FVector Dir = CubeToSphere(Face.N + A * Face.U + B * Face.V);
				const FVector Pos = Dir * SurfaceRadiusAt(Dir);
				Vertices.Add(Pos);
				UVs.Add(FVector2D(static_cast<double>(i) / Grid, static_cast<double>(j) / Grid));

				// 법선: 옆 두 방향으로 조금 옮긴 지표점과의 차이로 기울기를 구한다.
				// 면 경계에서도 같은 함수로 구하므로 이음매에 명암 차이가 생기지 않는다.
				FVector T1, T2;
				TangentBasis(Dir, T1, T2);
				const FVector D1 = (Dir + T1 * Eps).GetSafeNormal();
				const FVector D2 = (Dir + T2 * Eps).GetSafeNormal();
				FVector Normal = FVector::CrossProduct(D1 * SurfaceRadiusAt(D1) - Pos, D2 * SurfaceRadiusAt(D2) - Pos).GetSafeNormal();
				if ((Normal | Dir) < 0.0)
				{
					Normal = -Normal;
				}
				Normals.Add(Normal);
			}
		}

		// 삼각형 감기 순서: 언리얼은 정점 순서로 앞면을 정한다. 면마다 축 방향이 달라서 순서가 뒤집힐 수 있으므로
		// 첫 칸으로 앞면 방향을 직접 계산해 보고, 바깥을 향하지 않으면 이 면 전체의 순서를 뒤집는다.
		// 앞면 법선 = (P1 - P2) x (P0 - P2)  (ProceduralMeshComponent의 접선 계산과 같은 식)
		const FVector P0 = Vertices[Base], P1 = Vertices[Base + Row], P2 = Vertices[Base + 1];
		const bool bFlip = ((FVector::CrossProduct(P1 - P2, P0 - P2)) | (P0 + P1 + P2)) < 0.0;

		for (int32 j = 0; j < Grid; ++j)
		{
			for (int32 i = 0; i < Grid; ++i)
			{
				const int32 V00 = Base + j * Row + i;
				const int32 V10 = V00 + 1;
				const int32 V01 = V00 + Row;
				const int32 V11 = V01 + 1;
				if (!bFlip)
				{
					Triangles.Append({ V00, V01, V10, V10, V01, V11 });
				}
				else
				{
					Triangles.Append({ V00, V10, V01, V10, V11, V01 });
				}
			}
		}
	}

	Target->ClearAllMeshSections();
	if (IsMoodEnabled() && CVarPlanetFaceted.GetValueOnGameThread() == 0)
	{
		// 그림체: 매끈한 지형 (기본). 정점을 이웃 삼각형과 나눠 쓰고 부드러운 법선으로 칠해서 각이 보이지 않는다.
		// 색은 정점마다 정해 삼각형 안에서 부드럽게 섞인다:
		//   - 땅 주 색과 얼룩 색을 큰 노이즈로 넓고 부드럽게 섞는다.
		//   - 높이에 따라 줄무늬(지층)처럼 밝기가 오르내린다. 큰 노이즈로 줄을 휘게 해서 외계 지형의 물결무늬처럼 보이게 한다.
		//   - 가파른 비탈은 바위색으로 바꾼다 (기울기 = 법선이 바로 위에서 얼마나 기울었나).
		//   - 3m 크기 잔 노이즈로 밝기를 조금 흔들어 밋밋하지 않게 한다.
		const FMoodPalette Pal = MoodPalette(Planet.Biome);
		const float Amp = FMath::Max(1.f, Planet.Radius * Planet.TerrainAmplitude);
		TArray<FColor> Colors;
		Colors.SetNumUninitialized(Vertices.Num());
		for (int32 v = 0; v < Vertices.Num(); ++v)
		{
			const FVector& P = Vertices[v];
			const FVector Up = P.GetSafeNormal();
			const float Patch = FMath::Clamp(0.5f + 1.6f * HGNoise::Perlin(P / 800.0 + NoiseOffset * 0.5), 0.f, 1.f);
			const float Height = static_cast<float>((P.Size() - Planet.Radius) / Amp);
			const float Warp = HGNoise::Perlin(P / 2500.0 - NoiseOffset * 0.3);
			const float Strata = 0.5f + 0.5f * FMath::Sin((Height * 3.f + Warp * 2.5f) * UE_PI);
			const float Slope = 1.f - static_cast<float>(FMath::Abs(Normals[v] | Up));
			const float Speckle = HGNoise::Perlin(P / 250.0 + NoiseOffset);
			// 외계 이끼: 4m 크기 얼룩이 일정 값을 넘는 곳만 식물 색으로 덮는다 (땅에 낀 낯선 색 이끼/균사 무늬).
			const float Moss = FMath::SmoothStep(0.15f, 0.35f, HGNoise::Perlin(P / 400.0 - NoiseOffset * 0.8));

			FLinearColor Color = FMath::Lerp(Pal.Ground, Pal.GroundB, FMath::SmoothStep(0.25f, 0.75f, Patch));
			Color *= FMath::Lerp(0.85f, 1.15f, Strata) * (1.f + 0.18f * Speckle) * FMath::Lerp(0.9f, 1.1f, FMath::Clamp(Height * 0.5f + 0.5f, 0.f, 1.f));
			Color = FMath::Lerp(Color, Pal.Grass[1] * 0.8f, Moss * 0.7f);
			Color = FMath::Lerp(Color, Pal.Rock, FMath::SmoothStep(0.10f, 0.30f, Slope));
			Color.A = 1.f;
			Colors[v] = Color.ToFColor(false);
		}
		Target->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), bCreateCollision);
		if (UMaterialInterface* VertexColorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/HG/Materials/M_HG_VertexColor.M_HG_VertexColor")))
		{
			Target->SetMaterial(0, VertexColorMaterial);
		}
	}
	else if (IsMoodEnabled())
	{
		// 그림체: 각진(로우폴리) 지형. 삼각형마다 정점을 따로 두어 면 법선으로 칠하고, 면마다 색을 정해 꼭짓점 색으로 넣는다.
		// 색 = 땅 주 색과 얼룩 색을 큰 노이즈로 섞고, 높을수록 조금 밝게, 면마다 밝기를 세 단계로 흔든다 (복셀 소품과 어울리는 도트 질감).
		const FMoodPalette Pal = MoodPalette(Planet.Biome);
		TArray<FVector> FlatVertices;
		TArray<FVector> FlatNormals;
		TArray<FVector2D> FlatUVs;
		TArray<FColor> FlatColors;
		TArray<int32> FlatTriangles;
		FlatVertices.Reserve(Triangles.Num());
		FlatNormals.Reserve(Triangles.Num());
		FlatColors.Reserve(Triangles.Num());
		FlatTriangles.Reserve(Triangles.Num());
		const float Amp = FMath::Max(1.f, Planet.Radius * Planet.TerrainAmplitude);
		for (int32 t = 0; t + 2 < Triangles.Num(); t += 3)
		{
			const FVector A = Vertices[Triangles[t]], B = Vertices[Triangles[t + 1]], C = Vertices[Triangles[t + 2]];
			const FVector Centroid = (A + B + C) / 3.0;
			FVector N = FVector::CrossProduct(B - C, A - C).GetSafeNormal();
			if ((N | Centroid) < 0.0)
			{
				N = -N;
			}
			const float Patch = FMath::Clamp(0.5f + 1.4f * HGNoise::Perlin(Centroid / 1500.0 + NoiseOffset * 0.5), 0.f, 1.f);
			const float Height = FMath::Clamp(static_cast<float>((Centroid.Size() - Planet.Radius) / Amp) * 0.5f + 0.5f, 0.f, 1.f);
			FLinearColor Color = FMath::Lerp(Pal.Ground, Pal.GroundB, FMath::SmoothStep(0.35f, 0.65f, Patch)) * FMath::Lerp(0.85f, 1.15f, Height);
			Color = ShadeOf(Color, static_cast<uint32>(t / 3) * 2654435761u);
			const FColor Packed = Color.ToFColor(false);
			for (int32 k = 0; k < 3; ++k)
			{
				FlatTriangles.Add(FlatVertices.Num());
				FlatVertices.Add(Vertices[Triangles[t + k]]);
				FlatNormals.Add(N);
				FlatUVs.Add(UVs[Triangles[t + k]]);
				FlatColors.Add(Packed);
			}
		}
		Target->CreateMeshSection(0, FlatVertices, FlatTriangles, FlatNormals, FlatUVs, FlatColors, TArray<FProcMeshTangent>(), bCreateCollision);
		if (UMaterialInterface* VertexColorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/HG/Materials/M_HG_VertexColor.M_HG_VertexColor")))
		{
			Target->SetMaterial(0, VertexColorMaterial);
		}
	}
	else
	{
		Target->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), bCreateCollision);
		HGPlaceholder::Paint(Target, HGPlanet::GroundColor(Planet.Biome));
	}

	UE_LOG(LogHuoguo, Log, TEXT("행성 지형: 면당 격자 %d, 정점 %d, 삼각형 %d"), Grid, Vertices.Num(), Triangles.Num() / 3);
}

void AHGPlanetGenerator::ScatterProps()
{
	// 지형과 다른 수열을 쓰도록 시드를 섞는다. 배치 규칙을 바꿔도 지형 모양은 그대로 남는다.
	FRandomStream Rng(static_cast<int32>(HashCombine(GetTypeHash(Planet.Seed), 0x5eed)));
	const FVector Center = GetActorLocation();

	// 표면적(m^2)에 비례해서 개수를 정한다. 작은 행성이 휑하거나 큰 행성이 빽빽하지 않게 하기 위해서이다.
	const double RadiusM = Planet.Radius / 100.0;
	const double AreaM2 = 4.0 * UE_PI * RadiusM * RadiusM;
	// 소행성 지대의 본 소행성은 아주 작아서(반지름 10 ~ 15m) 최소 개수를 낮춘다. 재료는 둘레 소행성에도 더 놓는다 (BuildAsteroidField).
	// 폐우주선은 땅이 없어 바위와 재료를 행성 표면에 놓지 않는다 (재료 종류 인스턴스 메시만 만든다).
	const int32 RockCount = Planet.bDerelict ? 0 : FMath::Clamp(FMath::RoundToInt(AreaM2 / 2000.0 * Planet.Richness), Planet.bAsteroidField ? 6 : 20, 1500);
	// 재료 표식은 낮 재료와 밤 재료를 다 만든 뒤 지금 시간대가 아닌 것을 숨긴다. 한 번에 보이는 것은 대략 절반이므로
	// 한 시간대에 약 3500m²당 1개가 보이도록 두 배로 만든다.
	const int32 IngredientCount = Planet.bDerelict ? 0 : FMath::Clamp(FMath::RoundToInt(AreaM2 / 1750.0 * Planet.Richness), Planet.bAsteroidField ? 6 : 24, 800);

	// 착륙 지점 근처에는 아무것도 두지 않는다 (발판과 착륙한 사장님이 바위에 묻히지 않게).
	const double MinPoleAngle = (FlatRadius() * 0.8) / Planet.Radius;
	auto RandomDir = [&Rng, MinPoleAngle]()
	{
		for (;;)
		{
			const FVector Dir = Rng.VRand();
			if (FMath::Acos(FMath::Clamp(Dir.Z, -1.0, 1.0)) > MinPoleAngle)
			{
				return Dir;
			}
		}
	};

	// 지표 위의 변환: 위쪽(Z)을 지표 바깥으로, 둘레 방향은 무작위로 돌린다.
	auto SurfaceTransform = [this, &Rng, &Center](const FVector& Dir, float Sink, const FVector& Scale)
	{
		// 위 방향: 구면이면 Dir, 평지면 +Z (SurfaceUpOf).
		const FVector Up = SurfaceUpOf(Dir);
		FVector T1, T2;
		TangentBasis(Up, T1, T2);
		const float Spin = Rng.FRandRange(0.f, UE_TWO_PI);
		const FVector Forward = T1 * FMath::Cos(Spin) + T2 * FMath::Sin(Spin);
		const FQuat Rot = FRotationMatrix::MakeFromZX(Up, Forward).ToQuat();
		// 인스턴스 좌표는 컴포넌트(행성 중심) 기준이다. 평지면 원판 위 땅 점.
		const FVector Location = bFlatSite ? SurfacePointOf(Dir) - Up * Sink : Dir * (SurfaceRadiusAt(Dir) - Sink);
		return FTransform(Rot, Location, Scale);
	};

	RoundRocks->ClearInstances();
	BlockRocks->ClearInstances();
	for (int32 i = 0; i < RockCount; ++i)
	{
		const FVector Dir = RandomDir();
		const float S = Rng.FRandRange(0.6f, 3.4f);
		const FVector Scale(S, S * Rng.FRandRange(0.7f, 1.1f), S * Rng.FRandRange(0.5f, 0.9f));
		// 도형 중심이 지표에 오면 절반이 땅에 묻힌다. 살짝 더 묻어서 경사면에서도 떠 보이지 않게 한다.
		const FTransform T = SurfaceTransform(Dir, S * 15.f, Scale);
		(i % 3 == 0 ? BlockRocks : RoundRocks)->AddInstance(T);
	}

	HGPlaceholder::Paint(RoundRocks, HGPlanet::RockColor(Planet.Biome));
	HGPlaceholder::Paint(BlockRocks, HGPlanet::RockColor(Planet.Biome) * 0.8f);

	// 재료: 재료 DataTable의 종류마다 인스턴스 메시를 하나 만든다.
	for (UInstancedStaticMeshComponent* Ism : IngredientMarkers)
	{
		if (Ism)
		{
			Ism->DestroyComponent();
		}
	}
	IngredientMarkers.Reset();
	IngredientIds.Reset();
	IngredientColors.Reset();
	Markers.Reset();

	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	const UDataTable* Table = GI ? GI->GetIngredientTable() : nullptr;
	if (!Table || Table->GetRowNames().IsEmpty())
	{
		UE_LOG(LogHuoguo, Log, TEXT("행성 배치: 바위 %d, 재료 없음 (재료 DataTable이 없거나 비었다)"), RockCount);
		return;
	}

	// 지금 시간대. 다이빙 중에는 정거장에서 내려간 시간대 그대로다 (돌아가야 넘어간다).
	const EHGDayPhase Phase = GI ? GI->GetDayPhase() : EHGDayPhase::Day;

	// 이 행성 군계가 고향(HomeBiome)인 재료 종류들과, 종류마다 지금 시간대에 보이는지.
	TArray<int32> NativeKinds;
	TArray<bool> KindVisibleNow;
	for (const FName& Id : Table->GetRowNames())
	{
		const FHGIngredientRow* Row = Table->FindRow<FHGIngredientRow>(Id, TEXT("ScatterProps"));
		const int32 Kind = IngredientIds.Add(Id);
		IngredientColors.Add(Row ? Row->Color : FLinearColor::White);
		KindVisibleNow.Add(!Row || HGDay::IsAvailable(Row->AvailableAt, Phase));

		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("Ingredient_%s"), *Id.ToString()));
		Ism->SetStaticMesh(MarkerMesh);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetupAttachment(Terrain);
		Ism->RegisterComponent();
		HGPlaceholder::Paint(Ism, Row ? Row->Color : FLinearColor::White);
		IngredientMarkers.Add(Ism);

		if (Row && Row->HomeBiome == Planet.Biome)
		{
			NativeKinds.Add(Kind);
		}
	}

	// 표식 배치: 절반은 이 군계의 고향 재료, 나머지는 모든 재료에서 고르게.
	// 표식 번호 i는 이 순서로 정해진다. 같은 시드면 같은 번호에 같은 재료, 같은 자리가 나온다 (캔 기록이 이 번호를 쓴다).
	// 시간대와 상관없이 늘 같은 순서로 모든 표식을 만들고, 지금 시간대가 아닌 재료만 숨긴다.
	// 시간대에 따라 뽑는 재료를 바꾸면 같은 번호가 낮과 밤에 다른 재료를 가리켜서 캔 기록이 꼬이기 때문이다.
	const FTransform ToWorld = Terrain->GetComponentTransform();
	int32 HiddenCount = 0;
	int32 OtherTimeCount = 0;
	for (int32 i = 0; i < IngredientCount; ++i)
	{
		const bool bNative = !NativeKinds.IsEmpty() && Rng.FRand() < 0.5f;
		const int32 Kind = bNative ? NativeKinds[Rng.RandRange(0, NativeKinds.Num() - 1)] : Rng.RandRange(0, IngredientIds.Num() - 1);
		const FVector Dir = RandomDir();
		// 표식은 지표에서 살짝 떠 있게 한다 (Sink가 음수면 지표 위로 올라간다).
		const FTransform Local = SurfaceTransform(Dir, -22.f, FVector(MarkerScale));

		FHGIngredientMarker& Marker = Markers.AddDefaulted_GetRef();
		Marker.Kind = Kind;
		Marker.Instance = IngredientMarkers[Kind]->AddInstance(Local);
		Marker.Location = ToWorld.TransformPosition(Local.GetLocation());

		// 이 행성에서 이미 캔 표식은 처음부터 숨긴다.
		if (GI && GI->IsMarkerHarvested(Planet.Seed, i))
		{
			Marker.bHarvested = true;
			HideInstance(i);
			++HiddenCount;
		}
		// 지금 시간대에 나지 않는 재료도 숨긴다 (밤에 내려오면 낮 재료가 안 보인다).
		else if (!KindVisibleNow[Kind])
		{
			Marker.bOtherTime = true;
			HideInstance(i);
			++OtherTimeCount;
		}
		MaybeHideForScan(i);
	}

	UE_LOG(LogHuoguo, Log, TEXT("행성 배치 (%s): 바위 %d, 재료 %d (종류 %d, 다른 시간대 재료 %d개 숨김, 이미 캔 것 %d개 숨김, 지금 캘 수 있는 것 %d)"),
		HGDay::PhaseName(Phase), RockCount, IngredientCount, IngredientIds.Num(), OtherTimeCount, HiddenCount, GetRemainingIngredientCount());
}

void AHGPlanetGenerator::MaybeHideForScan(int32 MarkerId)
{
	FHGIngredientMarker& Marker = Markers[MarkerId];
	if (Marker.bHarvested || Marker.bOtherTime)
	{
		return;
	}
	// 표식 번호와 행성 시드로 정하므로 같은 행성이면 언제나 같은 표식이 숨는다 (캔 기록 번호와 맞물린다).
	const uint32 Hash = HashCombine(GetTypeHash(Planet.Seed), GetTypeHash(MarkerId * 2654435761u));
	if ((Hash % 1000u) < static_cast<uint32>(HiddenIngredientRatio * 1000.f))
	{
		Marker.bHidden = true;
		HideInstance(MarkerId);
	}
}

bool AHGPlanetGenerator::ScanMarker(int32 MarkerId, float WorldTime)
{
	if (!Markers.IsValidIndex(MarkerId))
	{
		return false;
	}
	FHGIngredientMarker& Marker = Markers[MarkerId];
	Marker.PingTime = WorldTime;
	if (!Marker.bHidden)
	{
		return false;
	}
	// 숨은 재료를 드러낸다: 숨길 때 0으로 줄인 크기를 표식 크기로 되돌린다.
	Marker.bHidden = false;
	UInstancedStaticMeshComponent* Ism = IngredientMarkers[Marker.Kind];
	FTransform Shown;
	Ism->GetInstanceTransform(Marker.Instance, Shown, false);
	Shown.SetScale3D(FVector(MarkerScale));
	Ism->UpdateInstanceTransform(Marker.Instance, Shown, false, true);
	return true;
}

void AHGPlanetGenerator::HideInstance(int32 MarkerId)
{
	const FHGIngredientMarker& Marker = Markers[MarkerId];

	// 크기 0 변환으로 바꿔 그리지 않게 한다. 인스턴스를 지우면(RemoveInstance) 뒤 인스턴스 번호가 당겨져서
	// 다른 표식이 가리키는 번호가 틀어지므로 지우지 않는다.
	UInstancedStaticMeshComponent* Ism = IngredientMarkers[Marker.Kind];
	FTransform Hidden;
	Ism->GetInstanceTransform(Marker.Instance, Hidden, false);
	Hidden.SetScale3D(FVector::ZeroVector);
	Ism->UpdateInstanceTransform(Marker.Instance, Hidden, false, true);
}

int32 AHGPlanetGenerator::FindNearestIngredient(const FVector& From, float MaxDistance) const
{
	int32 Best = INDEX_NONE;
	double BestDistSq = FMath::Square(static_cast<double>(MaxDistance));
	for (int32 i = 0; i < Markers.Num(); ++i)
	{
		if (!Markers[i].IsAvailable())
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(From, Markers[i].Location);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = i;
		}
	}
	return Best;
}

FName AHGPlanetGenerator::GetMarkerIngredient(int32 MarkerId) const
{
	return Markers.IsValidIndex(MarkerId) ? IngredientIds[Markers[MarkerId].Kind] : NAME_None;
}

FVector AHGPlanetGenerator::GetMarkerLocation(int32 MarkerId) const
{
	return Markers.IsValidIndex(MarkerId) ? Markers[MarkerId].Location : FVector::ZeroVector;
}

FName AHGPlanetGenerator::HarvestIngredient(int32 MarkerId)
{
	if (!Markers.IsValidIndex(MarkerId) || !Markers[MarkerId].IsAvailable())
	{
		return NAME_None;
	}
	Markers[MarkerId].bHarvested = true;
	HideInstance(MarkerId);
	if (UHGGameInstance* GI = GetGameInstance<UHGGameInstance>())
	{
		GI->MarkHarvested(Planet.Seed, MarkerId);
	}
	return IngredientIds[Markers[MarkerId].Kind];
}

FLinearColor AHGPlanetGenerator::GetIngredientColor(int32 Kind) const
{
	return IngredientColors.IsValidIndex(Kind) ? IngredientColors[Kind] : FLinearColor::White;
}

bool AHGPlanetGenerator::GetReturnPadLocation(FVector& OutLocation) const
{
	OutLocation = ReturnPadLocation;
	return bHasReturnPad;
}

int32 AHGPlanetGenerator::GetRemainingIngredientCount() const
{
	int32 Count = 0;
	for (const FHGIngredientMarker& Marker : Markers)
	{
		Count += Marker.IsAvailable() ? 1 : 0;
	}
	return Count;
}

FTransform AHGPlanetGenerator::GetLandingTransform() const
{
	// 폐우주선: 우주선을 바라보며 떠 있는 시작 자리.
	if (Planet.bDerelict)
	{
		return DerelictStart;
	}
	// 착륙 지점은 북극(+Z)이다. 그 주변은 SurfaceRadiusAt에서 평평하게 다졌다.
	// 캡슐 반높이(92) + 여유 3만큼 띄워서 발이 땅에 닿게 한다.
	const FVector Up = FVector::UpVector;
	if (bFlatSite)
	{
		// 평지: 이 액터 바로 아래 땅.
		return FTransform(FRotator::ZeroRotator, GetActorLocation() + Up * (FlatGroundZ(FVector2D::ZeroVector) + 95.0));
	}
	return FTransform(FRotator::ZeroRotator, GetActorLocation() + Up * (SurfaceRadiusAt(Up) + 95.f));
}

void AHGPlanetGenerator::SpawnReturnPad()
{
	if (ReturnLevel.IsNull())
	{
		UE_LOG(LogHuoguo, Warning, TEXT("%s: ReturnLevel이 비어 있어서 귀환 발판을 만들지 않는다"), *GetName());
		return;
	}

	// 착륙 지점에서 4m 떨어진 곳(평탄 구역 안)에 세운다. 발판 액터의 중심은 바닥 + 60이다.
	const FVector Dir = (FVector::UpVector * Planet.Radius + FVector::BackwardVector * 400.f).GetSafeNormal();
	// 평지면 SurfacePointOf가 원판 위 점(가운데에서 약 4m)과 땅 높이를 준다.
	const FVector Up = SurfaceUpOf(Dir);
	const FVector Location = bFlatSite ? GetActorLocation() + SurfacePointOf(Dir) + Up * 60.0 : GetActorLocation() + Dir * (SurfaceRadiusAt(Dir) + 60.f);
	const FTransform PadTransform(FRotationMatrix::MakeFromZ(Up).ToQuat(), Location);

	// 배치 전에 속성을 넣어야 OnConstruction이 올바른 색으로 칠하므로 지연 생성을 쓴다.
	AHGTravelPad* Pad = GetWorld()->SpawnActorDeferred<AHGTravelPad>(AHGTravelPad::StaticClass(), PadTransform, this);
	Pad->DestinationLevel = ReturnLevel;
	Pad->ArrivalTag = ReturnArrivalTag;
	Pad->DestinationName = ReturnName;
	Pad->PadColor = FLinearColor(0.95f, 0.62f, 0.15f);
	Pad->FinishSpawning(PadTransform);

	bHasReturnPad = true;
	ReturnPadLocation = Location;
}
