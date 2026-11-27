#pragma once

#include "CoreMinimal.h"

/**
 * 정수(고정소수점)로만 계산하는 펄린 노이즈. 행성 지형과 무늬에 쓴다.
 *
 * 왜 직접 만드나:
 *   엔진의 FMath::PerlinNoise3D는 실수(float/double)로 계산한다. 실수 계산은 CPU 종류, 컴파일러 설정(FMA 등), 엔진 버전에 따라
 *   마지막 자리가 조금씩 달라질 수 있다. 노이즈 안의 계산을 정수로만 하면 윈도우든 맥이든, 엔진이 바뀌어도 비트 하나까지 같은 값이 나온다.
 *   행성은 시드 하나로 다시 만들기 때문에(저장은 시드만 한다) 어느 컴퓨터에서든 같은 행성이 나와야 한다.
 *
 * 고정소수점:
 *   소수를 65536배 한 정수로 들고 다닌다 (1.0 = 65536, 0.5 = 32768). 덧셈은 그대로, 곱셈은 곱한 뒤 16비트 오른쪽으로 민다.
 *   펄린 노이즈는 정수 격자점에서 값이 0이라서, 정수 좌표만 넣으면 모두 0이 된다. 그래서 소수 자리를 정수 안에 담는다.
 *
 * 입력의 한계:
 *   FVector로 받는 함수는 들어온 실수 좌표를 1/65536 단위로 반올림해서 정수로 바꾼 뒤 계산한다.
 *   노이즈 안은 완전히 같지만, 좌표를 만드는 바깥 계산(방향 벡터 등)이 실수라서 그 좌표가 컴퓨터마다 마지막 자리에서 다르면
 *   반올림 경계에 걸린 극히 드문 경우 한 칸(1/65536) 차이가 날 수 있다. 정수 좌표(FixedX/Y/Z)를 직접 넣으면 그 걱정도 없다.
 */
namespace HGNoise
{
	// 고정소수점 소수 비트 수와 1.0
	inline constexpr int32 FracBits = 16;
	inline constexpr int64 One = int64(1) << FracBits;

	// 실수를 고정소수점 정수로 (1/65536 단위로 반올림).
	HUOGUOHUOGUO_API int64 ToFixed(double Value);

	/**
	 * 3D 펄린 노이즈 (Ken Perlin의 개선판과 같은 구조). 입력과 출력 모두 고정소수점.
	 * 결과는 대략 -One ~ +One (-1 ~ 1). 정수 격자점에서는 0.
	 */
	HUOGUOHUOGUO_API int32 PerlinFixed(int64 X, int64 Y, int64 Z);

	// 실수 좌표용 편의 함수. 결과는 대략 -1 ~ 1.
	HUOGUOHUOGUO_API float Perlin(const FVector& P);

	/**
	 * 여러 겹 노이즈(fBm). 한 겹 올라갈 때마다 세기는 PersistencePercent/100배(기본 50 = 절반), 좌표는 LacunarityPercent/100배.
	 * 정수배(200)로 키우면 격자점이 겹쳐 무늬가 반복되므로 203처럼 어긋난 값을 쓴다. 결과는 대략 -1 ~ 1
	 * (겹마다 세기의 합으로 나누므로 감쇠를 바꿔도 범위는 같다).
	 * 감쇠 50일 때 세기는 65536, 32768, ... 로 정확히 절반씩이라 예전(오른쪽 시프트) 결과와 비트까지 같다.
	 */
	HUOGUOHUOGUO_API float Fbm(const FVector& P, int32 Octaves, int32 LacunarityPercent = 203, int32 PersistencePercent = 50);

	/**
	 * 능선 노이즈. 한 겹마다 (1 - |노이즈|)² 을 더한다. 노이즈가 0을 지나는 줄을 따라 뾰족한 능선이 생긴다.
	 * 결과는 0 ~ 1 (평균 약 0.5).
	 */
	HUOGUOHUOGUO_API float Ridged(const FVector& P, int32 Octaves, int32 LacunarityPercent = 207);
}
