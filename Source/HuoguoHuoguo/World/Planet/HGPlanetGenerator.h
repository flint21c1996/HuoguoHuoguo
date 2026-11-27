#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/Planet/HGPlanetShape.h"
#include "World/Planet/HGPlanetTypes.h"
#include "HGPlanetGenerator.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UBillboardComponent;
class UStaticMesh;

// 폐우주선의 상호작용 지점 종류.
enum class EHGInteractType : uint8
{
	EnterShip, // 바깥 에어록 문: 안으로 들어간다 (인공 중력)
	ExitShip,  // 안쪽 에어록 문: 밖으로 나간다 (무중력)
	Blueprint, // 설계도 단말: 잠긴 가게 가구 하나를 연다
};

// 폐우주선의 상호작용 지점 하나 (E 키).
struct FHGInteractable
{
	EHGInteractType Type = EHGInteractType::EnterShip;
	FVector Location = FVector::ZeroVector;    // 상호작용 지점 (월드)
	FVector Destination = FVector::ZeroVector; // 들어가기/나가기: 옮겨 갈 자리 (월드)
	FName Furniture = NAME_None;               // 설계도: 열어 줄 가구 ID
	bool bUsed = false;                        // 설계도: 이미 썼다
};

// 행성 위 재료 표식 하나. 번호(배열 순서)는 시드로 정해지므로 같은 행성이면 언제나 같은 표식을 가리킨다.
struct FHGIngredientMarker
{
	int32 Kind = 0;               // IngredientIds/IngredientMarkers의 몇 번째 재료인지
	int32 Instance = INDEX_NONE;  // 그 재료 인스턴스 메시 안의 인스턴스 번호
	FVector Location = FVector::ZeroVector; // 월드 위치
	bool bHarvested = false;
	bool bOtherTime = false;      // 지금 시간대(낮/밤)에 나지 않는 재료라서 숨겼다
	bool bHidden = false;         // 숨은 재료: 스캐너 파동이 지나가야 드러난다
	float PingTime = -100.f;      // 스캐너 파동이 지나간 시각 (월드 시간, 미니맵이 잠깐 깜빡인다)

	// 지금 보이고 캘 수 있는지 (캐지 않았고, 지금 시간대 재료이고, 숨어 있지 않다).
	bool IsAvailable() const { return !bHarvested && !bOtherTime && !bHidden; }
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

	// 지형 모양 시험(HGPlanetTerrainTest.cpp)이 ChoosePlanet으로 행성을 정하고 지표 높이를 읽게 한다. 시험 빌드에만 쓰는 접근 허용이다.
	friend class FHGPlanetTerrainTest;

public:
	AHGPlanetGenerator();

	// 아직 만들지 않았으면 지금 만든다. 컨트롤러가 사장님을 땅 위에 세우기 전에 부른다.
	// 액터들의 BeginPlay 순서는 정해져 있지 않으므로, 지형이 먼저 있어야 하는 쪽이 직접 불러서 순서를 보장한다.
	void EnsureGenerated();

	// 중력의 중심 = 행성 중심. 평지 행성이면 땅 아래 아주 먼 곳(100km)이다.
	// 그러면 "중심에서 나를 향한 방향 = 위"로 계산하는 곳들(카메라, 스캐너, 표식)이 고칠 것 없이 평지에서 위 = +Z가 된다 (100m 거리에서 0.06도 차이).
	FVector GetGravityCenter() const { return bFlatSite ? GetActorLocation() - FVector(0.0, 0.0, FlatGravityDepth) : GetActorLocation(); }

	// 중력 중심에서 지표까지의 거리 (스캐너 파동 등). 평지면 FlatGravityDepth.
	float GetGravityRadius() const { return bFlatSite ? FlatGravityDepth : Planet.Radius; }

	// 평지 행성 맵인가 (랜드스케이프 분지 위를 걷는다).
	bool IsFlatSite() const { return bFlatSite; }

	// 평지 행성의 가짜 중력 중심 깊이(cm, 100km).
	static constexpr float FlatGravityDepth = 10000000.f;

	// 실제로 만든 행성 값.
	const FHGPlanetSeed& GetPlanet() const { return Planet; }

	// 실제로 쓴 지형 모양 값 (언덕, 능선, 크레이터 등).
	const FHGPlanetShape& GetShape() const { return TerrainShape; }

	// 실제로 판 크레이터 수 (착륙지와 겹쳐 못 놓은 것은 빠진다).
	int32 GetCraterCount() const { return Craters.Num(); }

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

	// ---- 스캐너 ----

	// 스캐너 파동이 표식에 닿았다: 깜빡임 시각을 남기고, 숨은 재료면 드러낸다. 숨은 재료를 드러냈으면 true.
	bool ScanMarker(int32 MarkerId, float WorldTime);

	// 스캔으로 드러나는 숨은 재료 비율 (0 ~ 1). 캔 것, 다른 시간대 것이 아닌 표식 중에서 시드로 고른다.
	UPROPERTY(EditAnywhere, Category = "Planet|Ingredients", meta = (ClampMin = "0", ClampMax = "1"))
	float HiddenIngredientRatio = 0.2f;

	// ---- 미니맵 등 표시용 ----

	// 재료 표식 전체 (캔 것, 다른 시간대 것 포함. IsAvailable()로 구분).
	const TArray<FHGIngredientMarker>& GetIngredientMarkers() const { return Markers; }

	// 재료 종류(Kind)의 표시 색. 범위를 벗어나면 흰색.
	FLinearColor GetIngredientColor(int32 Kind) const;

	// ---- 폐우주선 ----

	// 우주선의 "위" (월드). 안쪽 인공 중력은 이 반대 방향으로 끌고, 밖에서 떠다닐 때도 이 방향을 위로 삼아 시점을 맞춘다.
	FVector GetShipUp() const { return ShipFrame.GetRotation().GetUpVector(); }

	// 우주선 기준 좌표(방 배치에 쓴 좌표: X = 방이 이어지는 쪽, Y = 옆, Z = 위)를 월드로 바꾸는 변환.
	// 우주선 부품은 지형(루트 컴포넌트) 아래 ShipFrame만큼 놓였으므로 ShipFrame 다음에 이 액터의 변환을 적용한다.
	FTransform GetShipTransform() const { return ShipFrame * GetActorTransform(); }

	// From에서 MaxDistance 안의 가장 가까운 상호작용 지점 (쓴 설계도는 뺀다). 없으면 INDEX_NONE.
	int32 FindNearestInteractable(const FVector& From, float MaxDistance) const;
	const FHGInteractable& GetInteractable(int32 Index) const { return Interactables[Index]; }
	int32 GetInteractableCount() const { return Interactables.Num(); }

	// 우주선 윤곽 (월드, 바닥 높이의 네 모서리, 미니맵용). 폐우주선이 아니면 비어 있다.
	TArray<FVector> GetShipOutline() const;

	// 상호작용 안내 한 줄 ("[E] 우주선 안으로 들어가기" 등).
	FString DescribeInteractable(int32 Index) const;

	// 설계도 단말을 쓴다: 가구를 열고 결과 문장을 돌려준다. 같은 시드면 다시 내려와도 이미 쓴 것으로 남는다.
	FString UseBlueprint(int32 Index);

	// 중력을 가진 천체들. 0번은 늘 이 행성(소행성 지대면 착륙하는 본 소행성)이고, 소행성 지대면 둘레 소행성들이 뒤에 붙는다.
	const TArray<FHGGravityBody>& GetGravityBodies() const { return GravityBodies; }

	// 지형 볼거리(탁상 바위, 아치)의 바닥 가운데 위치 (월드). 만든 순서대로, 처음 둘이 착륙 지점 둘레다.
	const TArray<FVector>& GetLandmarkLocations() const { return LandmarkLocations; }

	// 귀환 발판 위치. 발판이 없으면 false.
	bool GetReturnPadLocation(FVector& OutLocation) const;

#if WITH_EDITOR
	// ---- 에디터 미리보기 (플레이하지 않고 지형만 본다) ----

	// 지금 값으로 미리보기 지형을 다시 만든다. 게임 월드에서는 아무것도 하지 않는다.
	UFUNCTION(CallInEditor, Category = "Planet|Editor Preview", meta = (DisplayName = "미리보기 다시 만들기"))
	void RebuildEditorPreview();

	// 미리보기 지형을 지운다.
	UFUNCTION(CallInEditor, Category = "Planet|Editor Preview", meta = (DisplayName = "미리보기 지우기"))
	void ClearEditorPreview();

	// 값을 바꾸거나 액터를 옮긴 뒤 에디터가 부른다. bPreviewInEditor가 켜져 있으면 미리보기를 다시 만든다.
	virtual void OnConstruction(const FTransform& Transform) override;

	// Planet Maker 창이 부른다: 행성 값과 지형 모양을 미리보기 값으로 넣고(되돌리기 가능) 미리보기를 다시 만든다.
	void SetEditorPreview(const FHGPlanetSeed& InPlanet, const FHGPlanetShape& InShape);

	// Planet Maker 평지 탭이 랜드스케이프를 만든 뒤 부른다: 이 생성기를 평지 방식으로 두고 땅 반폭(cm)을 맞춘다.
	void SetFlatSite(const FHGPlanetSeed& InPlanet, float HalfExtent);
#endif

protected:
	virtual void BeginPlay() override;

	// 켜면 에디터에서(플레이하지 않아도) 값을 바꿀 때마다 지형을 다시 만들어 보여 준다. 슬라이더는 놓을 때 한 번 만든다.
	// 미리보기 메시는 저장하지 않는 임시 부품이라 맵 파일이 커지지 않는다. 지형만 만들고(바위, 재료, 하늘 없음) 충돌도 만들지 않는다.
	UPROPERTY(EditAnywhere, Category = "Planet|Editor Preview", meta = (DisplayName = "에디터에서 미리보기"))
	bool bPreviewInEditor = false;

	// 미리보기 지형 메시 (임시, 저장 안 함, 플레이용 복제에도 따라가지 않음).
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UProceduralMeshComponent> EditorPreviewTerrain;

	// ---- 평지 행성 (2026-11-27) ----

	// 켜면 구 지형을 만들지 않고, 맵에 놓인 땅(랜드스케이프) 위에 바위, 재료, 식물, 귀환 발판을 놓는다. 중력은 아래(-Z)로.
	// 배치는 구 행성과 같은 규칙(같은 시드 = 같은 배치)을 쓰고, 구 위의 방향을 평지 위 원판으로 넓이를 지켜 옮긴다(램버트 정적 방위 투영).
	// 착륙 지점 = 이 액터 바로 아래 땅. 액터를 분지 가운데에 둔다.
	UPROPERTY(EditAnywhere, Category = "Planet|Flat", meta = (DisplayName = "평지 행성 맵"))
	bool bFlatSite = false;

	// 배치할 원판의 반지름(cm) = 땅 반폭. 랜드스케이프보다 크면 땅 밖 허공에 놓이므로 Planet Maker가 랜드스케이프를 만들 때 맞춰 둔다.
	// 재료와 바위 개수는 이 원판 넓이에 비례한다.
	UPROPERTY(EditAnywhere, Category = "Planet|Flat", meta = (DisplayName = "평지 반폭(cm)", EditCondition = "bFlatSite", ClampMin = "1000"))
	float FlatHalfExtent = 10000.f;

	// 정거장을 거치지 않고 이 맵을 바로 열었을 때(에디터에서 맵을 열고 플레이 등) 만들 행성.
	UPROPERTY(EditAnywhere, Category = "Planet")
	FHGPlanetSeed PreviewPlanet;

	// 켜면 PreviewPlanet의 Seed만 쓰고 나머지 값은 시드로 다시 뽑는다. 끄면 PreviewPlanet 값을 그대로 쓴다(수치 실험용).
	UPROPERTY(EditAnywhere, Category = "Planet")
	bool bPreviewFromSeedOnly = true;

	// 켜면 이 맵을 바로 열었을 때 지형 모양을 PreviewShape 값 그대로 쓴다 (Planet Maker에서 만지는 값).
	// 끄면 행성 값(군계, 크기)에 맞는 기본 모양(HGPlanet::DefaultShape)을 쓴다. 정거장에서 다이빙해 오면 늘 기본 모양이다.
	UPROPERTY(EditAnywhere, Category = "Planet|Shape")
	bool bUsePreviewShape = false;

	// 지형 모양 값: 언덕, 능선, 잔 굴곡, 크레이터 개수와 크기, 착륙지 평탄 반경, 높이 곡선.
	UPROPERTY(EditAnywhere, Category = "Planet|Shape", meta = (EditCondition = "bUsePreviewShape"))
	FHGPlanetShape PreviewShape;

	// 귀환 발판이 보낼 맵과 도착 태그.
	UPROPERTY(EditAnywhere, Category = "Planet|Return")
	TSoftObjectPtr<UWorld> ReturnLevel;

	UPROPERTY(EditAnywhere, Category = "Planet|Return")
	FName ReturnArrivalTag = TEXT("FromPlanet");

	UPROPERTY(EditAnywhere, Category = "Planet|Return")
	FText ReturnName;

	// 지형 격자 한 칸의 목표 길이(cm). 작을수록 지형이 매끈하지만 삼각형이 많아진다.
	// 2026-11-27 행성 크기를 절반으로 줄이면서 150 → 100. 작은 행성(반지름 20 ~ 60m)도 크레이터와 둔덕을 담을 만큼 촘촘하게 하고,
	// 삼각형 수는 줄어든 넓이 덕분에 예전과 비슷하다 (60m 행성 면당 95칸, 약 11만 삼각형). 중형은 면당 128칸 상한 그대로.
	UPROPERTY(EditAnywhere, Category = "Planet|Mesh", meta = (ClampMin = "50"))
	float TargetEdgeLength = 100.f;

	// 면 하나의 최대 격자 수. 128이면 면당 약 3만 3천 삼각형, 행성 전체 약 20만 삼각형.
	UPROPERTY(EditAnywhere, Category = "Planet|Mesh", meta = (ClampMin = "8", ClampMax = "255"))
	int32 MaxGridPerFace = 128;

	// 지형이 들쭉날쭉한 정도. 0이면 큰 언덕만(예전처럼 완만), 1이면 날카로운 능선 + 약 9m 폭 잔 굴곡을 얹는다.
	// 높을수록 가파른 곳이 늘어 걸어 오르지 못하는 비탈이 생길 수 있다.
	// 지형 모양 값(능선 세기, 둔덕 높이)에 한 번 더 곱하는 맵 전체 배율이다.
	UPROPERTY(EditAnywhere, Category = "Planet|Mesh", meta = (ClampMin = "0", ClampMax = "3"))
	float TerrainRuggedness = 1.f;

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

	// 지형 볼거리: 탁상 바위/아치를 몇 개 세울지 (착륙 지점 둘레 2개 + 나머지). 0이면 세우지 않는다.
	UPROPERTY(EditAnywhere, Category = "Planet|Landmarks", meta = (ClampMin = "0", ClampMax = "40"))
	int32 LandmarkCount = 8;

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

	// 실제로 쓰는 지형 모양 값. ChoosePlanet이 PreviewShape 또는 기본 모양으로 정한다.
	FHGPlanetShape TerrainShape;

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

	// 중력 천체 (GetGravityBodies).
	TArray<FHGGravityBody> GravityBodies;

	// 폐우주선: 우주선 기준 변환(행성 중심 기준, 위 = 우주선 바닥의 반대), 시작 변환(월드, 우주선을 바라보며 떠 있음), 상호작용 지점.
	FTransform ShipFrame = FTransform::Identity;
	FTransform DerelictStart = FTransform::Identity;
	TArray<FHGInteractable> Interactables;

	// 폐우주선을 만든다 (HGDerelict.cpp): 선체와 방, 문, 재료(선체 밖, 안쪽 상자), 설계도 단말, 조명, 떠 있는 잔해, 귀환 포드.
	void BuildDerelict();

	// 착륙 지점을 평평하게 다지는 실제 반경 = 평탄 반경 값, 단 반지름의 30%를 넘지 않는다 (소행성 지대 본 소행성 10 ~ 15m는 3 ~ 4.5m).
	// 행성도 작으면(반지름 33m 미만) 같은 규칙으로 줄인다. 그렇지 않으면 20m 행성의 반구 가까이가 평평해진다 (2026-11-27, 행성 크기 절반).
	float FlatRadius() const { return FMath::Min(TerrainShape.LandingFlatRadius, Planet.Radius * 0.3f); }

	// 소행성 지대: 본 소행성 둘레에 뛰어 건널 소행성 16 ~ 22개를 만들고(매끈한 메시, 충돌), 소행성마다 재료 1 ~ 2개를 놓는다.
	// 행성이면 중력 천체 목록에 행성 하나만 넣는다.
	void BuildAsteroidField();

	// 지형 볼거리 위치 (월드, GetLandmarkLocations).
	TArray<FVector> LandmarkLocations;

	// 하늘의 고리 행성, 위성, 정거장, 별 (ApplyMoodPreview에서 부른다).
	void BuildSkyBodies();

	// 탁상 바위와 아치 (매끈한 메시, 충돌 있음). 재료 표식과 귀환 발판 자리는 피한다.
	int32 BuildLandmarks();

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
	// 평지 행성이면 Dir을 평지 위 점으로 옮기고(FlatPointOf) 위 = +Z.
	FTransform SurfaceFrame(const FVector& Dir, float Spin, float Lift) const;

	// 구 위 방향 Dir에 해당하는 지표 점(액터 기준)과 그 점의 "위". 구면 Dir * 지표 거리 / 평지 원판 위 땅 높이.
	FVector SurfacePointOf(const FVector& Dir) const;
	FVector SurfaceUpOf(const FVector& Dir) const { return bFlatSite ? FVector::UpVector : Dir; }

	// 평지: 구 위 방향 → 원판 위 XY (액터 기준). 북극(착륙 지점) = 원판 가운데, 남극 = 원판 가장자리.
	// 반지름 = 반폭 x sin(극에서 각도 / 2)라 구 위에 고르게 뿌린 점이 원판 위에도 고르게 퍼진다 (넓이 보존).
	FVector2D FlatPointOf(const FVector& Dir) const;

	// 평지: 액터 기준 XY에서 아래로 쏴서 땅 높이(액터 기준 Z)를 찾는다. 땅이 없으면 0.
	double FlatGroundZ(const FVector2D& XY) const;

	// 평지: 랜드스케이프 머티리얼의 색 값(Ground, GroundB, Rock)을 군계 색으로.
	void PaintFlatGround();

	// 새로 만든 표식(번호 MarkerId)을 숨은 재료로 둘지 시드로 정하고, 그렇다면 숨긴다. 이미 캤거나 다른 시간대면 건드리지 않는다.
	void MaybeHideForScan(int32 MarkerId);

	// 표식 하나의 인스턴스를 안 보이게 한다 (크기를 0으로). 인스턴스를 지우면 뒤 번호가 당겨지므로 지우지 않는다.
	// 왜 숨겼는지(캠 / 다른 시간대)는 부르는 쪽이 표식의 플래그로 남긴다.
	void HideInstance(int32 MarkerId);

	void ChoosePlanet();

	// 지형 메시를 Target에 만든다. 게임에서는 Terrain(충돌 있음), 에디터 미리보기에서는 EditorPreviewTerrain(충돌 없음).
	void BuildTerrain(UProceduralMeshComponent* Target, bool bCreateCollision);
	void ScatterProps();
	void SpawnReturnPad();
};
