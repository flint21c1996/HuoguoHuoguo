#include "Game/HGShopGameMode.h"

#include "Player/HGShopBossCharacter.h"
#include "Player/HGShopPlayerController.h"

AHGShopGameMode::AHGShopGameMode()
{
	PlayerControllerClass = AHGShopPlayerController::StaticClass();
	// 모델 없는 C++ 기본 사장님. 게임 모드 BP(BP_HG_ShopGameMode)가 BP_HG_ShopBoss로 바꾼다.
	DefaultPawnClass = AHGShopBossCharacter::StaticClass();
}
