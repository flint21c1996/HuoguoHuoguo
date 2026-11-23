#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Shop/HGShopGrid.h"
#include "HGShopCombos.generated.h"

/**
 * 배치 콤보 하나의 규칙 (DataTable 한 줄).
 *
 * 원본은 저장소의 `Data/Combos.csv`이고, `Tools/GenerateGreyboxMaps.py`가 `/Game/HG/Data/DT_Combos`로 읽어 들인다.
 * 행 이름(첫 열 Name)이 콤보 ID다. 발견한 콤보 기록(GameInstance)은 이 ID를 남긴다.
 *
 * 판정: 식탁 하나를 기준으로, 식탁 둘레 한 칸(대각선 포함) 안에 걸친 가구를 종류별로 센다.
 * Needs에 적힌 가구가 모두 그 수 이상 있으면 콤보가 성립하고, 그 식탁 손님이 BonusPercent만큼 돈을 더 낸다.
 * 한 식탁에 여러 콤보가 함께 성립하면 보너스를 모두 더한다 (콤보 쌓기).
 */
USTRUCT(BlueprintType)
struct HUOGUOHUOGUO_API FHGComboRow : public FTableRowBase
{
	GENERATED_BODY()

	// 화면에 보이는 이름 ("초록 쉼터").
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	FText DisplayName;

	// 식탁 둘레에 있어야 하는 가구 ID (AHGFurniture::FurnitureId). '+'로 여러 개, '*숫자'로 개수.
	// 예: "Planter" (화분 1개 이상), "Planter+Lantern" (화분과 등불 모두), "Planter*2" (화분 2개 이상).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	FString Needs;

	// 이 콤보가 성립한 식탁 손님이 더 내는 돈 (기본 값의 몇 %).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	int32 BonusPercent = 10;
};

/** 식탁 하나에 성립한 콤보들. */
struct FHGComboResult
{
	TArray<FName> Ids;
	TArray<FString> Names;
	int32 BonusPercent = 0;

	int32 Num() const { return Ids.Num(); }

	// "초록 쉼터 +20%, 등불 아래 +20%" 처럼 한 줄로. 콤보가 없으면 빈 문자열.
	FString Describe(const UDataTable* Table) const;
};

namespace HGCombo
{
	// 식탁 사각형 둘레(맞닿은 칸, 대각선 포함)에 걸친 가구를 종류별로 센다. 식탁 자신은 세지 않는다.
	HUOGUOHUOGUO_API TMap<FName, int32> CountAround(const FIntRect& TableRect, const TArray<FHGGridItem>& Items);

	// 센 가구로 성립하는 콤보를 표의 행 순서대로 모은다. 표가 없으면 빈 결과.
	HUOGUOHUOGUO_API FHGComboResult Evaluate(const UDataTable* Table, const TMap<FName, int32>& Counts);

	// 위 둘을 합친 것: 식탁 사각형 하나의 콤보.
	HUOGUOHUOGUO_API FHGComboResult EvaluateTable(const UDataTable* Table, const FIntRect& TableRect, const TArray<FHGGridItem>& Items);
}
