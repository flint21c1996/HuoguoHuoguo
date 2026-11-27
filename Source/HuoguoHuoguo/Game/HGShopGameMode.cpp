#include "Game/HGShopGameMode.h"

#include "Player/HGShopPlayerController.h"

AHGShopGameMode::AHGShopGameMode()
{
	PlayerControllerClass = AHGShopPlayerController::StaticClass();
}
