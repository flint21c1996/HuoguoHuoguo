#include "Planet/HGPlanetGenerator.h"

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
#include "HuoguoHuoguo.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/HGTravelPad.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPlanetMood(
		TEXT("hg.PlanetMood"), 1,
		TEXT("행성 그림체 (복셀 + 노 맨즈 스카이 색): 각진 색 지형, 하늘 돔, 복셀 바위/나무/풀꽃/떠 있는 섬/구름. 기본 1. 0이면 다음에 만드는 행성을 예전 매끈한 단색 모습으로 만든다 (비교와 성능 확인용)."),
		ECVF_Default);

	/**
	 * 여러 겹 노이즈(fBm). 큰 언덕 위에 작은 굴곡을 겹쳐 자연스러운 지형을 만든다.
	 * 한 겹 올라갈 때마다 주파수는 약 2배, 세기는 절반이다. 결과는 대략 -1 ~ 1.
	 */
	float Fbm(FVector P, int32 Octaves)
	{
		float Sum = 0.f;
		float Amp = 1.f;
		float Norm = 0.f;
		for (int32 i = 0; i < Octaves; ++i)
		{
			Sum += Amp * FMath::PerlinNoise3D(P);
			Norm += Amp;
			Amp *= 0.5f;
			// 정수배로 키우면 격자점이 겹쳐 무늬가 반복되므로 2.03처럼 어긋난 값을 쓴다.
			P *= 2.03;
		}
		return Sum / Norm;
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
#endif
}

void AHGPlanetGenerator::BeginPlay()
{
	Super::BeginPlay();
	EnsureGenerated();
}

void AHGPlanetGenerator::EnsureGenerated()
{
	if (bGenerated)
	{
		return;
	}
	bGenerated = true;

	const double StartTime = FPlatformTime::Seconds();
	ChoosePlanet();
	BuildTerrain();
	ScatterProps();
	SpawnReturnPad();
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

FTransform AHGPlanetGenerator::SurfaceFrame(const FVector& Dir, float Spin, float Lift) const
{
	FVector T1, T2;
	TangentBasis(Dir, T1, T2);
	const FVector Forward = T1 * FMath::Cos(Spin) + T2 * FMath::Sin(Spin);
	return FTransform(FRotationMatrix::MakeFromZX(Dir, Forward).ToQuat(), Dir * (SurfaceRadiusAt(Dir) + Lift));
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
	auto AddVoxel = [&](const FTransform& Frame, const FVector& Local, float Size, const FLinearColor& Color, bool bShadow, bool bGlow = false)
	{
		const uint32 Key = HashCombine(HashCombine(HashCombine(Color.ToFColor(false).DWColor(), GetTypeHash(FMath::RoundToInt(Size))), GetTypeHash(bShadow)), GetTypeHash(bGlow));
		BatchGlow.Add(Key, bGlow);
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
	const int32 NearTrees = 14;
	const int32 FarTrees = FMath::Clamp(FMath::RoundToInt(AreaM2 / 900.0), 10, 120);
	for (int32 i = 0; i < NearTrees; ++i)
	{
		PlaceTree(RandomDirNearPole(3500.0 / R));
	}
	for (int32 i = 0; i < FarTrees; ++i)
	{
		PlaceTree(RandomDirAway());
	}

	// 3. 풀과 꽃: 착륙 지점 둘레 25m는 1m²당 3포기, 25~45m는 1포기, 나머지는 10m²당 1포기.
	const float FloraVoxel = 8.f;
	auto CapArea = [R](double Angle) { return 2.0 * UE_PI * R * R * (1.0 - FMath::Cos(Angle)) / 10000.0; };
	const double Inner = 2500.0 / R;
	const double Outer = 4500.0 / R;
	const int32 InnerCount = FMath::Clamp(FMath::RoundToInt(CapArea(Inner) * 3.0), 0, 9000);
	const int32 RingCount = FMath::Clamp(FMath::RoundToInt(CapArea(Outer) - CapArea(Inner)), 0, 6000);
	const int32 FarCount = FMath::Clamp(FMath::RoundToInt((AreaM2 - CapArea(Outer)) / 10.0), 0, 12000);
	auto PlaceTuft = [&](const FVector& Dir)
	{
		const FTransform Frame = SurfaceFrame(Dir, Rng.FRandRange(0.f, UE_TWO_PI), -2.f);
		const float Kind = Rng.FRand();
		const uint32 H = Rng.GetCurrentSeed();
		if (Kind < 0.7f)
		{
			// 풀잎: 1~3포기, 2~4칸 높이.
			const int32 Blades = Rng.RandRange(1, 3);
			for (int32 b = 0; b < Blades; ++b)
			{
				const FVector Base(Rng.RandRange(-1, 1) * FloraVoxel, Rng.RandRange(-1, 1) * FloraVoxel, 0.f);
				const FLinearColor Color = ShadeOf(Pal.Grass[Rng.RandRange(0, 1)], H + b);
				const int32 Height = Rng.RandRange(2, 4);
				for (int32 z = 0; z < Height; ++z)
				{
					AddVoxel(Frame, Base + FVector(0.f, 0.f, (z + 0.5f) * FloraVoxel), FloraVoxel, Color, false);
				}
			}
		}
		else if (Kind < 0.92f)
		{
			// 꽃: 줄기 2~3칸 + 십자 모양 꽃송이.
			const int32 Stem = Rng.RandRange(2, 3);
			for (int32 z = 0; z < Stem; ++z)
			{
				AddVoxel(Frame, FVector(0.f, 0.f, (z + 0.5f) * FloraVoxel), FloraVoxel, Pal.Grass[1], false);
			}
			const FLinearColor Petal = Pal.Flower[Rng.RandRange(0, 1)];
			const float Top = (Stem + 0.5f) * FloraVoxel;
			for (const FVector& Off : { FVector(0, 0, 0), FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) })
			{
				AddVoxel(Frame, FVector(Off.X * FloraVoxel, Off.Y * FloraVoxel, Top), FloraVoxel, Off.IsZero() ? FLinearColor(1.f, 0.9f, 0.3f) : Petal, false);
			}
		}
		else
		{
			// 덤불: 3x3x2 덩어리 (칸 16cm), 위쪽 모서리는 비운다.
			for (int32 x = -1; x <= 1; ++x)
			{
				for (int32 y = -1; y <= 1; ++y)
				{
					for (int32 z = 0; z < 2; ++z)
					{
						if (z == 1 && FMath::Abs(x) + FMath::Abs(y) == 2)
						{
							continue;
						}
						AddVoxel(Frame, FVector(x * 2.f * FloraVoxel, y * 2.f * FloraVoxel, (z + 0.5f) * 2.f * FloraVoxel), FloraVoxel * 2.f,
							ShadeOf(Pal.Shrub, H + x * 7 + y * 13 + z), true);
					}
				}
			}
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

	// 4. 하늘의 볼거리: 떠 있는 바위섬(윗면은 땅 색, 아래는 뾰족한 바위)과 복셀 구름. 둘 다 행성을 둘러 떠 있다.
	int32 SkyVoxels = 0;
	const int32 Islands = 9;
	for (int32 i = 0; i < Islands; ++i)
	{
		const FVector Dir = i < 5 ? RandomDirNearPole(FMath::Min(1.2, 9000.0 / R)) : RandomDirAway();
		const float Size = Rng.FRandRange(250.f, 600.f);
		const FTransform Frame = SurfaceFrame(Dir, Rng.FRandRange(0.f, UE_TWO_PI), Rng.FRandRange(1800.f, 4000.f));
		SkyVoxels += FillShell(Frame, FVector(-Size, -Size, -Size * 1.3f), FVector(Size, Size, 60.f), 40.f, true, false,
			[&](const FVector& P)
			{
				// 위는 거의 평평한 원판, 아래로 갈수록 좁아지는 뒤집힌 원뿔.
				const float Rxy = FMath::Sqrt(P.X * P.X + P.Y * P.Y);
				if (P.Z > 40.f) { return false; }
				if (P.Z >= 0.f) { return Rxy <= Size; }
				return Rxy <= Size * (1.f + P.Z / (Size * 1.3f));
			},
			[&](const FVector& P, uint32 H) { return P.Z >= 0.f ? ShadeOf(Pal.Ground, H) : ShadeOf(Pal.Rock, H); });
	}
	const int32 Clouds = FMath::Clamp(FMath::RoundToInt(AreaM2 / 2500.0), 12, 60);
	for (int32 i = 0; i < Clouds; ++i)
	{
		const FVector Dir = i < 8 ? RandomDirNearPole(FMath::Min(1.4, 12000.0 / R)) : RandomDirAway();
		const FTransform Frame = SurfaceFrame(Dir, Rng.FRandRange(0.f, UE_TWO_PI), Rng.FRandRange(4500.f, 7000.f));
		const FVector Ext(Rng.FRandRange(600.f, 1400.f), Rng.FRandRange(400.f, 800.f), Rng.FRandRange(150.f, 300.f));
		// 구름은 살짝 스스로 빛나게 칠해서(발광 머티리얼) 아래에서 올려다봐도 검게 보이지 않게 한다.
		SkyVoxels += FillShell(Frame, -Ext, Ext, 60.f, false, true,
			[&](const FVector& P) { return FMath::Square(P.X / Ext.X) + FMath::Square(P.Y / Ext.Y) + FMath::Square(P.Z / Ext.Z) <= 1.f; },
			[&](const FVector& P, uint32) { return P.Z > 0.f ? FLinearColor(1.f, 1.f, 1.f) : FLinearColor(0.88f, 0.90f, 0.96f); });
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
		Ism->RegisterComponent();
		Ism->AddInstances(Batch.Value, false, false);
		if (BatchGlow[Batch.Key])
		{
			HGVoxel::PaintGlow(Ism, BatchColor[Batch.Key], CloudGlow);
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
				Light->SetIntensity(Light->Intensity * 1.6f);
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
		Mid->SetVectorParameterValue(TEXT("Center"), FLinearColor(GetActorLocation()));
		Mid->SetVectorParameterValue(TEXT("Zenith"), Pal.Zenith);
		Mid->SetVectorParameterValue(TEXT("Horizon"), Pal.Horizon);
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

	UE_LOG(LogHuoguo, Log, TEXT("[Mood] 행성 그림체: 군계 %d, 바위 칸 %d, 나무 %d그루(칸 %d), 풀과 꽃 %d포기, 하늘 섬/구름 칸 %d, 복셀 인스턴스 %d, 색 묶음 %d"),
		static_cast<int32>(Planet.Biome), RockVoxels, NearTrees + FarTrees, TreeVoxels, InnerCount + RingCount + FarCount, SkyVoxels, Instances, Batches.Num());
}

void AHGPlanetGenerator::ChoosePlanet()
{
	const UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (GI && GI->GetDivePlanet().IsSet())
	{
		Planet = GI->GetDivePlanet().GetValue();
	}
	else
	{
		Planet = bPreviewFromSeedOnly ? HGPlanet::MakePlanet(PreviewPlanet.Seed) : PreviewPlanet;
	}

	// 노이즈 이동값도 시드에서 뽑는다. 큰 값으로 옮겨서 행성마다 노이즈 공간의 전혀 다른 곳을 읽게 한다.
	FRandomStream Rng(Planet.Seed);
	NoiseOffset = FVector(Rng.FRandRange(-1000.f, 1000.f), Rng.FRandRange(-1000.f, 1000.f), Rng.FRandRange(-1000.f, 1000.f));
}

float AHGPlanetGenerator::SurfaceRadiusAt(const FVector& Dir) const
{
	float Height = Fbm(Dir * Planet.TerrainFrequency + NoiseOffset, 5);

	// 생물 군계별 지형 성격. 얼음은 매끈하게, 버섯숲은 둔덕이 더 솟게 한다.
	if (Planet.Biome == EHGPlanetBiome::Ice)
	{
		Height *= 0.6f;
	}
	else if (Planet.Biome == EHGPlanetBiome::Fungal)
	{
		Height = Height > 0.f ? Height * 1.4f : Height * 0.7f;
	}

	// 들쭉날쭉함 (TerrainRuggedness). 위의 큰 언덕만 있으면 작은 행성 한 바퀴에 언덕이 서너 개뿐이라 완만해 보였다.
	// 그 위에 두 겹을 더 얹는다. 둘 다 같은 Dir이면 언제나 같은 값이라 메시, 배치, 착륙이 모두 그대로 맞는다.
	float Bumps = 0.f; // cm. 행성 크기와 상관없이 일정한 높이의 잔 굴곡
	if (TerrainRuggedness > 0.f)
	{
		// 생물 군계별 성격: 바위는 능선이 날카롭고, 버섯숲은 둔덕이 울퉁불퉁하고, 얼음은 비교적 얌전하다.
		float RidgeWeight = 0.5f;
		float BumpHeight = 70.f;
		if (Planet.Biome == EHGPlanetBiome::Ice)
		{
			RidgeWeight = 0.4f;
			BumpHeight = 45.f;
		}
		else if (Planet.Biome == EHGPlanetBiome::Rock)
		{
			RidgeWeight = 0.7f;
			BumpHeight = 80.f;
		}

		// 1) 능선(ridged noise): 노이즈 값이 0을 지나는 줄을 따라 1 - |노이즈|가 뾰족하게 솟는다. 제곱해서 봉우리를 더 날카롭게 한다.
		//    큰 언덕보다 약 2.7배 촘촘하게 깔고 세 겹 겹친다. 평균이 약 0.5라 빼서 행성이 통째로 부풀지 않게 한다.
		FVector P = Dir * (Planet.TerrainFrequency * 2.7f) + NoiseOffset * 1.37;
		float Ridge = 0.f;
		float Amp = 1.f;
		float Norm = 0.f;
		for (int32 i = 0; i < 3; ++i)
		{
			const float R = 1.f - FMath::Abs(FMath::PerlinNoise3D(P));
			Ridge += Amp * R * R;
			Norm += Amp;
			Amp *= 0.5f;
			P *= 2.07;
		}
		Height += RidgeWeight * TerrainRuggedness * (Ridge / Norm - 0.5f);

		// 2) 잔 굴곡: 약 9m 폭의 둔덕. 지형 격자 한 칸이 크면(중형 행성) 격자가 담을 수 있게 폭을 격자 4칸 이상으로 늘리고,
		//    폭이 늘어난 만큼 높이도 키워 기울기가 비슷하게 보이게 한다.
		const int32 Grid = FMath::Clamp(FMath::CeilToInt(Planet.Radius * UE_HALF_PI / TargetEdgeLength), 8, MaxGridPerFace);
		const float CellLength = Planet.Radius * UE_HALF_PI / Grid;
		const float Wavelength = FMath::Max(900.f, CellLength * 4.f);
		Bumps = BumpHeight * TerrainRuggedness * (Wavelength / 900.f)
			* Fbm(Dir * (Planet.Radius / Wavelength) - NoiseOffset * 0.71, 2) * 2.f;
	}

	// 착륙 지점(북극, +Z) 주변은 평평하게 다진다. 북극에서 잰 표면 거리가 평탄 반경 안이면 높이를 0으로,
	// 그 바깥 한 배 거리 동안은 부드럽게 원래 높이로 되돌린다.
	const float AngleFromPole = FMath::Acos(FMath::Clamp(Dir.Z, -1.0, 1.0));
	const float ArcDistance = AngleFromPole * Planet.Radius;
	const float Flatten = FMath::SmoothStep(LandingFlatRadius, LandingFlatRadius * 2.f, ArcDistance);

	return Planet.Radius * (1.f + Planet.TerrainAmplitude * Height * Flatten) + Bumps * Flatten;
}

void AHGPlanetGenerator::BuildTerrain()
{
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

	Terrain->ClearAllMeshSections();
	if (IsMoodEnabled())
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
			const float Patch = FMath::Clamp(0.5f + 1.4f * FMath::PerlinNoise3D(Centroid / 1500.0 + NoiseOffset * 0.5), 0.f, 1.f);
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
		Terrain->CreateMeshSection(0, FlatVertices, FlatTriangles, FlatNormals, FlatUVs, FlatColors, TArray<FProcMeshTangent>(), true);
		if (UMaterialInterface* VertexColorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/HG/Materials/M_HG_VertexColor.M_HG_VertexColor")))
		{
			Terrain->SetMaterial(0, VertexColorMaterial);
		}
	}
	else
	{
		Terrain->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		HGPlaceholder::Paint(Terrain, HGPlanet::GroundColor(Planet.Biome));
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
	const int32 RockCount = FMath::Clamp(FMath::RoundToInt(AreaM2 / 2000.0 * Planet.Richness), 20, 1500);
	// 재료 표식은 낮 재료와 밤 재료를 다 만든 뒤 지금 시간대가 아닌 것을 숨긴다. 한 번에 보이는 것은 대략 절반이므로
	// 한 시간대에 약 3500m²당 1개가 보이도록 두 배로 만든다.
	const int32 IngredientCount = FMath::Clamp(FMath::RoundToInt(AreaM2 / 1750.0 * Planet.Richness), 24, 800);

	// 착륙 지점 근처에는 아무것도 두지 않는다 (발판과 착륙한 사장님이 바위에 묻히지 않게).
	const double MinPoleAngle = (LandingFlatRadius * 0.8) / Planet.Radius;
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
		FVector T1, T2;
		TangentBasis(Dir, T1, T2);
		const float Spin = Rng.FRandRange(0.f, UE_TWO_PI);
		const FVector Forward = T1 * FMath::Cos(Spin) + T2 * FMath::Sin(Spin);
		const FQuat Rot = FRotationMatrix::MakeFromZX(Dir, Forward).ToQuat();
		// 인스턴스 좌표는 컴포넌트(행성 중심) 기준이다.
		return FTransform(Rot, Dir * (SurfaceRadiusAt(Dir) - Sink), Scale);
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
	}

	UE_LOG(LogHuoguo, Log, TEXT("행성 배치 (%s): 바위 %d, 재료 %d (종류 %d, 다른 시간대 재료 %d개 숨김, 이미 캔 것 %d개 숨김, 지금 캘 수 있는 것 %d)"),
		HGDay::PhaseName(Phase), RockCount, IngredientCount, IngredientIds.Num(), OtherTimeCount, HiddenCount, GetRemainingIngredientCount());
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
	// 착륙 지점은 북극(+Z)이다. 그 주변은 SurfaceRadiusAt에서 평평하게 다졌다.
	// 캡슐 반높이(92) + 여유 3만큼 띄워서 발이 땅에 닿게 한다.
	const FVector Up = FVector::UpVector;
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
	const FVector Location = GetActorLocation() + Dir * (SurfaceRadiusAt(Dir) + 60.f);
	const FTransform PadTransform(FRotationMatrix::MakeFromZ(Dir).ToQuat(), Location);

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
