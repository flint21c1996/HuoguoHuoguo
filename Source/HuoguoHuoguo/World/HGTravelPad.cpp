#include "World/HGTravelPad.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Explore/HGExplorerCharacter.h"
#include "Game/HGPlayerController.h"
#include "Dev/HGPlaceholderPaint.h"
#include "UObject/ConstructorHelpers.h"

AHGTravelPad::AHGTravelPad()
{
	PrimaryActorTick.bCanEverTick = false;

	// 트리거 상자: 눈에 보이지 않는 감지 영역. 폰(캐릭터)과의 겹침만 감지한다.
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(70.f, 70.f, 60.f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
	SetRootComponent(Trigger);

	// 바닥에 깔리는 얇은 판. 충돌은 없고 위치 표시만 한다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(Trigger);
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PadMesh->SetRelativeLocation(FVector(0.f, 0.f, -58.f));
	PadMesh->SetRelativeScale3D(FVector(1.3f, 1.3f, 0.04f));
	if (CylinderMesh.Succeeded())
	{
		PadMesh->SetStaticMesh(CylinderMesh.Object);
	}

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AHGTravelPad::HandleBeginOverlap);
}

void AHGTravelPad::SetPadColor(const FLinearColor& Color)
{
	HGPlaceholder::Paint(PadMesh, Color);
}

void AHGTravelPad::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 탐험 캐릭터가 밟았을 때만 반응한다. 경영 카메라 Pawn은 충돌이 꺼져 있어서 여기 들어오지 않는다.
	AHGExplorerCharacter* Explorer = Cast<AHGExplorerCharacter>(OtherActor);
	if (!Explorer)
	{
		return;
	}

	if (AHGPlayerController* PC = Cast<AHGPlayerController>(Explorer->GetController()))
	{
		PC->RequestTravel(this);
	}
}
