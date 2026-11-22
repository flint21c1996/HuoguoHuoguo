#include "Game/HGGameMode.h"

#include "Game/HGPlayerController.h"

AHGGameMode::AHGGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AHGPlayerController::StaticClass();
}
