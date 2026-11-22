#include "Shop/HGShopGameMode.h"

#include "Shop/HGShopPlayerController.h"

AHGShopGameMode::AHGShopGameMode()
{
	PlayerControllerClass = AHGShopPlayerController::StaticClass();
}
