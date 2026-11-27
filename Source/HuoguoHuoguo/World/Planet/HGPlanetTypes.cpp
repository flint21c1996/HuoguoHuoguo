#include "World/Planet/HGPlanetTypes.h"

#include "Math/RandomStream.h"

namespace
{
	// 이름 조각. 앞 음절 + 뒤 음절 + 번호로 "크락-7", "마라젠-12" 같은 이름을 만든다.
	const TCHAR* const NameHeads[] = { TEXT("크락"), TEXT("마라"), TEXT("얀"), TEXT("쑤안"), TEXT("부르"), TEXT("탕"), TEXT("후아"), TEXT("젠"), TEXT("키오"), TEXT("파오") };
	const TCHAR* const NameTails[] = { TEXT(""), TEXT("젠"), TEXT("로"), TEXT("탄"), TEXT("쿠"), TEXT("린") };
}

FString FHGPlanetSeed::Describe() const
{
	if (bDerelict)
	{
		return FString::Printf(TEXT("%s | 폐우주선 (선체에 %s 재료) | 무중력 + 안쪽 인공 중력 | %d시간 후 이탈"),
			*Name, HGPlanet::BiomeName(Biome), HoursLeft);
	}
	if (bAsteroidField)
	{
		return FString::Printf(TEXT("%s | 소행성 지대 (%s) | 본 소행성 %.0fm | 중력 %.1f | %d시간 후 이탈"),
			*Name, HGPlanet::BiomeName(Biome), Radius / 100.f, GravityScale, HoursLeft);
	}
	return FString::Printf(TEXT("%s | %s %s | 반지름 %.0fm | 중력 %.1f | %d시간 후 이탈"),
		*Name, SizeClass == EHGPlanetSize::Small ? TEXT("소형") : TEXT("중형"), HGPlanet::BiomeName(Biome),
		Radius / 100.f, GravityScale, HoursLeft);
}

namespace HGPlanet
{
	FHGPlanetSeed MakePlanet(int32 Seed)
	{
		// FRandomStream은 같은 시드에서 언제나 같은 수열을 낸다. 값을 뽑는 순서를 바꾸면 기존 행성 모양이 바뀌므로
		// 새 값을 추가할 때는 맨 뒤에서 뽑는다.
		FRandomStream Rng(Seed);
		FHGPlanetSeed P;
		P.Seed = Seed;
		P.Name = FString::Printf(TEXT("%s%s-%d"),
			NameHeads[Rng.RandRange(0, UE_ARRAY_COUNT(NameHeads) - 1)],
			NameTails[Rng.RandRange(0, UE_ARRAY_COUNT(NameTails) - 1)],
			Rng.RandRange(2, 99));
		P.Biome = static_cast<EHGPlanetBiome>(Rng.RandRange(0, 2));
		P.SizeClass = Rng.FRand() < 0.6f ? EHGPlanetSize::Small : EHGPlanetSize::Medium;

		if (P.SizeClass == EHGPlanetSize::Small)
		{
			P.Radius = Rng.FRandRange(4000.f, 12000.f);
			P.TerrainAmplitude = Rng.FRandRange(0.04f, 0.09f);
			P.TerrainFrequency = Rng.FRandRange(1.8f, 3.2f);
			// 작을수록 중력이 약하다. 40m면 약 0.35, 120m면 약 0.7.
			P.GravityScale = FMath::GetMappedRangeValueClamped(FVector2f(4000.f, 12000.f), FVector2f(0.35f, 0.7f), P.Radius);
		}
		else
		{
			P.Radius = Rng.FRandRange(30000.f, 60000.f);
			// 큰 행성은 같은 비율이면 언덕이 너무 높아지므로 비율을 낮게 잡고, 굴곡은 촘촘하게 한다.
			P.TerrainAmplitude = Rng.FRandRange(0.012f, 0.025f);
			P.TerrainFrequency = Rng.FRandRange(4.f, 7.f);
			P.GravityScale = Rng.FRandRange(0.8f, 1.f);
		}
		P.Richness = Rng.FRandRange(0.6f, 1.4f);
		P.HoursLeft = Rng.RandRange(2, 12);
		// 열에 셋 정도는 소행성 지대다. 맨 뒤에서 뽑으므로 위 값들(이름, 군계, 크기 등)은 예전과 같다.
		if (Rng.FRand() < 0.3f)
		{
			MakeAsteroidField(P);
		}
		// 남은 것 중 다섯에 하나 정도는 폐우주선. 역시 맨 뒤에서 뽑으므로 위 값들은 예전과 같다.
		else if (Rng.FRand() < 0.2f)
		{
			MakeDerelict(P);
		}
		return P;
	}

	void MakeDerelict(FHGPlanetSeed& P)
	{
		P.bDerelict = true;
		P.bAsteroidField = false;
		P.SizeClass = EHGPlanetSize::Small;
		// 반지름은 쓰지 않지만 맵의 다른 코드(재료 배치 등)가 기대하는 최소값을 둔다. 인공 중력은 지구의 0.8.
		P.Radius = 1000.f;
		P.GravityScale = 0.8f;
	}

	void MakeAsteroidField(FHGPlanetSeed& P)
	{
		// 시드에서 따로 수열을 만들어 MakePlanet의 수열과 섞이지 않게 한다 (콘솔 변수로 강제로 바꿀 때도 같은 값이 나온다).
		FRandomStream Rng(static_cast<int32>(HashCombine(GetTypeHash(P.Seed), 0xa57e401du)));
		P.bAsteroidField = true;
		P.SizeClass = EHGPlanetSize::Small;
		P.Radius = Rng.FRandRange(1000.f, 1500.f);
		P.TerrainAmplitude = 0.12f;
		P.TerrainFrequency = 2.2f;
		// 소행성은 작아서 중력이 약하다. 너무 약하면 걷다가 떠 버리므로 0.35 ~ 0.5.
		P.GravityScale = Rng.FRandRange(0.35f, 0.5f);
	}

	TArray<FHGPlanetSeed> MakeApproachingPlanets(int32 UniverseSeed, int32 Cycle, int32 Count)
	{
		TArray<FHGPlanetSeed> Result;
		const uint32 CycleHash = HashCombine(GetTypeHash(UniverseSeed), GetTypeHash(Cycle));
		for (int32 i = 0; i < Count; ++i)
		{
			int32 Seed = static_cast<int32>(HashCombine(CycleHash, GetTypeHash(i)) & 0x7fffffff);
			FHGPlanetSeed Planet = MakePlanet(Seed);

			// 첫 번째는 작은 행성, 두 번째는 중간 행성이 나올 때까지 시드를 다시 뽑는다.
			// 매 주기마다 작은 행성과 중간 행성을 둘 다 고를 수 있어야 한다는 기획 조건을 보장하기 위해서이다.
			// (아래에서 이탈 시간순으로 정렬하므로 목록에서의 순서는 바뀔 수 있다.)
			const EHGPlanetSize Wanted = (i == 0) ? EHGPlanetSize::Small : (i == 1 ? EHGPlanetSize::Medium : Planet.SizeClass);
			while (Planet.SizeClass != Wanted)
			{
				Seed = static_cast<int32>(HashCombine(static_cast<uint32>(Seed), 0x9e3779b9u) & 0x7fffffff);
				Planet = MakePlanet(Seed);
			}
			Result.Add(Planet);
		}

		// 곧 떠나는 행성이 위로 오게 정렬한다.
		Result.Sort([](const FHGPlanetSeed& A, const FHGPlanetSeed& B) { return A.HoursLeft < B.HoursLeft; });
		return Result;
	}

	EHGSiteType SiteTypeOf(const FHGPlanetSeed& Planet)
	{
		// 폐우주선과 소행성 지대는 크기 등급과 상관없이 그 환경이다. 둘 다 켜지는 일은 없지만(MakeDerelict가 소행성 지대를 끈다) 폐우주선을 먼저 본다.
		if (Planet.bDerelict)
		{
			return EHGSiteType::Derelict;
		}
		if (Planet.bAsteroidField)
		{
			return EHGSiteType::AsteroidField;
		}
		return Planet.SizeClass == EHGPlanetSize::Small ? EHGSiteType::PlanetSmall : EHGSiteType::PlanetMedium;
	}

	const TCHAR* BiomeName(EHGPlanetBiome Biome)
	{
		switch (Biome)
		{
		case EHGPlanetBiome::Ice:    return TEXT("얼음");
		case EHGPlanetBiome::Fungal: return TEXT("버섯숲");
		default:                     return TEXT("바위");
		}
	}

	FLinearColor GroundColor(EHGPlanetBiome Biome)
	{
		switch (Biome)
		{
		case EHGPlanetBiome::Ice:    return FLinearColor(0.62f, 0.78f, 0.86f);
		case EHGPlanetBiome::Fungal: return FLinearColor(0.30f, 0.22f, 0.38f);
		default:                     return FLinearColor(0.42f, 0.30f, 0.24f);
		}
	}

	FLinearColor RockColor(EHGPlanetBiome Biome)
	{
		switch (Biome)
		{
		case EHGPlanetBiome::Ice:    return FLinearColor(0.85f, 0.93f, 0.98f);
		case EHGPlanetBiome::Fungal: return FLinearColor(0.18f, 0.14f, 0.22f);
		default:                     return FLinearColor(0.24f, 0.21f, 0.19f);
		}
	}
}
