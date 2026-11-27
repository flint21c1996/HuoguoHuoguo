#include "Game/HGExploreGameMode.h"

#include "EngineUtils.h"
#include "HuoguoHuoguo.h"
#include "Player/HGExplorePlayerController.h"
#include "Player/HGExplorerCharacter.h"
#include "World/Planet/HGPlanetGenerator.h"

namespace
{
	AHGPlanetGenerator* FindPlanet(UWorld* World)
	{
		for (TActorIterator<AHGPlanetGenerator> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

AHGExploreGameMode::AHGExploreGameMode()
{
	PlayerControllerClass = AHGExplorePlayerController::StaticClass();
	// 모델 없는 C++ 기본 탐험 캐릭터. 환경별 게임 모드 BP가 환경별 캐릭터 BP(BP_HG_Explorer_*)로 바꾼다.
	DefaultPawnClass = AHGExplorerCharacter::StaticClass();
}

AActor* AHGExploreGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	// 행성 맵에는 PlayerStart가 없다. 생성기 자신을 시작 기준으로 돌려준다 (실제 위치는 RestartPlayer가 착륙 지점으로 정한다).
	// 엔진은 접속할 때 시작점을 미리 찾아 두는데, 이렇게 하지 않으면 "PlayerStart 없음" 경고가 난다.
	if (AHGPlanetGenerator* Planet = FindPlanet(GetWorld()))
	{
		return Planet;
	}
	return Super::FindPlayerStart_Implementation(Player, IncomingName);
}

void AHGExploreGameMode::RestartPlayer(AController* NewPlayer)
{
	AHGPlanetGenerator* Planet = FindPlanet(GetWorld());
	if (!Planet)
	{
		Super::RestartPlayer(NewPlayer);
		return;
	}

	// 접속은 액터들의 BeginPlay보다 먼저 일어난다. 착륙 지점을 알려면 지형이 있어야 하므로 여기서 먼저 만든다
	// (EnsureGenerated는 한 번만 만든다. 생성기 자신의 BeginPlay에서 다시 불러도 그대로다).
	Planet->EnsureGenerated();
	FTransform Landing = Planet->GetLandingTransform();
	Landing.SetScale3D(FVector::OneVector);
	RestartPlayerAtTransform(NewPlayer, Landing);
}
