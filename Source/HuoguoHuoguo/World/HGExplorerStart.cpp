#include "World/HGExplorerStart.h"

#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"

AHGExplorerStart::AHGExplorerStart()
{
	PrimaryActorTick.bCanEverTick = false;

	// 캡슐 크기는 탐험 캐릭터(AHGExplorerCharacter)와 같다. 배치 위치 확인용이라 충돌은 끄고 게임에서는 숨긴다.
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(40.f, 92.f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capsule->SetHiddenInGame(true);
	Capsule->ShapeColor = FColor(82, 214, 232);
	SetRootComponent(Capsule);

	// 화살표: 캐릭터가 처음 바라보는 방향.
	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(Capsule);
	Arrow->ArrowColor = FColor(82, 214, 232);
	Arrow->SetHiddenInGame(true);
}
