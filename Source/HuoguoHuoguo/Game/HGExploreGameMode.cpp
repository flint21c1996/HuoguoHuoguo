#include "Game/HGExploreGameMode.h"

#include "Player/HGExplorePlayerController.h"

AHGExploreGameMode::AHGExploreGameMode()
{
	PlayerControllerClass = AHGExplorePlayerController::StaticClass();
}
