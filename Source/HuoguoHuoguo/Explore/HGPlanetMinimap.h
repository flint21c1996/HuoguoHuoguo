#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HGPlanetMinimap.generated.h"

class AHGPlanetGenerator;

/**
 * 탐험 파트 오른쪽 위의 원형 미니맵. 행성(구) 표면을 사장님 중심으로 펼쳐서 그린다.
 *
 * 왜 평면 지도가 아닌가:
 *   행성은 공이라서 "위에서 내려다본 평면 지도"가 없다. 위도/경도로 펼치면 극 근처가 크게 찌그러지고,
 *   착륙 지점(북극) 주변이 하필 가장 찌그러진다.
 *
 * 쓰는 방법: 방위 등거리 투영 (사장님이 선 자리가 지도 중심)
 *   - 행성 위의 점 P에 대해, 행성 중심에서 본 "사장님 방향"과 "P 방향" 사이 각도 theta를 구한다.
 *     지도 중심에서 P까지의 거리 = theta * 행성 반지름 = 표면을 따라 걸어가야 하는 거리.
 *   - P가 어느 쪽에 있는지(방위)는 사장님 발밑 접평면에서 "P 쪽으로 출발하는 방향"으로 정한다.
 *   - 지도의 위쪽 = 지금 카메라가 보는 방향(지표에 눕힌 것). 그래서 지도에서 위에 보이면 W로 가면 된다.
 *   이 투영은 중심에서 본 거리와 방향이 정확하다. 멀리 있는 것끼리의 모양은 찌그러지지만 "어디로 몇 m"는 맞다.
 *
 * 두 가지 범위 (M 키):
 *   - 근거리: 반경 LocalRangeCm 안만 그린다. 범위 밖의 귀환 발판은 테두리에 화살표로 표시한다.
 *   - 행성 전체: 반경 = 반 바퀴(pi * 반지름). 행성의 모든 점이 원 안에 들어오고, 테두리 = 행성 정반대편.
 *     안쪽 흐린 원은 "중심에서 90도 떨어진 선"이다. 그 바깥이 행성 반대쪽 반구다.
 *
 * 위젯 에셋 없이 C++ 클래스만으로 만들고 NativePaint에서 Slate로 직접 그린다.
 * 위젯은 화면 전체를 덮고(입력은 받지 않음), 지도는 화면 오른쪽 위 모서리에서 ScreenMargin만큼 띄워 그린다.
 * 나중에 이 클래스를 상속한 위젯 블루프린트로 테두리 그림 등 장식을 얹을 수 있다.
 */
UCLASS()
class HUOGUOHUOGUO_API UHGPlanetMinimap : public UUserWidget
{
	GENERATED_BODY()

public:
	// 그릴 행성과 지도 중심이 될 사장님을 정한다.
	void Init(AHGPlanetGenerator* InPlanet, APawn* InViewer);

	// 근거리 <-> 행성 전체 범위를 바꾼다.
	void ToggleWholePlanet() { bWholePlanet = !bWholePlanet; }
	bool IsWholePlanet() const { return bWholePlanet; }

	/**
	 * 구 표면의 점 Point를 Viewer 중심 지도 좌표로 옮긴다 (방위 등거리 투영).
	 * 결과: X = 오른쪽, Y = 앞쪽(지도 위쪽). 길이 = 표면을 따라 걷는 거리(cm, 평균 반지름 기준).
	 * ViewForward는 아무 방향이어도 된다. 사장님 발밑 접평면에 눕혀서 "앞"으로 쓴다.
	 */
	static FVector2D ProjectToMap(const FVector& PlanetCenter, float PlanetRadius, const FVector& ViewerLocation, const FVector& ViewForward, const FVector& Point);

	// 지금 화면 기준(카메라가 보는 방향)으로 귀환 발판의 지도 좌표. 발판이나 행성이 없으면 false.
	bool GetReturnPadOnMap(FVector2D& OutMapCm) const;

	/**
	 * 레이더 방식 (소행성 지대, 폐우주선): 걸어 다닐 하나의 구면이 없으므로 위치를 "평면에 눕혀" 그린다.
	 *   기준 평면 = 사장님의 지금 "위"(중력 기준, 무중력이면 우주선의 위)에 수직인 평면, 지도 위쪽 = 카메라가 보는 방향.
	 *   점 P: 오른쪽 = (P - 사장님) · 오른쪽, 앞 = (P - 사장님) · 앞. 사장님보다 3m 넘게 위/아래면 작은 위/아래 삼각형을 덧그린다.
	 *   소행성은 크기대로 원(지금 서 있거나 끌리는 것은 밝게), 우주선은 윤곽선, 문은 청록, 설계도 단말은 금색.
	 *   M 키 = 반경 RadarNearCm <-> RadarFarCm.
	 */
	static FVector2D ProjectToRadar(const FVector& ViewerLocation, const FVector& Up, const FVector& ViewForward, const FVector& Point, float* OutHeight = nullptr);

	// 근거리 범위의 반경(cm).
	UPROPERTY(EditAnywhere, Category = "Minimap")
	float LocalRangeCm = 4000.f;

	// 레이더 반경(cm): 가까이 / 멀리 (M 키).
	UPROPERTY(EditAnywhere, Category = "Minimap")
	float RadarNearCm = 4000.f;

	UPROPERTY(EditAnywhere, Category = "Minimap")
	float RadarFarCm = 12000.f;

	// 원의 지름(Slate 단위, 화면 해상도 배율이 곱해진다).
	UPROPERTY(EditAnywhere, Category = "Minimap")
	float MapDiameter = 220.f;

	// 화면 오른쪽 위 모서리에서 지도까지 띄우는 거리(Slate 단위).
	UPROPERTY(EditAnywhere, Category = "Minimap")
	float ScreenMargin = 24.f;

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TWeakObjectPtr<AHGPlanetGenerator> Planet;
	TWeakObjectPtr<APawn> Viewer;
	bool bWholePlanet = false;

	// 처음 그릴 때 위치와 크기를 로그로 한 번 남긴다 (화면 배치 확인용).
	mutable bool bLoggedFirstPaint = false;

	// 지도 위쪽이 될 방향: 플레이어 카메라(컨트롤 회전)가 보는 방향. 없으면 사장님 몸 방향.
	FVector GetViewForward() const;

	// 레이더 방식인가 (소행성 지대, 폐우주선).
	bool IsRadar() const;

	// 레이더의 기준 "위": 사장님의 중력 기준 위 (탐험 사장님이 아니면 월드 Z).
	FVector GetRadarUp() const;

	// 레이더 방식으로 그린다 (NativePaint가 공통 배경 뒤에 부른다).
	int32 PaintRadar(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 Layer,
		const FVector2f& Origin, const FVector2f& Mid, float D, float Inner) const;
};
