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
		return Item.Actor.IsValid() && Item.Actor->IsA<AHGTable>();
	}
}

UHGFurniturePlacer::UHGFurniturePlacer()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 기본 목록: 1 = 식탁(2x2), 2 = 의자(1x1), 3 = 화분(1x1), 4 = 등불(1x1), 5 = 소스 바(2x1).
	Catalog.Add(AHGTable::StaticClass());
	Catalog.Add(AHGChair::StaticClass());
	Catalog.Add(AHGPlanter::StaticClass());
	Catalog.Add(AHGLantern::StaticClass());
	Catalog.Add(AHGSauceBar::StaticClass());
}

APlayerController* UHGFurniturePlacer::GetPC() const
{
	return Cast<APlayerController>(GetOwner());
}

TSubclassOf<AHGFurniture> UHGFurniturePlacer::GetHeldClass() const
{
	if (bMoving)
	{
		return MovingClass;
	}
	return Catalog.IsValidIndex(CatalogIndex) ? Catalog[CatalogIndex] : nullptr;
}

FText UHGFurniturePlacer::GetHeldName() const
{
	const TSubclassOf<AHGFurniture> Class = GetHeldClass();
	return Class ? Class->GetDefaultObject<AHGFurniture>()->DisplayName : FText::GetEmpty();
}

// ---------------------------------------------------------------------------
// 배치 기록
// ---------------------------------------------------------------------------

void UHGFurniturePlacer::RestoreLayout()
{
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		UE_LOG(LogHuoguo, Warning, TEXT("GameInstance가 UHGGameInstance가 아니라서 가구 배치를 기록하지 못한다"));
		return;
	}

	if (!GI->HasShopLayout())
	{
		// 게임을 켜고 처음 정거장에 왔다. 맵에 배치된 가구가 처음 배치다. 칸에 맞춰 옮긴 뒤 기록한다.
		SnapMapFurnitureToGrid();
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
		if (Item.Class && Item.Class->IsChildOf(AHGFurniture::StaticClass()))
		{
			Restored += SpawnFurniture(Item.Class.Get(), Item.Location, Item.Yaw) ? 1 : 0;
		}
	}
	UE_LOG(LogHuoguo, Log, TEXT("가구 배치: 기록에서 %d개를 다시 놓음 (맵의 처음 가구 %d개는 치움)"), Restored, MapFurniture.Num());
}

void UHGFurniturePlacer::SnapMapFurnitureToGrid()
{
	// 맵 에셋의 가구는 칸 배치 이전의 자유 좌표에 놓여 있다. 맵을 고치지 않고 실행할 때 가장 가까운 칸으로 옮긴다.
	// 식탁 같은 일반 가구를 먼저 맞추고, 의자(bFacesTable)는 식탁이 자리 잡은 뒤에 맞춰서 식탁 쪽으로 돌린다.
	TArray<AHGFurniture*> First;
	TArray<AHGFurniture*> Second;
	for (TActorIterator<AHGFurniture> It(GetWorld()); It; ++It)
	{
		(It->bFacesTable ? Second : First).Add(*It);
	}

	TArray<FHGGridItem> Placed;
	int32 Moved = 0;
	int32 Removed = 0;
	auto SnapOne = [&](AHGFurniture* Furniture)
	{
		float Yaw = SnapYaw(Furniture->GetActorRotation().Yaw);
		FIntRect Rect = Grid.SnapRect(Furniture->GetActorLocation(), FHGShopGrid::RotateSize(Furniture->Footprint, Yaw));
		if (Furniture->bFacesTable)
		{
			Yaw = FaceTableYaw(Rect, Placed, Yaw);
		}
		const FVector OldLocation = Furniture->GetActorLocation();
		Furniture->SetActorLocationAndRotation(Grid.RectCenter(Rect), FRotator(0.f, Yaw, 0.f));
		if (!CheckPlacement(Furniture, Rect, Placed))
		{
			UE_LOG(LogHuoguo, Warning, TEXT("가구 배치: 맵의 %s (%.0f, %.0f)를 칸에 맞출 수 없어 치움 (%s)"),
				*Furniture->DisplayName.ToString(), OldLocation.X, OldLocation.Y, *LastRejectReason);
			Furniture->Destroy();
			++Removed;
			return;
		}
		FHGGridItem& Item = Placed.AddDefaulted_GetRef();
		Item.Actor = Furniture;
		Item.Id = Furniture->FurnitureId;
		Item.Rect = Rect;
		++Moved;
	};
	for (AHGFurniture* Furniture : First)
	{
		SnapOne(Furniture);
	}
	for (AHGFurniture* Furniture : Second)
	{
		SnapOne(Furniture);
	}
	UE_LOG(LogHuoguo, Log, TEXT("가구 배치: 맵의 가구 %d개를 칸에 맞춤, %d개는 치움"), Moved, Removed);
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
		Item.Class = It->GetClass();
		Item.Location = It->GetActorLocation();
		Item.Yaw = It->GetActorRotation().Yaw;
	}
	GI->SetShopLayout(Layout);
}

AHGFurniture* UHGFurniturePlacer::SpawnFurniture(TSubclassOf<AHGFurniture> Class, const FVector& Location, float Yaw) const
{
	if (!Class)
	{
		return nullptr;
	}
	// 자리 검사는 이미 CheckPlacement에서 끝냈다. 엔진의 스폰 충돌 처리가 위치를 몰래 옮기지 않도록 항상 그 자리에 만든다.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<AHGFurniture>(Class, Location, FRotator(0.f, Yaw, 0.f), Params);
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
	bBuilding = false;
}

bool UHGFurniturePlacer::SelectCatalog(int32 Index)
{
	if (!Catalog.IsValidIndex(Index) || !Catalog[Index])
	{
		return false;
	}
	CancelMove();
	CatalogIndex = Index;
	if (bBuilding)
	{
		RespawnGhost();
	}
	return true;
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
	const TSubclassOf<AHGFurniture> Class = GetHeldClass();
	if (!Class)
	{
		return;
	}

	// 미뤄서 스폰(SpawnActorDeferred)하고, 스폰을 끝내기(FinishSpawning) 전에 유령으로 바꾼다.
	// 그래야 충돌이 켜진 실제 가구 상태로 한 프레임도 존재하지 않는다. 처음에는 숨겨 두고 커서 위치가 정해지면 보인다.
	const FTransform SpawnTransform(FRotator(0.f, HeldYaw, 0.f), FVector(0.f, 0.f, Grid.FloorZ));
	Ghost = GetWorld()->SpawnActorDeferred<AHGFurniture>(Class, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Ghost)
	{
		Ghost->MakeGhost();
		Ghost->SetActorHiddenInGame(true);
		Ghost->FinishSpawning(SpawnTransform);
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
	SpawnFurniture(MovingClass, MovingFrom.GetLocation(), MovingFrom.Rotator().Yaw);
	bMoving = false;
	MovingClass = nullptr;
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
	const FIntRect Rect = Grid.CursorRect(Point, FHGShopGrid::RotateSize(Ghost->Footprint, Yaw));
	if (Ghost->bFacesTable)
	{
		Yaw = FaceTableYaw(Rect, Items, HeldYaw);
	}

	Ghost->SetActorLocationAndRotation(Grid.RectCenter(Rect), FRotator(0.f, Yaw, 0.f));
	Ghost->SetActorHiddenInGame(false);
	bGhostValid = CheckPlacement(Ghost, Rect, Items);
	Ghost->SetPreviewState(bGhostValid ? EHGFurniturePreview::Valid : EHGFurniturePreview::Invalid);
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
			LastRejectReason = FString::Printf(TEXT("칸이 겹침: %s"), Item.Actor.IsValid() ? *Item.Actor->DisplayName.ToString() : *Item.Id.ToString());
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
		MovingClass = Picked->GetClass();
		MovingFrom = Picked->GetActorTransform();
		HeldYaw = SnapYaw(MovingFrom.Rotator().Yaw);
		const FString Name = Picked->DisplayName.ToString();
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
	AHGFurniture* Placed = SpawnFurniture(GetHeldClass(), Location, Yaw);
	if (!Placed)
	{
		return TEXT("가구를 만들지 못했다");
	}
	if (bMoving)
	{
		bMoving = false;
		MovingClass = nullptr;
		RespawnGhost();
	}
	SaveLayout();
	RefreshOccupiedCells();

	return FString::Printf(TEXT("놓았다: %s (칸 %d,%d, 방향 %.0f)"), *Placed->DisplayName.ToString(), Rect.Min.X, Rect.Min.Y, Yaw);
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
	const FString Name = Target->DisplayName.ToString();
	Target->Destroy();
	SaveLayout();
	RefreshOccupiedCells();
	return FString::Printf(TEXT("치웠다: %s"), *Name);
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
		const AHGFurniture* Default = Item.Class ? Cast<AHGFurniture>(Item.Class->GetDefaultObject()) : nullptr;
		const FIntRect Rect = Default ? Grid.RectFromCenter(Item.Location, FHGShopGrid::RotateSize(Default->Footprint, Item.Yaw)) : FIntRect();
		Text += FString::Printf(TEXT("\n  %d) %s (%.0f, %.0f) 칸 %d,%d~%d,%d 방향 %.0f"), i + 1,
			Default ? *Default->DisplayName.ToString() : TEXT("?"), Item.Location.X, Item.Location.Y,
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
	return FString::Printf(TEXT("%s, 가리킨 가구 %s, 옮기는 중 %s"), *GhostText,
		Hovered ? *Hovered->DisplayName.ToString() : TEXT("없음"), bMoving ? TEXT("예") : TEXT("아니오"));
}
