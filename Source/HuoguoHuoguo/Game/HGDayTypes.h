#pragma once

#include "CoreMinimal.h"
#include "HGDayTypes.generated.h"

/**
 * 하루의 시간대.
 *
 * 하루는 낮 -> 밤 -> 문 닫을 시간 순서로 흐른다. 행성에 다녀오면 시간대가 한 칸 넘어간다
 * (낮에 내려가면 돌아왔을 때 밤, 밤에 내려가면 돌아왔을 때 문 닫을 시간).
 * 문 닫을 시간에는 다이빙하지 않고 하루를 마친다. 하루를 마치면 다음 날 낮이 되고 접근 행성 목록이 바뀐다.
 * 경영과 탐험 양쪽이 쓰므로 공통 폴더(Game/)에 둔다. 상태는 UHGGameInstance가 들고 있다.
 */
UENUM(BlueprintType)
enum class EHGDayPhase : uint8
{
	Day,
	Night,
	Closing,
};

/**
 * 재료가 행성에서 보이는 시간대 (재료 CSV의 AvailableAt 열).
 * 같은 행성이라도 낮에 내려가면 낮 재료, 밤에 내려가면 밤 재료가 보인다. Any는 둘 다.
 */
UENUM(BlueprintType)
enum class EHGIngredientTime : uint8
{
	Any,
	Day,
	Night,
};

namespace HGDay
{
	// 화면에 쓸 시간대 이름 ("낮", "밤", "문 닫을 시간").
	inline const TCHAR* PhaseName(EHGDayPhase Phase)
	{
		switch (Phase)
		{
		case EHGDayPhase::Day:
			return TEXT("낮");
		case EHGDayPhase::Night:
			return TEXT("밤");
		default:
			return TEXT("문 닫을 시간");
		}
	}

	// 이 시간대에 그 재료가 행성에서 보이는지. 문 닫을 시간에는 다이빙하지 않지만, 혹시 그때 행성 맵을 열면 밤으로 친다.
	inline bool IsAvailable(EHGIngredientTime Time, EHGDayPhase Phase)
	{
		if (Time == EHGIngredientTime::Any)
		{
			return true;
		}
		const bool bDay = Phase == EHGDayPhase::Day;
		return (Time == EHGIngredientTime::Day) == bDay;
	}
}
