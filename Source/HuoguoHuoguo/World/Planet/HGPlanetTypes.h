#pragma once

#include "CoreMinimal.h"
#include "HGPlanetTypes.generated.h"

// 행성 지표의 성격. 색, 지형 높낮이, 나오는 재료가 달라진다.
UENUM(BlueprintType)
enum class EHGPlanetBiome : uint8
{
	Rock   UMETA(DisplayName = "바위"),   // 바위 소행성. 매운 고추 계열.
	Ice    UMETA(DisplayName = "얼음"),   // 얼음 행성. 해초 계열.
	Fungal UMETA(DisplayName = "버섯숲"), // 버섯 숲 행성. 버섯 계열.
};

// 행성 크기 등급. 작은 행성은 지평선이 휘어 보일 만큼 작고, 중간 행성은 걸을 때는 평지처럼 느껴진다.
UENUM(BlueprintType)
enum class EHGPlanetSize : uint8
{
	Small,
	Medium,
};

/**
 * 다이빙 목적지의 환경 종류. 환경마다 맵이 따로 있고, 맵마다 고유한 규칙(크기, 중력, 조명, 나오는 것)을 가진다.
 * 어느 맵으로 갈지는 프로젝트 설정 → Game → HG 환경 맵(UHGSiteSettings)에서 정한다.
 * 행성 값(FHGPlanetSeed)에서 HGPlanet::SiteTypeOf로 구한다.
 */
UENUM(BlueprintType)
enum class EHGSiteType : uint8
{
	PlanetSmall   UMETA(DisplayName = "소형 행성"),
	PlanetMedium  UMETA(DisplayName = "중형 행성"),
	PlanetLarge   UMETA(DisplayName = "평지 행성 (대형, 랜드스케이프)"),
	AsteroidField UMETA(DisplayName = "소행성 지대"),
	Derelict      UMETA(DisplayName = "폐우주선"),
};

/**
 * 중력을 가진 천체 하나 (행성 또는 소행성). 소행성 지대에서는 여러 개가 동시에 사장님을 끈다.
 *
 * 끄는 힘: 지표에서 FieldDepth(15m) 높이까지는 SurfaceGravity 그대로이고, 그 바깥에서는 거리 제곱에 반비례해 약해진다.
 * 실제 중력(지표부터 거리 제곱으로 약해짐)이면 반지름 10m 소행성은 탈출 속도가 초속 10m 정도라서 점프 한 번에 튕겨 나간다.
 * 게임에서는 소행성 둘레의 중력 껍질 안에서는 늘 같은 무게로 걷고 뛰게 하는 편이 손에 익는다 (슈퍼 마리오 갤럭시 방식).
 */
struct FHGGravityBody
{
	FVector Center = FVector::ZeroVector; // 월드 위치
	float Radius = 1000.f;                // 평균 반지름(cm)
	float SurfaceGravity = 0.5f;          // 지표 중력 배율 (1 = 지구)
	float FieldDepth = 1500.f;            // 중력이 일정한 껍질 두께(cm)

	// 중심에서 Distance 떨어진 곳에서 끄는 힘 (중력 배율).
	float PullAt(float Distance) const
	{
		const float Edge = Radius + FieldDepth;
		return Distance <= Edge ? SurfaceGravity : SurfaceGravity * FMath::Square(Edge / Distance);
	}
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

	// 평균 반지름(cm). 작은 행성 20~60m, 중간 행성 150~300m (2026-11-27에 절반으로 줄임).
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

	// 소행성 지대인가. 그렇다면 Radius는 착륙하는 본 소행성의 반지름이고, 둘레에 뛰어 건너는 소행성들이 더 생긴다 (AHGPlanetGenerator).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
	bool bAsteroidField = false;

	// 폐우주선인가. 그렇다면 땅이 없는 우주 한가운데에 버려진 우주선 한 척이 떠 있다 (AHGPlanetGenerator::BuildDerelict).
	// 밖에서는 제트팩으로 떠다니며 선체의 재료를 캐고, 에어록으로 들어가면 인공 중력이 살아 있는 방들을 걷는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
	bool bDerelict = false;

	// 평지 행성인가 (2026-11-27). 구가 아니라 평평한 분지(랜드스케이프) 위를 걷는다. 중력은 아래로, 가장자리 밖으로 떨어지면 착륙 지점으로 돌아온다.
	// 땅 모양과 넓이는 평지 맵(L_Planet_Large)의 랜드스케이프가 정한다 (Planet Maker 평지 탭에서 만든다). Radius는 재료 개수를 정하는 데만 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet")
	bool bFlat = false;

	// 목록과 안내에 쓰는 한 줄 설명. 예) "크락-7 | 소형 바위 | 반지름 80m | 중력 0.6 | 6시간 후 이탈"
	FString Describe() const;
};

namespace HGPlanet
{
	// 시드 하나로 행성 하나를 만든다. 같은 시드 = 같은 행성.
	HUOGUOHUOGUO_API FHGPlanetSeed MakePlanet(int32 Seed);

	// 행성 값을 소행성 지대로 바꾼다: 본 소행성 반지름 10 ~ 15m, 지형은 울퉁불퉁하게. 시드로 정하므로 같은 시드면 같다.
	HUOGUOHUOGUO_API void MakeAsteroidField(FHGPlanetSeed& Planet);

	// 행성 값을 폐우주선으로 바꾼다 (소행성 지대는 해제). 군계는 선체에 자란 재료의 성격으로 남는다.
	HUOGUOHUOGUO_API void MakeDerelict(FHGPlanetSeed& Planet);

	// 행성 값을 평지 행성으로 바꾼다 (소행성 지대, 폐우주선은 해제). 중력은 0.9. 넓이는 평지 맵의 랜드스케이프가 정한다.
	HUOGUOHUOGUO_API void MakeFlat(FHGPlanetSeed& Planet);

	// 궤도 주기(게임 내 날짜) Cycle에 정거장으로 접근하는 행성 Count개.
	// 같은 주기에는 언제나 같은 목록이 나온다. 크기 등급이 한쪽으로 몰리지 않도록 작은 행성과 중간 행성을 섞는다.
	// FlatChance(0 ~ 1): 셋째부터 각 행성이 평지 행성이 될 확률 (프로젝트 설정 HG 환경 맵의 "평지 행성 확률").
	HUOGUOHUOGUO_API TArray<FHGPlanetSeed> MakeApproachingPlanets(int32 UniverseSeed, int32 Cycle, int32 Count, float FlatChance = 0.f);

	// 행성 값이 어느 환경 종류인지. 폐우주선 > 소행성 지대 > 평지(PlanetLarge) > 크기 등급 순으로 본다.
	HUOGUOHUOGUO_API EHGSiteType SiteTypeOf(const FHGPlanetSeed& Planet);

	HUOGUOHUOGUO_API const TCHAR* BiomeName(EHGPlanetBiome Biome);

	// 생물 군계별 지면/바위 색.
	HUOGUOHUOGUO_API FLinearColor GroundColor(EHGPlanetBiome Biome);
	HUOGUOHUOGUO_API FLinearColor RockColor(EHGPlanetBiome Biome);
}
