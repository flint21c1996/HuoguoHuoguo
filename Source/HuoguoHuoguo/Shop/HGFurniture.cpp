#include "Shop/HGFurniture.h"

#include "Components/StaticMeshComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 자리 표시 색. 정거장 블록아웃 맵의 팔레트와 비슷하게 맞춘다.
	const FLinearColor WoodColor(0.45f, 0.27f, 0.14f);
	const FLinearColor SteelColor(0.50f, 0.53f, 0.57f);
	const FLinearColor BrothColor(0.78f, 0.16f, 0.08f);
	const FLinearColor SeatColor(0.90f, 0.85f, 0.75f);

	// 미리보기 색. 원래 색 대신 이 색으로 칠한다.
	FLinearColor PreviewColor(EHGFurniturePreview State)
	{
		switch (State)
		{
		case EHGFurniturePreview::Valid:   return FLinearColor(0.15f, 0.85f, 0.30f);
		case EHGFurniturePreview::Invalid: return FLinearColor(0.95f, 0.12f, 0.10f);
		case EHGFurniturePreview::Hover:   return FLinearColor(1.00f, 0.62f, 0.10f);
		default:                           return FLinearColor::White;
		}
	}
}

AHGFurniture::AHGFurniture()
{
	PrimaryActorTick.bCanEverTick = false;

	// 액터 원점 = 바닥 위 가구 중심. 부품은 모두 이 기준으로 위치를 잡는다.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

UStaticMesh* AHGFurniture::LoadBasicShape(const TCHAR* Shape)
{
	// static이 아닌 지역 변수로 찾는다. 함수 하나로 여러 도형을 불러오므로 static이면 처음 찾은 도형만 남는다.
	const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape);
	ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(*Path);
	return Finder.Succeeded() ? Finder.Object : nullptr;
}

UStaticMeshComponent* AHGFurniture::AddPart(FName Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, const FLinearColor& Color, bool bCollide)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Root);
	Part->SetStaticMesh(Mesh);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeScale3D(Scale);
	if (bCollide)
	{
		// 벽이나 카운터처럼 사장님과 손님을 막는다. 커서 가리키기(Visibility 추적)에도 걸린다.
		Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Parts.Add(Part);
	PartColors.Add(Color);
	PartCollides.Add(bCollide);
	return Part;
}

void AHGFurniture::SetPartColor(UStaticMeshComponent* Part, const FLinearColor& Color)
{
	const int32 Index = Parts.IndexOfByKey(Part);
	if (Index == INDEX_NONE || PartColors[Index].Equals(Color))
	{
		return;
	}
	PartColors[Index] = Color;
	Repaint();
}

void AHGFurniture::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Repaint();
}

void AHGFurniture::MakeGhost()
{
	bGhost = true;
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		UStaticMeshComponent* Part = Parts[i];
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		if (PartCollides[i])
		{
			// 질의 전용(물리 시뮬레이션 없음) + 모든 채널 겹침: 아무것도 막지 않는다(막기는 양쪽이 모두 Block일 때만 일어난다).
			// 커서 추적(LineTraceSingle)은 막는 것만 맞히므로 유령에는 걸리지 않는다.
			// 충돌 모양(바디)은 만들어지므로 UWorld::ComponentOverlapMulti로 "이 모양이 이 자리에서 무엇과 겹치는지"를 물을 수 있다.
			// 겹침 이벤트는 끈다. 유령이 지나갈 때마다 발판 같은 트리거가 반응하면 안 되기 때문이다.
			Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Part->SetCollisionResponseToAllChannels(ECR_Overlap);
			Part->SetGenerateOverlapEvents(false);
		}
	}
}

void AHGFurniture::SetPreviewState(EHGFurniturePreview State)
{
	if (Preview == State)
	{
		return;
	}
	Preview = State;
	Repaint();
}

void AHGFurniture::GetCollidingParts(TArray<UStaticMeshComponent*>& OutParts) const
{
	OutParts.Reset();
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		if (PartCollides[i] && Parts[i])
		{
			OutParts.Add(Parts[i]);
		}
	}
}

void AHGFurniture::Repaint()
{
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		UStaticMeshComponent* Part = Parts[i];
		if (!Part)
		{
			continue;
		}
		const FLinearColor Color = Preview == EHGFurniturePreview::None ? PartColors[i] : PreviewColor(Preview);

		// 이미 동적 머티리얼이 붙어 있으면 색 값만 바꾼다. 상태가 바뀔 때마다 머티리얼을 새로 만들지 않기 위해서이다.
		if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
		{
			Mid->SetVectorParameterValue(TEXT("Color"), Color);
		}
		else
		{
			HGPlaceholder::Paint(Part, Color);
		}
	}
}

AHGTable::AHGTable()
{
	DisplayName = NSLOCTEXT("HG", "Furniture_Table", "식탁");
	FurnitureId = TEXT("Table");
	Footprint = FIntPoint(2, 2);

	UStaticMesh* Cylinder = LoadBasicShape(TEXT("Cylinder"));
	// 상판: 지름 156, 두께 8, 윗면 높이 82. 의자 좌석(높이 45)보다 높아서 의자를 밑으로 밀어 넣을 수 있다.
	AddPart(TEXT("Top"), Cylinder, FVector(0.f, 0.f, 78.f), FVector(1.56f, 1.56f, 0.08f), WoodColor);
	// 가운데 다리와 바닥 받침. 바닥에 닿는 것은 이 둘뿐이다.
	AddPart(TEXT("Leg"), Cylinder, FVector(0.f, 0.f, 38.f), FVector(0.2f, 0.2f, 0.76f), SteelColor);
	AddPart(TEXT("Base"), Cylinder, FVector(0.f, 0.f, 2.f), FVector(0.7f, 0.7f, 0.04f), SteelColor);
	// 훠궈 냄비와 국물. 장식이라 충돌이 없다.
	AddPart(TEXT("Pot"), Cylinder, FVector(0.f, 0.f, 92.f), FVector(0.6f, 0.6f, 0.2f), SteelColor, false);
	AddPart(TEXT("Broth"), Cylinder, FVector(0.f, 0.f, 102.5f), FVector(0.52f, 0.52f, 0.01f), BrothColor, false);
}

AHGChair::AHGChair()
{
	DisplayName = NSLOCTEXT("HG", "Furniture_Chair", "의자");
	FurnitureId = TEXT("Chair");
	bFacesTable = true;

	UStaticMesh* Cylinder = LoadBasicShape(TEXT("Cylinder"));
	UStaticMesh* Cube = LoadBasicShape(TEXT("Cube"));
	// 좌석: 지름 56, 높이 45 (원통 그대로 바닥까지 채운 단순한 의자).
	AddPart(TEXT("Seat"), Cylinder, FVector(0.f, 0.f, 22.5f), FVector(0.56f, 0.56f, 0.45f), SeatColor);
	// 등받이: 좌석 뒤쪽(-X) 끝에 세운 판. 높이 45~95.
	AddPart(TEXT("Back"), Cube, FVector(-24.f, 0.f, 70.f), FVector(0.08f, 0.5f, 0.5f), WoodColor);
}

AHGPlanter::AHGPlanter()
{
	DisplayName = NSLOCTEXT("HG", "Furniture_Planter", "화분");
	FurnitureId = TEXT("Planter");

	UStaticMesh* Cylinder = LoadBasicShape(TEXT("Cylinder"));
	UStaticMesh* Sphere = LoadBasicShape(TEXT("Sphere"));
	// 토분: 지름 50, 높이 45. 그 위에 둥근 잎 덤불 (지름 72). 둘 다 충돌이 있어 커서로 가리킬 수 있다.
	AddPart(TEXT("Pot"), Cylinder, FVector(0.f, 0.f, 22.5f), FVector(0.5f, 0.5f, 0.45f), FLinearColor(0.62f, 0.32f, 0.20f));
	AddPart(TEXT("Leaves"), Sphere, FVector(0.f, 0.f, 78.f), FVector(0.72f, 0.72f, 0.66f), FLinearColor(0.18f, 0.55f, 0.22f));
}

AHGSauceBar::AHGSauceBar()
{
	DisplayName = NSLOCTEXT("HG", "Furniture_SauceBar", "소스 바");
	FurnitureId = TEXT("SauceBar");
	Footprint = FIntPoint(2, 1);

	UStaticMesh* Cube = LoadBasicShape(TEXT("Cube"));
	UStaticMesh* Cylinder = LoadBasicShape(TEXT("Cylinder"));
	// 몸체: 길이 180(X), 폭 80(Y), 높이 85. 2x1칸(200 x 100) 안에 들어가도록 칸보다 조금 작다.
	AddPart(TEXT("Body"), Cube, FVector(0.f, 0.f, 42.5f), FVector(1.8f, 0.8f, 0.85f), WoodColor);
	AddPart(TEXT("Top"), Cube, FVector(0.f, 0.f, 87.f), FVector(1.86f, 0.86f, 0.04f), SteelColor);
	// 소스 그릇 셋 (마장 갈색, 고추기름 빨강, 파 초록). 장식이라 충돌이 없다.
	const FLinearColor SauceColors[] = { FLinearColor(0.45f, 0.28f, 0.12f), BrothColor, FLinearColor(0.30f, 0.62f, 0.20f) };
	for (int32 i = 0; i < 3; ++i)
	{
		AddPart(*FString::Printf(TEXT("Sauce_%d"), i), Cylinder, FVector(-55.f + i * 55.f, 0.f, 93.f), FVector(0.28f, 0.28f, 0.08f), SauceColors[i], false);
	}
}

AHGLantern::AHGLantern()
{
	DisplayName = NSLOCTEXT("HG", "Furniture_Lantern", "등불");
	FurnitureId = TEXT("Lantern");

	UStaticMesh* Cylinder = LoadBasicShape(TEXT("Cylinder"));
	UStaticMesh* Sphere = LoadBasicShape(TEXT("Sphere"));
	// 바닥 받침, 가는 기둥(높이 150), 꼭대기의 붉은 등 (가게 천장 등불과 같은 색).
	AddPart(TEXT("Base"), Cylinder, FVector(0.f, 0.f, 3.f), FVector(0.42f, 0.42f, 0.06f), SteelColor);
	AddPart(TEXT("Pole"), Cylinder, FVector(0.f, 0.f, 80.f), FVector(0.07f, 0.07f, 1.5f), SteelColor);
	AddPart(TEXT("Lamp"), Sphere, FVector(0.f, 0.f, 165.f), FVector(0.46f, 0.46f, 0.56f), FLinearColor(0.95f, 0.22f, 0.10f));
}
