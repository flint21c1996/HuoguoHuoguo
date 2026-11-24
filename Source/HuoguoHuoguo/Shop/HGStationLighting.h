#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HGStationLighting.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;

/**
 * 정거장 조명을 하루 시간대에 맞춘다 (경영 컨트롤러에 붙는 컴포넌트, 복셀 + 조명 시제품).
 *
 * 낮: 맵에 놓인 그대로 (해 8, 스카이 라이트 1.2).
 * 밤과 문 닫을 시간: 해를 NightSunScale배로 줄이고 푸른 달빛 색으로, 스카이 라이트도 줄인다.
 *   그리고 가구의 밤 불빛(등불, 홍등, 식탁 냄비, 수조)을 켠다. 노출은 맵의 볼륨(EV 2.0)에 고정돼 있어서
 *   가게가 어두워지고 불빛 둘레만 밝아진다.
 *
 * 맵의 해와 스카이 라이트를 처음 찾을 때 낮 값을 기억해 두고 거기서 계산한다. 맵을 다시 열면 다시 찾는다.
 */
UCLASS(ClassGroup = (Huoguo))
class HUOGUOHUOGUO_API UHGStationLighting : public UActorComponent
{
	GENERATED_BODY()

public:
	// 맵 시작에 한 번. 해와 스카이 라이트를 찾아 낮 값을 기억한다.
	void Start();

	// 밤 조명으로 바꾸거나 낮으로 되돌린다. 가구의 밤 불빛도 함께 켜고 끈다.
	void Apply(bool bNight);

	bool IsNight() const { return bNight; }

	// 밤에 해 밝기 배율과 색.
	UPROPERTY(EditAnywhere, Category = "Lighting")
	float NightSunScale = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Lighting")
	FLinearColor NightSunColor = FLinearColor(0.55f, 0.65f, 1.0f);

	// 밤에 스카이 라이트 밝기 배율.
	UPROPERTY(EditAnywhere, Category = "Lighting")
	float NightSkyScale = 0.35f;

private:
	TWeakObjectPtr<UDirectionalLightComponent> Sun;
	TWeakObjectPtr<USkyLightComponent> Sky;
	float DaySunIntensity = 0.f;
	FLinearColor DaySunColor = FLinearColor::White;
	float DaySkyIntensity = 0.f;
	bool bNight = false;
};
