#include "World/Light/HGStationLighting.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HuoguoHuoguo.h"
#include "Shop/Furniture/HGFurniture.h"

void UHGStationLighting::Start()
{
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		break;
	}
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		Sky = It->GetLightComponent();
		break;
	}
	if (Sun.IsValid())
	{
		DaySunIntensity = Sun->Intensity;
		DaySunColor = Sun->GetLightColor();
	}
	if (Sky.IsValid())
	{
		DaySkyIntensity = Sky->Intensity;
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Lighting] 정거장 조명: 해 %s (%.1f), 스카이 라이트 %s (%.2f)"),
		Sun.IsValid() ? TEXT("있음") : TEXT("없음"), DaySunIntensity, Sky.IsValid() ? TEXT("있음") : TEXT("없음"), DaySkyIntensity);
}

void UHGStationLighting::Apply(bool bInNight)
{
	bNight = bInNight;
	if (Sun.IsValid())
	{
		Sun->SetIntensity(bNight ? DaySunIntensity * NightSunScale : DaySunIntensity);
		Sun->SetLightColor(bNight ? NightSunColor : DaySunColor);
	}
	if (Sky.IsValid())
	{
		Sky->SetIntensity(bNight ? DaySkyIntensity * NightSkyScale : DaySkyIntensity);
	}
	int32 Lights = 0;
	for (TActorIterator<AHGFurniture> It(GetWorld()); It; ++It)
	{
		It->SetNightLight(bNight);
		++Lights;
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Lighting] %s 조명 (가구 %d개에 밤 불빛 %s)"), bNight ? TEXT("밤") : TEXT("낮"), Lights, bNight ? TEXT("켬") : TEXT("끔"));
}
