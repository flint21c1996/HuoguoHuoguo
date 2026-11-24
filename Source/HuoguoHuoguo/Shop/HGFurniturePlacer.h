#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Shop/HGShopGrid.h"
#include "HGFurniturePlacer.generated.h"

class AHGFurniture;
class APlayerController;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * 경영 파트의 가구 배치 모드. 경영 컨트롤러에 붙는 컴포넌트다.
 *
 * 배치는 카이로소프트식 칸(그리드) 배치다 (규칙은 FHGShopGrid).
 *   - 배치 모드에 들어가면 가구를 놓을 수 있는 칸(배치 영역)이 바닥에 칸 무늬로 보인다.
 *   - 커서가 가리키는 바닥 지점에 "유령" 가구를 칸에 맞춰 띄운다. 놓을 수 있으면 초록, 없으면 빨강.
 *   - 클릭하면 그 자리에 실제 가구를 놓는다.
 *   - 커서가 이미 놓인 가구를 가리키면 그 가구가 주황으로 바뀌고, 클릭하면 집어 들어 옮길 수 있다.
 *   - 지우기 키: 옮기던 가구는 원래 자리로 되돌리고, 가리킨 가구는 없앤다.
 *   - 의자는 식탁 바로 옆 칸에 대면 저절로 식탁 쪽으로 돌아앉는다.
 *   - 유령 자리에 놓으면 생길 콤보를 미리 알려 준다 (GetPreviewText).
 *
 * 놓을 수 있는 자리 판정 (CheckPlacement):
 *   1. 칸: 가구의 칸 사각형이 배치 영역 안에 있고, 다른 가구의 칸과 겹치지 않아야 한다.
 *   2. 겹침: 부품의 충돌 모양이 벽, 카운터, 사장님, 발판 같은 가구 아닌 것과 겹치지 않아야 한다.
 *
 * 배치 기록:
 *   가구를 놓거나 없앨 때마다 맵에 있는 모든 가구(종류, 위치, 방향)를 GameInstance에 적는다.
 *   행성에 다녀와 정거장 맵이 다시 열리면 RestoreLayout이 맵에 배치된 처음 가구를 지우고 기록대로 다시 놓는다.
 *   기록이 아직 없으면(게임 시작 후 처음 정거장) 맵에 배치된 가구를 칸에 맞춰 옮긴 뒤 그것을 처음 배치로 기록한다.
 *   기록은 게임이 켜져 있는 동안만 남는다. 파일 저장은 저장 기능을 만들 때 붙인다.
 */
UCLASS(ClassGroup = (Huoguo), meta = (BlueprintSpawnableComponent))
class HUOGUOHUOGUO_API UHGFurniturePlacer : public UActorComponent
{
	GENERATED_BODY()

public:
	UHGFurniturePlacer();

	// 가게 바닥 칸 규칙 (칸 크기, 바닥 범위, 배치 영역, 손님 입구).
	UPROPERTY(EditAnywhere, Category = "Furniture")
	FHGShopGrid Grid;

	// R 한 번에 도는 각도. 칸 배치라서 90도 단위다.
	UPROPERTY(EditAnywhere, Category = "Furniture")
	float RotateStepDegrees = 90.f;

	// 맵 시작에 한 번 부른다. 위의 "배치 기록" 설명 참고.
	void RestoreLayout();

	void BeginBuild();
	// 배치 모드를 끝낸다. 옮기던 가구가 있으면 원래 자리로 되돌린다.
	void EndBuild();
	bool IsBuilding() const { return bBuilding; }

	// ---- 가구 고르기 ----
	// 고를 수 있는 가구 = 가구 DataTable(Data/Furniture.csv)의 모든 행, 행 순서대로. 처음 부를 때 불러온다.
	const TArray<FName>& GetCatalog();

	// 분류를 고른다 (숫자 키 1 식탁, 2 의자, 3 장식). 그 분류에서 마지막으로 골랐던 가구(처음이면 첫 가구)를 든다.
	bool SelectCategory(EHGFurnitureCategory Category);

	// 같은 분류 안에서 이전/다음 가구 (Z/C). Direction = -1 / +1, 끝에서 돌아간다.
	void CycleInCategory(int32 Direction);

	// ID로 가구를 고른다 (개발 명령). 목록에 없으면 false.
	bool SelectFurniture(FName FurnitureId);

	// 위 셋 모두 옮기던 가구가 있으면 원래 자리로 되돌린 뒤 바꾼다.

	// 지금 손에 든 가구 ID: 옮기는 중이면 그 가구, 아니면 고른 가구.
	FName GetHeldId() const;

	// 손에 든 가구 한 줄 설명: "원앙 냄비 원탁 (식탁 2/6, 2x2, 값 180, 손님 +10%)".
	FString DescribeHeld();

	// 손에 든 가구(유령)를 돌린다.
	void RotateHeld(float Degrees);

	// 매 프레임: 커서가 가리키는 곳으로 유령을 옮기고, 놓을 수 있는지 판정하고, 가리킨 가구를 표시한다.
	void UpdateCursor();

	// 클릭. 결과를 화면에 낼 문장으로 돌려준다.
	FString Click();

	// 지우기 키. 결과 문장을 돌려준다 (할 일이 없으면 빈 문자열).
	FString RemoveOrCancel();

	// 시험용: 마우스 대신 이 월드 바닥 지점을 커서로 쓴다. 매 프레임 이 지점을 화면 좌표로 바꾼 뒤
	// 마우스와 같은 경로(화면 좌표 -> 광선 -> 바닥)로 처리하므로 화면 좌표 변환까지 함께 검사된다. unset이면 실제 마우스.
	void SetCursorWorldOverride(const TOptional<FVector>& WorldPoint) { CursorWorldOverride = WorldPoint; }

	// 지금 손에 든 가구 이름 (화면 안내용).
	FText GetHeldName() const;

	// 유령 자리에 놓으면 생길 콤보 안내 ("이 자리: 초록 쉼터 +20%"). 없거나 놓을 수 없으면 빈 문자열.
	const FString& GetPreviewText() const { return PreviewText; }

	// 가구 배치 기록을 사람이 읽을 수 있게 줄 단위로 만든다 (개발 명령 HGFurniture).
	FString DescribeLayout() const;

	// 지금 상태 요약 한 줄: 유령 위치, 놓을 수 있는지, 가리킨 가구 (시험 로그용).
	FString DescribeCursor() const;

private:
	// 겹침 검사 때 부품을 바닥에서 띄우는 높이(cm). 바닥에 맞닿은 다리/받침이 바닥 자체와 겹친다고 판정되지 않게 한다.
	static constexpr float PlacementLift = 2.f;

	// 마지막 배치 검사에서 놓을 수 없었던 이유 (메시지와 시험 로그용).
	mutable FString LastRejectReason;

	FString PreviewText;

	UPROPERTY() TObjectPtr<AHGFurniture> Ghost;
	UPROPERTY() TObjectPtr<AHGFurniture> Hovered;

	// 배치 모드 동안 바닥에 깔리는 칸 무늬. 액터 하나에 인스턴스 메시 세 장 (ShowGridOverlay 설명 참고).
	UPROPERTY() TObjectPtr<AActor> GridOverlay;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GridTiles;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> OccupiedTiles;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> HighlightTiles;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HighlightMaterial;

	// 칸 판을 바닥 윗면에서 띄우는 높이. 겹치는 판끼리 깜빡이지 않게 층마다 다르다.
	static constexpr float GridLift = 0.6f;
	static constexpr float OccupiedLift = 1.0f;
	static constexpr float HighlightLift = 1.4f;

	bool bHighlightShown = false;
	FIntRect HighlightRect;
	FLinearColor HighlightColor = FLinearColor::White;

	bool bBuilding = false;
	bool bGhostValid = false;
	float HeldYaw = 0.f;

	// 가구 목록 (DataTable 행 이름, 행 순서). 표를 UPROPERTY로 잡아 두어 게임 중에 내려가지 않게 한다.
	UPROPERTY() TObjectPtr<const UDataTable> FurnitureTable;
	TArray<FName> Catalog;
	bool bCatalogLoaded = false;

	// 고른 가구와, 분류마다 마지막으로 골랐던 가구 (분류 키를 다시 누르면 그 가구로 돌아간다).
	FName SelectedId;
	TMap<EHGFurnitureCategory, FName> LastInCategory;

	// 이미 놓인 가구를 집어 옮기는 중인지와 원래 자리. 취소하면 원래 자리에 다시 놓는다.
	bool bMoving = false;
	FName MovingId;
	FTransform MovingFrom;

	TOptional<FVector> CursorWorldOverride;

	APlayerController* GetPC() const;

	// 분류의 가구 ID들 (행 순서).
	TArray<FName> GetCategoryItems(EHGFurnitureCategory Category);

	// 손에 든 것을 바꾼다 (옮기던 가구는 되돌리고 유령을 새로 만든다).
	void HoldFurniture(FName FurnitureId);

	// 커서 위치에서 화면 안쪽으로 나가는 광선. 실패하면 false.
	bool GetCursorRay(FVector& OutOrigin, FVector& OutDirection) const;

	void RespawnGhost();
	void DestroyGhost();
	void SetHovered(AHGFurniture* NewHovered);

	void ShowGridOverlay();
	void HideGridOverlay();

	// 칸 사각형의 칸마다 판 하나씩 Tiles에 더한다. Fill = 칸 대비 판 크기 (틈을 남겨 칸 경계가 보이게).
	void AddCellInstances(UInstancedStaticMeshComponent* Tiles, const FIntRect& Rect, float Lift, float Fill) const;

	// 놓인 가구가 차지한 칸 표시를 다시 만든다. 가구를 놓고, 집고, 치울 때 부른다.
	void RefreshOccupiedCells();

	// 손에 든(또는 가리킨) 가구가 차지할 칸을 그 색으로 칠한다. ClearHighlight로 지운다.
	void SetHighlight(const FIntRect& Rect, const FLinearColor& Color);
	void ClearHighlight();

	// 옮기던 가구를 원래 자리에 다시 놓고 옮기기를 끝낸다.
	void CancelMove();

	// Rect 자리에 놓을 의자가 바라볼 방향. 상하좌우 바로 옆에 식탁이 있으면 그쪽, 없으면 Fallback.
	float FaceTableYaw(const FIntRect& Rect, const TArray<FHGGridItem>& Items, float Fallback) const;

	// Candidate가 칸 사각형 Rect를 차지할 때 놓을 수 있는가. Items는 맵의 다른 가구.
	bool CheckPlacement(const AHGFurniture* Candidate, const FIntRect& Rect, const TArray<FHGGridItem>& Items) const;

	// 유령(Id, Rect)을 놓으면 생길 콤보 안내 문장.
	FString BuildPreviewText(FName Id, const FIntRect& Rect, const TArray<FHGGridItem>& Items) const;

	// 가구 ID로 실제 가구(또는 bGhost면 유령)를 만든다. 클래스는 행의 ActorClass, 비었으면 AHGFurniture.
	AHGFurniture* SpawnFurniture(FName FurnitureId, const FVector& Location, float Yaw, bool bGhost = false) const;

	// 맵에 처음 배치된 가구를 ID와 자리만 읽고 지운 뒤, 데이터로 만든 가구를 가장 가까운 칸에 다시 놓는다.
	// 맞출 수 없는 가구(영역 밖, 칸 겹침)는 놓지 않는다.
	void ReplaceMapFurniture();

	// 맵의 실제 가구 전부를 GameInstance에 기록한다.
	void SaveLayout() const;
};
