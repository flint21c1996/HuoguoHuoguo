#include "World/Planet/HGNoise.h"

namespace
{
	using HGNoise::FracBits;
	using HGNoise::One;

	/**
	 * 섞기 표(순열 표). 0~255를 한 번씩 담은 표를 두 번 이어 붙인다 (인덱스가 255를 넘어도 바로 읽게).
	 * 표는 정해진 정수 섞기(xorshift + 피셔-예이츠)로 만든다. 실수나 시드가 없어서 어디서 만들어도 같은 표가 나온다.
	 * 이 표나 섞는 방법을 바꾸면 모든 행성 모양이 바뀐다.
	 */
	struct FPermutation
	{
		uint8 P[512];

		FPermutation()
		{
			for (int32 i = 0; i < 256; ++i)
			{
				P[i] = static_cast<uint8>(i);
			}
			uint32 State = 0x9E3779B9u;
			for (int32 i = 255; i > 0; --i)
			{
				State ^= State << 13;
				State ^= State >> 17;
				State ^= State << 5;
				const int32 j = static_cast<int32>(State % static_cast<uint32>(i + 1));
				const uint8 Tmp = P[i];
				P[i] = P[j];
				P[j] = Tmp;
			}
			for (int32 i = 0; i < 256; ++i)
			{
				P[256 + i] = P[i];
			}
		}
	};

	const FPermutation& Perm()
	{
		static const FPermutation Table;
		return Table;
	}

	// 고정소수점 곱셈: (A × B) / One. 음수도 산술 시프트(내림)로 같은 규칙이다 (C++20에서 정의된 동작).
	FORCEINLINE int64 Mul(int64 A, int64 B)
	{
		return (A * B) >> FracBits;
	}

	// 부드러운 보간 곡선 6t⁵ - 15t⁴ + 10t³ (t = 0 ~ One). 격자 경계에서 기울기가 0이라 무늬가 끊겨 보이지 않는다.
	FORCEINLINE int64 Fade(int64 T)
	{
		const int64 T3 = Mul(Mul(T, T), T);
		const int64 Inner = Mul(T, Mul(T, 6 * One) - 15 * One) + 10 * One;
		return Mul(T3, Inner);
	}

	FORCEINLINE int64 Lerp(int64 T, int64 A, int64 B)
	{
		return A + Mul(B - A, T);
	}

	// 격자 꼭짓점의 기울기와 (꼭짓점 → 점) 거리의 내적. 기울기는 12방향(정육면체 모서리 방향)을 해시로 고른다.
	FORCEINLINE int64 Grad(int32 Hash, int64 X, int64 Y, int64 Z)
	{
		const int32 H = Hash & 15;
		const int64 U = H < 8 ? X : Y;
		const int64 V = H < 4 ? Y : (H == 12 || H == 14 ? X : Z);
		return ((H & 1) ? -U : U) + ((H & 2) ? -V : V);
	}
}

int64 HGNoise::ToFixed(double Value)
{
	// 1/65536 단위로 반올림. floor는 어느 플랫폼에서나 같은 결과다.
	return static_cast<int64>(FMath::FloorToDouble(Value * static_cast<double>(One) + 0.5));
}

int32 HGNoise::PerlinFixed(int64 X, int64 Y, int64 Z)
{
	const uint8* P = Perm().P;

	// 격자 칸 번호(내림)와 칸 안의 위치(0 ~ One-1). 음수 좌표도 2의 보수라서 & 연산이 "내림 + 나머지"로 맞게 나뉜다.
	const int32 Xi = static_cast<int32>((X >> FracBits) & 255);
	const int32 Yi = static_cast<int32>((Y >> FracBits) & 255);
	const int32 Zi = static_cast<int32>((Z >> FracBits) & 255);
	const int64 Xf = X & (One - 1);
	const int64 Yf = Y & (One - 1);
	const int64 Zf = Z & (One - 1);

	const int64 U = Fade(Xf);
	const int64 V = Fade(Yf);
	const int64 W = Fade(Zf);

	// 칸의 꼭짓점 8개를 해시로 고른다.
	const int32 A = P[Xi] + Yi;
	const int32 AA = P[A] + Zi;
	const int32 AB = P[A + 1] + Zi;
	const int32 B = P[Xi + 1] + Yi;
	const int32 BA = P[B] + Zi;
	const int32 BB = P[B + 1] + Zi;

	// 8개 꼭짓점 값을 x, y, z 순서로 보간한다.
	const int64 X1 = Lerp(U, Grad(P[AA], Xf, Yf, Zf), Grad(P[BA], Xf - One, Yf, Zf));
	const int64 X2 = Lerp(U, Grad(P[AB], Xf, Yf - One, Zf), Grad(P[BB], Xf - One, Yf - One, Zf));
	const int64 Y1 = Lerp(V, X1, X2);
	const int64 X3 = Lerp(U, Grad(P[AA + 1], Xf, Yf, Zf - One), Grad(P[BA + 1], Xf - One, Yf, Zf - One));
	const int64 X4 = Lerp(U, Grad(P[AB + 1], Xf, Yf - One, Zf - One), Grad(P[BB + 1], Xf - One, Yf - One, Zf - One));
	const int64 Y2 = Lerp(V, X3, X4);
	return static_cast<int32>(Lerp(W, Y1, Y2));
}

float HGNoise::Perlin(const FVector& P)
{
	return static_cast<float>(static_cast<double>(PerlinFixed(ToFixed(P.X), ToFixed(P.Y), ToFixed(P.Z))) / static_cast<double>(One));
}

float HGNoise::Fbm(const FVector& P, int32 Octaves, int32 LacunarityPercent)
{
	int64 X = ToFixed(P.X);
	int64 Y = ToFixed(P.Y);
	int64 Z = ToFixed(P.Z);
	int64 Sum = 0;
	int64 Norm = 0;
	int64 Amp = One;
	for (int32 i = 0; i < Octaves; ++i)
	{
		Sum += Mul(Amp, PerlinFixed(X, Y, Z));
		Norm += Amp;
		Amp >>= 1;
		// 좌표를 LacunarityPercent/100배. 정수 나눗셈도 어디서나 같은 규칙(0 쪽으로 버림)이다.
		X = X * LacunarityPercent / 100;
		Y = Y * LacunarityPercent / 100;
		Z = Z * LacunarityPercent / 100;
	}
	return Norm > 0 ? static_cast<float>(static_cast<double>(Sum) / static_cast<double>(Norm)) : 0.f;
}

float HGNoise::Ridged(const FVector& P, int32 Octaves, int32 LacunarityPercent)
{
	int64 X = ToFixed(P.X);
	int64 Y = ToFixed(P.Y);
	int64 Z = ToFixed(P.Z);
	int64 Sum = 0;
	int64 Norm = 0;
	int64 Amp = One;
	for (int32 i = 0; i < Octaves; ++i)
	{
		const int64 N = PerlinFixed(X, Y, Z);
		const int64 R = One - FMath::Min<int64>(N < 0 ? -N : N, One);
		Sum += Mul(Amp, Mul(R, R));
		Norm += Amp;
		Amp >>= 1;
		X = X * LacunarityPercent / 100;
		Y = Y * LacunarityPercent / 100;
		Z = Z * LacunarityPercent / 100;
	}
	return Norm > 0 ? static_cast<float>(static_cast<double>(Sum) / static_cast<double>(Norm)) : 0.f;
}
