#include "Planet/HGPlanetGenerator.h"

#include "Components/BillboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/HGGameInstance.h"
#include "HuoguoHuoguo.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/HGTravelPad.h"

namespace
{
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
	IngredientMarkers.Add(MakeIsm(TEXT("Ingredient_Mushroom"), SphereMesh.Object, false));
	IngredientMarkers.Add(MakeIsm(TEXT("Ingredient_Kelp"), SphereMesh.Object, false));
	IngredientMarkers.Add(MakeIsm(TEXT("Ingredient_Chili"), SphereMesh.Object, false));

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

	UE_LOG(LogHuoguo, Log, TEXT("행성 생성 완료: %s (시드 %d), %.0f ms"),
		*Planet.Describe(), Planet.Seed, (FPlatformTime::Seconds() - StartTime) * 1000.0);
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

	// 착륙 지점(북극, +Z) 주변은 평평하게 다진다. 북극에서 잰 표면 거리가 평탄 반경 안이면 높이를 0으로,
	// 그 바깥 한 배 거리 동안은 부드럽게 원래 높이로 되돌린다.
	const float AngleFromPole = FMath::Acos(FMath::Clamp(Dir.Z, -1.0, 1.0));
	const float ArcDistance = AngleFromPole * Planet.Radius;
	const float Flatten = FMath::SmoothStep(LandingFlatRadius, LandingFlatRadius * 2.f, ArcDistance);

	return Planet.Radius * (1.f + Planet.TerrainAmplitude * Height * Flatten);
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
	Terrain->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	HGPlaceholder::Paint(Terrain, HGPlanet::GroundColor(Planet.Biome));

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
	const int32 IngredientCount = FMath::Clamp(FMath::RoundToInt(AreaM2 / 3500.0 * Planet.Richness), 12, 400);

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

	// 재료: 생물 군계에 맞는 재료가 절반, 나머지는 고르게 섞는다.
	const int32 Favored = Planet.Biome == EHGPlanetBiome::Fungal ? 0 : (Planet.Biome == EHGPlanetBiome::Ice ? 1 : 2);
	for (UInstancedStaticMeshComponent* Ism : IngredientMarkers)
	{
		Ism->ClearInstances();
	}
	for (int32 i = 0; i < IngredientCount; ++i)
	{
		const int32 Kind = Rng.FRand() < 0.5f ? Favored : Rng.RandRange(0, 2);
		const FVector Dir = RandomDir();
		IngredientMarkers[Kind]->AddInstance(SurfaceTransform(Dir, -22.f, FVector(0.4f)));
	}

	HGPlaceholder::Paint(RoundRocks, HGPlanet::RockColor(Planet.Biome));
	HGPlaceholder::Paint(BlockRocks, HGPlanet::RockColor(Planet.Biome) * 0.8f);
	HGPlaceholder::Paint(IngredientMarkers[0], FLinearColor(0.55f, 0.40f, 0.95f));
	HGPlaceholder::Paint(IngredientMarkers[1], FLinearColor(0.30f, 0.85f, 0.65f));
	HGPlaceholder::Paint(IngredientMarkers[2], FLinearColor(0.95f, 0.25f, 0.15f));

	UE_LOG(LogHuoguo, Log, TEXT("행성 배치: 바위 %d, 재료 %d"), RockCount, IngredientCount);
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
}
