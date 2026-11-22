#include "Game/HGGameMode.h"

#include "Dev/HGGreyboxBuilder.h"
#include "Engine/World.h"
#include "Game/HGPlayerController.h"
#include "HuoguoHuoguo.h"
#include "Shop/HGShopCameraPawn.h"

AHGGameMode::AHGGameMode()
{
	// 플레이어는 경영 카메라로 시작한다. 탐험 캐릭터는 컨트롤러가 따로 만든다.
	DefaultPawnClass = AHGShopCameraPawn::StaticClass();
	PlayerControllerClass = AHGPlayerController::StaticClass();
}

void AHGGameMode::StartPlay()
{
	if (bBuildGreybox)
	{
		FActorSpawnParameters Params;
		Params.Name = TEXT("HG_Greybox");
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AHGGreyboxBuilder* Builder = GetWorld()->SpawnActor<AHGGreyboxBuilder>(AHGGreyboxBuilder::StaticClass(), FTransform::Identity, Params))
		{
			Layout = Builder->Build();
		}
	}
	else
	{
		// 정식 레벨용 경로: 아직 마커 액터가 없으므로 원점 기준 기본값을 쓴다.
		Layout.ExplorerStart = FTransform(FVector(0.f, 0.f, 120.f));
		Layout.bValid = true;
	}

	UE_LOG(LogHuoguo, Log, TEXT("월드 준비 완료. 가게 초점 %s, 사장님 시작 %s"),
		*Layout.ShopFocus.ToString(), *Layout.ExplorerStart.GetLocation().ToString());

	// 이미 접속해 있는 플레이어들의 모드를 초기화한다. 늦게 접속하는 플레이어는 자기 BeginPlay에서 처리한다.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AHGPlayerController* PC = Cast<AHGPlayerController>(It->Get()))
		{
			PC->InitializeModes(Layout);
		}
	}

	Super::StartPlay();
}
