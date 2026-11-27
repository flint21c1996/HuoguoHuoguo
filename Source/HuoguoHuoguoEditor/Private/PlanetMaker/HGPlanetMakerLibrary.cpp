#include "PlanetMaker/HGPlanetMakerLibrary.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "ScopedTransaction.h"
#include "World/Planet/HGPlanetGenerator.h"

#define LOCTEXT_NAMESPACE "HGPlanetMakerLibrary"

namespace
{
	UWorld* EditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	// 랜드스케이프 높이 값(uint16): 32768 = 0. 랜드스케이프 Z 배율이 100이면 1 = 100/128 cm.
	constexpr float LandscapeScale = 100.f;
	uint16 ToLandscapeHeight(float HeightCm)
	{
		const float Value = 32768.f + HeightCm * 128.f / LandscapeScale;
		return static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(Value), 0, 65535));
	}
}

bool UHGPlanetMakerLibrary::HasLandscape()
{
	UWorld* World = EditorWorld();
	return World && TActorIterator<ALandscapeProxy>(World);
}

FHGPlanetPreset UHGPlanetMakerLibrary::GetDefaultPreset(EHGSiteType SiteType)
{
	return HGPlanet::DefaultPreset(SiteType);
}

FString UHGPlanetMakerLibrary::ApplyBasinToLandscape(const FHGPlanetPreset& Preset)
{
	UWorld* World = EditorWorld();
	if (!World)
	{
		return TEXT("에디터 맵이 열려 있지 않아요.");
	}
	const double StartTime = FPlatformTime::Seconds();
	const FHGBasinShape& Basin = Preset.Basin;
	const int32 Quads = Basin.QuadsPerSide();
	const int32 Verts = Quads + 1;

	// 랜드스케이프 가운데 = 행성 생성기 위치 (착륙 지점이 분지 가운데가 되게).
	AHGPlanetGenerator* Generator = nullptr;
	for (TActorIterator<AHGPlanetGenerator> It(World); It; ++It)
	{
		Generator = *It;
		break;
	}
	const FVector Center = Generator ? Generator->GetActorLocation() : FVector::ZeroVector;

	// 1) 높이 계산 (가운데 기준 XY, 1칸 = 100cm). 노이즈 이동값은 시드에서 한 번만 구한다.
	const FVector NoiseOffset = FHGBasinShape::NoiseOffsetOf(Preset.Seed);
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(Verts * Verts);
	float MinHeight = TNumericLimits<float>::Max();
	float MaxHeight = TNumericLimits<float>::Lowest();
	for (int32 Y = 0; Y < Verts; ++Y)
	{
		for (int32 X = 0; X < Verts; ++X)
		{
			const FVector2D XY((X - Quads * 0.5) * 100.0, (Y - Quads * 0.5) * 100.0);
			const float H = Basin.HeightAt(XY, NoiseOffset);
			MinHeight = FMath::Min(MinHeight, H);
			MaxHeight = FMath::Max(MaxHeight, H);
			Heights[Y * Verts + X] = ToLandscapeHeight(H);
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("ApplyBasin", "Planet Maker: 분지 랜드스케이프 만들기"));

	// 2) 있던 랜드스케이프를 지운다.
	TArray<ALandscapeProxy*> OldLandscapes;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		OldLandscapes.Add(*It);
	}
	for (ALandscapeProxy* Old : OldLandscapes)
	{
		World->EditorDestroyActor(Old, true);
	}

	// 3) 새 랜드스케이프 (엔진의 "새 랜드스케이프 만들기"와 같은 순서). 왼쪽 아래 모서리가 액터 위치다.
	const FVector Corner = Center - FVector(Quads * 0.5 * LandscapeScale, Quads * 0.5 * LandscapeScale, 0.0);
	ALandscape* Landscape = World->SpawnActor<ALandscape>(Corner, FRotator::ZeroRotator);
	if (!Landscape)
	{
		return TEXT("랜드스케이프를 만들지 못했어요.");
	}
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/HG/Materials/M_HG_LandscapeBasin.M_HG_LandscapeBasin"));
	Landscape->LandscapeMaterial = Material;
	// 게임에서 땅 색을 군계에 맞춰 바꾸려면(PaintFlatGround) 부품마다 동적 머티리얼이 있어야 한다. 꺼져 있으면 색 값 바꾸기가 무시된다.
	Landscape->bUseDynamicMaterialInstance = true;
	Landscape->SetActorRelativeScale3D(FVector(LandscapeScale));

	TMap<FGuid, TArray<uint16>> HeightData;
	HeightData.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayerData;
	LayerData.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
	// 부품 하나 = 섹션 1개 x 63칸.
	Landscape->Import(FGuid::NewGuid(), 0, 0, Quads, Quads, 1, 63, HeightData, TEXT(""), LayerData,
		ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	FActorLabelUtilities::SetActorLabelUnique(Landscape, TEXT("BasinLandscape"));
	if (ULandscapeInfo* Info = Landscape->GetLandscapeInfo())
	{
		Info->UpdateLayerInfoMap(Landscape);
	}

	// 4) 떨어짐 높이: 땅에서 가장 낮은 곳보다 20m 아래.
	AWorldSettings* WorldSettings = World->GetWorldSettings();
	WorldSettings->Modify();
	WorldSettings->KillZ = Center.Z + MinHeight - 2000.f;

	// 5) 생성기를 평지 방식으로. 재료와 바위는 둑 꼭대기 안쪽 원판에만 놓는다.
	const float PlacementRadius = Basin.HalfExtentCm() * Basin.RimRatio;
	if (Generator)
	{
		Generator->SetFlatSite(Preset.ToPlanetSeed(), PlacementRadius);
	}

	return FString::Printf(TEXT("분지 랜드스케이프를 만들었어요: %dm x %dm (1m 격자), 높이 %.0f ~ %.0fm, KillZ %.0fm, 배치 반지름 %.0fm%s%s | %.0fms"),
		Quads, Quads, MinHeight / 100.f, MaxHeight / 100.f, WorldSettings->KillZ / 100.f, PlacementRadius / 100.f,
		Generator ? TEXT("") : TEXT(" | 행성 생성기가 없어서 배치는 설정하지 못했어요"),
		Material ? TEXT("") : TEXT(" | 머티리얼 M_HG_LandscapeBasin이 없어요 (Tools/Invoke-HGMapGen.ps1)"),
		(FPlatformTime::Seconds() - StartTime) * 1000.0);
}

#undef LOCTEXT_NAMESPACE
