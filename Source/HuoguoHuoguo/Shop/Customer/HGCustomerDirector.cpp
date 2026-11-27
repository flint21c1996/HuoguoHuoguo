#include "Shop/Customer/HGCustomerDirector.h"

#include "Algo/Reverse.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/HGGameInstance.h"
#include "Game/HGIngredientTypes.h"
#include "GameFramework/Pawn.h"
#include "HuoguoHuoguo.h"
#include "Shop/Customer/HGCustomer.h"
#include "Shop/Furniture/HGFurniture.h"
#include "Shop/Furniture/HGFurniturePlacer.h"
#include "Shop/Core/HGShopCombos.h"
#include "GameFramework/PlayerStart.h"

namespace
{
	// 손님 몸 색 (종족 자리 표시). 종족 데이터가 생기면 종족 표의 색으로 바꾼다.
	const FLinearColor CustomerColors[] =
	{
		FLinearColor(0.30f, 0.55f, 0.95f),
		FLinearColor(0.85f, 0.35f, 0.75f),
		FLinearColor(0.40f, 0.80f, 0.45f),
		FLinearColor(0.95f, 0.75f, 0.25f),
		FLinearColor(0.55f, 0.45f, 0.90f),
	};

	// 김 색: 콤보 수가 많을수록 진한 금빛.
	FLinearColor SteamColor(int32 ComboCount)
	{
		if (ComboCount <= 0)
		{
			return FLinearColor(0.92f, 0.92f, 0.92f);
		}
		return ComboCount == 1 ? FLinearColor(1.00f, 0.85f, 0.45f) : FLinearColor(1.00f, 0.55f, 0.18f);
	}

	const FLinearColor BasicFoodColor(1.0f, 0.58f, 0.19f);

	void GetStockedIngredients(const UHGGameInstance& GI, TArray<FName>& OutIngredients)
	{
		for (const TPair<FName, int32>& Entry : GI.GetBag())
		{
			if (!Entry.Key.IsNone() && Entry.Value > 0)
			{
				OutIngredients.Add(Entry.Key);
			}
		}
		OutIngredients.Sort(FNameLexicalLess());
	}

	FLinearColor GetFoodColor(UHGGameInstance& GI, FName Ingredient)
	{
		const FHGIngredientRow* Row = Ingredient.IsNone() ? nullptr : GI.FindIngredient(Ingredient);
		return Row ? Row->Color : BasicFoodColor;
	}

	const FIntPoint Directions[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
}

UHGCustomerDirector::UHGCustomerDirector()
{
	PrimaryComponentTick.bCanEverTick = true;
	Random.Initialize(20261124);
}

const FHGShopGrid& UHGCustomerDirector::GetGrid() const
{
	static const FHGShopGrid DefaultGrid;
	return Placer.IsValid() ? Placer->Grid : DefaultGrid;
}

bool UHGCustomerDirector::IsOpen() const
{
	const UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	return GI && GI->GetDayPhase() != EHGDayPhase::Closing;
}

void UHGCustomerDirector::Message(const FString& Text) const
{
	if (OnMessage)
	{
		OnMessage(Text);
	}
	else
	{
		UE_LOG(LogHuoguo, Log, TEXT("[Customer] %s"), *Text);
	}
}

void UHGCustomerDirector::Start(UHGFurniturePlacer* InPlacer)
{
	Placer = InPlacer;
	ComputeStaticBlocked();
	RefreshSkewerDisplay();
	bStarted = true;
}

// ---------------------------------------------------------------------------
// 칸과 길 찾기
// ---------------------------------------------------------------------------

void UHGCustomerDirector::ComputeStaticBlocked()
{
	// 바닥 칸마다 칸 가운데에 상자(칸의 70%, 바닥 위 20 ~ 140)를 두고 겹치는 고정물/트리거를 찾는다.
	// 가구, 손님, 사장님, 시작점 표식은 빼고 본다 (가구는 매번 칸으로 따로 센다).
	const FHGShopGrid& Grid = GetGrid();
	StaticBlocked.Reset();

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(Grid.TileSize * 0.35f, Grid.TileSize * 0.35f, 60.f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HGCustomerStatic), false);

	TArray<FString> BlockedBy;
	for (int32 X = Grid.FloorMin.X; X < Grid.FloorMax.X; ++X)
	{
		for (int32 Y = Grid.FloorMin.Y; Y < Grid.FloorMax.Y; ++Y)
		{
			const FIntPoint Tile(X, Y);
			TArray<FOverlapResult> Overlaps;
			GetWorld()->OverlapMultiByObjectType(Overlaps, Grid.TileCenter(Tile) + FVector(0.f, 0.f, 80.f), FQuat::Identity, Objects, Box, Params);
			for (const FOverlapResult& Overlap : Overlaps)
			{
				const AActor* Other = Overlap.GetActor();
				if (!Other || Other->IsA<AHGFurniture>() || Other->IsA<AHGCustomer>() || Other->IsA<APawn>() || Other->IsA<APlayerStart>())
				{
					continue;
				}
				StaticBlocked.Add(Tile);
				BlockedBy.AddUnique(Other->GetActorNameOrLabel());
				break;
			}
		}
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Customer] 손님이 못 지나가는 고정 칸 %d개 (%s)"), StaticBlocked.Num(), *FString::Join(BlockedBy, TEXT(", ")));
}

void UHGCustomerDirector::GatherOccupied(const TArray<FHGGridItem>& Items, TSet<FIntPoint>& OutOccupied)
{
	OutOccupied.Reset();
	for (const FHGGridItem& Item : Items)
	{
		for (int32 X = Item.Rect.Min.X; X < Item.Rect.Max.X; ++X)
		{
			for (int32 Y = Item.Rect.Min.Y; Y < Item.Rect.Max.Y; ++Y)
			{
				OutOccupied.Add(FIntPoint(X, Y));
			}
		}
	}
}

bool UHGCustomerDirector::FindPath(const FIntPoint& Start, const FIntPoint& Goal, const TSet<FIntPoint>& Occupied, TArray<FVector>& OutPoints) const
{
	const FHGShopGrid& Grid = GetGrid();
	auto IsWalkable = [&](const FIntPoint& Tile)
	{
		if (Tile == Start || Tile == Goal)
		{
			return Grid.IsOnFloor(Tile);
		}
		return Grid.IsOnFloor(Tile) && !StaticBlocked.Contains(Tile) && !Occupied.Contains(Tile);
	};

	// 너비 우선 탐색: 칸마다 걸음 수가 같으므로 처음 닿은 길이 가장 짧다. 어디서 왔는지(Came)를 따라 거꾸로 길을 만든다.
	TMap<FIntPoint, FIntPoint> Came;
	TArray<FIntPoint> Queue;
	Queue.Add(Start);
	Came.Add(Start, Start);
	bool bFound = Start == Goal;
	for (int32 Head = 0; Head < Queue.Num() && !bFound; ++Head)
	{
		const FIntPoint Current = Queue[Head];
		for (const FIntPoint& Dir : Directions)
		{
			const FIntPoint Next = Current + Dir;
			if (Came.Contains(Next) || !IsWalkable(Next))
			{
				continue;
			}
			Came.Add(Next, Current);
			if (Next == Goal)
			{
				bFound = true;
				break;
			}
			Queue.Add(Next);
		}
	}
	if (!bFound)
	{
		return false;
	}

	TArray<FIntPoint> Tiles;
	for (FIntPoint Tile = Goal; Tile != Start; Tile = Came[Tile])
	{
		Tiles.Add(Tile);
	}
	Algo::Reverse(Tiles);
	OutPoints.Reset();
	for (const FIntPoint& Tile : Tiles)
	{
		OutPoints.Add(Grid.TileCenter(Tile));
	}
	return true;
}

// ---------------------------------------------------------------------------
// 손님 흐름
// ---------------------------------------------------------------------------

FString UHGCustomerDirector::SpawnCustomerNow()
{
	FString Result;
	TrySpawnCustomer(Result);
	return Result;
}

bool UHGCustomerDirector::TrySpawnCustomer(FString& OutResult)
{
	if (!bStarted)
	{
		OutResult = TEXT("손님 흐름이 아직 시작되지 않았다");
		return false;
	}
	const FHGShopGrid& Grid = GetGrid();
	TArray<FHGGridItem> Items;
	HGShopGrid::GatherItems(GetWorld(), Grid, Items);

	// 진열대 앞의 칸과 식탁은 손님마다 따로 잡아 둔다.
	TSet<FIntPoint> ReservedPickupTiles;
	TSet<const AHGFurniture*> ReservedTables;
	TSet<int32> ActiveSkewerCounts;
	for (const FVisit& Visit : Visits)
	{
		if (Visit.State == EVisitState::ToShelf || Visit.State == EVisitState::Picking)
		{
			ReservedPickupTiles.Add(Visit.PickupTile);
		}
		if (Visit.State != EVisitState::Leaving && Visit.Table.IsValid())
		{
			ReservedTables.Add(Visit.Table.Get());
			ActiveSkewerCounts.Add(Visit.SkewerCount);
		}
	}

	// 빈자리: 식탁을 바라보는 의자 (의자 바로 앞 칸이 식탁). (의자, 식탁) 짝으로 모은다.
	TArray<TPair<AHGFurniture*, AHGFurniture*>> Seats;
	for (const FHGGridItem& Chair : Items)
	{
		AHGFurniture* ChairActor = Chair.Actor.Get();
		if (!ChairActor || !ChairActor->IsChair())
		{
			continue;
		}
		const FIntPoint Front = HGShopGrid::GetFrontTile(Grid, ChairActor);
		for (const FHGGridItem& Table : Items)
		{
			AHGFurniture* TableActor = Table.IsTable() ? Table.Actor.Get() : nullptr;
			if (TableActor && !ReservedTables.Contains(TableActor) && Table.Rect.Contains(Front))
			{
				Seats.Emplace(ChairActor, TableActor);
				break;
			}
		}
	}
	if (Seats.IsEmpty())
	{
		OutResult = TEXT("빈자리가 없어서 손님이 들어오지 않았다 (식탁을 바라보는 의자가 자리다)");
		return false;
	}

	// 빈자리를 섞어서 길이 있는 첫 자리로 보낸다.
	for (int32 i = Seats.Num() - 1; i > 0; --i)
	{
		Seats.Swap(i, Random.RandRange(0, i));
	}
	TSet<FIntPoint> Occupied;
	GatherOccupied(Items, Occupied);
	TArray<FVector> ShelfPath;
	const TPair<AHGFurniture*, AHGFurniture*>* ChosenSeat = nullptr;
	FIntPoint PickupTile = SkewerPickupTile;
	for (const int32 Offset : { 0, 1, -1 })
	{
		PickupTile = SkewerPickupTile + FIntPoint(0, Offset);
		if (ReservedPickupTiles.Contains(PickupTile) || StaticBlocked.Contains(PickupTile) || Occupied.Contains(PickupTile)
			|| !FindPath(Grid.EntranceTile, PickupTile, Occupied, ShelfPath))
		{
			continue;
		}
		ChosenSeat = Seats.FindByPredicate([&](const TPair<AHGFurniture*, AHGFurniture*>& Seat)
		{
			TArray<FVector> SeatPath;
			return FindPath(PickupTile, Grid.WorldToTile(Seat.Key->GetActorLocation()), Occupied, SeatPath);
		});
		if (ChosenSeat)
		{
			break;
		}
	}
	if (!ChosenSeat)
	{
		OutResult = TEXT("진열대 앞에 빈 칸이 없거나 자리에 갈 수 없다");
		return false;
	}
	if (!ShelfPath.IsEmpty())
	{
		// 접시가 진열대와 겹치지 않는 거리에서 꼬치를 고른다.
		const FVector ToDisplay = (SkewerDisplayLocation - ShelfPath.Last()).GetSafeNormal2D();
		ShelfPath.Last() += ToDisplay * (Grid.TileSize * 0.32f);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHGCustomer* Customer = GetWorld()->SpawnActor<AHGCustomer>(CustomerClass ? CustomerClass.Get() : AHGCustomer::StaticClass(), Grid.TileCenter(Grid.EntranceTile), FRotator::ZeroRotator, Params);
	if (!Customer)
	{
		OutResult = TEXT("손님을 만들지 못했다");
		return false;
	}
	Customer->SetBodyColor(CustomerColors[Random.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(CustomerColors)) - 1)]);
	Customer->ShowPlate();
	Customer->WalkPath(ShelfPath);

	FVisit& Visit = Visits.AddDefaulted_GetRef();
	Visit.Customer = Customer;
	Visit.Chair = ChosenSeat->Key;
	Visit.Table = ChosenSeat->Value;
	Visit.PickupTile = PickupTile;

	// 동시에 보이는 접시는 가능한 한 다른 높이로 고른다. 모두 쓰는 중이면 반복한다.
	const int32 MinSkewers = FMath::Max(1, SkewersPerCustomerRange.X);
	const int32 MaxSkewers = FMath::Max(MinSkewers, SkewersPerCustomerRange.Y);
	const int32 InitialCount = Random.RandRange(MinSkewers, MaxSkewers);
	Visit.SkewerCount = InitialCount;
	while (ActiveSkewerCounts.Contains(Visit.SkewerCount))
	{
		Visit.SkewerCount = Visit.SkewerCount == MaxSkewers ? MinSkewers : Visit.SkewerCount + 1;
		if (Visit.SkewerCount == InitialCount)
		{
			break;
		}
	}
	++SpawnedCount;

	const FIntPoint ChairTile = Grid.WorldToTile(Visit.Chair->GetActorLocation());
	OutResult = FString::Printf(TEXT("손님 %d번이 들어왔다 -> 진열대 (%d, %d) -> 의자 칸 (%d, %d), 꼬치 목표 %d개"),
		SpawnedCount, PickupTile.X, PickupTile.Y, ChairTile.X, ChairTile.Y, Visit.SkewerCount);
	UE_LOG(LogHuoguo, Log, TEXT("[Customer] %s"), *OutResult);
	return true;
}

void UHGCustomerDirector::SendHome(FVisit& Visit)
{
	ReturnUneatenSkewers(Visit);
	Visit.State = EVisitState::Leaving;
	AHGCustomer* Customer = Visit.Customer.Get();
	if (!Customer)
	{
		return;
	}
	Customer->ClearMeal();
	const FHGShopGrid& Grid = GetGrid();
	TArray<FHGGridItem> Items;
	HGShopGrid::GatherItems(GetWorld(), Grid, Items);
	TSet<FIntPoint> Occupied;
	GatherOccupied(Items, Occupied);

	TArray<FVector> Points;
	if (!FindPath(Grid.WorldToTile(Customer->GetActorLocation()), Grid.EntranceTile, Occupied, Points))
	{
		// 길이 막혔으면(나갈 길을 가구로 막았으면) 입구로 곧게 걸어간다. 손님은 충돌이 없어서 갇히지 않는다.
		Points = { Grid.TileCenter(Grid.EntranceTile) };
	}
	Customer->WalkPath(Points);
}

void UHGCustomerDirector::PickSkewer(FVisit& Visit)
{
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	FName Ingredient;
	if (GI)
	{
		TArray<FName> Available;
		GetStockedIngredients(*GI, Available);
		if (!Available.IsEmpty())
		{
			const FName Chosen = Available[Random.RandRange(0, Available.Num() - 1)];
			if (GI->RemoveFromBag(Chosen, 1))
			{
				Ingredient = Chosen;
			}
		}
	}
	const TArray<TWeakObjectPtr<AHGSkewer>>* DisplayStack = DisplaySkewers.Find(Ingredient);
	const AHGSkewer* TopSkewer = DisplayStack && !DisplayStack->IsEmpty() ? DisplayStack->Last().Get() : nullptr;
	const FVector PickupLocation = TopSkewer ? TopSkewer->GetActorLocation() : SkewerDisplayLocation;
	Visit.Skewers.Add(Ingredient);
	Visit.Customer->AddSkewer(GI ? GetFoodColor(*GI, Ingredient) : BasicFoodColor,
		PickupLocation, FMath::Max(SkewerPickInterval, 0.15f) * 0.6f);
	UE_LOG(LogHuoguo, Log, TEXT("[Meal] %s 진열대에서 %s 선택: 접시 %d개"),
		*GetNameSafe(Visit.Customer.Get()), Ingredient.IsNone() ? TEXT("기본 꼬치") : *Ingredient.ToString(), Visit.Skewers.Num());
	RefreshSkewerDisplay();
}

void UHGCustomerDirector::ReturnUneatenSkewers(FVisit& Visit)
{
	if (UHGGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UHGGameInstance>() : nullptr)
	{
		// 쌓은 순서의 역순으로 먹으므로 배열 앞쪽에 남은 것만 돌려준다.
		for (int32 Index = 0; Index < Visit.Skewers.Num() - Visit.EatenCount; ++Index)
		{
			if (!Visit.Skewers[Index].IsNone())
			{
				GI->AddToBag(Visit.Skewers[Index], 1);
			}
		}
	}
	Visit.Skewers.Reset();
}

void UHGCustomerDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FVisit& Visit : Visits)
	{
		ReturnUneatenSkewers(Visit);
	}
	for (const TPair<FName, TArray<TWeakObjectPtr<AHGSkewer>>>& Entry : DisplaySkewers)
	{
		for (const TWeakObjectPtr<AHGSkewer>& Part : Entry.Value)
		{
			if (AHGSkewer* Skewer = Part.Get())
			{
				Skewer->Destroy();
			}
		}
	}
	DisplaySkewers.Reset();
	Super::EndPlay(EndPlayReason);
}

void UHGCustomerDirector::RefreshSkewerDisplay()
{
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		return;
	}
	TArray<FName> Stocked;
	GetStockedIngredients(*GI, Stocked);
	Stocked.Insert(NAME_None, 0);
	for (auto It = DisplaySkewers.CreateIterator(); It; ++It)
	{
		if (!Stocked.Contains(It.Key()))
		{
			for (const TWeakObjectPtr<AHGSkewer>& Part : It.Value())
			{
				if (AHGSkewer* Skewer = Part.Get())
				{
					Skewer->Destroy();
				}
			}
			It.RemoveCurrent();
		}
	}

	const float Spacing = FMath::Min(19.f, 108.f / FMath::Max(1, Stocked.Num() - 1));
	for (int32 Index = 0; Index < Stocked.Num(); ++Index)
	{
		const FName Ingredient = Stocked[Index];
		const FVector Base = SkewerDisplayLocation + FVector(0.f, (Index - (Stocked.Num() - 1) * 0.5f) * Spacing, 0.f);
		const int32 Height = Ingredient.IsNone() ? 5 : FMath::Min(GI->GetBagCount(Ingredient), 5);
		TArray<TWeakObjectPtr<AHGSkewer>>& Stack = DisplaySkewers.FindOrAdd(Ingredient);
		while (Stack.Num() > Height)
		{
			if (AHGSkewer* Skewer = Stack.Pop().Get())
			{
				Skewer->Destroy();
			}
		}
		Stack.SetNum(Height);
		for (int32 Layer = 0; Layer < Height; ++Layer)
		{
			const FVector Location = Base + FVector(0.f, 0.f, Layer * AHGSkewer::StackSpacing);
			if (AHGSkewer* Existing = Stack[Layer].Get())
			{
				Existing->SetActorLocation(Location);
			}
			else if (AHGSkewer* Skewer = GetWorld()->SpawnActor<AHGSkewer>(Location, FRotator::ZeroRotator))
			{
				Skewer->SetFoodColor(GetFoodColor(*GI, Ingredient));
				Stack[Layer] = Skewer;
			}
		}
	}
}

bool UHGCustomerDirector::ServeNearbyBroth(const FVector& BossLocation)
{
	FVisit* Nearest = nullptr;
	double BestDistanceSq = FMath::Square(BrothServeRange);
	for (FVisit& Visit : Visits)
	{
		if (Visit.State == EVisitState::WaitingForBroth && Visit.Customer.IsValid() && Visit.Table.IsValid() && !Visit.Table->IsGhost())
		{
			const double DistanceSq = FVector::DistSquared2D(BossLocation, Visit.Table->GetActorLocation());
			if (DistanceSq <= BestDistanceSq)
			{
				Nearest = &Visit;
				BestDistanceSq = DistanceSq;
			}
		}
	}
	if (!Nearest)
	{
		return false;
	}

	Nearest->Customer->SetWaitingForBroth(false);
	Nearest->State = EVisitState::Eating;
	UE_LOG(LogHuoguo, Log, TEXT("[Meal] 사장님이 %s 식탁에 육수를 냈다"), *GetNameSafe(Nearest->Customer.Get()));
	StartNextDip(*Nearest);
	RefreshSteam();
	return true;
}

void UHGCustomerDirector::StartNextDip(FVisit& Visit)
{
	Visit.Timer = FMath::Max(EatSeconds / FMath::Max(1, Visit.Skewers.Num()), 0.5f);
	const FVector BrothLocation = Visit.Table->GetActorLocation() + FVector(0.f, 0.f, 125.f);
	const FName Ingredient = Visit.Skewers[Visit.Skewers.Num() - 1 - Visit.EatenCount];
	Visit.Customer->StartDip(BrothLocation, Visit.Timer);
	UE_LOG(LogHuoguo, Log, TEXT("[Meal] %s %d/%d번째 꼬치를 육수에 담근다: %s"),
		*GetNameSafe(Visit.Customer.Get()), Visit.EatenCount + 1, Visit.Skewers.Num(),
		Ingredient.IsNone() ? TEXT("기본 꼬치") : *Ingredient.ToString());
}

void UHGCustomerDirector::Pay(FVisit& Visit)
{
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	AHGFurniture* Table = Visit.Table.Get();
	if (!GI || !Table)
	{
		return;
	}

	// 진열대에서 챙겨 끝까지 먹은 접시를 계산한다. 재료를 다시 빼지 않는다.
	int32 BasePrice = 0;
	TMap<FString, int32> Eaten;
	TArray<FString> EatenOrder;
	for (const FName IngredientId : Visit.Skewers)
	{
		const FHGIngredientRow* Row = IngredientId.IsNone() ? nullptr : GI->FindIngredient(IngredientId);
		BasePrice += Row ? Row->Price : BasicSkewerPrice;
		const FString Name = IngredientId.IsNone() ? TEXT("기본 꼬치") : GI->GetIngredientName(IngredientId);
		if (!Eaten.Contains(Name))
		{
			EatenOrder.Add(Name);
		}
		Eaten.FindOrAdd(Name) += 1;
	}

	// 콤보: 지금 식탁 둘레의 가구로 판정한다 (먹는 동안 배치를 바꿨으면 바뀐 배치로 낸다).
	const FHGShopGrid& Grid = GetGrid();
	TArray<FHGGridItem> Items;
	HGShopGrid::GatherItems(GetWorld(), Grid, Items);
	const UDataTable* ComboTable = GI->GetComboTable();
	const FHGComboResult Combos = HGCombo::EvaluateTable(ComboTable, HGShopGrid::GetFurnitureRect(Grid, Table), Items);

	// 가구 보너스: 식탁(냄비 등급)과 앉은 의자의 BonusPercent (Data/Furniture.csv). 콤보 보너스와 모두 더한다.
	const int32 TableBonus = Table->GetData().BonusPercent;
	const AHGFurniture* Chair = Visit.Chair.Get();
	const int32 ChairBonus = Chair ? Chair->GetData().BonusPercent : 0;
	const int32 TotalBonus = Combos.BonusPercent + TableBonus + ChairBonus;
	const int32 Total = FMath::RoundToInt(BasePrice * (100 + TotalBonus) / 100.f);
	GI->AddMoney(Total);
	GI->AddServedCustomer();
	++ServedCount;
	for (const FName& Id : Combos.Ids)
	{
		GI->DiscoverCombo(Id);
	}

	// 식탁 위로 금액이 떠오른다. 콤보가 있으면 김 색과 같은 금빛, 없으면 흰색.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AHGMoneyPopup* Popup = GetWorld()->SpawnActor<AHGMoneyPopup>(MoneyPopupClass ? MoneyPopupClass.Get() : AHGMoneyPopup::StaticClass(), Table->GetActorLocation() + FVector(0.f, 0.f, 200.f), FRotator::ZeroRotator, Params))
	{
		Popup->SetupText(FString::Printf(TEXT("+%d"), Total), Combos.Num() > 0 ? SteamColor(Combos.Num()).ToFColor(true) : FColor::White);
	}

	TArray<FString> EatenParts;
	for (const FString& Name : EatenOrder)
	{
		EatenParts.Add(FString::Printf(TEXT("%s %d"), *Name, Eaten[Name]));
	}
	const FString ComboText = Combos.Num() > 0 ? Combos.Describe(ComboTable) : FString(TEXT("없음"));
	Message(FString::Printf(TEXT("손님 계산 +%d (꼬치 %s = %d, %s +%d%%, %s +%d%%, 콤보 %s) | 가진 돈 %d"),
		Total, *FString::Join(EatenParts, TEXT(", ")), BasePrice,
		*Table->GetDisplayName().ToString(), TableBonus, Chair ? *Chair->GetDisplayName().ToString() : TEXT("의자"), ChairBonus,
		*ComboText, GI->GetMoney()));

	// 손님이 늘면 소문이 나서 열리는 가구 (Unlock = Customers).
	const TArray<FString> Opened = HGFurnitureUnlock::UnlockByCustomers(GI);
	if (!Opened.IsEmpty())
	{
		Message(FString::Printf(TEXT("손님 %d명! 소문을 듣고 새 가구가 들어왔다: %s"), GI->GetServedCustomers(), *FString::Join(Opened, TEXT(", "))));
	}
	if (OnMoneyChanged)
	{
		OnMoneyChanged();
	}
}

void UHGCustomerDirector::RefreshSteam()
{
	TSet<AHGFurniture*> EatingTables;
	for (const FVisit& Visit : Visits)
	{
		if (Visit.State == EVisitState::Eating && Visit.Table.IsValid())
		{
			EatingTables.Add(Visit.Table.Get());
		}
	}

	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	const UDataTable* ComboTable = GI ? GI->GetComboTable() : nullptr;
	TArray<FHGGridItem> Items;
	if (!EatingTables.IsEmpty())
	{
		HGShopGrid::GatherItems(GetWorld(), GetGrid(), Items);
	}
	for (TActorIterator<AHGFurniture> It(GetWorld()); It; ++It)
	{
		if (It->IsGhost() || !It->IsTable())
		{
			continue;
		}
		const bool bHasMeal = EatingTables.Contains(*It);
		It->SetBrothServed(bHasMeal);
		if (bHasMeal)
		{
			const FHGComboResult Combos = HGCombo::EvaluateTable(ComboTable, HGShopGrid::GetFurnitureRect(GetGrid(), *It), Items);
			It->SetSteam(true, SteamColor(Combos.Num()));
		}
		else
		{
			It->SetSteam(false);
		}
	}
}

void UHGCustomerDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bStarted)
	{
		return;
	}

	// 1. 새 손님. 문 닫을 시간에는 오지 않는다 (가게 안의 손님은 마저 먹고 나간다).
	if (IsOpen())
	{
		ArrivalTimer -= DeltaTime;
		if (ArrivalTimer <= 0.f)
		{
			FString Result;
			TrySpawnCustomer(Result);
			ArrivalTimer = ArrivalInterval * Random.FRandRange(0.6f, 1.4f);
		}
	}

	// 2. 손님마다 상태 진행. 지우면서 돌므로 뒤에서부터 본다.
	bool bSteamDirty = false;
	for (int32 i = Visits.Num() - 1; i >= 0; --i)
	{
		FVisit& Visit = Visits[i];
		AHGCustomer* Customer = Visit.Customer.Get();
		if (!Customer)
		{
			ReturnUneatenSkewers(Visit);
			Visits.RemoveAt(i);
			bSteamDirty = true;
			continue;
		}
		const bool bSeatGone = !Visit.Chair.IsValid() || !Visit.Table.IsValid() || Visit.Chair->IsGhost() || Visit.Table->IsGhost();
		if (Visit.State != EVisitState::Leaving && bSeatGone)
		{
			if (Visit.State == EVisitState::WaitingForBroth || Visit.State == EVisitState::Eating)
			{
				Customer->StandUp();
			}
			Message(TEXT("손님 자리가 없어져서 남은 꼬치를 돌려놓고 나갔다"));
			SendHome(Visit);
			bSteamDirty = true;
			continue;
		}

		switch (Visit.State)
		{
		case EVisitState::ToShelf:
			if (!Customer->IsWalking())
			{
				const FVector ToDisplay = SkewerDisplayLocation - Customer->GetActorLocation();
				Customer->SetActorRotation(FRotator(0.f, ToDisplay.Rotation().Yaw, 0.f));
				Visit.State = EVisitState::Picking;
				Visit.Timer = FMath::Max(SkewerPickInterval, 0.15f);
			}
			break;

		case EVisitState::Picking:
			Visit.Timer -= DeltaTime;
			if (Visit.Timer <= 0.f)
			{
				if (Visit.Skewers.Num() < Visit.SkewerCount)
				{
					PickSkewer(Visit);
					Visit.Timer = FMath::Max(SkewerPickInterval, 0.15f);
					break;
				}
				TArray<FHGGridItem> Items;
				HGShopGrid::GatherItems(GetWorld(), GetGrid(), Items);
				TSet<FIntPoint> Occupied;
				GatherOccupied(Items, Occupied);
				TArray<FVector> Path;
				if (FindPath(Visit.PickupTile, GetGrid().WorldToTile(Visit.Chair->GetActorLocation()), Occupied, Path))
				{
					Customer->WalkPath(Path);
					Visit.State = EVisitState::ToSeat;
					UE_LOG(LogHuoguo, Log, TEXT("[Meal] %s 꼬치 %d개를 들고 식탁으로 이동"), *GetNameSafe(Customer), Visit.Skewers.Num());
				}
				else
				{
					SendHome(Visit);
				}
			}
			break;

		case EVisitState::ToSeat:
			if (!Customer->IsWalking())
			{
				const FVector TableLocation = Visit.Table->GetActorLocation();
				const FVector ChairLocation = Visit.Chair->GetActorLocation();
				const FVector ToTable = TableLocation - ChairLocation;
				Customer->SitAt(ChairLocation, ToTable.Rotation().Yaw);
				const FVector PlateLocation = FMath::Lerp(ChairLocation, TableLocation, 0.61f) + FVector(0.f, 0.f, 85.f);
				Customer->PlacePlate(PlateLocation);
				Customer->SetWaitingForBroth(true);
				Visit.State = EVisitState::WaitingForBroth;
				UE_LOG(LogHuoguo, Log, TEXT("[Meal] %s 꼬치 %d개를 들고 착석, 육수 대기: 식탁 %s"),
					*GetNameSafe(Customer), Visit.Skewers.Num(), *TableLocation.ToCompactString());
			}
			break;

		case EVisitState::WaitingForBroth:
			break;

		case EVisitState::Eating:
			Visit.Timer -= DeltaTime;
			if (Visit.Timer <= 0.f)
			{
				++Visit.EatenCount;
				if (Visit.EatenCount < Visit.Skewers.Num())
				{
					StartNextDip(Visit);
				}
				else
				{
					Pay(Visit);
					Customer->StandUp();
					SendHome(Visit);
					bSteamDirty = true;
				}
			}
			break;

		case EVisitState::Leaving:
			if (!Customer->IsWalking())
			{
				Customer->Destroy();
				Visits.RemoveAt(i);
			}
			break;
		}
	}

	// 3. 김. 손님이 앉거나 일어날 때, 그리고 배치가 바뀌었을 수 있으니 0.5초마다 다시 칠한다.
	SteamTimer -= DeltaTime;
	if (bSteamDirty || SteamTimer <= 0.f)
	{
		RefreshSteam();
		RefreshSkewerDisplay();
		SteamTimer = 0.5f;
	}
}

// ---------------------------------------------------------------------------
// 개발용 설명
// ---------------------------------------------------------------------------

FString UHGCustomerDirector::DescribeCustomers() const
{
	const UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	int32 Counts[static_cast<int32>(EVisitState::Leaving) + 1] = {};
	for (const FVisit& Visit : Visits)
	{
		++Counts[static_cast<int32>(Visit.State)];
	}
	return FString::Printf(TEXT("손님: 진열대 %d, 자리로 가는 중 %d, 육수 대기 %d, 먹는 중 %d, 나가는 중 %d | 들어온 손님 %d, 계산한 손님 %d | 가진 돈 %d | 발견한 콤보 %d개 | 영업 %s"),
		Counts[0] + Counts[1], Counts[2], Counts[3], Counts[4], Counts[5], SpawnedCount, ServedCount, GI ? GI->GetMoney() : 0,
		GI ? GI->GetDiscoveredCombos().Num() : 0, IsOpen() ? TEXT("중") : TEXT("끝 (문 닫을 시간)"));
}

FString UHGCustomerDirector::DescribeTables() const
{
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	const UDataTable* ComboTable = GI ? GI->GetComboTable() : nullptr;
	const FHGShopGrid& Grid = GetGrid();
	TArray<FHGGridItem> Items;
	HGShopGrid::GatherItems(GetWorld(), Grid, Items);

	FString Text;
	int32 Index = 0;
	for (const FHGGridItem& Table : Items)
	{
		if (!Table.Actor.IsValid() || !Table.IsTable())
		{
			continue;
		}
		int32 SeatCount = 0;
		for (const FHGGridItem& Chair : Items)
		{
			if (Chair.Actor.IsValid() && Chair.IsChair() && Table.Rect.Contains(HGShopGrid::GetFrontTile(Grid, Chair.Actor.Get())))
			{
				++SeatCount;
			}
		}
		const FHGComboResult Combos = HGCombo::EvaluateTable(ComboTable, Table.Rect, Items);
		Text += FString::Printf(TEXT("%s식탁 %d: 칸 %d,%d~%d,%d, 자리 %d, 콤보 %s (보너스 %d%%)"), Index > 0 ? TEXT("\n") : TEXT(""), Index + 1,
			Table.Rect.Min.X, Table.Rect.Min.Y, Table.Rect.Max.X - 1, Table.Rect.Max.Y - 1, SeatCount,
			Combos.Num() > 0 ? *Combos.Describe(ComboTable) : TEXT("없음"), Combos.BonusPercent);
		++Index;
	}
	return Text.IsEmpty() ? FString(TEXT("식탁 없음")) : Text;
}
