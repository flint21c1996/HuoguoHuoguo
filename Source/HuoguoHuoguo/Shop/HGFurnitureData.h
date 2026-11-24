#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "HGFurnitureData.generated.h"

class AHGFurniture;

/** 가구 분류. 분류마다 게임에서 하는 일이 다르다. */
UENUM(BlueprintType)
enum class EHGFurnitureCategory : uint8
{
	Table,  // 식탁: 냄비가 있고, 바라보는 의자가 자리다. 손님이 이 식탁에서 먹는다.
	Chair,  // 의자: 식탁을 바라보면 손님 자리. 식탁 옆에 대면 저절로 식탁을 본다.
	Decor   // 장식: 식탁 둘레에 두면 콤보 재료가 된다.
};

/**
 * 가구 한 종류의 데이터 (DataTable 한 줄). 원본은 저장소의 `Data/Furniture.csv`다 (엑셀로 열어 고칠 수 있다).
 * `Tools/GenerateGreyboxMaps.py`가 `/Game/HG/Data/DT_Furniture`로 읽어 들인다.
 *
 * 행 이름(첫 열 Name)이 가구 ID다. 배치 기록과 콤보 규칙(Data/Combos.csv)이 이 ID로 가구를 가리킨다.
 * 행 순서가 배치 모드의 고르기 순서다 (분류마다 위에서 아래로).
 *
 * 생김새: 블루프린트 아트가 생기기 전까지는 Category + Style로 C++이 기본 도형을 조합해 만든다 (AHGFurniture::BuildParts).
 * ActorClass에 블루프린트(BP_...)를 적으면 그 클래스로 만든다. 비우면 AHGFurniture 기본 클래스.
 */
USTRUCT(BlueprintType)
struct HUOGUOHUOGUO_API FHGFurnitureRow : public FTableRowBase
{
	GENERATED_BODY()

	// 화면에 보이는 이름 ("원앙 냄비 원탁").
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	EHGFurnitureCategory Category = EHGFurnitureCategory::Decor;

	// 생김새 종류. 분류마다 쓸 수 있는 값이 다르다 (AHGFurniture::BuildParts 설명, Docs/Content/Data.md 표).
	// 식탁: Round, Square / 의자: Stool, Wood, Cushion, Arm / 장식: Bush, Bamboo, Lantern, PaperLantern, SauceBar, Tank, Screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	FName Style;

	// 식탁의 훠궈 냄비 종류: Single(단일), YinYang(원앙, 홍탕/백탕 반반), NineGrid(구궁격), Big(큰 냄비). 식탁이 아니면 비운다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	FName Pot;

	// 차지하는 칸 수 (회전 0도 기준, X = 가구 앞쪽).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	int32 SizeX = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	int32 SizeY = 1;

	// 사는 값 (아직 쓰지 않는다. 가게 키우기/가구 사기가 생기면 쓴다).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	int32 Price = 0;

	// 이 가구가 손님 값에 더하는 %. 식탁: 그 식탁 손님 모두, 의자: 그 의자에 앉은 손님. 장식은 0 (장식은 콤보로 더한다).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	int32 BonusPercent = 0;

	// 콤보 규칙이 쓰는 꼬리표. '|'로 여러 개 (예: "Plant|Planet"). 콤보 Needs는 ID나 꼬리표를 쓸 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	FString Tags;

	// 주 색과 보조 색 (#RRGGBB). 기본 도형 생김새에 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	FString ColorA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	FString ColorB;

	// 이 행을 만들 액터 클래스 (블루프린트 아트용). 비우면 AHGFurniture.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	TSoftClassPtr<AHGFurniture> ActorClass;

	// 설명 한 줄 (나중에 가구 목록 화면에 쓴다).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furniture")
	FText Description;

	FIntPoint GetFootprint() const { return FIntPoint(FMath::Max(1, SizeX), FMath::Max(1, SizeY)); }
};

namespace HGFurnitureData
{
	// 가구 DataTable 경로.
	inline const TCHAR* TablePath = TEXT("/Game/HG/Data/DT_Furniture.DT_Furniture");

	// 가구 DataTable을 불러온다 (이미 메모리에 있으면 그것). 없으면 nullptr. 에디터(생성 스크립트)와 게임 어디서나 쓸 수 있다.
	HUOGUOHUOGUO_API const UDataTable* LoadTable();

	// ID로 한 줄. 없으면 nullptr.
	HUOGUOHUOGUO_API const FHGFurnitureRow* FindRow(FName FurnitureId);

	// "#RRGGBB" -> 색. 비었거나 틀리면 Fallback.
	HUOGUOHUOGUO_API FLinearColor ParseColor(const FString& Hex, const FLinearColor& Fallback);

	// "Plant|Light" -> {Plant, Light}.
	HUOGUOHUOGUO_API TArray<FName> ParseTags(const FString& Tags);

	HUOGUOHUOGUO_API const TCHAR* CategoryName(EHGFurnitureCategory Category);
}
