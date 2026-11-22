#include "Explore/HGExploreGameMode.h"

#include "Explore/HGExplorePlayerController.h"

AHGExploreGameMode::AHGExploreGameMode()
{
	PlayerControllerClass = AHGExplorePlayerController::StaticClass();
}
