#include "World/Scan/HGScanWave.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Debug/HGVoxelShapes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/HGGameInstance.h"
#include "GameFramework/PlayerController.h"
#include "HuoguoHuoguo.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/Planet/HGPlanetGenerator.h"
#include "UObject/ConstructorHelpers.h"

AHGScanWave::AHGScanWave()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);

	// 인스턴스 메시 둘: 월드 원점에 고정해 인스턴스 좌표 = 월드 좌표. 매 프레임 다시 놓으므로 움직이는(Movable) 컴포넌트.
	UStaticMesh* const Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
	auto MakeIsm = [this, Cube](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Ism = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Ism->SetupAttachment(RootComponent);
		Ism->SetMobility(EComponentMobility::Movable);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetCastShadow(false);
		Ism->SetCanEverAffectNavigation(false);
		if (Cube)
		{
			Ism->SetStaticMesh(Cube);
		}
		return Ism;
	};
	Ring = MakeIsm(TEXT("Ring"));
	Wires = MakeIsm(TEXT("Wires"));

	// 화면 효과: 경계 없는 포스트 프로세스 (이 액터가 있는 동안 화면 전체에 얹힌다).
	Effect = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Effect"));
	Effect->SetupAttachment(RootComponent);
	Effect->bUnbound = true;
	Effect->Priority = 10.f;
}

void AHGScanWave::SetOtherBodies(const TArray<FHGGravityBody>& Bodies, int32 SelfIndex)
{
	Others.Reset();
	if (!Bodies.IsValidIndex(SelfIndex))
	{
		return;
	}
	const FHGGravityBody& Self = Bodies[SelfIndex];
	for (int32 i = 0; i < Bodies.Num(); ++i)
	{
		if (i != SelfIndex)
		{
			Others.Add(FVector4(Bodies[i].Center, Bodies[i].Radius));
		}
	}
	// 지표 사이 간격이 가까운 이웃부터 (화면 효과에는 앞의 8개만 넘긴다).
	Others.Sort([&Self](const FVector4& A, const FVector4& B)
	{
		const double GapA = FVector::Dist(FVector(A), Self.Center) - A.W;
		const double GapB = FVector::Dist(FVector(B), Self.Center) - B.W;
		return GapA < GapB;
	});
}

void AHGScanWave::Begin(AHGPlanetGenerator* InPlanet, const FVector& InCenter, float InRadius, const FVector& Start, float InRange,
	TFunction<void(const FString&)> InOnFinished)
{
	Planet = InPlanet;
	Center = InCenter;
	StartPoint = Start;
	Radius = FMath::Max(InRadius, 100.f);
	// 행성 반 바퀴(pi * R)를 넘게 퍼지면 반대편에서 고리가 다시 오므라든다. 그 전에 멈춘다.
	Range = FMath::Min(InRange, Radius * UE_PI * 0.95f);
	Up = (Start - Center).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	Tangent1 = FVector::CrossProduct(Up, FMath::Abs(Up.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	Tangent2 = FVector::CrossProduct(Up, Tangent1);
	OnFinished = MoveTemp(InOnFinished);
	SetActorTransform(FTransform::Identity);

	// 산소 청록으로 밝게 빛나게 한다.
	const FLinearColor Cyan(0.3f, 1.f, 0.9f);
	HGVoxel::PaintGlow(Ring, Cyan, 8.f);
	HGVoxel::PaintGlow(Wires, Cyan, 10.f);

	if (UMaterialInterface* ScanMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/HG/Materials/M_HG_ScanPulse.M_HG_ScanPulse")))
	{
		EffectMaterial = UMaterialInstanceDynamic::Create(ScanMaterial, this);
		EffectMaterial->SetVectorParameterValue(TEXT("Origin"), FLinearColor(Start));
		EffectMaterial->SetVectorParameterValue(TEXT("Center"), FLinearColor(Center));
		EffectMaterial->SetVectorParameterValue(TEXT("ScanColor"), Cyan);
		// 천체 반지름의 1.25배 안쪽만 받는다 (지형 높낮이, 나무, 볼거리까지). 하늘의 섬과 다른 소행성은 빠진다.
		EffectMaterial->SetScalarParameterValue(TEXT("Limit"), Radius * 1.25f + 300.f);
		EffectMaterial->SetScalarParameterValue(TEXT("Width"), 220.f);
		EffectMaterial->SetScalarParameterValue(TEXT("Grid"), 100.f);
		EffectMaterial->SetScalarParameterValue(TEXT("Radius"), 0.f);
		EffectMaterial->SetScalarParameterValue(TEXT("Fade"), 0.f);
		EffectMaterial->SetScalarParameterValue(TEXT("BodyRadius"), Radius);
		// 이웃 천체 8칸: 없으면 반지름 0(머티리얼이 건너뛴다).
		for (int32 i = 0; i < 8; ++i)
		{
			const FLinearColor Neighbor = Others.IsValidIndex(i)
				? FLinearColor(static_cast<float>(Others[i].X), static_cast<float>(Others[i].Y), static_cast<float>(Others[i].Z), static_cast<float>(Others[i].W))
				: FLinearColor(0.f, 0.f, 0.f, 0.f);
			EffectMaterial->SetVectorParameterValue(*FString::Printf(TEXT("Neighbor%d"), i), Neighbor);
		}
		Effect->Settings.AddBlendable(EffectMaterial, 1.f);
	}
	else
	{
		UE_LOG(LogHuoguo, Warning, TEXT("[Scan] 화면 효과 머티리얼 M_HG_ScanPulse가 없어 지표 고리로 대신 그린다 (Tools/Invoke-HGMapGen.ps1)"));
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Scan] 스캔 시작: 천체 반지름 %.0fm, 퍼질 거리 %.0fm, 화면 효과 %s, 뺄 이웃 천체 %d"), Radius / 100.f, Range / 100.f,
		EffectMaterial ? TEXT("있음") : TEXT("없음"), Others.Num());
}

void AHGScanWave::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Planet.IsValid())
	{
		Destroy();
		return;
	}
	Elapsed += DeltaSeconds;
	const float T = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	if (!bWaveDone)
	{
		// 처음이 빠르고 끝이 느리게 퍼진다 (물결처럼).
		const float Spread = 1.f - FMath::Square(1.f - T);
		const float Angle = Spread * Range / Radius;
		TouchMarkers(Angle);
		if (EffectMaterial)
		{
			UpdateEffect(Angle, T);
		}
		else
		{
			PlaceRing(Angle, T);
		}
		if (T >= 1.f)
		{
			bWaveDone = true;
			Ring->ClearInstances();
			if (EffectMaterial)
			{
				EffectMaterial->SetScalarParameterValue(TEXT("Fade"), 0.f);
			}
			FString Result = FString::Printf(TEXT("스캔 완료: 재료 %d개 감지"), TouchedCount);
			if (RevealedCount > 0)
			{
				Result += FString::Printf(TEXT(", 숨은 재료 %d개 드러남"), RevealedCount);
			}
			if (NewDiscoveries > 0)
			{
				Result += FString::Printf(TEXT(", 도감 새 기록 %d"), NewDiscoveries);
			}
			UE_LOG(LogHuoguo, Log, TEXT("[Scan] %s"), *Result);
			if (OnFinished)
			{
				OnFinished(Result);
			}
		}
	}
	// 파동이 끝나도 와이어 상자가 다 사라질 때까지 기다렸다가 없앤다.
	const bool bWiresLeft = UpdateWires();
	if (bWaveDone && !bWiresLeft)
	{
		Destroy();
	}
}

void AHGScanWave::TouchMarkers(float Angle)
{
	AHGPlanetGenerator* P = Planet.Get();
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	const float CosAngle = FMath::Cos(Angle);
	const float Now = GetWorld()->GetTimeSeconds();
	const TArray<FHGIngredientMarker>& Markers = P->GetIngredientMarkers();
	for (int32 i = 0; i < Markers.Num(); ++i)
	{
		const FHGIngredientMarker& Marker = Markers[i];
		// 캔 것, 다른 시간대 것은 건너뛴다. 숨은 재료는 이번에 드러날 수 있으니 본다.
		if (Marker.bHarvested || Marker.bOtherTime || Touched.Contains(i))
		{
			continue;
		}
		// 같은 천체 위의 표식만 (중심에서 반지름의 1.6배 안). 다른 소행성의 표식은 파동이 닿지 않는다.
		const FVector ToMarker = Marker.Location - Center;
		if (ToMarker.Size() > Radius * 1.6f)
		{
			continue;
		}
		if ((ToMarker.GetSafeNormal() | Up) < CosAngle)
		{
			continue;
		}
		// 이 재료가 붙어 있는 천체가 이웃 쪽이면 건너뛴다 (중심까지 거리 / 반지름이 더 작은 이웃이 있으면).
		const double Self = ToMarker.Size() / Radius;
		bool bOnNeighbor = false;
		for (const FVector4& Other : Others)
		{
			if (FVector::Dist(Marker.Location, FVector(Other)) / Other.W < Self)
			{
				bOnNeighbor = true;
				break;
			}
		}
		if (bOnNeighbor)
		{
			continue;
		}
		Touched.Add(i);
		++TouchedCount;
		if (P->ScanMarker(i, Now))
		{
			++RevealedCount;
		}
		if (GI && GI->DiscoverIngredient(P->GetMarkerIngredient(i)))
		{
			++NewDiscoveries;
		}
		Analyzing.Add({ Marker.Location, ToMarker.GetSafeNormal(), Elapsed });
	}
}

void AHGScanWave::UpdateEffect(float Angle, float Fade)
{
	// 표면 거리 d = Angle * R에 해당하는 직선 거리 (현의 길이). 화면 효과는 시작점에서의 직선 거리로 고리를 그린다.
	const float Chord = 2.f * Radius * FMath::Sin(Angle * 0.5f);
	EffectMaterial->SetScalarParameterValue(TEXT("Radius"), Chord);
	// 처음 0.1초에 켜지고 마지막 30%에서 꺼진다.
	const float In = FMath::Clamp(Elapsed / 0.1f, 0.f, 1.f);
	const float Out = 1.f - FMath::SmoothStep(0.7f, 1.f, Fade);
	EffectMaterial->SetScalarParameterValue(TEXT("Fade"), In * Out);
	if (!bLoggedFirst && Fade > 0.3f)
	{
		bLoggedFirst = true;
		UE_LOG(LogHuoguo, Log, TEXT("[Scan] 화면 효과 진행: 고리 직선 반지름 %.1fm"), Chord / 100.f);
	}
}

bool AHGScanWave::UpdateWires()
{
	Wires->ClearInstances();
	TArray<FTransform> Edges;
	bool bAny = false;
	for (const FAnalyze& A : Analyzing)
	{
		const float Age = Elapsed - A.Start;
		if (Age < 0.f || Age > AnalyzeSeconds)
		{
			continue;
		}
		bAny = true;
		// 처음에 크게 튀어나왔다가 줄어들며 돈다. 끝 30%에서 가늘어지며 사라진다.
		const float T = Age / AnalyzeSeconds;
		const float H = AnalyzeHalfSize * (1.f + 0.6f * FMath::Exp(-T * 8.f));
		const float Thick = 3.f * (1.f - FMath::SmoothStep(0.7f, 1.f, T));
		const FVector Side = FVector::CrossProduct(A.Up, FMath::Abs(A.Up.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		const FQuat Frame = FQuat(A.Up, Age * UE_PI) * FRotationMatrix::MakeFromZX(A.Up, Side).ToQuat();
		const FVector Mid = A.Location + A.Up * H * 0.3f;
		// 정육면체 모서리 12개: 각 축 방향 모서리 4개씩.
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (int32 k = 0; k < 4; ++k)
			{
				const float S1 = (k & 1) ? 1.f : -1.f;
				const float S2 = (k & 2) ? 1.f : -1.f;
				FVector Offset;
				FVector Size;
				if (Axis == 0)
				{
					Offset = FVector(0.f, S1 * H, S2 * H);
					Size = FVector(2.f * H, Thick, Thick);
				}
				else if (Axis == 1)
				{
					Offset = FVector(S1 * H, 0.f, S2 * H);
					Size = FVector(Thick, 2.f * H, Thick);
				}
				else
				{
					Offset = FVector(S1 * H, S2 * H, 0.f);
					Size = FVector(Thick, Thick, 2.f * H);
				}
				Edges.Add(FTransform(Frame, Mid + Frame.RotateVector(Offset), Size / 100.f));
			}
		}
	}
	if (Edges.Num() > 0)
	{
		Wires->AddInstances(Edges, false, true);
	}
	return bAny || !bWaveDone;
}

void AHGScanWave::PlaceRing(float Angle, float Fade)
{
	Ring->ClearInstances();
	if (Angle <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	// 고리 점: 중심에서 Dir 방향으로 천체 바깥 한참 위에서 중심 쪽으로 선을 그어 처음 닿는 지표에 놓는다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HGScanRing), false, this);
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		Params.AddIgnoredActor(PC->GetPawn());
	}
	const float SinA = FMath::Sin(Angle);
	const float CosA = FMath::Cos(Angle);
	const float Size = FMath::Lerp(0.3f, 0.05f, Fade * Fade);
	TArray<FTransform> Points;
	Points.Reserve(RingPoints);
	for (int32 k = 0; k < RingPoints; ++k)
	{
		const float Phi = UE_TWO_PI * k / RingPoints;
		const FVector Dir = (Up * CosA + (Tangent1 * FMath::Cos(Phi) + Tangent2 * FMath::Sin(Phi)) * SinA).GetSafeNormal();
		const FVector From = Center + Dir * (Radius * 1.5f + 3000.f);
		const FVector To = Center + Dir * (Radius * 0.5f);
		FHitResult Hit;
		FVector Spot = Center + Dir * Radius;
		FVector Normal = Dir;
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
		{
			Spot = Hit.ImpactPoint;
			Normal = Hit.ImpactNormal;
		}
		// 고리를 따라(둘레 방향으로) 길쭉한 납작한 판. 둘레 방향 = Phi로 미분한 방향.
		const FVector Along = Tangent2 * FMath::Cos(Phi) - Tangent1 * FMath::Sin(Phi);
		const FQuat Rot = FRotationMatrix::MakeFromZX(Normal, Along).ToQuat();
		Points.Add(FTransform(Rot, Spot + Normal * 8.f, FVector(Size * 2.f, Size, 0.06f)));
	}
	Ring->AddInstances(Points, false, true);
}
