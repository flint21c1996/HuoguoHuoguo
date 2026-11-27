#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "HGPlanetShape.generated.h"

struct FHGPlanetSeed;

/**
 * 행성 지형의 모양 값 묶음 (언덕, 능선, 잔 굴곡, 크레이터, 착륙지, 높이 곡선).
 *
 * 하는 일:
 *   AHGPlanetGenerator::SurfaceRadiusAt(지표 높이)과 ChoosePlanet(크레이터 배치)이 이 값만 읽는다.
 *   예전에는 이 숫자들이 생성기 코드 안에 박혀 있어서, 지형을 바꾸려면 코드를 고쳐야 했다.
 *   이제 Planet Maker 창이나 디테일 패널에서 바꾸고 JSON으로 저장한다 (기획은 에디터에서 사람이 정한다).
 *
 * 기본값:
 *   HGPlanet::DefaultShape가 행성 값(군계, 크기)에 맞춰 채운다. 묶음으로 옮길 때는 예전 코드 숫자와 정확히 같았다
 *   (시험 HG.Planet.TerrainShape로 확인). 그 뒤 행성 크기를 절반으로 줄이면서 크레이터 반지름 기본값도 절반으로 바꿨다 (2026-11-27).
 *
 * 비율(%) 값을 정수로 두는 까닭:
 *   노이즈(HGNoise)는 정수 계산이라 어느 컴퓨터에서나 같다. 겹마다 좌표를 몇 배 할지(배율)와 세기를 몇 배 줄일지(감쇠)도
 *   정수 %로 받아 그 정수 계산 안에서 쓴다.
 */
USTRUCT(BlueprintType)
struct HUOGUOHUOGUO_API FHGPlanetShape
{
	GENERATED_BODY()

	// ---- 큰 언덕 (여러 겹 노이즈, fBm) ----

	// 언덕 노이즈를 몇 겹 겹칠지. 겹이 많을수록 큰 언덕 위에 작은 굴곡이 더 얹힌다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "언덕", meta = (DisplayName = "겹 수", ClampMin = "1", ClampMax = "8"))
	int32 HillOctaves = 5;

	// 한 겹 올라갈 때 노이즈를 몇 % 촘촘하게 할지 (203 = 2.03배). 클수록 윗겹 굴곡이 잘다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "언덕", meta = (DisplayName = "겹 배율(%)", ClampMin = "120", ClampMax = "400"))
	int32 HillLacunarityPercent = 203;

	// 한 겹 올라갈 때 세기를 몇 %로 줄일지 (50 = 절반). 클수록 잔 굴곡이 세게 드러나 거칠어진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "언덕", meta = (DisplayName = "겹 감쇠(%)", ClampMin = "10", ClampMax = "90"))
	int32 HillPersistencePercent = 50;

	// 솟은 곳(언덕 값이 양수)에 곱하는 배율. 1보다 크면 언덕이 더 솟는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "언덕", meta = (DisplayName = "솟은 곳 배율", ClampMin = "0", ClampMax = "3"))
	float HighlandScale = 1.f;

	// 꺼진 곳(언덕 값이 음수)에 곱하는 배율. 0에 가까우면 골짜기가 메워져 평원이 된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "언덕", meta = (DisplayName = "꺼진 곳 배율", ClampMin = "0", ClampMax = "3"))
	float LowlandScale = 1.f;

	// 높이 곡선 (가로 = 언덕 높이 -1 ~ 1, 세로 = 바뀐 높이). 키가 없으면 쓰지 않는다.
	// 예) 위쪽을 눌러 평평하게 하면 고원(메사), 가운데를 가파르게 하면 계단 같은 단구가 된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "언덕", meta = (DisplayName = "높이 곡선"))
	FRuntimeFloatCurve HeightCurve;

	// ---- 능선 (ridged noise) ----

	// 날카로운 능선의 세기 (반지름에 대한 비율이 아니라 언덕 높이 단위). 0이면 능선이 없다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "능선", meta = (DisplayName = "세기", ClampMin = "0", ClampMax = "2"))
	float RidgeWeight = 0.5f;

	// 능선이 큰 언덕보다 몇 배 촘촘한지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "능선", meta = (DisplayName = "촘촘함 배율", ClampMin = "0.5", ClampMax = "10"))
	float RidgeFrequencyScale = 2.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "능선", meta = (DisplayName = "겹 수", ClampMin = "1", ClampMax = "6"))
	int32 RidgeOctaves = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "능선", meta = (DisplayName = "겹 배율(%)", ClampMin = "120", ClampMax = "400"))
	int32 RidgeLacunarityPercent = 207;

	// ---- 잔 굴곡 (행성 크기와 상관없는 일정한 둔덕) ----

	// 둔덕 높이(cm). 0이면 없다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "잔 굴곡", meta = (DisplayName = "높이(cm)", ClampMin = "0", ClampMax = "500"))
	float BumpHeight = 70.f;

	// 둔덕 폭(cm). 지형 격자가 담지 못할 만큼 좁으면 격자 4칸 폭으로 넓히고, 넓힌 만큼 높이도 키운다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "잔 굴곡", meta = (DisplayName = "폭(cm)", ClampMin = "200", ClampMax = "5000"))
	float BumpWavelength = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "잔 굴곡", meta = (DisplayName = "겹 수", ClampMin = "1", ClampMax = "4"))
	int32 BumpOctaves = 2;

	// ---- 크레이터 (지형 자체를 판다. 데칼이 아니다) ----

	// 크레이터 개수 범위. 시드로 이 사이에서 고른다. 둘 다 0이면 크레이터가 없다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "크레이터", meta = (DisplayName = "최소 개수", ClampMin = "0", ClampMax = "100"))
	int32 CraterCountMin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "크레이터", meta = (DisplayName = "최대 개수", ClampMin = "0", ClampMax = "100"))
	int32 CraterCountMax = 6;

	// 크레이터 반지름 범위(cm). 지형 격자가 그릇 모양을 담을 수 있게 실제로는 격자 4칸보다 작아지지 않는다.
	// 기본 2.5 ~ 7m (2026-11-27 행성 크기를 절반으로 줄이면서 예전 5 ~ 14m도 절반으로).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "크레이터", meta = (DisplayName = "최소 반지름(cm)", ClampMin = "100", ClampMax = "20000"))
	float CraterRadiusMin = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "크레이터", meta = (DisplayName = "최대 반지름(cm)", ClampMin = "100", ClampMax = "20000"))
	float CraterRadiusMax = 700.f;

	// 깊이 = 반지름 x 이 비율 (범위 안에서 시드로 고름).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "크레이터", meta = (DisplayName = "최소 깊이 비율", ClampMin = "0", ClampMax = "1"))
	float CraterDepthRatioMin = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "크레이터", meta = (DisplayName = "최대 깊이 비율", ClampMin = "0", ClampMax = "1"))
	float CraterDepthRatioMax = 0.3f;

	// 가장자리 둑 높이 = 깊이 x 이 비율.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "크레이터", meta = (DisplayName = "둑 높이 비율", ClampMin = "0", ClampMax = "2"))
	float CraterRimHeight = 0.35f;

	// 가장자리 둑 폭 = 반지름 x 이 비율.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "크레이터", meta = (DisplayName = "둑 폭 비율", ClampMin = "0.05", ClampMax = "1"))
	float CraterRimWidth = 0.25f;

	// ---- 착륙지 ----

	// 착륙 지점(북극) 둘레를 평평하게 다지는 반경(cm). 그 바깥 한 배 거리 동안 원래 높이로 부드럽게 돌아간다.
	// 소행성 지대의 본 소행성은 작아서 실제로는 반지름의 30%를 넘지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "착륙지", meta = (DisplayName = "평탄 반경(cm)", ClampMin = "0", ClampMax = "10000"))
	float LandingFlatRadius = 1000.f;
};

namespace HGPlanet
{
	// 행성 값(군계, 크기)에 맞는 기본 지형 모양.
	//   군계: 얼음 = 언덕 0.6배, 능선 0.4, 둔덕 45cm / 버섯숲 = 솟은 곳 1.4배, 꺼진 곳 0.7배 / 바위 = 능선 0.7, 둔덕 80cm
	//   크기: 소형 크레이터 3 ~ 6개, 중형 6 ~ 12개
	HUOGUOHUOGUO_API FHGPlanetShape DefaultShape(const FHGPlanetSeed& Planet);
}
