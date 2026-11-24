#pragma once

#include "CoreMinimal.h"
#include "Shop/HGFurnitureData.h"
#include "HGShopGrid.generated.h"

class AHGFurniture;
class UWorld;

/**
 * 가게 바닥의 칸(그리드) 규칙.
 *
 * 카이로소프트식 배치: 가구는 칸 단위로만 놓이고, 가구마다 차지하는 칸 수(발자국)가 있다 (식탁 2x2, 의자 1x1).
 * 칸 번호 (X, Y)는 월드 좌표를 TileSize로 나눠 내림한 값이다. 칸 (0, 0)은 월드 X 0~100, Y 0~100.
 * 칸 사각형(FIntRect)은 Min 포함, Max 제외다. 2x2 식탁이 칸 (-4, -3)부터 놓이면 Min (-4, -3), Max (-2, -1).
 *
 * 영역 두 가지
 *   - 바닥(Floor): 손님이 걸을 수 있는 가게 바닥 전체.
 *   - 배치 영역(Build): 가구를 놓을 수 있는 칸. 바닥보다 작다 (주방, 에어록, 입구 칸은 빠진다).
 *     가게를 키우면 이 영역이 넓어진다 (아직 없음).
 */
USTRUCT()
struct HUOGUOHUOGUO_API FHGShopGrid
{
	GENERATED_BODY()

	// 칸 한 변(cm).
	UPROPERTY(EditAnywhere, Category = "Grid")
	float TileSize = 100.f;

	// 가게 바닥 윗면 높이.
	UPROPERTY(EditAnywhere, Category = "Grid")
	float FloorZ = 0.f;

	// 가게 바닥 전체 칸 범위 (Min 포함, Max 제외). L_Station 바닥 12m x 10m = 칸 (-6, -5) ~ (5, 4).
	UPROPERTY(EditAnywhere, Category = "Grid")
	FIntPoint FloorMin = FIntPoint(-6, -5);

	UPROPERTY(EditAnywhere, Category = "Grid")
	FIntPoint FloorMax = FIntPoint(6, 5);

	// 가구를 놓을 수 있는 칸 범위 (Min 포함, Max 제외). 기본: 홀 9 x 8칸 (월드 X -500 ~ 400, Y -500 ~ 300).
	// 서쪽 한 줄(입구 통로), 북쪽 주방, 동쪽 진열대 앞은 빠진다.
	UPROPERTY(EditAnywhere, Category = "Grid")
	FIntPoint BuildMin = FIntPoint(-5, -5);

	UPROPERTY(EditAnywhere, Category = "Grid")
	FIntPoint BuildMax = FIntPoint(4, 3);

	// 손님이 나타나고 사라지는 칸 (가게 남서쪽 구석).
	UPROPERTY(EditAnywhere, Category = "Grid")
	FIntPoint EntranceTile = FIntPoint(-6, -5);

	// 월드 지점이 들어 있는 칸.
	FIntPoint WorldToTile(const FVector& Point) const;

	// 칸 가운데의 바닥 지점.
	FVector TileCenter(const FIntPoint& Tile) const;

	// 칸 사각형 가운데의 바닥 지점 (가구 원점이 놓일 자리).
	FVector RectCenter(const FIntRect& Rect) const;

	// 커서 지점에 가장 가깝게, 크기 Size(칸)인 가구를 놓을 때의 칸 사각형.
	// 가구 가운데가 커서에 가장 가까워지도록 고른다. 짝수 크기는 칸 경계에, 홀수 크기는 칸 가운데에 중심이 온다.
	FIntRect SnapRect(const FVector& Point, const FIntPoint& Size) const;

	// 가구 원점(가운데)과 크기로 그 가구가 차지한 칸 사각형을 구한다. SnapRect와 같은 계산이다.
	FIntRect RectFromCenter(const FVector& Center, const FIntPoint& Size) const { return SnapRect(Center, Size); }

	// 배치 커서용: 커서가 가리키는 칸을 반드시 포함하는 칸 사각형. 커서 칸이 가구의 첫 칸(Min)이 되고 +X, +Y 쪽으로 뻗는다.
	// 홀수 크기(3칸 등)는 커서 칸이 가운데가 되도록 한 칸 당긴다. 칸 꼭짓점이 아니라 "가리킨 칸"이 기준이다.
	FIntRect CursorRect(const FVector& Point, const FIntPoint& Size) const;

	bool IsInBuildArea(const FIntRect& Rect) const;
	bool IsOnFloor(const FIntPoint& Tile) const;

	// 회전에 따라 발자국 크기를 돌린다. 90도, 270도면 가로세로가 바뀐다.
	static FIntPoint RotateSize(const FIntPoint& Size, float Yaw);

	// 두 칸 사각형이 칸을 함께 쓰는가.
	static bool Overlaps(const FIntRect& A, const FIntRect& B);

	// 두 사각형이 겹치지 않고 맞닿거나 대각선으로 붙어 있는가 (A를 한 칸 넓힌 둘레에 B가 걸친다). 콤보 판정에 쓴다.
	static bool Touches(const FIntRect& A, const FIntRect& B);

	// Tile이 Rect의 상하좌우 바로 옆 칸인가 (대각선 제외). 의자가 식탁의 자리인지 판정에 쓴다.
	static bool IsSideNeighbor(const FIntRect& Rect, const FIntPoint& Tile);
};

/** 칸 위에 놓인 가구 하나 (배치 검사, 콤보, 손님 자리 찾기에 쓰는 가벼운 사본). */
struct FHGGridItem
{
	TWeakObjectPtr<AHGFurniture> Actor;
	FName Id;
	EHGFurnitureCategory Category = EHGFurnitureCategory::Decor;
	TArray<FName> Tags;
	FIntRect Rect;

	bool IsTable() const { return Category == EHGFurnitureCategory::Table; }
	bool IsChair() const { return Category == EHGFurnitureCategory::Chair; }
};

namespace HGShopGrid
{
	// 가구 하나가 지금 차지한 칸 사각형.
	HUOGUOHUOGUO_API FIntRect GetFurnitureRect(const FHGShopGrid& Grid, const AHGFurniture* Furniture);

	// 가구 앞쪽(+X)으로 바로 한 칸 앞의 칸. 의자는 이 칸이 들어 있는 식탁의 자리다 (식탁 두 개 사이 의자는 바라보는 쪽 식탁).
	HUOGUOHUOGUO_API FIntPoint GetFrontTile(const FHGShopGrid& Grid, const AHGFurniture* Furniture);

	// 맵의 실제 가구(유령 제외)를 모두 모은다.
	HUOGUOHUOGUO_API void GatherItems(UWorld* World, const FHGShopGrid& Grid, TArray<FHGGridItem>& OutItems);
}
