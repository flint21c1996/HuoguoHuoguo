#include "World/Planet/HGPlanetShape.h"

#include "World/Planet/HGPlanetTypes.h"

FHGPlanetShape HGPlanet::DefaultShape(const FHGPlanetSeed& Planet)
{
	FHGPlanetShape Shape;

	// 생물 군계별 지형 성격. 얼음은 매끈하게, 버섯숲은 둔덕이 더 솟게, 바위는 능선이 날카롭게.
	switch (Planet.Biome)
	{
	case EHGPlanetBiome::Ice:
		Shape.HighlandScale = 0.6f;
		Shape.LowlandScale = 0.6f;
		Shape.RidgeWeight = 0.4f;
		Shape.BumpHeight = 45.f;
		break;
	case EHGPlanetBiome::Fungal:
		Shape.HighlandScale = 1.4f;
		Shape.LowlandScale = 0.7f;
		break;
	default:
		Shape.RidgeWeight = 0.7f;
		Shape.BumpHeight = 80.f;
		break;
	}

	// 크레이터 개수: 작은 행성은 적게, 중형은 많이.
	if (Planet.SizeClass == EHGPlanetSize::Small)
	{
		Shape.CraterCountMin = 3;
		Shape.CraterCountMax = 6;
	}
	else
	{
		Shape.CraterCountMin = 6;
		Shape.CraterCountMax = 12;
	}
	return Shape;
}
