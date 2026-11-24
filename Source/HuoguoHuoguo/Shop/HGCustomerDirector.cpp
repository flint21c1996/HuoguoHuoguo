#include "Shop/HGCustomerDirector.h"

#include "Algo/Reverse.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/HGGameInstance.h"
#include "Game/HGIngredientTypes.h"
#include "GameFramework/Pawn.h"
#include "HuoguoHuoguo.h"
#include "Shop/HGCustomer.h"
#include "Shop/HGFurniture.h"
#include "Shop/HGFurniturePlacer.h"
#include "Shop/HGShopCombos.h"
#include "World/HGExplorerStart.h"

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
				if (!Other || Other->IsA<AHGFurniture>() || Other->IsA<AHGCustomer>() || Other->IsA<APawn>() || Other->IsA<AHGExplorerStart>())
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

	// 다른 손님이 잡은 의자.
	TSet<const AHGFurniture*> Reserved;
	for (const FVisit& Visit : Visits)
	{
		if (Visit.State != EVisitState::Leaving && Visit.Chair.IsValid())
		{
			Reserved.Add(Visit.Chair.Get());
		}
	}

	// 빈자리: 식탁을 바라보는 의자 (의자 바로 앞 칸이 식탁). (의자, 식탁) 짝으로 모은다.
	TArray<TPair<AHGFurniture*, AHGFurniture*>> Seats;
	for (const FHGGridItem& Chair : Items)
	{
		AHGFurniture* ChairActor = Chair.Actor.Get();
		if (!ChairActor || !ChairActor->IsChair() || Reserved.Contains(ChairActor))
		{
			continue;
		}
		const FIntPoint Front = HGShopGrid::GetFrontTile(Grid, ChairActor);
		for (const FHGGridItem& Table : Items)
		{
			AHGFurniture* TableActor = Table.IsTable() ? Table.Actor.Get() : nullptr;
			if (TableActor && Table.Rect.Contains(Front))
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
	for (const TPair<AHGFurniture*, AHGFurniture*>& Seat : Seats)
	{
		TArray<FVector> Points;
		const FIntPoint ChairTile = Grid.WorldToTile(Seat.Key->GetActorLocation());
		if (!FindPath(Grid.EntranceTile, ChairTile, Occupied, Points))
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AHGCustomer* Customer = GetWorld()->SpawnActor<AHGCustomer>(AHGCustomer::StaticClass(), Grid.TileCenter(Grid.EntranceTile), FRotator::ZeroRotator, Params);
		if (!Customer)
		{
			OutResult = TEXT("손님을 만들지 못했다");
			return false;
		}
		Customer->SetBodyColor(CustomerColors[Random.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(CustomerColors)) - 1)]);
		Customer->WalkPath(Points);

		FVisit& Visit = Visits.AddDefaulted_GetRef();
		Visit.Customer = Customer;
		Visit.Chair = Seat.Key;
		Visit.Table = Seat.Value;
		Visit.State = EVisitState::ToSeat;
		++SpawnedCount;

		OutResult = FString::Printf(TEXT("손님 %d번이 들어왔다 -> 의자 칸 (%d, %d), %d칸 걸음"), SpawnedCount, ChairTile.X, ChairTile.Y, Points.Num());
		UE_LOG(LogHuoguo, Log, TEXT("[Customer] %s"), *OutResult);
		return true;
	}
	OutResult = TEXT("빈자리까지 가는 길이 없어서 손님이 들어오지 않았다");
	return false;
}

void UHGCustomerDirector::SendHome(FVisit& Visit)
{
	Visit.State = EVisitState::Leaving;
	AHGCustomer* Customer = Visit.Customer.Get();
	if (!Customer)
	{
		return;
	}
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

void UHGCustomerDirector::Pay(FVisit& Visit)
{
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	AHGFurniture* Table = Visit.Table.Get();
	if (!GI || !Table)
	{
		return;
	}

	// 꼬치 값: 가방 재료를 하나씩 꺼내 그 재료의 Price, 가방이 비면 기본 꼬치.
	int32 BasePrice = 0;
	TMap<FString, int32> Eaten;
	TArray<FString> EatenOrder;
	for (int32 i = 0; i < SkewersPerCustomer; ++i)
	{
		FName IngredientId;
		FString Name;
		if (GI->TakeOneFromBag(IngredientId))
		{
			const FHGIngredientRow* Row = GI->FindIngredient(IngredientId);
			BasePrice += Row ? Row->Price : BasicSkewerPrice;
			Name = GI->GetIngredientName(IngredientId);
		}
		else
		{
			BasePrice += BasicSkewerPrice;
			Name = TEXT("기본 꼬치");
		}
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
	if (AHGMoneyPopup* Popup = GetWorld()->SpawnActor<AHGMoneyPopup>(AHGMoneyPopup::StaticClass(), Table->GetActorLocation() + FVector(0.f, 0.f, 200.f), FRotator::ZeroRotator, Params))
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
		if (EatingTables.Contains(*It))
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
			Visits.RemoveAt(i);
			bSteamDirty = true;
			continue;
		}
		const bool bSeatGone = !Visit.Chair.IsValid() || !Visit.Table.IsValid();

		switch (Visit.State)
		{
		case EVisitState::ToSeat:
			if (bSeatGone)
			{
				Message(TEXT("손님이 가려던 자리가 없어져서 그냥 나갔다"));
				SendHome(Visit);
			}
			else if (!Customer->IsWalking())
			{
				// 의자에 앉아 식탁 가운데를 본다.
				const FVector ToTable = Visit.Table->GetActorLocation() - Visit.Chair->GetActorLocation();
				Customer->SitAt(Visit.Chair->GetActorLocation(), ToTable.Rotation().Yaw);
				Visit.State = EVisitState::Eating;
				Visit.Timer = EatSeconds;
				bSteamDirty = true;
			}
			break;

		case EVisitState::Eating:
			if (bSeatGone)
			{
				Customer->StandUp();
				Message(TEXT("먹던 자리가 없어져서 손님이 계산하지 않고 나갔다"));
				SendHome(Visit);
				bSteamDirty = true;
				break;
			}
			Visit.Timer -= DeltaTime;
			if (Visit.Timer <= 0.f)
			{
				Pay(Visit);
				Customer->StandUp();
				SendHome(Visit);
				bSteamDirty = true;
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
		SteamTimer = 0.5f;
	}
}

// ---------------------------------------------------------------------------
// 개발용 설명
// ---------------------------------------------------------------------------

FString UHGCustomerDirector::DescribeCustomers() const
{
	const UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	int32 Counts[3] = { 0, 0, 0 };
	for (const FVisit& Visit : Visits)
	{
		++Counts[static_cast<int32>(Visit.State)];
	}
	return FString::Printf(TEXT("손님: 자리로 가는 중 %d, 먹는 중 %d, 나가는 중 %d | 들어온 손님 %d, 계산한 손님 %d | 가진 돈 %d | 발견한 콤보 %d개 | 영업 %s"),
		Counts[0], Counts[1], Counts[2], SpawnedCount, ServedCount, GI ? GI->GetMoney() : 0,
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
