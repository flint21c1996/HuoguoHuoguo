#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HAL/IConsoleManager.h"
#include "HGLookSettings.generated.h"

namespace HGLook
{
	// 윤곽선에서 빼는 물체(행성 외계 식물)의 커스텀 스텐실 번호. 그림체 머티리얼의 NoLineStencil과 같아야 한다.
	constexpr int32 NoOutlineStencil = 1;

	// 지금 셀 그림체가 켜져 있는가 (콘솔 변수 hg.Look >= 1).
	// 켜져 있을 때만 스텐실 물체를 커스텀 깊이 패스에 그린다 (꺼져 있으면 그 패스 비용이 0).
	inline bool NeedsStencil()
	{
		static IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("hg.Look"));
		return Var && Var->GetInt() >= 1;
	}
}

/**
 * 그림체 비교(hg.Look, L 키) 화면 효과의 조절 값. 프로젝트 설정 → Game → HG 그림체에 나온다.
 *
 * 값은 Config/DefaultGame.ini에 저장된다. 게임 중(PIE)에 값을 바꾸면 컨트롤러가 다음 프레임에 머티리얼 인스턴스에 넣어서 바로 보인다.
 * 머티리얼 자체(M_HG_LookToon)는 Tools/GenerateGreyboxMaps.py가 만들고, 그 기본값은 여기 값으로 덮어써진다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "HG 그림체"))
class HUOGUOHUOGUO_API UHGLookSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 값이 바뀔 때마다 1씩 오른다 (저장하지 않음). 컨트롤러가 이 번호가 바뀌었을 때만 머티리얼에 값을 넣는다 (매 프레임 비교하지 않으려고).
	// 게임(패키지)에서는 설정이 실행 중에 바뀌지 않으므로 0 그대로다.
	int32 Revision = 0;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		++Revision;
	}
#endif

	// ---- 셀: 외곽선 ----

	// 외곽선 굵기(화면 픽셀). 1보다 작으면 1px 선을 그만큼 옅게 칠한다 (0.5 = 반투명 1px). 0이면 외곽선을 끈다.
	UPROPERTY(config, EditAnywhere, Category = "셀 외곽선", meta = (DisplayName = "굵기", ClampMin = "0", ClampMax = "4"))
	float LineWidth = 0.5f;

	// 외곽선 색. 화면 색에 곱한다 (검을수록 진한 선).
	UPROPERTY(config, EditAnywhere, Category = "셀 외곽선", meta = (DisplayName = "색"))
	FLinearColor LineColor = FLinearColor(0.12f, 0.09f, 0.11f);

	// 진한 선 기준: 테두리 뒤가 이만큼(상대 깊이) 멀어지면 진한 선. 작을수록 작은 높이 차이에도 선이 생긴다.
	UPROPERTY(config, EditAnywhere, Category = "셀 외곽선", meta = (DisplayName = "진한 선 기준", ClampMin = "0.0001", ClampMax = "0.2"))
	float LineDepth = 0.004f;

	// 진한 선 거르기: 클수록 땅의 꺾임이나 비스듬한 면을 선으로 잘못 잡는 일이 줄지만, 비탈 위 테두리도 빠진다.
	UPROPERTY(config, EditAnywhere, Category = "셀 외곽선", meta = (DisplayName = "진한 선 거르기", ClampMin = "1", ClampMax = "10"))
	float Ratio = 4.f;

	// 옅은 선(높낮이) 기준: 언덕 능선이 뒤쪽 땅을 가리는 곳처럼 깊이 차이가 작은 테두리.
	UPROPERTY(config, EditAnywhere, Category = "셀 외곽선", meta = (DisplayName = "옅은 선 기준", ClampMin = "0.0001", ClampMax = "0.2"))
	float FaintLineDepth = 0.002f;

	// 옅은 선 거르기: 진한 선보다 느슨하다 (능선 앞쪽은 비스듬한 비탈이라). 너무 작으면 땅에 줄무늬가 생긴다.
	UPROPERTY(config, EditAnywhere, Category = "셀 외곽선", meta = (DisplayName = "옅은 선 거르기", ClampMin = "1", ClampMax = "10"))
	float FaintRatio = 1.5f;

	// 옅은 선 진하기 (0 = 끔, 1 = 진한 선과 같음).
	UPROPERTY(config, EditAnywhere, Category = "셀 외곽선", meta = (DisplayName = "옅은 선 진하기", ClampMin = "0", ClampMax = "1"))
	float FaintLine = 0.35f;

	// ---- 셀: 명암 계단 ----

	// 계단 간격(밝기 2배 = 1). 1이면 밝기가 2배 차이 날 때마다 한 단, 2면 4배마다 한 단(밝은 면/그늘 면 두 단에 가깝다). 0이면 계단을 끈다.
	UPROPERTY(config, EditAnywhere, Category = "셀 명암", meta = (DisplayName = "계단 간격", ClampMin = "0", ClampMax = "4"))
	float Bands = 1.f;

	// 계단 경계의 부드러움 (0 = 딱 끊김, 0.5 = 계단 없이 부드러움).
	UPROPERTY(config, EditAnywhere, Category = "셀 명암", meta = (DisplayName = "경계 부드러움", ClampMin = "0", ClampMax = "0.5"))
	float BandSoft = 0.08f;

	// 채도 배율 (1 = 그대로). 계단을 켰을 때만 적용된다.
	UPROPERTY(config, EditAnywhere, Category = "셀 명암", meta = (DisplayName = "채도", ClampMin = "0", ClampMax = "3"))
	float Saturation = 1.2f;
};
