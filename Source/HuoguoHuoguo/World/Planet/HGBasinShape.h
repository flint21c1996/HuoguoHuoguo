#pragma once

#include "CoreMinimal.h"
#include "HGBasinShape.generated.h"

/**
 * 평지 행성(분지)의 땅 모양 값 (2026-11-27).
 *
 * 하는 일:
 *   Planet Maker 평지 탭에서 이 값을 만지고 [랜드스케이프에 넣기]를 누르면, HeightAt으로 땅 높이를 계산해
 *   평지 맵(L_Planet_Large)의 랜드스케이프를 새로 만든다. 그 뒤 원하는 곳만 랜드스케이프 도구로 손보면 된다.
 *
 * 모양 (가운데에서 바깥으로):
 *   착륙지(평평) → 바닥(잔 언덕) → 둑(가장자리가 솟아 분지를 감쌈, 둘레를 따라 높낮이가 달라 고개가 생긴다)
 *   → 바깥 비탈(둑 너머로 꺼짐) → 랜드스케이프 끝(허공). 둑 너머로 떨어지면 착륙 지점으로 돌아온다.
 *
 * 높이 계산의 노이즈는 정수 노이즈(HGNoise)라서 같은 값이면 같은 땅이 나온다.
 */
USTRUCT(BlueprintType)
struct HUOGUOHUOGUO_API FHGBasinShape
{
	GENERATED_BODY()

	// 땅 한 변의 길이(m). 랜드스케이프는 63m 단위로 만들어서 실제로는 이 값 이상인 가장 가까운 63의 배수가 된다.
	// 200m면 252m(1m 격자 252칸). 클수록 돌아다닐 곳이 넓지만 걷는 시간과 연산이 는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "분지", meta = (DisplayName = "한 변 길이(m)", ClampMin = "63", ClampMax = "1008", UIMin = "100", UIMax = "600"))
	float SizeM = 200.f;

	// 가운데 평평한 바닥의 반지름 (반폭에 대한 비율). 이 바깥부터 둑으로 오르기 시작한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "분지", meta = (DisplayName = "바닥 반지름 비율", ClampMin = "0.05", ClampMax = "0.95"))
	float FloorRatio = 0.5f;

	// 둑 꼭대기 위치 (반폭에 대한 비율). 바닥 반지름보다 커야 한다. 이 바깥은 내리막이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "분지", meta = (DisplayName = "둑 위치 비율", ClampMin = "0.1", ClampMax = "1"))
	float RimRatio = 0.82f;

	// 둑 높이(cm, 바닥 기준).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "분지", meta = (DisplayName = "둑 높이(cm)", ClampMin = "0", ClampMax = "20000", UIMax = "6000"))
	float RimHeight = 2200.f;

	// 둑 높이가 둘레를 따라 달라지는 정도 (0 = 고른 둑, 1 = 낮은 고개와 높은 봉우리가 섞임).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "분지", meta = (DisplayName = "둑 높낮이 변화", ClampMin = "0", ClampMax = "1"))
	float RimVariation = 0.45f;

	// 둑 너머 땅 끝이 바닥보다 얼마나 낮은지(cm). 땅 끝에서 떨어지는 곳이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "분지", meta = (DisplayName = "바깥 낙차(cm)", ClampMin = "0", ClampMax = "20000", UIMax = "8000"))
	float OuterDrop = 3000.f;

	// 바닥의 잔 언덕 높이(cm). 0이면 바닥이 판판하다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "언덕", meta = (DisplayName = "언덕 높이(cm)", ClampMin = "0", ClampMax = "3000", UIMax = "1000"))
	float HillHeight = 300.f;

	// 언덕 하나의 폭(cm). 클수록 완만하고 넓은 언덕.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "언덕", meta = (DisplayName = "언덕 폭(cm)", ClampMin = "500", ClampMax = "50000", UIMax = "20000"))
	float HillWavelength = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "언덕", meta = (DisplayName = "겹 수", ClampMin = "1", ClampMax = "8"))
	int32 HillOctaves = 4;

	// 착륙 지점(가운데) 둘레를 평평하게 다지는 반경(cm). 그 바깥 한 배 거리 동안 언덕이 부드럽게 돌아온다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "언덕", meta = (DisplayName = "착륙지 평탄 반경(cm)", ClampMin = "0", ClampMax = "5000"))
	float LandingFlatRadius = 1000.f;

	// 랜드스케이프 한 변의 칸 수 (63의 배수). 1칸 = 1m.
	int32 QuadsPerSide() const;

	// 실제 반폭(cm) = 칸 수 x 100 / 2.
	float HalfExtentCm() const { return QuadsPerSide() * 50.f; }

	// 시드마다 노이즈 공간의 다른 곳을 읽게 하는 이동값. 한 번 구해 HeightAt에 넘긴다 (점마다 다시 구하지 않게).
	static FVector NoiseOffsetOf(int32 Seed);

	// 가운데 기준 XY(cm)의 땅 높이(cm, 바닥 = 0 근처). NoiseOffset(NoiseOffsetOf(시드))이 언덕과 둑 높낮이 무늬를 정한다.
	float HeightAt(const FVector2D& XY, const FVector& NoiseOffset) const;
};
