#include "Shop/Core/HGShopGrid.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Shop/Furniture/HGFurniture.h"

FIntPoint FHGShopGrid::WorldToTile(const FVector& Point) const
{
	return FIntPoint(FMath::FloorToInt(Point.X / TileSize), FMath::FloorToInt(Point.Y / TileSize));
}

FVector FHGShopGrid::TileCenter(const FIntPoint& Tile) const
{
	return FVector((Tile.X + 0.5) * TileSize, (Tile.Y + 0.5) * TileSize, FloorZ);
}

FVector FHGShopGrid::RectCenter(const FIntRect& Rect) const
{
	return FVector((Rect.Min.X + Rect.Max.X) * 0.5 * TileSize, (Rect.Min.Y + Rect.Max.Y) * 0.5 * TileSize, FloorZ);
}

FIntRect FHGShopGrid::SnapRect(const FVector& Point, const FIntPoint& Size) const
{
	// 가구 가운데 = (Min + Size/2) * TileSize. 이것이 Point에 가장 가까운 정수 Min은
	// Point/TileSize - Size/2 를 반올림한 값이다 (반올림 = 0.5를 더하고 내림).
	const FIntPoint Min(
		FMath::FloorToInt(Point.X / TileSize - Size.X * 0.5 + 0.5),
		FMath::FloorToInt(Point.Y / TileSize - Size.Y * 0.5 + 0.5));
	return FIntRect(Min, Min + Size);
}

FIntRect FHGShopGrid::CursorRect(const FVector& Point, const FIntPoint& Size) const
{
	// (Size - 1) / 2: 1칸 -> 0, 2칸 -> 0, 3칸 -> 1. 짝수 크기는 커서 칸에서 시작하고, 홀수 크기는 커서 칸이 가운데다.
	const FIntPoint Cell = WorldToTile(Point);
	const FIntPoint Min(Cell.X - (Size.X - 1) / 2, Cell.Y - (Size.Y - 1) / 2);
	return FIntRect(Min, Min + Size);
}

bool FHGShopGrid::IsInBuildArea(const FIntRect& Rect) const
{
	return Rect.Min.X >= BuildMin.X && Rect.Min.Y >= BuildMin.Y && Rect.Max.X <= BuildMax.X && Rect.Max.Y <= BuildMax.Y;
}

bool FHGShopGrid::IsOnFloor(const FIntPoint& Tile) const
{
	return Tile.X >= FloorMin.X && Tile.Y >= FloorMin.Y && Tile.X < FloorMax.X && Tile.Y < FloorMax.Y;
}

FIntPoint FHGShopGrid::RotateSize(const FIntPoint& Size, float Yaw)
{
	// Yaw를 90도 단위로 반올림해서 홀수 번 돌았으면(90, 270) 가로세로를 바꾼다.
	const int32 Quarter = FMath::RoundToInt(Yaw / 90.f);
	return (Quarter % 2 != 0) ? FIntPoint(Size.Y, Size.X) : Size;
}

bool FHGShopGrid::Overlaps(const FIntRect& A, const FIntRect& B)
{
	return A.Min.X < B.Max.X && B.Min.X < A.Max.X && A.Min.Y < B.Max.Y && B.Min.Y < A.Max.Y;
}

bool FHGShopGrid::Touches(const FIntRect& A, const FIntRect& B)
{
	// A를 사방으로 한 칸 넓힌 사각형에 B가 걸치면 맞닿은 것이다 (대각선 포함). 서로 겹치면 맞닿은 것이 아니다.
	const FIntRect Ring(A.Min - FIntPoint(1, 1), A.Max + FIntPoint(1, 1));
	return Overlaps(Ring, B) && !Overlaps(A, B);
}

bool FHGShopGrid::IsSideNeighbor(const FIntRect& Rect, const FIntPoint& Tile)
{
	const bool bInX = Tile.X >= Rect.Min.X && Tile.X < Rect.Max.X;
	const bool bInY = Tile.Y >= Rect.Min.Y && Tile.Y < Rect.Max.Y;
	// 위아래로 붙음: X 범위 안에서 Y가 바로 바깥. 좌우로 붙음: Y 범위 안에서 X가 바로 바깥.
	return (bInX && (Tile.Y == Rect.Min.Y - 1 || Tile.Y == Rect.Max.Y))
		|| (bInY && (Tile.X == Rect.Min.X - 1 || Tile.X == Rect.Max.X));
}

FIntRect HGShopGrid::GetFurnitureRect(const FHGShopGrid& Grid, const AHGFurniture* Furniture)
{
	const FIntPoint Size = FHGShopGrid::RotateSize(Furniture->GetFootprint(), Furniture->GetActorRotation().Yaw);
	return Grid.RectFromCenter(Furniture->GetActorLocation(), Size);
}

FIntPoint HGShopGrid::GetFrontTile(const FHGShopGrid& Grid, const AHGFurniture* Furniture)
{
	// 앞쪽 방향을 네 방향 중 하나로 반올림한다 (Yaw 0 = +X, 90 = +Y, 180 = -X, -90 = -Y).
	const FVector Forward = Furniture->GetActorForwardVector();
	const FIntPoint Step = FMath::Abs(Forward.X) >= FMath::Abs(Forward.Y)
		? FIntPoint(Forward.X >= 0.0 ? 1 : -1, 0)
		: FIntPoint(0, Forward.Y >= 0.0 ? 1 : -1);
	return GetFurnitureRect(Grid, Furniture).Min + Step;
}

void HGShopGrid::GatherItems(UWorld* World, const FHGShopGrid& Grid, TArray<FHGGridItem>& OutItems)
{
	OutItems.Reset();
	// TActorIterator는 Destroy()로 없애는 중인 액터를 건너뛴다. 방금 집거나 치운 가구는 들어가지 않는다.
	for (TActorIterator<AHGFurniture> It(World); It; ++It)
	{
		if (It->IsGhost())
		{
			continue;
		}
		FHGGridItem& Item = OutItems.AddDefaulted_GetRef();
		Item.Actor = *It;
		Item.Id = It->FurnitureId;
		Item.Category = It->GetCategory();
		Item.Tags = It->GetTags();
		Item.Rect = GetFurnitureRect(Grid, *It);
	}
}
