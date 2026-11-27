#include "World/Planet/HGPlanetPreset.h"

#include "JsonObjectConverter.h"
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
	return true;
}
