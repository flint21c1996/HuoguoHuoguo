#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Game/HGWorldLayout.h"
#include "HGGameMode.generated.h"

/**
 * 훠궈훠궈 기본 게임 모드.
 *
 * 시작 순서:
 *   레벨이 열리면 플레이어 컨트롤러와 기본 Pawn(경영 카메라)이 먼저 만들어지고, 그다음 StartPlay가 불린다.
 *   StartPlay에서 회색 박스 월드를 만든 뒤 각 컨트롤러에 시작 위치를 넘겨 모드를 초기화한다.
 *   그 후 Super::StartPlay가 모든 액터의 BeginPlay를 호출한다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHGGameMode();

	virtual void StartPlay() override;

	const FHGWorldLayout& GetLayout() const { return Layout; }

protected:
	// true면 시작할 때 개발용 회색 박스 월드를 만든다. 정식 레벨에서는 꺼 둔다.
	UPROPERTY(EditAnywhere, Category = "Dev")
	bool bBuildGreybox = true;

private:
	FHGWorldLayout Layout;
};
