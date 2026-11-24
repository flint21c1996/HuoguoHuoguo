#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Planet/HGPlanetTypes.h"
#include "HGPlanetGenerator.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UBillboardComponent;
class UStaticMesh;

// 행성 위 재료 표식 하나. 번호(배열 순서)는 시드로 정해지므로 같은 행성이면 언제나 같은 표식을 가리킨다.
struct FHGIngredientMarker
{
	int32 Kind = 0;               // IngredientIds/IngredientMarkers의 몇 번째 재료인지
	int32 Instance = INDEX_NONE;  // 그 재료 인스턴스 메시 안의 인스턴스 번호
	FVector Location = FVector::ZeroVector; // 월드 위치
	bool bHarvested = false;
	bool bOtherTime = false;      // 지금 시간대(낮/밤)에 나지 않는 재료라서 숨겼다

	// 지금 보이고 캘 수 있는지 (캐지 않았고 지금 시간대 재료).
	bool IsAvailable() const { return !bHarvested && !bOtherTime; }
};

/**
 * 행성 표면 맵(L_PlanetSurface)에 하나 배치하는 절차적 행성.
 *
 * 하는 일:
 *   1) 정거장에서 다이빙해 왔으면 GameInstance가 넘겨준 행성 값을, 이 맵을 바로 열었으면 PreviewPlanet을 쓴다.
 *   2) 큐브스피어(정육면체 여섯 면을 격자로 나눠 구로 부풀린 것)에 노이즈로 높낮이를 주어 지형 메시를 만든다.
 *   3) 같은 시드로 바위와 재료를 표면에 흩고, 착륙 지점 옆에 정거장 귀환 발판을 세운다.
 *   행성의 중심은 이 액터의 위치이고, 사장님은 이 중심을 향하는 중력을 받는다.
 *
 * 왜 큐브스피어인가:
 *   위도/경도 격자 구(UV 구)는 양 극으로 갈수록 삼각형이 바늘처럼 모인다. 그 위를 걸으면 극에서 지형이 찌그러진다.
 *   큐브스피어는 어디서나 격자 크기가 비슷해서 행성 전체를 고르게 걸을 수 있다.
 *
 * 맵에는 조명과 이 액터만 배치해 두고, 지형은 게임을 시작할 때 만든다. 손으로 다듬는 곳(정거장)은 맵 배치로,
 * 매번 달라지는 곳(행성)은 시드로 만든다는 역할 분담이다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGPlanetGenerator : public AActor
{
	GENERATED_BODY()

public:
	AHGPlanetGenerator();

	// 아직 만들지 않았으면 지금 만든다. 컨트롤러가 사장님을 땅 위에 세우기 전에 부른다.
	// 액터들의 BeginPlay 순서는 정해져 있지 않으므로, 지형이 먼저 있어야 하는 쪽이 직접 불러서 순서를 보장한다.
	void EnsureGenerated();

	// 중력의 중심 = 행성 중심.
	FVector GetGravityCenter() const { return GetActorLocation(); }

	// 실제로 만든 행성 값.
	const FHGPlanetSeed& GetPlanet() const { return Planet; }

	// 착륙 지점(북극)에 캡슐 발이 닿도록 선 변환. 위쪽(Z)이 지표 바깥을 향한다.
	FTransform GetLandingTransform() const;

	// 중심에서 Dir(단위 벡터) 방향으로 지표까지의 거리. 지형 메시와 배치가 모두 이 함수 하나를 쓴다.
	float SurfaceRadiusAt(const FVector& Dir) const;

	// ---- 재료 채집 ----

	// From에서 MaxDistance 안에 있는 가장 가까운 캐지 않은 재료 표식 번호. 없으면 INDEX_NONE.
	int32 FindNearestIngredient(const FVector& From, float MaxDistance) const;

	// 표식의 재료 ID(DataTable 행 이름)와 위치. 번호가 틀리면 NAME_None / 원점.
	FName GetMarkerIngredient(int32 MarkerId) const;
	FVector GetMarkerLocation(int32 MarkerId) const;

	// 표식을 캔다: 표식을 숨기고 GameInstance에 "이 행성의 이 번호는 캤음"을 남긴다. 캔 재료 ID를 돌려준다.
	// 이미 캤거나 번호가 틀리면 NAME_None.
	FName HarvestIngredient(int32 MarkerId);

	// 지금 캘 수 있는 재료 표식 수 (캐지 않았고 지금 시간대 재료).
	int32 GetRemainingIngredientCount() const;

	// ---- 미니맵 등 표시용 ----

	// 재료 표식 전체 (캔 것, 다른 시간대 것 포함. IsAvailable()로 구분).
	const TArray<FHGIngredientMarker>& GetIngredientMarkers() const { return Markers; }

	// 재료 종류(Kind)의 표시 색. 범위를 벗어나면 흰색.
	FLinearColor GetIngredientColor(int32 Kind) const;

	// 귀환 발판 위치. 발판이 없으면 false.
	bool GetReturnPadLocation(FVector& OutLocation) const;

protected:
	virtual void BeginPlay() override;

	// 정거장을 거치지 않고 이 맵을 바로 열었을 때(에디터에서 맵을 열고 플레이 등) 만들 행성.
	UPROPERTY(EditAnywhere, Category = "Planet")
	FHGPlanetSeed PreviewPlanet;

	// 켜면 PreviewPlanet의 Seed만 쓰고 나머지 값은 시드로 다시 뽑는다. 끄면 PreviewPlanet 값을 그대로 쓴다(수치 실험용).
	UPROPERTY(EditAnywhere, Category = "Planet")
	bool bPreviewFromSeedOnly = true;

	// 귀환 발판이 보낼 맵과 도착 태그.
	UPROPERTY(EditAnywhere, Category = "Planet|Return")
	TSoftObjectPtr<UWorld> ReturnLevel;

	UPROPERTY(EditAnywhere, Category = "Planet|Return")
	FName ReturnArrivalTag = TEXT("FromPlanet");

	UPROPERTY(EditAnywhere, Category = "Planet|Return")
	FText ReturnName;

	// 지형 격자 한 칸의 목표 길이(cm). 작을수록 지형이 매끈하지만 삼각형이 많아진다.
	UPROPERTY(EditAnywhere, Category = "Planet|Mesh", meta = (ClampMin = "50"))
	float TargetEdgeLength = 150.f;

	// 면 하나의 최대 격자 수. 128이면 면당 약 3만 3천 삼각형, 행성 전체 약 20만 삼각형.
	UPROPERTY(EditAnywhere, Category = "Planet|Mesh", meta = (ClampMin = "8", ClampMax = "255"))
	int32 MaxGridPerFace = 128;

	// 지형이 들쭉날쭉한 정도. 0이면 큰 언덕만(예전처럼 완만), 1이면 날카로운 능선 + 약 9m 폭 잔 굴곡을 얹는다.
	// 높을수록 가파른 곳이 늘어 걸어 오르지 못하는 비탈이 생길 수 있다.
	UPROPERTY(EditAnywhere, Category = "Planet|Mesh", meta = (ClampMin = "0", ClampMax = "3"))
	float TerrainRuggedness = 1.f;

	// 착륙 지점 주변을 평평하게 다지는 반경(cm).
	UPROPERTY(EditAnywhere, Category = "Planet|Mesh")
	float LandingFlatRadius = 1000.f;

	UPROPERTY(VisibleAnywhere, Category = "Planet")
	TObjectPtr<UProceduralMeshComponent> Terrain;

	UPROPERTY(VisibleAnywhere, Category = "Planet")
	TObjectPtr<UInstancedStaticMeshComponent> RoundRocks;

	UPROPERTY(VisibleAnywhere, Category = "Planet")
	TObjectPtr<UInstancedStaticMeshComponent> BlockRocks;

	// 재료 표식 모양 (구). 재료 종류마다 이 모양의 인스턴스 메시를 실행 중에 하나씩 만들고 재료 색으로 칠한다.
	UPROPERTY(EditAnywhere, Category = "Planet|Ingredients")
	TObjectPtr<UStaticMesh> MarkerMesh;

	// 그림체(hg.PlanetMood): 하늘 돔 반지름(cm). 가장 큰 행성과 멀리 보이는 것보다 커야 한다.
	UPROPERTY(EditAnywhere, Category = "Planet|Mood")
	float SkyDomeRadius = 300000.f;

	// 그림체: 하늘 밝기 (하늘 머티리얼 Brightness). 노출이 EV 2.5로 고정이라 여기서 맞춘다.
	UPROPERTY(EditAnywhere, Category = "Planet|Mood")
	float SkyBrightness = 3.f;

	// 그림체: 하늘빛 간접광 세기 (실시간 캡처 스카이 라이트).
	UPROPERTY(EditAnywhere, Category = "Planet|Mood")
	float SkyLightIntensity = 1.f;

	// 그림체: 하늘에 각진 구름을 띄울지. 기본은 끔 (우주의 작은 행성에 구름이 있으면 어색하다).
	UPROPERTY(EditAnywhere, Category = "Planet|Mood")
	bool bSkyClouds = false;

	// 그림체: 하늘 꼭대기 색을 우주색(거의 검은 남색) 쪽으로 섞는 비율. 대기가 얇아 우주가 비쳐 보이는 느낌. 0 = 팔레트 그대로.
	UPROPERTY(EditAnywhere, Category = "Planet|Sky", meta = (ClampMin = "0", ClampMax = "1"))
	float SpaceTint = 0.45f;

	// 그림체: 하늘의 별 개수와 밝기 (발광 머티리얼 Glow). 낮에도 보이도록 밝게 둔다.
	UPROPERTY(EditAnywhere, Category = "Planet|Sky")
	int32 StarCount = 1400;

	UPROPERTY(EditAnywhere, Category = "Planet|Sky")
	float StarGlow = 10.f;

	// 그림체: 하늘에 걸린 고리 행성, 위성, 정거장을 띄울지.
	UPROPERTY(EditAnywhere, Category = "Planet|Sky")
	bool bSkyBodies = true;

	// 그림체: 외계 식물의 빛나는 끝(구슬, 관 끝, 덩굴 끝)이 스스로 빛나는 세기 (발광 머티리얼 Glow).
	UPROPERTY(EditAnywhere, Category = "Planet|Mood")
	float FloraGlow = 1.5f;

	// 그림체: 구름이 스스로 빛나는 세기 (발광 머티리얼 Glow).
	UPROPERTY(EditAnywhere, Category = "Planet|Mood")
	float CloudGlow = 0.6f;

	// 재료 표식 크기 배율 (1 = 지름 1m).
	UPROPERTY(EditAnywhere, Category = "Planet|Ingredients")
	float MarkerScale = 0.4f;

	// 재료 종류별 인스턴스 메시. IngredientIds와 같은 순서이고, 재료 DataTable의 행 순서를 따른다.
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Planet|Ingredients")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> IngredientMarkers;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> EditorIcon;
#endif

private:
	bool bGenerated = false;
	FHGPlanetSeed Planet;

	// 시드마다 노이즈 공간의 다른 곳을 읽도록 옮기는 값. 같은 노이즈 함수로 서로 다른 지형을 얻는다.
	FVector NoiseOffset = FVector::ZeroVector;

	// 재료 종류 ID (IngredientMarkers와 같은 순서)와 표식 전체.
	TArray<FName> IngredientIds;
	TArray<FLinearColor> IngredientColors;
	TArray<FHGIngredientMarker> Markers;

	bool bHasReturnPad = false;
	FVector ReturnPadLocation = FVector::ZeroVector;

	// 크레이터 하나: 행성 중심에서 본 방향, 반지름(cm), 깊이(cm). ChoosePlanet이 시드로 정하고 SurfaceRadiusAt이 높이에 더한다.
	struct FCrater
	{
		FVector Dir = FVector::UpVector;
		float Radius = 800.f;
		float Depth = 200.f;
	};
	TArray<FCrater> Craters;

	// 하늘의 고리 행성, 위성, 정거장, 별 (ApplyMoodPreview에서 부른다).
	void BuildSkyBodies();

	/**
	 * 행성 그림체 (콘솔 변수 hg.PlanetMood, 기본 1). 모양은 복셀, 색과 하늘은 노 맨즈 스카이에서 배운 방향이다.
	 * 켜져 있으면 생성이 끝난 뒤:
	 *   - 하늘: 행성을 감싸는 큰 구(하늘 돔)에 하늘 머티리얼을 입힌다. "위"를 행성 중심에서 계산하므로 행성 어디서나 맞다.
	 *     하늘 머티리얼이 하늘(Is Sky)로 표시되어 있어서, 실시간 캡처 스카이 라이트가 돔 색을 그늘에 비춘다.
	 *   - 바위: 매끈한 바위는 숨기고(충돌은 그대로) 같은 자리에 15cm 복셀 껍질 바위를 그린다.
	 *   - 나무: 군계마다 다른 20cm 복셀 나무 (얼음 = 수정 기둥, 버섯숲 = 큰 버섯, 바위 = 자홍색 침엽수).
	 *   - 풀과 꽃: 착륙 지점 둘레에 빽빽하게, 멀수록 드문드문 (그림만, 충돌 없음).
	 *   - 하늘: 땅처럼 각진 떠 있는 바위섬. 각진 구름은 bSkyClouds를 켰을 때만 (기본 끔).
	 * 지형 자체는 BuildTerrain이 이 변수를 보고 각진 면 + 면마다 색으로 만든다.
	 */
	void ApplyMoodPreview();

	// 지표 Dir 위에 놓을 변환 (위 = 지표 바깥, 둘레 방향 Spin). 행성 중심 기준 좌표.
	FTransform SurfaceFrame(const FVector& Dir, float Spin, float Lift) const;

	// 표식 하나의 인스턴스를 안 보이게 한다 (크기를 0으로). 인스턴스를 지우면 뒤 번호가 당겨지므로 지우지 않는다.
	// 왜 숨겼는지(캠 / 다른 시간대)는 부르는 쪽이 표식의 플래그로 남긴다.
	void HideInstance(int32 MarkerId);

	void ChoosePlanet();
	void BuildTerrain();
	void ScatterProps();
	void SpawnReturnPad();
};
