#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Planet/HGPlanetTypes.h"
#include "HGIngredientTypes.generated.h"

/**
 * 재료 한 종류의 데이터 (DataTable 한 줄).
 *
 * 원본은 저장소의 `Data/Ingredients.csv`다. `Tools/GenerateGreyboxMaps.py`가 그 CSV를
 * `/Game/HG/Data/DT_Ingredients` DataTable 에셋으로 읽어 들인다. 재료를 늘리거나 값을 바꿀 때는 CSV만 고친다.
 * 행 이름(첫 열 Name)이 재료의 ID이고, 가방과 저장 기록은 이 ID(FName)로 재료를 가리킨다.
 *
 * 경영(손님, 요리)과 탐험(채집) 양쪽이 쓰므로 공통 폴더(Game/)에 둔다.
 */
USTRUCT(BlueprintType)
struct HUOGUOHUOGUO_API FHGIngredientRow : public FTableRowBase
{
	GENERATED_BODY()

	// 화면에 보이는 이름 ("포자버섯").
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ingredient")
	FText DisplayName;

	// 행성 위 표식과 진열대 자리 표시 색.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ingredient")
	FLinearColor Color = FLinearColor::White;

	// 기본 값어치 (아직 쓰지 않는다. 요리와 손님 계산이 생기면 쓴다).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ingredient")
	int32 Price = 10;

	// 이 재료가 많이 나는 생물 군계. 그 군계 행성에서는 재료 표식의 절반이 이 재료다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ingredient")
	EHGPlanetBiome HomeBiome = EHGPlanetBiome::Rock;
};
