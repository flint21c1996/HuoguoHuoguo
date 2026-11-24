#include "Shop/HGFurniturePlacer.h"

#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/HGGameInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "HuoguoHuoguo.h"
#include "Shop/HGFurniture.h"
#include "Shop/HGFurnitureData.h"
#include "Shop/HGShopCombos.h"

namespace
{
	// 차지할 칸 색: 놓을 수 있음 / 없음 / 가리킨 가구.
	const FLinearColor ValidCellColor(0.20f, 0.85f, 0.30f);
	const FLinearColor InvalidCellColor(0.95f, 0.18f, 0.12f);
	const FLinearColor HoverCellColor(1.00f, 0.60f, 0.10f);

	// Yaw를 가장 가까운 90도 배수로 맞춘다 (칸 배치는 네 방향만 쓴다).
	float SnapYaw(float Yaw)
	{
		return FRotator::NormalizeAxis(FMath::RoundToFloat(Yaw / 90.f) * 90.f);
	}

	bool IsTableItem(const FHGGridItem& Item)
	{
		return Item.IsTable();
	}
}

UHGFurniturePlacer::UHGFurniturePlacer()
{
	PrimaryComponentTick.bCanEverTick = false;
}

APlayerController* UHGFurniturePlacer::GetPC() const
{
	return Cast<APlayerController>(GetOwner());
}

// ---------------------------------------------------------------------------
// 가구 목록 (Data/Furniture.csv -> DT_Furniture)
// ---------------------------------------------------------------------------

const TArray<FName>& UHGFurniturePlacer::GetCatalog()
{
	if (!bCatalogLoaded)
	{
		bCatalogLoaded = true;
		FurnitureTable = HGFurnitureData::LoadTable();
		if (FurnitureTable)
		{
			Catalog = FurnitureTable->GetRowNames();
			UE_LOG(LogHuoguo, Log, TEXT("가구 목록: %d종 (식탁 %d, 의자 %d, 장식 %d)"), Catalog.Num(),
				GetCategoryItems(EHGFurnitureCategory::Table).Num(), GetCategoryItems(EHGFurnitureCategory::Chair).Num(),
				GetCategoryItems(EHGFurnitureCategory::Decor).Num());
		}
		else
		{
			UE_LOG(LogHuoguo, Error, TEXT("가구 DataTable(%s)을 불러오지 못했다. Tools/Invoke-HGMapGen.ps1로 Data/Furniture.csv를 읽어 들일 것"),
				HGFurnitureData::TablePath);
		}
		if (SelectedId.IsNone() && !Catalog.IsEmpty())
		{
			SelectedId = Catalog[0];
		}
	}
	return Catalog;
}

TArray<FName> UHGFurniturePlacer::GetCategoryItems(EHGFurnitureCategory Category)
{
	TArray<FName> Items;
	for (const FName& Id : GetCatalog())
	{
		const FHGFurnitureRow* Row = HGFurnitureData::FindRow(Id);
		if (Row && Row->Category == Category)
		{
			Items.Add(Id);
		}
	}
	return Items;
}

FName UHGFurniturePlacer::GetHeldId() const
{
	return bMoving ? MovingId : SelectedId;
}

FText UHGFurniturePlacer::GetHeldName() const
{
	const FHGFurnitureRow* Row = HGFurnitureData::FindRow(GetHeldId());
	return Row ? Row->DisplayName : FText::FromName(GetHeldId());
}

FString UHGFurniturePlacer::DescribeHeld()
{
	const FName Id = GetHeldId();
	const FHGFurnitureRow* Row = HGFurnitureData::FindRow(Id);
	if (!Row)
	{
		return Id.IsNone() ? FString(TEXT("없음")) : Id.ToString();
	}
	const TArray<FName> Same = GetCategoryItems(Row->Category);
	const FIntPoint Size = Row->GetFootprint();
	FString Text = FString::Printf(TEXT("%s (%s %d/%d, %dx%d, 값 %d"), *Row->DisplayName.ToString(), HGFurnitureData::CategoryName(Row->Category),
		Same.IndexOfByKey(Id) + 1, Same.Num(), Size.X, Size.Y, Row->Price);
	if (Row->BonusPercent != 0)
	{
		Text += FString::Printf(TEXT(", 손님 +%d%%"), Row->BonusPercent);
	}
	if (!Row->Tags.IsEmpty())
	{
		Text += FString::Printf(TEXT(", %s"), *Row->Tags);
	}
	Text += TEXT(")");

	// 잠긴 가구면 여는 조건을, 열린 가구면 가진 돈과 열린 가구 수를 붙인다.
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	const FString Lock = HGFurnitureUnlock::DescribeLock(GI, Id, *Row);
	const FIntPoint Opened = HGFurnitureUnlock::CountUnlocked(GI);
	Text += Lock.IsEmpty() ? FString::Printf(TEXT(" | 가진 돈 %d"), GI ? GI->GetMoney() : 0) : FString::Printf(TEXT(" | %s"), *Lock);
	return Text + FString::Printf(TEXT(" | 열린 가구 %d/%d"), Opened.X, Opened.Y);
}

void UHGFurniturePlacer::HoldFurniture(FName FurnitureId)
{
	CancelMove();
	SelectedId = FurnitureId;
	if (const FHGFurnitureRow* Row = HGFurnitureData::FindRow(FurnitureId))
	{
		LastInCategory.Add(Row->Category, FurnitureId);
	}
	if (bBuilding)
	{
		RespawnGhost();
		UpdateCursor();
	}
}

bool UHGFurniturePlacer::SelectCategory(EHGFurnitureCategory Category)
{
	const TArray<FName> Items = GetCategoryItems(Category);
	if (Items.IsEmpty())
	{
		return false;
	}
	// 그 분류에서 마지막으로 골랐던 가구. 처음이면 첫 번째 열린 가구 (모두 잠겼으면 첫 가구).
	const FName* Last = LastInCategory.Find(Category);
	FName Pick = Items[0];
	if (Last && Items.Contains(*Last))
	{
		Pick = *Last;
	}
	else
	{
		const UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
		for (const FName& Id : Items)
		{
			const FHGFurnitureRow* Row = HGFurnitureData::FindRow(Id);
			if (Row && HGFurnitureUnlock::IsUnlocked(GI, Id, *Row))
			{
				Pick = Id;
				break;
			}
		}
	}
	HoldFurniture(Pick);
	return true;
}

void UHGFurniturePlacer::CycleInCategory(int32 Direction)
{
	const FHGFurnitureRow* Row = HGFurnitureData::FindRow(GetHeldId());
	if (!Row)
	{
		return;
	}
	const TArray<FName> Items = GetCategoryItems(Row->Category);
	if (Items.IsEmpty())
	{
		return;
	}
	const int32 Index = FMath::Max(0, Items.IndexOfByKey(GetHeldId()));
	HoldFurniture(Items[(Index + Direction + Items.Num()) % Items.Num()]);
}

bool UHGFurniturePlacer::SelectFurniture(FName FurnitureId)
{
	if (!GetCatalog().Contains(FurnitureId))
	{
		return false;
	}
	HoldFurniture(FurnitureId);
	return true;
}

// ---------------------------------------------------------------------------
// 배치 기록
// ---------------------------------------------------------------------------

void UHGFurniturePlacer::RestoreLayout()
{
	GetCatalog();
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		UE_LOG(LogHuoguo, Warning, TEXT("GameInstance가 UHGGameInstance가 아니라서 가구 배치를 기록하지 못한다"));
		return;
	}

	if (!GI->HasShopLayout())
	{
		// 게임을 켜고 처음 정거장에 왔다. 맵에 배치된 가구가 처음 배치다. 데이터로 다시 만들어 칸에 맞춘 뒤 기록한다.
		ReplaceMapFurniture();
		SaveLayout();
		UE_LOG(LogHuoguo, Log, TEXT("가구 배치: 맵에 배치된 가구 %d개를 처음 배치로 기록"), GI->GetShopLayout().Num());
		return;
	}

	// 행성에 다녀와 정거장 맵이 새로 열렸다. 맵에 배치된 처음 가구를 치우고 기록대로 다시 놓는다.
	TArray<AHGFurniture*> MapFurniture;
	for (TActorIterator<AHGFurniture> It(GetWorld()); It; ++It)
	{
		MapFurniture.Add(*It);
	}
	for (AHGFurniture* Furniture : MapFurniture)
	{
		Furniture->Destroy();
	}

	int32 Restored = 0;
	for (const FHGPlacedFurniture& Item : GI->GetShopLayout())
	{
		Restored += SpawnFurniture(Item.FurnitureId, Item.Location, Item.Yaw) ? 1 : 0;
	}
	UE_LOG(LogHuoguo, Log, TEXT("가구 배치: 기록에서 %d개를 다시 놓음 (맵의 처음 가구 %d개는 치움)"), Restored, MapFurniture.Num());
}

void UHGFurniturePlacer::ReplaceMapFurniture()
{
	// 맵 에셋의 가구(AHGTable, AHGChair 액터)는 칸 배치와 데이터 이전에 놓였다. 맵을 고치지 않고, 실행할 때
	// ID와 자리만 읽어 지우고 데이터로 다시 만든다. 맵에서 불러온 액터는 생성 스크립트가 다시 돌지 않아 부품이 없을 수 있기 때문이다.
	struct FMapItem
	{
		FName Id;
		FVector Location;
		float Yaw;
	};
	TArray<FMapItem> First;
	TArray<FMapItem> Second;
	TArray<AHGFurniture*> MapActors;
	for (TActorIterator<AHGFurniture> It(GetWorld()); It; ++It)
	{
		const FHGFurnitureRow* Row = HGFurnitureData::FindRow(It->FurnitureId);
		const FMapItem Item{ It->FurnitureId, It->GetActorLocation(), It->GetActorRotation().Yaw };
		// 식탁 같은 일반 가구를 먼저 놓고, 의자는 식탁이 자리 잡은 뒤에 놓아서 식탁 쪽으로 돌린다.
		(Row && Row->Category == EHGFurnitureCategory::Chair ? Second : First).Add(Item);
		MapActors.Add(*It);
	}
	for (AHGFurniture* Actor : MapActors)
	{
		Actor->Destroy();
	}

	TArray<FHGGridItem> Placed;
	int32 Made = 0;
	int32 Skipped = 0;
	auto PlaceOne = [&](const FMapItem& Item)
	{
		const FHGFurnitureRow* Row = HGFurnitureData::FindRow(Item.Id);
		if (!Row)
		{
			UE_LOG(LogHuoguo, Warning, TEXT("가구 배치: 맵의 가구 ID '%s'가 가구 데이터에 없어 놓지 않음"), *Item.Id.ToString());
			++Skipped;
			return;
		}
		float Yaw = SnapYaw(Item.Yaw);
		const FIntRect Rect = Grid.SnapRect(Item.Location, FHGShopGrid::RotateSize(Row->GetFootprint(), Yaw));
		if (Row->Category == EHGFurnitureCategory::Chair)
		{
			Yaw = FaceTableYaw(Rect, Placed, Yaw);
		}
		AHGFurniture* Furniture = SpawnFurniture(Item.Id, Grid.RectCenter(Rect), Yaw);
		if (!Furniture || !CheckPlacement(Furniture, Rect, Placed))
		{
			UE_LOG(LogHuoguo, Warning, TEXT("가구 배치: 맵의 %s (%.0f, %.0f)를 칸에 맞출 수 없어 놓지 않음 (%s)"),
				*Item.Id.ToString(), Item.Location.X, Item.Location.Y, *LastRejectReason);
			if (Furniture)
			{
				Furniture->Destroy();
			}
			++Skipped;
			return;
		}
		FHGGridItem& Added = Placed.AddDefaulted_GetRef();
		Added.Actor = Furniture;
		Added.Id = Item.Id;
		Added.Category = Row->Category;
		Added.Tags = Furniture->GetTags();
		Added.Rect = Rect;
		++Made;
	};
	for (const FMapItem& Item : First)
	{
		PlaceOne(Item);
	}
	for (const FMapItem& Item : Second)
	{
		PlaceOne(Item);
	}
	UE_LOG(LogHuoguo, Log, TEXT("가구 배치: 맵의 가구 %d개를 데이터로 다시 만들어 칸에 맞춤, %d개는 놓지 않음"), Made, Skipped);
}

void UHGFurniturePlacer::SaveLayout() const
{
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		return;
	}

	// TActorIterator는 Destroy()로 없애는 중인 액터를 건너뛴다. 그래서 방금 치운 가구는 기록에 들어가지 않는다.
	TArray<FHGPlacedFurniture> Layout;
	for (TActorIterator<AHGFurniture> It(GetWorld()); It; ++It)
	{
		if (It->IsGhost())
		{
			continue;
		}
		FHGPlacedFurniture& Item = Layout.AddDefaulted_GetRef();
		Item.FurnitureId = It->FurnitureId;
		Item.Location = It->GetActorLocation();
		Item.Yaw = It->GetActorRotation().Yaw;
	}
	GI->SetShopLayout(Layout);
}

AHGFurniture* UHGFurniturePlacer::SpawnFurniture(FName FurnitureId, const FVector& Location, float Yaw, bool bGhost) const
{
	const FHGFurnitureRow* Row = HGFurnitureData::FindRow(FurnitureId);
	if (!Row)
	{
		return nullptr;
	}
	// 행에 블루프린트 클래스가 있으면 그것, 없으면 기본 가구 클래스.
	UClass* Class = Row->ActorClass.IsNull() ? nullptr : Row->ActorClass.LoadSynchronous();
	if (!Class)
	{
		Class = AHGFurniture::StaticClass();
	}

	// 미뤄서 스폰하고, 스폰을 끝내기(FinishSpawning = 생성 스크립트 실행) 전에 ID를 정한다. 생성 스크립트가 그 ID의 행으로 부품을 만든다.
	// 유령이면 그 전에 유령으로 표시해서 부품이 처음부터 유령 충돌로 만들어진다 (충돌이 켜진 상태로 한 프레임도 존재하지 않는다).
	// 자리 검사는 따로 한다. 엔진의 스폰 충돌 처리가 위치를 몰래 옮기지 않도록 항상 그 자리에 만든다.
	const FTransform Transform(FRotator(0.f, Yaw, 0.f), Location);
	AHGFurniture* Furniture = GetWorld()->SpawnActorDeferred<AHGFurniture>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Furniture)
	{
		return nullptr;
	}
	Furniture->FurnitureId = FurnitureId;
	if (bGhost)
	{
		Furniture->MakeGhost();
		Furniture->SetActorHiddenInGame(true);
	}
	Furniture->FinishSpawning(Transform);
	return Furniture;
}

// ---------------------------------------------------------------------------
// 배치 모드
// ---------------------------------------------------------------------------

void UHGFurniturePlacer::BeginBuild()
{
	if (bBuilding)
	{
		return;
	}
	GetCatalog();
	bBuilding = true;
	ShowGridOverlay();
	RespawnGhost();
	UpdateCursor();
}

void UHGFurniturePlacer::EndBuild()
{
	if (!bBuilding)
	{
		return;
	}
	CancelMove();
	SetHovered(nullptr);
	DestroyGhost();
	HideGridOverlay();
	PreviewText.Reset();
	bBuilding = false;
}

void UHGFurniturePlacer::RotateHeld(float Degrees)
{
	HeldYaw = SnapYaw(HeldYaw + Degrees);
	// 유령의 실제 방향은 다음 UpdateCursor가 정한다 (의자는 식탁 쪽으로 다시 돌 수 있다).
	UpdateCursor();
}

void UHGFurniturePlacer::RespawnGhost()
{
	DestroyGhost();
	// 처음에는 숨겨 두고 커서 위치가 정해지면 보인다.
	Ghost = SpawnFurniture(GetHeldId(), FVector(0.f, 0.f, Grid.FloorZ), HeldYaw, true);
	if (Ghost)
	{
		Ghost->SetPreviewState(EHGFurniturePreview::Invalid);
	}
	bGhostValid = false;
}

void UHGFurniturePlacer::DestroyGhost()
{
	if (Ghost)
	{
		Ghost->Destroy();
		Ghost = nullptr;
	}
	bGhostValid = false;
}

void UHGFurniturePlacer::SetHovered(AHGFurniture* NewHovered)
{
	if (Hovered == NewHovered)
	{
		return;
	}
	if (IsValid(Hovered))
	{
		Hovered->SetPreviewState(EHGFurniturePreview::None);
	}
	Hovered = NewHovered;
	if (Hovered)
	{
		Hovered->SetPreviewState(EHGFurniturePreview::Hover);
	}
}

void UHGFurniturePlacer::CancelMove()
{
	if (!bMoving)
	{
		return;
	}
	SpawnFurniture(MovingId, MovingFrom.GetLocation(), MovingFrom.Rotator().Yaw);
	bMoving = false;
	MovingId = NAME_None;
	SaveLayout();
	RefreshOccupiedCells();
	if (bBuilding)
	{
		RespawnGhost();
	}
}

void UHGFurniturePlacer::ShowGridOverlay()
{
	HideGridOverlay();
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Plane)
	{
		return;
	}

	// 빈 액터에 인스턴스 메시 컴포넌트 세 장을 겹쳐 붙인다. 칸마다 얇은 판(1m 평면을 줄인 것) 하나.
	//   GridTiles      배치 영역의 모든 칸 (청록). 판 사이 틈으로 가게 바닥이 비쳐서 칸 무늬가 된다.
	//   OccupiedTiles  놓인 가구가 차지한 칸 (짙은 남색). 가구마다 몇 칸을 쓰는지 보인다.
	//   HighlightTiles 손에 든 가구가 차지할 칸 (초록 = 놓을 수 있음, 빨강 = 없음) 또는 가리킨 가구의 칸 (주황).
	// 위로 갈수록 조금씩 더 띄워 바닥과, 서로 간에 깜빡이며 겹쳐 보이지(z-fighting) 않게 한다.
	// 충돌, 그림자가 없어서 사장님과 커서 추적에 영향이 없다.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GridOverlay = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!GridOverlay)
	{
		return;
	}
	auto MakeTiles = [this, Plane](const TCHAR* Name) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* Tiles = NewObject<UInstancedStaticMeshComponent>(GridOverlay, Name);
		Tiles->SetMobility(EComponentMobility::Movable);
		Tiles->SetStaticMesh(Plane);
		Tiles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Tiles->SetCastShadow(false);
		if (GridOverlay->GetRootComponent())
		{
			Tiles->SetupAttachment(GridOverlay->GetRootComponent());
		}
		else
		{
			GridOverlay->SetRootComponent(Tiles);
		}
		Tiles->RegisterComponent();
		return Tiles;
	};

	GridTiles = MakeTiles(TEXT("GridTiles"));
	HGPlaceholder::Paint(GridTiles, FLinearColor(0.30f, 0.72f, 0.66f));
	AddCellInstances(GridTiles, FIntRect(Grid.BuildMin, Grid.BuildMax), GridLift, 0.92f);

	OccupiedTiles = MakeTiles(TEXT("OccupiedTiles"));
	HGPlaceholder::Paint(OccupiedTiles, FLinearColor(0.16f, 0.30f, 0.42f));
	RefreshOccupiedCells();

	HighlightTiles = MakeTiles(TEXT("HighlightTiles"));
	HighlightMaterial = HGPlaceholder::Paint(HighlightTiles, ValidCellColor);
	bHighlightShown = false;
}

void UHGFurniturePlacer::HideGridOverlay()
{
	if (GridOverlay)
	{
		GridOverlay->Destroy();
		GridOverlay = nullptr;
	}
	GridTiles = nullptr;
	OccupiedTiles = nullptr;
	HighlightTiles = nullptr;
	HighlightMaterial = nullptr;
	bHighlightShown = false;
}

// ---------------------------------------------------------------------------
// 커서
// ---------------------------------------------------------------------------

bool UHGFurniturePlacer::GetCursorRay(FVector& OutOrigin, FVector& OutDirection) const
{
	APlayerController* PC = GetPC();
	if (!PC)
	{
		return false;
	}

	FVector2D Screen;
	if (CursorWorldOverride.IsSet())
	{
		// 시험용: 월드 지점을 화면 좌표로 바꿔서 마우스 위치인 것처럼 쓴다.
		if (!PC->ProjectWorldLocationToScreen(CursorWorldOverride.GetValue(), Screen))
		{
			return false;
		}
	}
	else
	{
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (!PC->GetMousePosition(MouseX, MouseY))
		{
			return false; // 커서가 게임 창 밖에 있다.
		}
		Screen = FVector2D(MouseX, MouseY);
	}

	// 화면 좌표 -> 카메라 위치에서 그 픽셀을 지나 세계 안쪽으로 나가는 광선.
	return PC->DeprojectScreenPositionToWorld(Screen.X, Screen.Y, OutOrigin, OutDirection);
}

void UHGFurniturePlacer::UpdateCursor()
{
	if (!bBuilding)
	{
		return;
	}
	if (!Ghost)
	{
		RespawnGhost();
		if (!Ghost)
		{
			return;
		}
	}

	auto HideGhost = [this]()
	{
		Ghost->SetActorHiddenInGame(true);
		bGhostValid = false;
		PreviewText.Reset();
		ClearHighlight();
	};

	FVector Origin;
	FVector Direction;
	if (!GetCursorRay(Origin, Direction))
	{
		SetHovered(nullptr);
		HideGhost();
		return;
	}

	// 1. 광선이 바닥 높이의 수평면과 만나는 점 = 커서가 가리키는 바닥 칸. 카메라가 늘 내려다보므로 Direction.Z < 0이다.
	//    Origin + Direction * T 의 Z가 FloorZ가 되는 T를 구한다.
	if (Direction.Z > -KINDA_SMALL_NUMBER)
	{
		SetHovered(nullptr);
		HideGhost();
		return;
	}
	const double T = (Grid.FloorZ - Origin.Z) / Direction.Z;
	const FVector Point = Origin + Direction * T;
	const FIntPoint CursorCell = Grid.WorldToTile(Point);

	// 2. 그 칸을 차지한 가구가 있으면 그 가구를 가리킨 것이다. 가구를 옮기는 중에는 새로 집을 수 없으므로 보지 않는다.
	//    가구 모양에 광선을 쏘는 대신 칸으로 판정한다: 아이소 시점에서는 앞쪽 가구의 윗부분이 뒤쪽 빈 칸을 가려서,
	//    빈 칸을 가리켜도 앞 식탁이 잡히는 일이 있었다. 칸 배치에서는 "가리킨 칸"이 기준이다.
	TArray<FHGGridItem> Items;
	HGShopGrid::GatherItems(GetWorld(), Grid, Items);
	AHGFurniture* NewHovered = nullptr;
	if (!bMoving)
	{
		for (const FHGGridItem& Item : Items)
		{
			if (Item.Rect.Contains(CursorCell))
			{
				NewHovered = Item.Actor.Get();
				break;
			}
		}
	}
	SetHovered(NewHovered);
	if (Hovered)
	{
		// 가리킨 가구가 차지한 칸을 주황으로 보여 준다. 클릭하면 이 칸들이 비고 가구를 손에 든다.
		HideGhost();
		SetHighlight(HGShopGrid::GetFurnitureRect(Grid, Hovered), HoverCellColor);
		return;
	}

	// 3. 빈 칸이면 손에 든 가구를 놓아 본다. 가리킨 칸이 가구의 첫 칸이 되고, 가구 크기만큼 칸을 차지한다.
	float Yaw = HeldYaw;
	const FIntRect Rect = Grid.CursorRect(Point, FHGShopGrid::RotateSize(Ghost->GetFootprint(), Yaw));
	if (Ghost->FacesTable())
	{
		Yaw = FaceTableYaw(Rect, Items, HeldYaw);
	}

	Ghost->SetActorLocationAndRotation(Grid.RectCenter(Rect), FRotator(0.f, Yaw, 0.f));
	Ghost->SetActorHiddenInGame(false);
	bGhostValid = CheckPlacement(Ghost, Rect, Items) && CanAffordHeld();
	Ghost->SetPreviewState(bGhostValid ? EHGFurniturePreview::Valid : EHGFurniturePreview::Invalid);
	PreviewText = bGhostValid ? BuildPreviewText(Ghost->FurnitureId, Rect, Items) : FString();
	SetHighlight(Rect, bGhostValid ? ValidCellColor : InvalidCellColor);
}

void UHGFurniturePlacer::SetHighlight(const FIntRect& Rect, const FLinearColor& Color)
{
	if (!HighlightTiles)
	{
		return;
	}
	// 같은 칸, 같은 색이면 다시 만들지 않는다 (매 프레임 불리므로).
	if (bHighlightShown && Rect == HighlightRect && Color.Equals(HighlightColor))
	{
		return;
	}
	bHighlightShown = true;
	HighlightRect = Rect;
	HighlightColor = Color;
	if (HighlightMaterial)
	{
		HighlightMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
	HighlightTiles->ClearInstances();
	AddCellInstances(HighlightTiles, Rect, HighlightLift, 0.96f);
}

void UHGFurniturePlacer::ClearHighlight()
{
	if (HighlightTiles && bHighlightShown)
	{
		HighlightTiles->ClearInstances();
	}
	bHighlightShown = false;
}

void UHGFurniturePlacer::AddCellInstances(UInstancedStaticMeshComponent* Tiles, const FIntRect& Rect, float Lift, float Fill) const
{
	const float Scale = Fill * Grid.TileSize / 100.f;
	for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X)
	{
		for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y)
		{
			const FVector Location = Grid.TileCenter(FIntPoint(X, Y)) + FVector(0.f, 0.f, Lift);
			Tiles->AddInstance(FTransform(FRotator::ZeroRotator, Location, FVector(Scale, Scale, 1.f)), true);
		}
	}
}

void UHGFurniturePlacer::RefreshOccupiedCells()
{
	if (!OccupiedTiles)
	{
		return;
	}
	// 놓인 가구가 차지한 칸을 짙은 색으로 깐다. 가구마다 몇 칸을 쓰는지(식탁 2x2, 소스바 2x1)가 바닥에 보인다.
	OccupiedTiles->ClearInstances();
	TArray<FHGGridItem> Items;
	HGShopGrid::GatherItems(GetWorld(), Grid, Items);
	for (const FHGGridItem& Item : Items)
	{
		AddCellInstances(OccupiedTiles, Item.Rect, OccupiedLift, 0.92f);
	}
}

float UHGFurniturePlacer::FaceTableYaw(const FIntRect& Rect, const TArray<FHGGridItem>& Items, float Fallback) const
{
	// 의자는 1x1칸이므로 Rect.Min이 의자 칸이다. 그 칸이 식탁의 상하좌우 바로 옆이면 식탁 가운데 쪽을 본다.
	// 지금 방향(Fallback)으로 이미 식탁을 보고 있으면 그대로 둔다. 식탁 두 개 사이에서는 R로 돌려 어느 식탁을 볼지 고른다.
	const float Radians = FMath::DegreesToRadians(SnapYaw(Fallback));
	const FIntPoint Front = Rect.Min + FIntPoint(FMath::RoundToInt(FMath::Cos(Radians)), FMath::RoundToInt(FMath::Sin(Radians)));
	for (const FHGGridItem& Item : Items)
	{
		if (IsTableItem(Item) && Item.Rect.Contains(Front))
		{
			return SnapYaw(Fallback);
		}
	}
	for (const FHGGridItem& Item : Items)
	{
		if (IsTableItem(Item) && FHGShopGrid::IsSideNeighbor(Item.Rect, Rect.Min))
		{
			const FVector ToTable = Grid.RectCenter(Item.Rect) - Grid.RectCenter(Rect);
			return SnapYaw(FMath::RadiansToDegrees(FMath::Atan2(ToTable.Y, ToTable.X)));
		}
	}
	return Fallback;
}

bool UHGFurniturePlacer::CheckPlacement(const AHGFurniture* Candidate, const FIntRect& Rect, const TArray<FHGGridItem>& Items) const
{
	LastRejectReason.Reset();
	UWorld* World = GetWorld();

	// 1. 칸: 배치 영역 안이고, 다른 가구와 칸이 겹치지 않아야 한다.
	if (!Grid.IsInBuildArea(Rect))
	{
		LastRejectReason = TEXT("가구를 놓을 수 있는 칸 밖");
		return false;
	}
	for (const FHGGridItem& Item : Items)
	{
		if (Item.Actor.Get() != Candidate && FHGShopGrid::Overlaps(Item.Rect, Rect))
		{
			LastRejectReason = FString::Printf(TEXT("칸이 겹침: %s"), Item.Actor.IsValid() ? *Item.Actor->GetDisplayName().ToString() : *Item.Id.ToString());
			return false;
		}
	}

	// 2. 겹침: 부품의 충돌 모양을 그 자리(조금 띄움)에 두었을 때 가구가 아닌 막는 물체와 겹치는가.
	//    가구끼리는 1번(칸)에서 이미 판정했다. 물체 종류로 묻는다: 고정물(벽, 카운터), 움직이는 물체(발판 트리거), 폰(사장님), 물리 물체.
	TArray<UStaticMeshComponent*> Parts;
	Candidate->GetCollidingParts(Parts);
	FComponentQueryParams Params(SCENE_QUERY_STAT(HGFurniturePlace), Candidate);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);

	const FVector Lift(0.f, 0.f, PlacementLift);
	TArray<FOverlapResult> Overlaps;
	for (const UStaticMeshComponent* Part : Parts)
	{
		World->ComponentOverlapMulti(Overlaps, Part, Part->GetComponentLocation() + Lift, Part->GetComponentQuat(), Params, Objects);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			const AActor* Other = Overlap.GetActor();
			if (!Other || Other == Candidate || Other->IsA<AHGFurniture>())
			{
				continue;
			}
			LastRejectReason = FString::Printf(TEXT("겹침: %s"), *Other->GetActorNameOrLabel());
			return false;
		}
	}
	return true;
}

FString UHGFurniturePlacer::BuildPreviewText(FName Id, const FIntRect& Rect, const TArray<FHGGridItem>& Items) const
{
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	const UDataTable* Table = GI ? GI->GetComboTable() : nullptr;
	if (!Table)
	{
		return FString();
	}

	// 유령을 놓았다고 치고 계산한다.
	TArray<FHGGridItem> With = Items;
	FHGGridItem& GhostItem = With.AddDefaulted_GetRef();
	GhostItem.Id = Id;
	GhostItem.Rect = Rect;
	if (Ghost)
	{
		GhostItem.Category = Ghost->GetCategory();
		GhostItem.Tags = Ghost->GetTags();
	}

	if (Ghost && Ghost->IsTable())
	{
		// 식탁: 이 자리 둘레에 이미 있는 장식으로 생기는 콤보.
		const FHGComboResult Result = HGCombo::EvaluateTable(Table, Rect, With);
		return Result.Num() > 0
			? FString::Printf(TEXT("이 자리: %s"), *Result.Describe(Table))
			: FString(TEXT("이 자리: 콤보 없음 (식탁 둘레에 화분, 등불, 소스 바를 놓으면 콤보)"));
	}

	// 장식, 의자: 맞닿는 식탁마다 놓기 전과 놓은 뒤를 비교해서 새로 생기는 콤보만 알린다.
	TArray<FString> NewCombos;
	for (const FHGGridItem& Item : Items)
	{
		if (!IsTableItem(Item) || !FHGShopGrid::Touches(Item.Rect, Rect))
		{
			continue;
		}
		const FHGComboResult Before = HGCombo::EvaluateTable(Table, Item.Rect, Items);
		const FHGComboResult After = HGCombo::EvaluateTable(Table, Item.Rect, With);
		for (int32 i = 0; i < After.Ids.Num(); ++i)
		{
			if (!Before.Ids.Contains(After.Ids[i]))
			{
				const FHGComboRow* Row = Table->FindRow<FHGComboRow>(After.Ids[i], TEXT("Preview"), false);
				NewCombos.AddUnique(FString::Printf(TEXT("%s +%d%%"), *After.Names[i], Row ? Row->BonusPercent : 0));
			}
		}
	}
	return NewCombos.IsEmpty() ? FString() : FString::Printf(TEXT("옆 식탁에 새 콤보: %s"), *FString::Join(NewCombos, TEXT(", ")));
}

// ---------------------------------------------------------------------------
// 클릭과 지우기
// ---------------------------------------------------------------------------

FString UHGFurniturePlacer::Click()
{
	if (!bBuilding)
	{
		return FString();
	}
	UpdateCursor();

	// 가리킨 가구를 집는다: 지우고 같은 종류, 같은 방향의 유령을 손에 든다. 원래 자리는 취소용으로 남긴다.
	if (Hovered && !bMoving)
	{
		AHGFurniture* Picked = Hovered;
		SetHovered(nullptr);
		bMoving = true;
		MovingId = Picked->FurnitureId;
		MovingFrom = Picked->GetActorTransform();
		HeldYaw = SnapYaw(MovingFrom.Rotator().Yaw);
		const FString Name = Picked->GetDisplayName().ToString();
		Picked->Destroy();
		RefreshOccupiedCells();
		RespawnGhost();
		UpdateCursor();
		return FString::Printf(TEXT("집었다: %s. 클릭으로 놓기, X로 취소"), *Name);
	}

	if (!Ghost || Ghost->IsHidden() || !bGhostValid)
	{
		return LastRejectReason.IsEmpty()
			? FString(TEXT("여기에는 놓을 수 없다"))
			: FString::Printf(TEXT("여기에는 놓을 수 없다: %s"), *LastRejectReason);
	}

	const FVector Location = Ghost->GetActorLocation();
	const float Yaw = Ghost->GetActorRotation().Yaw;
	const FIntRect Rect = HGShopGrid::GetFurnitureRect(Grid, Ghost);
	AHGFurniture* Placed = SpawnFurniture(GetHeldId(), Location, Yaw);
	if (!Placed)
	{
		return TEXT("가구를 만들지 못했다");
	}
	const bool bWasMoving = bMoving;
	if (bMoving)
	{
		bMoving = false;
		MovingId = NAME_None;
		RespawnGhost();
	}
	SaveLayout();
	RefreshOccupiedCells();

	// 새로 놓은 가구는 값을 낸다. 이미 놓인 가구를 옮긴 것은 공짜다 (위에서 bMoving을 끄기 전 값으로 판정).
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	FString PriceText;
	if (!bWasMoving && GI)
	{
		const int32 Price = Placed->GetData().Price;
		GI->AddMoney(-Price);
		PriceText = FString::Printf(TEXT(", -%d, 가진 돈 %d"), Price, GI->GetMoney());
	}

	FString Result = FString::Printf(TEXT("놓았다: %s (칸 %d,%d, 방향 %.0f%s)"), *Placed->GetDisplayName().ToString(), Rect.Min.X, Rect.Min.Y, Yaw, *PriceText);

	// 이 가구가 닿는(또는 이 가구 자신인) 식탁에서 처음 만든 콤보를 기록하고 알린다.
	if (const UDataTable* Table = GI ? GI->GetComboTable() : nullptr)
	{
		TArray<FHGGridItem> Items;
		HGShopGrid::GatherItems(GetWorld(), Grid, Items);
		TArray<FString> Discovered;
		for (const FHGGridItem& Item : Items)
		{
			if (!IsTableItem(Item) || !(Item.Actor.Get() == Placed || FHGShopGrid::Touches(Item.Rect, Rect)))
			{
				continue;
			}
			const FHGComboResult Combos = HGCombo::EvaluateTable(Table, Item.Rect, Items);
			for (int32 i = 0; i < Combos.Ids.Num(); ++i)
			{
				if (GI->DiscoverCombo(Combos.Ids[i]))
				{
					Discovered.Add(Combos.Names[i]);
				}
			}
		}
		if (!Discovered.IsEmpty())
		{
			Result += FString::Printf(TEXT(" | 새 콤보 발견: %s!"), *FString::Join(Discovered, TEXT(", ")));
			UE_LOG(LogHuoguo, Log, TEXT("[Combo] 새 콤보 발견: %s (발견한 콤보 %d개)"), *FString::Join(Discovered, TEXT(", ")), GI->GetDiscoveredCombos().Num());
		}
	}
	return Result;
}

FString UHGFurniturePlacer::RemoveOrCancel()
{
	if (!bBuilding)
	{
		return FString();
	}
	if (bMoving)
	{
		CancelMove();
		return TEXT("옮기기를 취소하고 원래 자리에 되돌렸다");
	}

	UpdateCursor();
	if (!Hovered)
	{
		return TEXT("치울 가구를 커서로 가리킨다");
	}
	AHGFurniture* Target = Hovered;
	SetHovered(nullptr);
	const FString Name = Target->GetDisplayName().ToString();
	const int32 Refund = Target->GetData().Price / 2;
	Target->Destroy();
	SaveLayout();
	RefreshOccupiedCells();

	// 치우면 값의 절반을 돌려받는다 (카이로식 되팔기).
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	if (GI)
	{
		GI->AddMoney(Refund);
	}
	return FString::Printf(TEXT("치웠다: %s (+%d 돌려받음, 가진 돈 %d)"), *Name, Refund, GI ? GI->GetMoney() : 0);
}

bool UHGFurniturePlacer::CanAffordHeld() const
{
	// 옮기는 중인 가구는 이미 산 것이라 따지지 않는다.
	if (bMoving)
	{
		return true;
	}
	const FHGFurnitureRow* Row = HGFurnitureData::FindRow(SelectedId);
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	if (!Row || !GI)
	{
		return true;
	}
	if (!HGFurnitureUnlock::IsUnlocked(GI, SelectedId, *Row))
	{
		LastRejectReason = HGFurnitureUnlock::DescribeLock(GI, SelectedId, *Row);
		return false;
	}
	if (GI->GetMoney() < Row->Price)
	{
		LastRejectReason = FString::Printf(TEXT("돈이 모자람 (값 %d, 가진 돈 %d)"), Row->Price, GI->GetMoney());
		return false;
	}
	return true;
}

FString UHGFurniturePlacer::TryUnlockHeld()
{
	const FName Id = GetHeldId();
	const FHGFurnitureRow* Row = HGFurnitureData::FindRow(Id);
	if (!Row)
	{
		return TEXT("손에 든 가구가 없다");
	}
	FString Message;
	HGFurnitureUnlock::TryUnlock(GetWorld()->GetGameInstance<UHGGameInstance>(), Id, *Row, Message);
	UpdateCursor();
	return Message;
}

// ---------------------------------------------------------------------------
// 개발용 설명
// ---------------------------------------------------------------------------

FString UHGFurniturePlacer::DescribeLayout() const
{
	const UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		return TEXT("GameInstance 없음");
	}
	const TArray<FHGPlacedFurniture>& Layout = GI->GetShopLayout();
	FString Text = FString::Printf(TEXT("가구 %d개"), Layout.Num());
	for (int32 i = 0; i < Layout.Num(); ++i)
	{
		const FHGPlacedFurniture& Item = Layout[i];
		const FHGFurnitureRow* Row = HGFurnitureData::FindRow(Item.FurnitureId);
		const FIntRect Rect = Row ? Grid.RectFromCenter(Item.Location, FHGShopGrid::RotateSize(Row->GetFootprint(), Item.Yaw)) : FIntRect();
		Text += FString::Printf(TEXT("\n  %d) %s [%s] (%.0f, %.0f) 칸 %d,%d~%d,%d 방향 %.0f"), i + 1,
			Row ? *Row->DisplayName.ToString() : TEXT("?"), *Item.FurnitureId.ToString(), Item.Location.X, Item.Location.Y,
			Rect.Min.X, Rect.Min.Y, Rect.Max.X - 1, Rect.Max.Y - 1, Item.Yaw);
	}
	return Text;
}

FString UHGFurniturePlacer::DescribeCursor() const
{
	if (!bBuilding)
	{
		return TEXT("배치 모드 아님");
	}
	const FString GhostText = (Ghost && !Ghost->IsHidden())
		? FString::Printf(TEXT("%s 유령 (%.0f, %.0f) 방향 %.0f %s"), *GetHeldName().ToString(),
			Ghost->GetActorLocation().X, Ghost->GetActorLocation().Y, Ghost->GetActorRotation().Yaw,
			bGhostValid ? TEXT("놓을 수 있음") : *FString::Printf(TEXT("놓을 수 없음(%s)"), *LastRejectReason))
		: FString(TEXT("유령 숨김"));
	return FString::Printf(TEXT("%s, 가리킨 가구 %s, 옮기는 중 %s, 미리보기 \"%s\""), *GhostText,
		Hovered ? *Hovered->GetDisplayName().ToString() : TEXT("없음"), bMoving ? TEXT("예") : TEXT("아니오"), *PreviewText);
}
