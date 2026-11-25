#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HGCodexWidget.generated.h"

/**
 * 도감 화면 (J 키). 탐험하면서 발견한 것을 모아 보여 준다.
 *
 * 항목:
 *   - 재료: 재료 DataTable의 모든 재료. 발견한 것(스캔으로 보거나 캔 것)은 이름, 나는 시간대, 고향 군계, 캔 횟수.
 *     아직 못 본 것은 "???"로 남겨 무엇이 더 있는지 궁금하게 한다.
 *   - 장소: 군계 셋 x 종류 셋(행성, 소행성 지대, 폐우주선) 중 다녀온 곳.
 *   - 가게: 발견한 콤보 수, 열린 가구 수, 폐우주선에서 찾은 설계도 수.
 *
 * 기록은 GameInstance에 있다 (게임이 켜져 있는 동안만, 저장 파일은 아직 없다).
 * 위젯 에셋 없이 NativePaint에서 글자와 상자로 직접 그린다 (미니맵과 같은 방식). 정식 UI가 생기면 바꾼다.
 */
UCLASS()
class HUOGUOHUOGUO_API UHGCodexWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
