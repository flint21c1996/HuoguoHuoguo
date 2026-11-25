#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HGScanWave.generated.h"

class AHGPlanetGenerator;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;

/**
 * 스캐너 파동 하나. 사장님 발밑에서 시작해 지금 서 있는 천체(행성 또는 밟은 소행성)의 지표를 따라 고리처럼 퍼져 나간다.
 *
 * 왜 "지표를 따라"인가:
 *   스캐너는 "내가 서 있는 땅"을 훑는 장비다 (사용자 결정). 그래서 파동은 그 천체 위에서만 퍼지고,
 *   다른 소행성이나 하늘의 물체에는 닿지 않는다.
 *
 * 그리는 방식 (사용자 제안: 데칼처럼, 닿은 것은 와이어프레임처럼):
 *   - 화면 효과(포스트 프로세스 머티리얼 M_HG_ScanPulse). 화면의 각 점을 월드 위치로 되돌려 시작점에서의 거리가
 *     지금 고리 반지름 근처면 빛나게 한다. 땅, 바위, 식물 어디에나 데칼처럼 얹힌다.
 *     고리 위와 바로 뒤의 표면에는 1m 격자 선이 비쳐 "분석 중" 느낌을 준다. 천체 안쪽(반지름의 1.25배)만 받는다.
 *   - 닿은 재료 둘레에는 빛나는 와이어 상자(모서리 12개)가 돌며 나타났다 사라진다.
 *   - 머티리얼 에셋이 없으면(맵 생성 도구를 안 돌렸으면) 예전처럼 지표에 판을 줄지어 놓는 고리로 대신 그린다.
 *
 * 퍼지는 방식:
 *   천체 중심 C, 반지름 R, 시작점의 "위" U. 지금 퍼진 표면 거리 d이면 고리는 U에서 각도 d / R 만큼 떨어진 원이다.
 *   화면 효과는 직선 거리를 쓰므로 표면 거리 d에 해당하는 직선 거리(현의 길이 2R sin(d / 2R))를 넘긴다.
 *   표식(재료)은 중심에서 본 방향이 고리 안쪽에 들어오면 "닿은" 것으로 친다 (같은 천체 위의 표식만).
 *
 * 닿은 표식: 미니맵에서 잠깐 깜빡이고, 숨은 재료면 드러나고, 도감에 "발견"으로 남는다. 다 퍼지면 결과를 알리고 사라진다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGScanWave : public AActor
{
	GENERATED_BODY()

public:
	AHGScanWave();

	/**
	 * 파동을 시작한다. Center/Radius = 천체 중심과 평균 반지름, Start = 사장님 위치, Range = 퍼질 표면 거리(cm).
	 * OnFinished: 다 퍼졌을 때 결과 문장을 받을 함수 (컨트롤러가 화면에 띄운다).
	 */
	void Begin(AHGPlanetGenerator* InPlanet, const FVector& InCenter, float InRadius, const FVector& Start, float InRange,
		TFunction<void(const FString&)> InOnFinished);

	virtual void Tick(float DeltaSeconds) override;

	// 퍼지는 데 걸리는 시간(초).
	UPROPERTY(EditAnywhere, Category = "Scan")
	float Duration = 2.2f;

	// 대신 그리는 고리(머티리얼이 없을 때)의 점 개수.
	UPROPERTY(EditAnywhere, Category = "Scan")
	int32 RingPoints = 72;

	// 와이어 상자가 보이는 시간(초)과 반 크기(cm).
	UPROPERTY(EditAnywhere, Category = "Scan")
	float AnalyzeSeconds = 1.4f;

	UPROPERTY(EditAnywhere, Category = "Scan")
	float AnalyzeHalfSize = 38.f;

protected:
	// 대신 그리는 고리 판들 (월드 좌표 인스턴스).
	UPROPERTY(VisibleAnywhere, Category = "Scan")
	TObjectPtr<UInstancedStaticMeshComponent> Ring;

	// 닿은 재료 둘레의 와이어 상자 모서리들 (월드 좌표 인스턴스).
	UPROPERTY(VisibleAnywhere, Category = "Scan")
	TObjectPtr<UInstancedStaticMeshComponent> Wires;

	// 화면 효과 (경계 없음 = 화면 전체).
	UPROPERTY(VisibleAnywhere, Category = "Scan")
	TObjectPtr<UPostProcessComponent> Effect;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EffectMaterial;

private:
	TWeakObjectPtr<AHGPlanetGenerator> Planet;
	FVector Center = FVector::ZeroVector;
	FVector StartPoint = FVector::ZeroVector;
	float Radius = 1000.f;
	float Range = 6000.f;
	FVector Up = FVector::UpVector;
	FVector Tangent1 = FVector::ForwardVector;
	FVector Tangent2 = FVector::RightVector;
	float Elapsed = 0.f;
	bool bWaveDone = false;
	TFunction<void(const FString&)> OnFinished;

	// 이미 닿은 표식 번호와 결과 집계.
	TSet<int32> Touched;
	int32 TouchedCount = 0;
	int32 RevealedCount = 0;
	int32 NewDiscoveries = 0;
	bool bLoggedFirst = false;

	// 와이어 상자 하나: 표식 위치, 그 자리의 "위", 나타난 시각(파동 경과 시간).
	struct FAnalyze
	{
		FVector Location;
		FVector Up;
		float Start;
	};
	TArray<FAnalyze> Analyzing;

	// 각도 Angle(라디안) 안에 들어온 표식을 처리한다.
	void TouchMarkers(float Angle);

	// 화면 효과 파라미터를 맞춘다 (고리 반지름, 세기).
	void UpdateEffect(float Angle, float Fade);

	// (머티리얼이 없을 때) 각도 Angle의 고리 점들을 지표에 맞춰 다시 놓는다.
	void PlaceRing(float Angle, float Fade);

	// 와이어 상자들을 다시 그린다. 다 사라졌으면 false.
	bool UpdateWires();
};
