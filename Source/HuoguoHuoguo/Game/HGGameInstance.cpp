#include "Game/HGGameInstance.h"

#include "Engine/DataTable.h"
#include "Game/HGIngredientTypes.h"
#include "HuoguoHuoguo.h"

namespace
{
	// 한 주기에 접근하는 행성 수.
	constexpr int32 ApproachingPlanetCount = 4;
}

UHGGameInstance::UHGGameInstance()
{
	// 소프트 참조라서 에셋이 아직 없어도 컴파일과 실행은 된다. 실제로 쓸 때 불러온다.
	IngredientTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/HG/Data/DT_Ingredients.DT_Ingredients")));
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
	ForgetDepartedPlanets();
}

void UHGGameInstance::ForgetDepartedPlanets()
{
	TSet<int32> StillHere;
	for (const FHGPlanetSeed& Planet : ApproachingPlanets)
	{
		StillHere.Add(Planet.Seed);
	}
	for (auto It = HarvestedByPlanet.CreateIterator(); It; ++It)
	{
		if (!StillHere.Contains(It.Key()))
		{
			UE_LOG(LogHuoguo, Log, TEXT("행성(시드 %d)이 떠나서 캔 재료 기록 %d개를 지움"), It.Key(), It.Value().Num());
			It.RemoveCurrent();
		}
	}
}

const UDataTable* UHGGameInstance::GetIngredientTable()
{
	if (!LoadedIngredientTable && !IngredientTable.IsNull())
	{
		LoadedIngredientTable = IngredientTable.LoadSynchronous();
	}
	if (!LoadedIngredientTable && !bIngredientTableMissingLogged)
	{
		bIngredientTableMissingLogged = true;
		UE_LOG(LogHuoguo, Error, TEXT("재료 DataTable(%s)을 불러오지 못했다. Tools/Invoke-HGMapGen.ps1로 Data/Ingredients.csv를 읽어 들일 것"),
			*IngredientTable.ToString());
	}
	return LoadedIngredientTable;
}

const FHGIngredientRow* UHGGameInstance::FindIngredient(FName IngredientId)
{
	const UDataTable* Table = GetIngredientTable();
	return Table ? Table->FindRow<FHGIngredientRow>(IngredientId, TEXT("FindIngredient"), false) : nullptr;
}

FString UHGGameInstance::GetIngredientName(FName IngredientId)
{
	const FHGIngredientRow* Row = FindIngredient(IngredientId);
	return Row ? Row->DisplayName.ToString() : IngredientId.ToString();
}

void UHGGameInstance::AddToBag(FName IngredientId, int32 Count)
{
	Bag.FindOrAdd(IngredientId) += Count;
}

int32 UHGGameInstance::GetBagCount(FName IngredientId) const
{
	const int32* Count = Bag.Find(IngredientId);
	return Count ? *Count : 0;
}

FString UHGGameInstance::DescribeBag()
{
	// 표의 행 순서대로 늘어놓는다. 표에 없는 ID(표에서 지운 재료)는 뒤에 붙인다.
	TArray<FName> Order;
	if (const UDataTable* Table = GetIngredientTable())
	{
		Order = Table->GetRowNames();
	}
	for (const TPair<FName, int32>& Pair : Bag)
	{
		Order.AddUnique(Pair.Key);
	}

	TArray<FString> Parts;
	for (const FName& Id : Order)
	{
		const int32 Count = GetBagCount(Id);
		if (Count > 0)
		{
			Parts.Add(FString::Printf(TEXT("%s %d"), *GetIngredientName(Id), Count));
		}
	}
	return Parts.IsEmpty() ? FString(TEXT("비어 있음")) : FString::Join(Parts, TEXT(", "));
}

bool UHGGameInstance::IsMarkerHarvested(int32 PlanetSeed, int32 MarkerId) const
{
	const TSet<int32>* Set = HarvestedByPlanet.Find(PlanetSeed);
	return Set && Set->Contains(MarkerId);
}

void UHGGameInstance::MarkHarvested(int32 PlanetSeed, int32 MarkerId)
{
	HarvestedByPlanet.FindOrAdd(PlanetSeed).Add(MarkerId);
}

int32 UHGGameInstance::GetHarvestedCount(int32 PlanetSeed) const
{
	const TSet<int32>* Set = HarvestedByPlanet.Find(PlanetSeed);
	return Set ? Set->Num() : 0;
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
