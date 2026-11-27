#include "World/Planet/HGPlanetPreset.h"

#include "HAL/FileManager.h"
#include "HuoguoHuoguo.h"
#include "JsonObjectConverter.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FHGPlanetSeed FHGPlanetPreset::ToPlanetSeed() const
{
	// 시드로 먼저 채우고(이름, 남은 시간 등) 프리셋 값으로 덮는다.
	FHGPlanetSeed P = HGPlanet::MakePlanet(Seed);
	P.Name = Name;
	P.Biome = Biome;
	P.Radius = Radius;
	P.TerrainAmplitude = TerrainAmplitude;
	P.TerrainFrequency = TerrainFrequency;
	P.GravityScale = GravityScale;
	P.Richness = Richness;
	P.bAsteroidField = SiteType == EHGSiteType::AsteroidField;
	P.bDerelict = SiteType == EHGSiteType::Derelict;
	// 평지 행성(대형)은 랜드스케이프 분지다. 넓이는 분지 값에서, 재료 개수용 반지름은 반폭의 절반(원판 넓이 = 구 겉넓이).
	P.bFlat = SiteType == EHGSiteType::PlanetLarge;
	if (P.bFlat)
	{
		P.Radius = Basin.HalfExtentCm() * Basin.RimRatio * 0.5f;
	}
	P.SizeClass = (SiteType == EHGSiteType::PlanetMedium || SiteType == EHGSiteType::PlanetLarge) ? EHGPlanetSize::Medium : EHGPlanetSize::Small;
	P.PresetName = FileName;
	return P;
}

FString HGPlanet::PresetDirectory()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("HG"), TEXT("Planets"));
}

FHGPlanetPreset HGPlanet::DefaultPreset(EHGSiteType SiteType)
{
	// 원하는 환경 종류가 나오는 첫 시드를 찾는다. 소행성 지대와 폐우주선은 아무 행성이나 바꾸면 된다.
	const EHGSiteType SearchType = SiteType == EHGSiteType::PlanetLarge ? EHGSiteType::PlanetMedium
		: (SiteType == EHGSiteType::AsteroidField || SiteType == EHGSiteType::Derelict) ? EHGSiteType::PlanetSmall
		: SiteType;
	FHGPlanetSeed Planet = MakePlanet(1);
	for (int32 Seed = 1; Seed < 1000; ++Seed)
	{
		Planet = MakePlanet(Seed);
		if (SiteTypeOf(Planet) == SearchType)
		{
			break;
		}
	}
	if (SiteType == EHGSiteType::AsteroidField)
	{
		MakeAsteroidField(Planet);
	}
	else if (SiteType == EHGSiteType::Derelict)
	{
		MakeDerelict(Planet);
	}
	else if (SiteType == EHGSiteType::PlanetLarge)
	{
		MakeFlat(Planet);
	}

	FHGPlanetPreset Preset;
	Preset.Name = Planet.Name;
	Preset.SiteType = SiteType;
	Preset.Seed = Planet.Seed;
	Preset.Biome = Planet.Biome;
	Preset.Radius = Planet.Radius;
	Preset.TerrainAmplitude = Planet.TerrainAmplitude;
	Preset.TerrainFrequency = Planet.TerrainFrequency;
	Preset.GravityScale = Planet.GravityScale;
	Preset.Richness = Planet.Richness;
	Preset.Shape = DefaultShape(Planet);
	return Preset;
}

bool HGPlanet::SavePresetJson(const FHGPlanetPreset& Preset, const FString& FilePath, FString& OutError)
{
	FHGPlanetPreset ToSave = Preset;
	ToSave.Version = FHGPlanetPreset::CurrentVersion;

	// UPROPERTY 이름이 그대로 JSON 키가 된다 (예: "radius", "shape": { "craterCountMin": 3, ... }).
	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(ToSave, Json))
	{
		OutError = TEXT("JSON으로 바꾸지 못했다.");
		return false;
	}
	if (!FFileHelper::SaveStringToFile(Json, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("파일을 쓰지 못했다: %s"), *FilePath);
		return false;
	}
	return true;
}

bool HGPlanet::LoadPresetJson(const FString& FilePath, FHGPlanetPreset& OutPreset, FString& OutError)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FilePath))
	{
		OutError = FString::Printf(TEXT("파일을 읽지 못했다: %s"), *FilePath);
		return false;
	}

	// 파일에 없는 값은 기본값으로 남도록 기본 프리셋 위에 읽는다.
	FHGPlanetPreset Loaded;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Loaded))
	{
		OutError = FString::Printf(TEXT("JSON 형식이 맞지 않는다: %s"), *FilePath);
		return false;
	}
	if (Loaded.Version > FHGPlanetPreset::CurrentVersion)
	{
		OutError = FString::Printf(TEXT("더 새 버전(%d)의 파일이라 읽지 않는다. 지금 버전은 %d."), Loaded.Version, FHGPlanetPreset::CurrentVersion);
		return false;
	}
	OutPreset = Loaded;
	OutPreset.FileName = FPaths::GetBaseFilename(FilePath);
	return true;
}

TArray<FHGPlanetPreset> HGPlanet::LoadPresetsInDirectory(const FString& Directory)
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("*.json")), true, false);
	// 파일 목록 순서는 운영체제마다 다를 수 있다. 섞는 결과가 어디서나 같도록 이름 순으로 정렬한다.
	Files.Sort();

	TArray<FHGPlanetPreset> Result;
	for (const FString& File : Files)
	{
		FHGPlanetPreset Preset;
		FString Error;
		if (LoadPresetJson(FPaths::Combine(Directory, File), Preset, Error))
		{
			Result.Add(MoveTemp(Preset));
		}
		else
		{
			UE_LOG(LogHuoguo, Warning, TEXT("[Planet] 고정 행성 프리셋을 건너뜀: %s"), *Error);
		}
	}
	return Result;
}

void HGPlanet::MixFixedPlanets(TArray<FHGPlanetSeed>& Planets, const TArray<FHGPlanetPreset>& Fixed, int32 UniverseSeed, int32 Cycle, float Chance)
{
	if (Fixed.IsEmpty() || Chance <= 0.f)
	{
		return;
	}

	// 고정 행성을 구별하는 이름: 파일 이름 (파일에서 읽지 않은 프리셋이면 행성 이름).
	auto KeyOf = [](const FHGPlanetPreset& Preset) { return Preset.FileName.IsEmpty() ? Preset.Name : Preset.FileName; };
	TSet<FString> Used;
	for (const FHGPlanetSeed& Planet : Planets)
	{
		if (!Planet.PresetName.IsEmpty())
		{
			Used.Add(Planet.PresetName);
		}
	}
	auto CountOf = [&Planets](EHGSiteType Type)
	{
		int32 Count = 0;
		for (const FHGPlanetSeed& Planet : Planets)
		{
			Count += SiteTypeOf(Planet) == Type ? 1 : 0;
		}
		return Count;
	};

	// 자리마다 따로 수열을 만든다. 생성 행성 시드 수열과 섞이지 않게 고유한 값을 섞는다.
	const uint32 CycleHash = HashCombine(HashCombine(GetTypeHash(UniverseSeed), GetTypeHash(Cycle)), 0xf17ed00du);
	for (int32 i = 0; i < Planets.Num(); ++i)
	{
		FRandomStream Rng(static_cast<int32>(HashCombine(CycleHash, GetTypeHash(i))));
		if (Rng.FRand() >= Chance || !Planets[i].PresetName.IsEmpty())
		{
			continue;
		}

		// 매일 소형과 중형을 둘 다 고를 수 있어야 한다 (MakeApproachingPlanets의 기획 조건). 그 종류의 마지막 하나면 같은 종류로만 바꾼다.
		const EHGSiteType OldType = SiteTypeOf(Planets[i]);
		const bool bLastOfKind = (OldType == EHGSiteType::PlanetSmall || OldType == EHGSiteType::PlanetMedium) && CountOf(OldType) == 1;
		TArray<int32> Candidates;
		for (int32 f = 0; f < Fixed.Num(); ++f)
		{
			if (!Used.Contains(KeyOf(Fixed[f])) && (!bLastOfKind || Fixed[f].SiteType == OldType))
			{
				Candidates.Add(f);
			}
		}
		if (Candidates.IsEmpty())
		{
			continue;
		}

		const FHGPlanetPreset& Pick = Fixed[Candidates[Rng.RandRange(0, Candidates.Num() - 1)]];
		FHGPlanetSeed NewPlanet = Pick.ToPlanetSeed();
		NewPlanet.PresetName = KeyOf(Pick);
		// 곧 떠나는 순서(목록 순서)를 그대로 두려고 바꾼 자리의 남은 시간을 받는다.
		NewPlanet.HoursLeft = Planets[i].HoursLeft;
		Planets[i] = NewPlanet;
		Used.Add(NewPlanet.PresetName);
	}
}
