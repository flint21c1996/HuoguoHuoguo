#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Planet/HGPlanetTypes.h"
#include "HGPlanetGenerator.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UBillboardComponent;

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

	// 착륙 지점 주변을 평평하게 다지는 반경(cm).
	UPROPERTY(EditAnywhere, Category = "Planet|Mesh")
	float LandingFlatRadius = 1000.f;

	UPROPERTY(VisibleAnywhere, Category = "Planet")
	TObjectPtr<UProceduralMeshComponent> Terrain;

	UPROPERTY(VisibleAnywhere, Category = "Planet")
	TObjectPtr<UInstancedStaticMeshComponent> RoundRocks;

	UPROPERTY(VisibleAnywhere, Category = "Planet")
	TObjectPtr<UInstancedStaticMeshComponent> BlockRocks;

	// 재료 표식. 0 = 버섯, 1 = 해초, 2 = 고추. 아직 채집 기능은 없고 위치만 보여 준다.
	UPROPERTY(VisibleAnywhere, Category = "Planet")
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

	void ChoosePlanet();
	void BuildTerrain();
	void ScatterProps();
	void SpawnReturnPad();
};
