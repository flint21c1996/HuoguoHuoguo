#include "Game/HGGameInstance.h"

namespace
{
	// 한 주기에 접근하는 행성 수.
	constexpr int32 ApproachingPlanetCount = 4;
}

const TArray<FHGPlanetSeed>& UHGGameInstance::GetApproachingPlanets()
{
	if (!bPlanetsBuilt)
	{
		ApproachingPlanets = HGPlanet::MakeApproachingPlanets(UniverseSeed, OrbitCycle, ApproachingPlanetCount);
		SelectedPlanetIndex = 0;
		bPlanetsBuilt = true;
	}
	return ApproachingPlanets;
}

void UHGGameInstance::AdvanceOrbitCycle()
{
	++OrbitCycle;
	bPlanetsBuilt = false;
	GetApproachingPlanets();
}

bool UHGGameInstance::SelectPlanet(int32 Index)
{
	if (!GetApproachingPlanets().IsValidIndex(Index))
	{
		return false;
	}
	SelectedPlanetIndex = Index;
	return true;
}

const FHGPlanetSeed* UHGGameInstance::GetSelectedPlanet()
{
	const TArray<FHGPlanetSeed>& Planets = GetApproachingPlanets();
	return Planets.IsValidIndex(SelectedPlanetIndex) ? &Planets[SelectedPlanetIndex] : nullptr;
}

void UHGGameInstance::CommitDiveToSelectedPlanet()
{
	if (const FHGPlanetSeed* Planet = GetSelectedPlanet())
	{
		DivePlanet = *Planet;
	}
	else
	{
		DivePlanet.Reset();
	}
}
