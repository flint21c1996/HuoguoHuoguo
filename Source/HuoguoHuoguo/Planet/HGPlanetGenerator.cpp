#include "Planet/HGPlanetGenerator.h"

#include "Components/BillboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Debug/HGPlaceholderPaint.h"
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
