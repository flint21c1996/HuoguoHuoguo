#include "World/HGShopCameraAnchor.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"

AHGShopCameraAnchor::AHGShopCameraAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 화살표: 경영 카메라가 바라보는 수평 방향을 에디터 뷰포트에 보여 준다.
	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(Root);
	Arrow->ArrowColor = FColor(244, 178, 62);
	Arrow->ArrowSize = 4.f;
	Arrow->SetHiddenInGame(true);

#if WITH_EDITORONLY_DATA
	// 아이콘: 뷰포트에서 클릭해 고를 수 있게 한다. 에디터 전용 데이터라 패키징된 게임에는 들어가지 않는다.
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Root);
	}
#endif
}
