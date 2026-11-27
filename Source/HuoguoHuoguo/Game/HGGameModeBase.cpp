#include "Game/HGGameModeBase.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "HuoguoHuoguo.h"

AHGGameModeBase::AHGGameModeBase()
{
	// 하위 게임 모드(C++)가 기본 캐릭터 클래스를 정하고, 게임 모드 BP가 그것을 캐릭터 BP로 바꾼다.
	DefaultPawnClass = nullptr;
}

AActor* AHGGameModeBase::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	AActor* Start = Super::FindPlayerStart_Implementation(Player, IncomingName);
	const APlayerStart* PlayerStart = Cast<APlayerStart>(Start);
	if (!IncomingName.IsEmpty() && (!PlayerStart || PlayerStart->PlayerStartTag != FName(*IncomingName)))
	{
		UE_LOG(LogHuoguo, Warning, TEXT("[Start] 도착 태그 '%s'인 PlayerStart가 없어서 %s를 쓴다"), *IncomingName, *GetNameSafe(Start));
	}
	return Start;
}

AActor* AHGGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	APlayerStart* Fallback = nullptr;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag == TEXT("Default"))
		{
			return *It;
		}
		if (!Fallback)
		{
			Fallback = *It;
		}
	}
	// PlayerStart가 하나도 없으면 엔진 기본 동작(월드 원점 근처 등)에 맡긴다.
	return Fallback ? Fallback : Super::ChoosePlayerStart_Implementation(Player);
}
