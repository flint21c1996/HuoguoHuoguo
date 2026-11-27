#include "Misc/AutomationTest.h"
#include "World/Planet/HGNoise.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 정수 노이즈(HGNoise) 시험. 세션 프론트엔드 또는 명령 `Automation RunTests HG.Noise` 로 돌린다.
 *  1) 정수 격자점에서는 0
 *  2) 값 범위가 대략 -1 ~ 1
 *  3) 같은 입력이면 언제나 같은 값
 *  4) 고정된 점들의 값을 모두 더한 체크섬이 기록해 둔 값과 같다.
 *     체크섬이 바뀌면 노이즈 결과가 바뀐 것이다 (모든 행성 모양이 바뀜). 플랫폼이나 엔진을 바꾼 뒤에도 같아야 한다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHGNoiseTest, "HG.Noise.Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHGNoiseTest::RunTest(const FString& Parameters)
{
	using namespace HGNoise;

	// 1) 격자점
	for (int64 i = -3; i <= 3; ++i)
	{
		TestEqual(TEXT("격자점에서 0"), PerlinFixed(i * One, (i * 7) * One, (i * 13) * One), 0);
	}

	// 2) 범위, 4) 체크섬 (정수 덧셈이라 순서만 같으면 어디서나 같다)
	int64 Checksum = 0;
	int32 MinV = INT32_MAX;
	int32 MaxV = INT32_MIN;
	for (int32 i = 0; i < 4096; ++i)
	{
		const int64 X = int64(i) * 5003 - 9000000;
		const int64 Y = int64(i) * 7919 + 123456;
		const int64 Z = int64(i) * 104729 - 777;
		const int32 V = PerlinFixed(X, Y, Z);
		Checksum = Checksum * 31 + V;
		MinV = FMath::Min(MinV, V);
		MaxV = FMath::Max(MaxV, V);
	}
	AddInfo(FString::Printf(TEXT("[HGNoise] checksum %lld, min %d, max %d (One = %lld)"), Checksum, MinV, MaxV, One));
	TestTrue(TEXT("최솟값이 -1.1 이상"), MinV >= -One * 11 / 10);
	TestTrue(TEXT("최댓값이 1.1 이하"), MaxV <= One * 11 / 10);

	// 3) 같은 입력 = 같은 값 (실수 입력 경로 포함)
	const FVector P(12.345, -6.789, 1001.5);
	TestEqual(TEXT("Perlin 반복"), Perlin(P), Perlin(P));
	TestEqual(TEXT("Fbm 반복"), Fbm(P, 5), Fbm(P, 5));
	TestEqual(TEXT("Ridged 반복"), Ridged(P, 3), Ridged(P, 3));

	// 4) 기록해 둔 체크섬 (2026-11-27, Windows x64에서 처음 기록). 다른 플랫폼, 다른 엔진 버전에서도 같아야 한다.
	constexpr int64 ExpectedChecksum = -2982263534300911962LL;
	TestEqual(TEXT("노이즈 체크섬 (바뀌면 모든 행성 모양이 바뀐 것)"), Checksum, ExpectedChecksum);
	return true;
}

#endif
