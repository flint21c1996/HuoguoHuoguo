#include "World/Planet/HGBasinShape.h"

#include "Math/RandomStream.h"
#include "World/Planet/HGNoise.h"

int32 FHGBasinShape::QuadsPerSide() const
{
	// 랜드스케이프 부품(컴포넌트) 하나 = 63칸. 한 변을 63칸 단위로 올림한다.
	constexpr int32 QuadsPerComponent = 63;
	const int32 Components = FMath::Clamp(FMath::CeilToInt(SizeM / QuadsPerComponent), 1, 16);
	return Components * QuadsPerComponent;
}

FVector FHGBasinShape::NoiseOffsetOf(int32 Seed)
{
	// 구 행성과 같은 방식: 시드 수열에서 큰 이동값을 뽑아 행성마다 노이즈 공간의 전혀 다른 곳을 읽는다.
	FRandomStream Rng(Seed);
	return FVector(Rng.FRandRange(-1000.f, 1000.f), Rng.FRandRange(-1000.f, 1000.f), Rng.FRandRange(-1000.f, 1000.f));
}

float FHGBasinShape::HeightAt(const FVector2D& XY, const FVector& Offset) const
{
	const double Half = HalfExtentCm();
	const double Distance = XY.Size();
	const float R = static_cast<float>(Distance / Half); // 0 = 가운데, 1 = 변의 가운데

	// 1) 둑: 바닥 → 둑 꼭대기까지 부드럽게 오르고, 꼭대기 → 땅 끝까지 내려가 바깥 낙차만큼 바닥보다 낮아진다.
	//    둑 높이는 둘레 각도에 따라 노이즈로 달라진다 (낮은 곳 = 넘어갈 수 있는 고개).
	const float Floor = FMath::Min(FloorRatio, RimRatio - 0.02f);
	const double Angle = FMath::Atan2(XY.Y, XY.X);
	const float RimNoise = HGNoise::Fbm(FVector(FMath::Cos(Angle) * 1.8, FMath::Sin(Angle) * 1.8, 0.0) + Offset * 0.37, 3);
	const float LocalRim = RimHeight * FMath::Max(0.05f, 1.f + RimVariation * RimNoise * 1.6f);
	float Base;
	if (R <= Floor)
	{
		Base = 0.f;
	}
	else if (R <= RimRatio)
	{
		Base = LocalRim * FMath::SmoothStep(Floor, RimRatio, R);
	}
	else
	{
		const float Out = FMath::SmoothStep(RimRatio, 1.f, R);
		Base = FMath::Lerp(LocalRim, -OuterDrop, Out);
	}

	// 2) 바닥 언덕: 여러 겹 노이즈. 가운데 착륙지는 평평하게 다지고, 둑 쪽으로 갈수록 언덕을 조금 키운다.
	const FVector P(XY.X / HillWavelength, XY.Y / HillWavelength, 0.0);
	const float Hills = HGNoise::Fbm(P + Offset, HillOctaves);
	const float Flatten = LandingFlatRadius > 0.f ? FMath::SmoothStep(LandingFlatRadius, LandingFlatRadius * 2.f, static_cast<float>(Distance)) : 1.f;
	const float Toward = FMath::Lerp(0.6f, 1.2f, FMath::Clamp(R / FMath::Max(Floor, 0.05f), 0.f, 1.f));
	return Base + HillHeight * Hills * 2.f * Toward * Flatten;
}
