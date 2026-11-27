#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "World/Planet/HGPlanetGenerator.h"
#include "World/Planet/HGPlanetPreset.h"
#include "World/Planet/HGPlanetTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 행성 지형 모양 시험. 명령 `Automation RunTests HG.Planet` 로 돌린다.
 *
 * 하는 일:
 *   여러 시드의 행성(소형, 중형, 세 군계, 소행성 지대)마다 ChoosePlanet으로 지형 값(노이즈 이동값, 크레이터)을 정하고,
 *   구 위에 고르게 뿌린 방향 4000개에서 SurfaceRadiusAt(중심에서 지표까지 거리)을 읽는다.
 *   거리를 mm 단위 정수로 반올림해 차례로 섞은 체크섬이 기록해 둔 값과 같은지 본다.
 *
 * 왜 필요한가:
 *   지형 값(언덕 겹 수, 능선 세기, 크레이터 크기 등)을 코드 숫자에서 편집 가능한 묶음으로 옮길 때,
 *   기본값이 예전 숫자와 정확히 같아서 이미 있던 행성 모양이 그대로인지 증명한다.
 *   체크섬이 바뀌면 모든 시드의 행성 모양이 바뀐 것이다. 일부러 바꿨다면 새 값을 기록한다.
 *
 * 노이즈는 정수라 어디서나 같지만, 그 뒤 계산(삼각함수, 실수 곱)은 아직 실수다. 그래서 이 체크섬은
 * 같은 컴퓨터, 같은 빌드에서 "코드를 정리하기 전과 후가 같은가"를 보는 용도다 (다른 플랫폼 일치는 HG.Noise가 본다).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHGPlanetTerrainTest, "HG.Planet.TerrainShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHGPlanetTerrainTest::RunTest(const FString& Parameters)
{
	// 시험할 행성들: 시드 1 ~ 10으로 만든 행성 + 시드 11부터 처음 나오는 중형 행성 둘 + 같은 시드 둘을 소행성 지대로 바꾼 것.
	// (시드 1 ~ 10에는 중형이 없어서 따로 찾는다.)
	TArray<FHGPlanetSeed> Planets;
	for (int32 Seed = 1; Seed <= 10; ++Seed)
	{
		Planets.Add(HGPlanet::MakePlanet(Seed));
	}
	int32 MediumCount = 0;
	for (int32 Seed = 11; Seed < 1000 && MediumCount < 2; ++Seed)
	{
		const FHGPlanetSeed Candidate = HGPlanet::MakePlanet(Seed);
		if (Candidate.SizeClass == EHGPlanetSize::Medium && !Candidate.bAsteroidField && !Candidate.bDerelict)
		{
			Planets.Add(Candidate);
			++MediumCount;
		}
	}
	TestEqual(TEXT("중형 행성 둘을 찾음"), MediumCount, 2);
	for (int32 Seed : { 3, 8 })
	{
		FHGPlanetSeed Field = HGPlanet::MakePlanet(Seed);
		HGPlanet::MakeAsteroidField(Field);
		Planets.Add(Field);
	}

	// 구 위에 고르게 뿌린 방향 (피보나치 나선). 매번 같은 방향들이다.
	constexpr int32 SampleCount = 4000;
	TArray<FVector> Dirs;
	Dirs.Reserve(SampleCount);
	const double GoldenAngle = UE_DOUBLE_PI * (3.0 - FMath::Sqrt(5.0));
	for (int32 i = 0; i < SampleCount; ++i)
	{
		const double Z = 1.0 - 2.0 * (i + 0.5) / SampleCount;
		const double R = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
		const double Phi = GoldenAngle * i;
		Dirs.Add(FVector(R * FMath::Cos(Phi), R * FMath::Sin(Phi), Z));
	}

	// 월드 없이 생성기를 만든다. 월드가 없으면 GameInstance도 없어서 ChoosePlanet이 PreviewPlanet을 그대로 쓴다.
	AHGPlanetGenerator* Generator = NewObject<AHGPlanetGenerator>(GetTransientPackage());
	if (!TestNotNull(TEXT("생성기 만들기"), Generator))
	{
		return false;
	}
	Generator->bPreviewFromSeedOnly = false;

	uint64 Checksum = 0;
	for (const FHGPlanetSeed& Seed : Planets)
	{
		Generator->PreviewPlanet = Seed;
		Generator->ChoosePlanet();

		uint64 PlanetSum = 0;
		for (const FVector& Dir : Dirs)
		{
			const double Radius = Generator->SurfaceRadiusAt(Dir);
			const int64 Millimeters = static_cast<int64>(FMath::RoundToDouble(Radius * 10.0));
			PlanetSum = PlanetSum * 1099511628211ull + static_cast<uint64>(Millimeters);
		}
		Checksum = Checksum * 31ull + PlanetSum;
		AddInfo(FString::Printf(TEXT("[Terrain] %s | 크레이터 %d | 행성 체크섬 %lld"),
			*Generator->GetPlanet().Describe(), Generator->Craters.Num(), static_cast<int64>(PlanetSum)));
	}
	// 지형 모양 값(PreviewShape)이 실제로 쓰이는지:
	//   기본 모양을 그대로 넣으면 같은 지형, 크레이터를 0개로 하면 크레이터가 없고 지형이 달라져야 한다.
	{
		auto PlanetChecksum = [&Dirs, Generator]()
		{
			uint64 Sum = 0;
			for (const FVector& Dir : Dirs)
			{
				Sum = Sum * 1099511628211ull + static_cast<uint64>(static_cast<int64>(FMath::RoundToDouble(Generator->SurfaceRadiusAt(Dir) * 10.0)));
			}
			return Sum;
		};
		const FHGPlanetSeed Seed = Planets[4]; // 소형 바위
		Generator->PreviewPlanet = Seed;
		Generator->bUsePreviewShape = false;
		Generator->ChoosePlanet();
		const uint64 DefaultSum = PlanetChecksum();

		Generator->bUsePreviewShape = true;
		Generator->PreviewShape = HGPlanet::DefaultShape(Seed);
		Generator->ChoosePlanet();
		TestEqual(TEXT("기본 모양을 PreviewShape로 넣으면 같은 지형"), PlanetChecksum(), DefaultSum);

		Generator->PreviewShape.CraterCountMin = 0;
		Generator->PreviewShape.CraterCountMax = 0;
		Generator->ChoosePlanet();
		TestEqual(TEXT("크레이터 0개"), Generator->Craters.Num(), 0);
		TestNotEqual(TEXT("크레이터를 없애면 지형이 달라짐"), PlanetChecksum(), DefaultSum);
		Generator->bUsePreviewShape = false;
	}
	Generator->MarkAsGarbage();

	AddInfo(FString::Printf(TEXT("[Terrain] 전체 체크섬 %lld"), static_cast<int64>(Checksum)));

	// 기록해 둔 체크섬.
	//   8815671282531004901: 2026-11-27, 지형 값을 묶음으로 옮기기 전 코드에서 기록. 옮긴 뒤에도 같았다.
	//   6078182265922157260 (지금): 2026-11-27, 행성 크기 절반 + 격자 100cm + 크레이터 절반 + 평탄 반경 상한 (일부러 바꿈).
	constexpr int64 ExpectedChecksum = 6078182265922157260LL;
	TestEqual(TEXT("지형 체크섬 (바뀌면 모든 시드의 행성 모양이 바뀐 것)"), static_cast<int64>(Checksum), ExpectedChecksum);
	return true;
}

/**
 * 행성 프리셋 JSON 시험: 저장한 파일을 다시 읽으면 같은 값이 나오는지, 환경 종류별 처음 값이 맞는 종류인지.
 * 파일은 Saved/Automation 아래 임시로 쓰고 지운다 (Content는 건드리지 않는다).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHGPlanetPresetTest, "HG.Planet.PresetJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHGPlanetPresetTest::RunTest(const FString& Parameters)
{
	for (EHGSiteType Type : { EHGSiteType::PlanetSmall, EHGSiteType::PlanetMedium, EHGSiteType::PlanetLarge, EHGSiteType::AsteroidField, EHGSiteType::Derelict })
	{
		const FHGPlanetPreset Preset = HGPlanet::DefaultPreset(Type);
		const FHGPlanetSeed Planet = Preset.ToPlanetSeed();
		const EHGSiteType Expected = Type; // 대형 = 평지 행성 (2026-11-27부터 bFlat)
		TestEqual(FString::Printf(TEXT("처음 값의 환경 종류 (%d)"), static_cast<int32>(Type)), HGPlanet::SiteTypeOf(Planet), Expected);
	}

	FHGPlanetPreset Preset = HGPlanet::DefaultPreset(EHGSiteType::PlanetMedium);
	Preset.Name = TEXT("시험 행성");
	Preset.Shape.CraterCountMin = 17;
	Preset.Shape.CraterCountMax = 23;
	Preset.Shape.HillPersistencePercent = 61;
	Preset.Shape.HeightCurve.GetRichCurve()->AddKey(-1.f, -0.5f);
	Preset.Shape.HeightCurve.GetRichCurve()->AddKey(1.f, 0.8f);

	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("HGPresetTest.json"));
	FString Error;
	TestTrue(TEXT("저장"), HGPlanet::SavePresetJson(Preset, Path, Error));
	FHGPlanetPreset Loaded;
	TestTrue(TEXT("불러오기"), HGPlanet::LoadPresetJson(Path, Loaded, Error));
	IFileManager::Get().Delete(*Path);

	TestEqual(TEXT("이름"), Loaded.Name, Preset.Name);
	TestEqual(TEXT("환경 종류"), Loaded.SiteType, Preset.SiteType);
	TestEqual(TEXT("시드"), Loaded.Seed, Preset.Seed);
	TestEqual(TEXT("반지름"), Loaded.Radius, Preset.Radius);
	TestEqual(TEXT("크레이터 최소"), Loaded.Shape.CraterCountMin, 17);
	TestEqual(TEXT("크레이터 최대"), Loaded.Shape.CraterCountMax, 23);
	TestEqual(TEXT("겹 감쇠"), Loaded.Shape.HillPersistencePercent, 61);
	TestEqual(TEXT("높이 곡선 키 수"), Loaded.Shape.HeightCurve.GetRichCurveConst()->GetNumKeys(), 2);
	TestEqual(TEXT("높이 곡선 값"), Loaded.Shape.HeightCurve.GetRichCurveConst()->Eval(1.f), 0.8f);
	TestEqual(TEXT("버전"), Loaded.Version, FHGPlanetPreset::CurrentVersion);
	return true;
}

#endif
