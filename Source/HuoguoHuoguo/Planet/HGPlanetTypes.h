#pragma once

#include "CoreMinimal.h"
#include "HGPlanetTypes.generated.h"

// 행성 지표의 성격. 색, 지형 높낮이, 나오는 재료가 달라진다.
UENUM(BlueprintType)
enum class EHGPlanetBiome : uint8
{
	Rock,   // 바위 소행성. 매운 고추 계열.
	Ice,    // 얼음 행성. 해초 계열.
	Fungal, // 버섯 숲 행성. 버섯 계열.
};

// 행성 크기 등급. 작은 행성은 지평선이 휘어 보일 만큼 작고, 중간 행성은 걸을 때는 평지처럼 느껴진다.
UENUM(BlueprintType)
enum class EHGPlanetSize : uint8
{
	Small,
	Medium,
};

/**
 * 행성 하나를 정하는 값 묶음.
 *
 * 행성은 맵 에셋이 아니라 이 값들로 존재한다. 같은 Seed로 MakePlanet을 부르면 언제나 같은 값이 나오고,
 * 행성 표면 맵의 생성기(AHGPlanetGenerator)는 이 값으로 지형과 배치를 다시 만든다.
 * 그래서 저장할 것은 Seed 하나와 "플레이어가 바꾼 것"(캔 자원 등)뿐이다.
 */
USTRUCT(BlueprintType)
struct HUOGUOHUOGUO_API FHGPlanetSeed
{
	GENERATED_BODY()

	// 모든 절차적 값의 출발점.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
	int32 Seed = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
	FString Name = TEXT("크락-7");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
	EHGPlanetBiome Biome = EHGPlanetBiome::Rock;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
	EHGPlanetSize SizeClass = EHGPlanetSize::Small;

	// 평균 반지름(cm). 작은 행성 40~120m, 중간 행성 300~600m.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet", meta = (ClampMin = "1000"))
	float Radius = 8000.f;

	// 지형 높낮이. 반지름에 대한 비율 (0.04 = 반지름의 4%만큼 오르내림).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet", meta = (ClampMin = "0", ClampMax = "0.2"))
	float TerrainAmplitude = 0.05f;

	// 지형 굴곡의 촘촘함. 클수록 언덕이 작고 많아진다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet", meta = (ClampMin = "0.5"))
	float TerrainFrequency = 2.5f;

	// 캐릭터 중력 배율. 1 = 지구, 작은 행성일수록 낮게 잡아 점프가 붕 뜨게 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet", meta = (ClampMin = "0.1"))
	float GravityScale = 0.6f;

	// 바위 개수와 재료 개수의 배율.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet", meta = (ClampMin = "0"))
	float Richness = 1.f;

	// 정거장 근처에 머무는 남은 시간(게임 내 시간). 0이 되면 목록에서 빠진다. 지금은 표시용이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
	int32 HoursLeft = 6;

	// 목록과 안내에 쓰는 한 줄 설명. 예) "크락-7 | 소형 바위 | 반지름 80m | 중력 0.6 | 6시간 후 이탈"
	FString Describe() const;
};

namespace HGPlanet
{
	// 시드 하나로 행성 하나를 만든다. 같은 시드 = 같은 행성.
	HUOGUOHUOGUO_API FHGPlanetSeed MakePlanet(int32 Seed);

	// 궤도 주기(게임 내 날짜) Cycle에 정거장으로 접근하는 행성 Count개.
	// 같은 주기에는 언제나 같은 목록이 나온다. 크기 등급이 한쪽으로 몰리지 않도록 작은 행성과 중간 행성을 섞는다.
	HUOGUOHUOGUO_API TArray<FHGPlanetSeed> MakeApproachingPlanets(int32 UniverseSeed, int32 Cycle, int32 Count);

	HUOGUOHUOGUO_API const TCHAR* BiomeName(EHGPlanetBiome Biome);

	// 생물 군계별 지면/바위 색.
	HUOGUOHUOGUO_API FLinearColor GroundColor(EHGPlanetBiome Biome);
	HUOGUOHUOGUO_API FLinearColor RockColor(EHGPlanetBiome Biome);
}
