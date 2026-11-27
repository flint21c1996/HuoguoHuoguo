#include "Shop/Furniture/HGFurniture.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Debug/HGVoxelShapes.h"
#include "Game/HGGameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HuoguoHuoguo.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// 자리 표시 기본 색. 행의 ColorA/ColorB가 비었을 때 쓴다.
	const FLinearColor WoodColor(0.45f, 0.27f, 0.14f);
	const FLinearColor SteelColor(0.50f, 0.53f, 0.57f);
	const FLinearColor RedBroth(0.78f, 0.16f, 0.08f);    // 홍탕 (마라)
	const FLinearColor WhiteBroth(0.92f, 0.88f, 0.78f);  // 백탕
	const FLinearColor SeatColor(0.90f, 0.85f, 0.75f);
	const FLinearColor ClayColor(0.62f, 0.32f, 0.20f);
	const FLinearColor LeafColor(0.18f, 0.55f, 0.22f);

	// 밤에 등이 빛나는 세기 (발광 머티리얼 Glow 값).
	constexpr float LampGlow = 6.f;

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

	// 액터 원점 = 바닥 위 가구 가운데 = 차지한 칸 사각형의 가운데. 부품은 모두 이 기준으로 위치를 잡는다.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

TArray<FName> AHGFurniture::GetFurnitureIdOptions() const
{
	const UDataTable* Table = HGFurnitureData::LoadTable();
	return Table ? Table->GetRowNames() : TArray<FName>();
}

FText AHGFurniture::GetDisplayName() const
{
	return bHasData && !Data.DisplayName.IsEmpty() ? Data.DisplayName : FText::FromName(FurnitureId);
}

// ---------------------------------------------------------------------------
// 만들기
// ---------------------------------------------------------------------------

void AHGFurniture::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 행을 찾아 담는다. 못 찾으면 1x1 장식 기본값으로 두고 회색 상자를 만든다 (CSV에서 지운 ID 등).
	if (const FHGFurnitureRow* Row = HGFurnitureData::FindRow(FurnitureId))
	{
		Data = *Row;
		bHasData = true;
	}
	else
	{
		Data = FHGFurnitureRow();
		bHasData = false;
		if (!FurnitureId.IsNone() && GetWorld() && GetWorld()->IsGameWorld())
		{
			UE_LOG(LogHuoguo, Warning, TEXT("가구 데이터에 '%s' 행이 없다 (Data/Furniture.csv)"), *FurnitureId.ToString());
		}
	}
	Tags = HGFurnitureData::ParseTags(Data.Tags);

	// 게임 중이면 지금 시간대에 맞춰 밤 불빛을 켠 채로 만든다 (밤에 새로 놓은 등불도 바로 켜진다).
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		if (const UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>())
		{
			bNightLightOn = GI->GetDayPhase() != EHGDayPhase::Day;
		}
	}
	Rebuild();
}

bool AHGFurniture::IsVoxelStyle()
{
	return HGVoxel::IsEnabled();
}

void AHGFurniture::Rebuild()
{
	ClearParts();
	bHasLightAnchor = false;

	// 복셀 모드면 모양을 적어 두었다가 한꺼번에 복셀로 바꾼다.
	const bool bVoxel = IsVoxelStyle();
	bCapturing = bVoxel;
	Captured.Reset();
	BuildParts();
	bCapturing = false;
	if (bVoxel)
	{
		VoxelizeCaptured();
	}

	// 밤 불빛. 유령(미리보기)에는 달지 않는다.
	if (bHasLightAnchor && !bGhost)
	{
		NightLight = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
		NightLight->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		NightLight->SetupAttachment(Root);
		NightLight->SetRelativeLocation(LightLocation);
		NightLight->SetIntensityUnits(ELightUnits::Candelas);
		NightLight->SetIntensity(LightIntensity);
		NightLight->SetLightColor(LightColor);
		NightLight->SetAttenuationRadius(450.f);
		// 그림자는 끈다: 가게에 불빛이 수십 개 생길 수 있어서 그림자까지 그리면 무겁다.
		NightLight->SetCastShadows(false);
		NightLight->SetVisibility(bNightLightOn);
		NightLight->RegisterComponent();
	}

	Repaint();
	SetNightLight(bNightLightOn);
	if (bSteamOn)
	{
		SetSteam(true, SteamColor);
	}
}

void AHGFurniture::ClearParts()
{
	for (UStaticMeshComponent* Part : Parts)
	{
		if (IsValid(Part))
		{
			Part->DestroyComponent();
		}
	}
	if (IsValid(NightLight))
	{
		NightLight->DestroyComponent();
	}
	NightLight = nullptr;
	Parts.Reset();
	SteamParts.Reset();
	GlowParts.Reset();
	PartColors.Reset();
	PartCollides.Reset();
}

void AHGFurniture::SetLightAnchor(const FVector& Location, const FLinearColor& Color, float Intensity)
{
	bHasLightAnchor = true;
	LightLocation = Location;
	LightColor = Color;
	LightIntensity = Intensity;
}

void AHGFurniture::SetNightLight(bool bOn)
{
	bNightLightOn = bOn;
	if (NightLight)
	{
		NightLight->SetVisibility(bOn);
	}
	for (UStaticMeshComponent* Part : GlowParts)
	{
		if (UMaterialInstanceDynamic* Mid = Part ? Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)) : nullptr)
		{
			Mid->SetScalarParameterValue(TEXT("Glow"), bOn && !bGhost ? LampGlow : 0.f);
		}
	}
}

void AHGFurniture::VoxelizeCaptured()
{
	// 적어 둔 모양을 공용 도우미로 복셀화한다 (Debug/HGVoxelShapes). 무리 0 = 몸, 1 = 김(숨겨 두고 손님이 먹을 때 켠다).
	// 발광 모양(등)은 발광 머티리얼 무리가 되어 밤에만 빛난다 (SetNightLight). 가구는 바닥 아래로 내려가지 않게 Z < 0 칸을 버린다.
	const TArray<FHGVoxelGroup> Groups = HGVoxel::Build(this, Root, Captured, HGVoxel::DefaultSize, !bGhost, true);
	for (const FHGVoxelGroup& Group : Groups)
	{
		Parts.Add(Group.Component);
		PartColors.Add(Group.Color);
		PartCollides.Add(false);
		if (Group.Group == 1)
		{
			Group.Component->SetCastShadow(false);
			Group.Component->SetVisibility(false);
			SteamParts.Add(Group.Component);
		}
		if (Group.bGlow)
		{
			GlowParts.Add(Group.Component);
		}
	}
}
UStaticMeshComponent* AHGFurniture::AddPart(const TCHAR* Shape, const FVector& Location, const FVector& Scale, const FLinearColor& Color,
	bool bCollide, const FRotator& Rotation)
{
	// 복셀 모드: 모양만 적어 둔다. 충돌 부품은 보이지 않는 채로 만들어 배치 검사와 막기를 예전과 같게 한다.
	if (bCapturing)
	{
		FHGVoxelShape& Captured_ = Captured.AddDefaulted_GetRef();
		Captured_.Shape = Shape;
		Captured_.Location = Location;
		Captured_.Scale = Scale;
		Captured_.Rotation = Rotation;
		Captured_.Color = Color;
		Captured_.Group = bAddingSteam ? 1 : 0;
		Captured_.bGlow = bAddingGlow;
		if (!bCollide)
		{
			return nullptr;
		}
	}

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));

	// 생성 스크립트에서 만든 부품으로 표시한다. 에디터에서 가구를 옮겨 생성 스크립트가 다시 돌면 엔진이 지우고, 맵에 저장되지 않는다.
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	Part->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	Part->SetupAttachment(Root);
	Part->SetStaticMesh(Mesh);
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Scale);
	ApplyCollision(Part, bCollide);
	Part->RegisterComponent();

	if (bCapturing)
	{
		// 복셀이 대신 보이므로 충돌 부품은 숨긴다.
		Part->SetVisibility(false);
		Part->SetHiddenInGame(true);
	}

	// 등처럼 밤에 빛나는 부품은 발광 머티리얼로 칠해 둔다 (처음 밝기 0, SetNightLight가 켠다). Repaint는 색만 바꾼다.
	if (bAddingGlow && !bCapturing)
	{
		HGVoxel::PaintGlow(Part, Color, 0.f);
		GlowParts.Add(Part);
	}

	Parts.Add(Part);
	PartColors.Add(Color);
	PartCollides.Add(bCollide);
	return Part;
}

void AHGFurniture::ApplyCollision(UStaticMeshComponent* Part, bool bCollide) const
{
	if (!bCollide)
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		return;
	}
	if (bGhost)
	{
		// 질의 전용(물리 시뮬레이션 없음) + 모든 채널 겹침: 아무것도 막지 않는다(막기는 양쪽이 모두 Block일 때만 일어난다).
		// 충돌 모양(바디)은 만들어지므로 UWorld::ComponentOverlapMulti로 "이 모양이 이 자리에서 무엇과 겹치는지"를 물을 수 있다.
		// 겹침 이벤트는 끈다. 유령이 지나갈 때마다 발판 같은 트리거가 반응하면 안 되기 때문이다.
		Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Part->SetCollisionResponseToAllChannels(ECR_Overlap);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
	}
	else
	{
		// 벽이나 카운터처럼 사장님을 막는다.
		Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
}

void AHGFurniture::BuildParts()
{
	const FLinearColor A = HGFurnitureData::ParseColor(Data.ColorA, FLinearColor(-1.f, -1.f, -1.f));
	const FLinearColor B = HGFurnitureData::ParseColor(Data.ColorB, FLinearColor(-1.f, -1.f, -1.f));
	switch (Data.Category)
	{
	case EHGFurnitureCategory::Table: BuildTable(A, B); break;
	case EHGFurnitureCategory::Chair: BuildChair(A, B); break;
	default:                          BuildDecor(A, B); break;
	}
}

namespace
{
	// 행 색이 비었으면(음수 표시) 기본 색.
	FLinearColor Or(const FLinearColor& Color, const FLinearColor& Fallback)
	{
		return Color.R < 0.f ? Fallback : Color;
	}
}

void AHGFurniture::BuildTable(const FLinearColor& InA, const FLinearColor& InB)
{
	const FLinearColor Top = Or(InA, WoodColor);
	const FLinearColor Pot = Or(InB, SteelColor);
	const FIntPoint Size = GetFootprint();
	const float W = Size.X * 100.f;
	const float D = Size.Y * 100.f;
	const float MinSide = FMath::Min(W, D);

	if (Data.Style == TEXT("Square"))
	{
		// 사각 상판(칸보다 15씩 작게)과 네 모서리 다리.
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 78.f), FVector((W - 30.f) / 100.f, (D - 30.f) / 100.f, 0.08f), Top);
		for (const FVector2D& Corner : { FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(-1, -1) })
		{
			AddPart(TEXT("Cube"), FVector(Corner.X * (W * 0.5f - 30.f), Corner.Y * (D * 0.5f - 30.f), 37.f), FVector(0.08f, 0.08f, 0.74f), Top * 0.7f);
		}
	}
	else
	{
		// 둥근 상판(칸의 78%), 가운데 다리, 바닥 받침. 의자는 바로 옆 칸에 놓이므로 상판 밑으로 들어가지 않는다.
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 78.f), FVector(W * 0.78f / 100.f, D * 0.78f / 100.f, 0.08f), Top);
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 38.f), FVector(MinSide * 0.1f / 100.f, MinSide * 0.1f / 100.f, 0.76f), SteelColor);
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 2.f), FVector(MinSide * 0.35f / 100.f, MinSide * 0.35f / 100.f, 0.04f), SteelColor);
	}

	// 냄비 크기: 식탁 짧은 변의 30% (2x2 식탁이면 지름 60). 큰 냄비는 1.5배.
	const float PotSize = MinSide * 0.3f / 100.f * (Data.Pot == TEXT("Big") ? 1.5f : 1.f);
	BuildPot(PotSize, Pot);

	// 김: 냄비 위로 작아지며 올라가는 구 세 개. 장식이라 충돌이 없고, 손님이 먹는 동안만 보인다.
	const float S = PotSize / 0.6f;
	const FVector Offsets[] = { FVector(-6.f, 4.f, 122.f), FVector(5.f, -3.f, 146.f), FVector(-2.f, 2.f, 172.f) };
	const float Sizes[] = { 0.34f, 0.26f, 0.18f };
	// 복셀 모드에서는 김도 복셀 묶음 하나로 바뀐다 (AddPart가 null을 돌려주고 VoxelizeCaptured가 김 묶음을 만든다).
	bAddingSteam = true;
	for (int32 i = 0; i < 3; ++i)
	{
		if (UStaticMeshComponent* Puff = AddPart(TEXT("Sphere"), FVector(Offsets[i].X * S, Offsets[i].Y * S, Offsets[i].Z), FVector(Sizes[i] * S), FLinearColor::White, false))
		{
			Puff->SetCastShadow(false);
			Puff->SetVisibility(false);
			SteamParts.Add(Puff);
		}
	}
	bAddingSteam = false;

	// 밤 불빛: 냄비 위 따뜻한 작은 빛 (식탁마다 냄비 불이 켜진 느낌).
	// 냄비 바로 위에 두면 냄비가 하얗게 타서 조금 높이 둔다.
	SetLightAnchor(FVector(0.f, 0.f, 175.f), FLinearColor(1.0f, 0.55f, 0.25f), 2.5f);
}

void AHGFurniture::BuildPot(float P, const FLinearColor& PotColor)
{
	// 냄비는 모두 장식(충돌 없음)이다. 상판 윗면 높이 82 위에 놓인다. P = 냄비 지름(m).
	const FName Pot = Data.Pot;
	if (Pot == TEXT("NineGrid"))
	{
		// 구궁격: 네모난 냄비를 가로세로 두 줄씩 칸막이로 나눈 9칸 냄비.
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 92.f), FVector(P, P, 0.2f), PotColor, false);
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 102.f), FVector(P * 0.9f, P * 0.9f, 0.01f), RedBroth, false);
		const float Third = P * 100.f / 6.f;
		for (const float Offset : { -Third, Third })
		{
			AddPart(TEXT("Cube"), FVector(Offset, 0.f, 103.f), FVector(0.02f, P * 0.9f, 0.03f), PotColor, false);
			AddPart(TEXT("Cube"), FVector(0.f, Offset, 103.f), FVector(P * 0.9f, 0.02f, 0.03f), PotColor, false);
		}
		return;
	}

	AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 92.f), FVector(P, P, 0.2f), PotColor, false);
	if (Pot == TEXT("YinYang"))
	{
		// 원앙 냄비: 가운데 칸막이로 나눠 한쪽은 홍탕, 한쪽은 백탕. 반원 대신 납작한 타원 두 개로 표시한다.
		AddPart(TEXT("Cylinder"), FVector(-P * 22.f, 0.f, 102.5f), FVector(P * 0.42f, P * 0.84f, 0.01f), RedBroth, false);
		AddPart(TEXT("Cylinder"), FVector(P * 22.f, 0.f, 102.5f), FVector(P * 0.42f, P * 0.84f, 0.01f), WhiteBroth, false);
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 103.f), FVector(0.02f, P * 0.9f, 0.03f), PotColor, false);
		return;
	}
	// 단일 냄비(Single)와 큰 냄비(Big): 홍탕 하나.
	AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 102.5f), FVector(P * 0.87f, P * 0.87f, 0.01f), RedBroth, false);
}

void AHGFurniture::BuildChair(const FLinearColor& InA, const FLinearColor& InB)
{
	// 의자는 1x1칸. 앞쪽(+X)이 손님이 보는 방향, 등받이는 -X 쪽.
	const FLinearColor A = Or(InA, WoodColor);
	const FLinearColor B = Or(InB, SeatColor);
	const FName Style = Data.Style;
	if (Style == TEXT("Stool"))
	{
		// 둥근 등받이 없는 의자.
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 22.5f), FVector(0.48f, 0.48f, 0.45f), A);
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 46.f), FVector(0.5f, 0.5f, 0.03f), B, false);
	}
	else if (Style == TEXT("Cushion"))
	{
		// 방석 의자: 두툼한 방석과 높은 등받이.
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 20.f), FVector(0.56f, 0.56f, 0.40f), A);
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 44.f), FVector(0.54f, 0.54f, 0.08f), B, false);
		AddPart(TEXT("Cube"), FVector(-24.f, 0.f, 74.f), FVector(0.1f, 0.52f, 0.6f), A);
	}
	else if (Style == TEXT("Arm"))
	{
		// 팔걸이 의자: 네모난 좌석, 방석, 높은 등받이, 양쪽 팔걸이.
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 21.f), FVector(0.6f, 0.6f, 0.42f), A);
		AddPart(TEXT("Cube"), FVector(2.f, 0.f, 45.f), FVector(0.52f, 0.52f, 0.08f), B, false);
		AddPart(TEXT("Cube"), FVector(-26.f, 0.f, 80.f), FVector(0.1f, 0.6f, 0.75f), A);
		for (const float Side : { -27.f, 27.f })
		{
			AddPart(TEXT("Cube"), FVector(0.f, Side, 58.f), FVector(0.5f, 0.08f, 0.26f), A);
		}
	}
	else
	{
		// 나무 의자(Wood, 기본): 둥근 좌석과 등받이 판.
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 22.5f), FVector(0.56f, 0.56f, 0.45f), B);
		AddPart(TEXT("Cube"), FVector(-24.f, 0.f, 70.f), FVector(0.08f, 0.5f, 0.5f), A);
	}
}

void AHGFurniture::BuildDecor(const FLinearColor& InA, const FLinearColor& InB)
{
	const FName Style = Data.Style;
	const FIntPoint Size = GetFootprint();
	const float W = Size.X * 100.f;
	const float D = Size.Y * 100.f;

	if (Style == TEXT("Bush"))
	{
		// 화분: 토분 + 둥근 잎 덤불. A = 잎, B = 화분.
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 22.5f), FVector(0.5f, 0.5f, 0.45f), Or(InB, ClayColor));
		AddPart(TEXT("Sphere"), FVector(0.f, 0.f, 78.f), FVector(0.72f, 0.72f, 0.66f), Or(InA, LeafColor));
	}
	else if (Style == TEXT("Bamboo"))
	{
		// 대나무: 네모 화분에 높이가 다른 대 셋, 대 끝마다 작은 잎.
		const FLinearColor Leaf = Or(InA, FLinearColor(0.35f, 0.65f, 0.25f));
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 17.5f), FVector(0.5f, 0.5f, 0.35f), Or(InB, FLinearColor(0.30f, 0.30f, 0.32f)));
		const FVector Stalks[] = { FVector(-10.f, -8.f, 170.f), FVector(10.f, -4.f, 200.f), FVector(0.f, 11.f, 150.f) };
		for (const FVector& Stalk : Stalks)
		{
			AddPart(TEXT("Cylinder"), FVector(Stalk.X, Stalk.Y, 35.f + (Stalk.Z - 35.f) * 0.5f), FVector(0.06f, 0.06f, (Stalk.Z - 35.f) / 100.f), Leaf);
			AddPart(TEXT("Sphere"), FVector(Stalk.X, Stalk.Y, Stalk.Z), FVector(0.22f, 0.22f, 0.14f), Leaf * 0.8f, false);
		}
	}
	else if (Style == TEXT("Lantern"))
	{
		// 등불: 받침, 가는 기둥, 꼭대기의 둥근 등. A = 등, B = 기둥.
		const FLinearColor Pole = Or(InB, SteelColor);
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 3.f), FVector(0.42f, 0.42f, 0.06f), Pole);
		AddPart(TEXT("Cylinder"), FVector(0.f, 0.f, 80.f), FVector(0.07f, 0.07f, 1.5f), Pole);
		bAddingGlow = true;
		AddPart(TEXT("Sphere"), FVector(0.f, 0.f, 165.f), FVector(0.46f, 0.46f, 0.56f), Or(InA, FLinearColor(0.95f, 0.22f, 0.10f)));
		bAddingGlow = false;
		// 등 안에 빛을 두면 등의 안쪽 면만 밝아지고 겉은 어둡다. 등 바로 아래에 두어 등과 바닥을 함께 비춘다.
		SetLightAnchor(FVector(0.f, 0.f, 125.f), FLinearColor(1.0f, 0.6f, 0.35f), 25.f);
	}
	else if (Style == TEXT("PaperLantern"))
	{
		// 홍등: 나무 틀에 매단 붉은 종이 등과 위아래 금색 테.
		const FLinearColor Frame = Or(InB, FLinearColor(0.85f, 0.65f, 0.20f));
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 4.f), FVector(0.5f, 0.5f, 0.08f), WoodColor);
		AddPart(TEXT("Cube"), FVector(-30.f, 0.f, 100.f), FVector(0.07f, 0.07f, 1.9f), WoodColor);
		AddPart(TEXT("Cube"), FVector(-5.f, 0.f, 192.f), FVector(0.55f, 0.06f, 0.06f), WoodColor);
		bAddingGlow = true;
		AddPart(TEXT("Sphere"), FVector(10.f, 0.f, 140.f), FVector(0.55f, 0.55f, 0.48f), Or(InA, FLinearColor(0.90f, 0.10f, 0.08f)));
		bAddingGlow = false;
		AddPart(TEXT("Cylinder"), FVector(10.f, 0.f, 165.f), FVector(0.28f, 0.28f, 0.05f), Frame, false);
		AddPart(TEXT("Cylinder"), FVector(10.f, 0.f, 115.f), FVector(0.28f, 0.28f, 0.05f), Frame, false);
		SetLightAnchor(FVector(10.f, 0.f, 105.f), FLinearColor(1.0f, 0.35f, 0.2f), 25.f);
	}
	else if (Style == TEXT("SauceBar"))
	{
		// 소스 바: 칸보다 조금 작은 긴 바와 소스 그릇 셋 (마장 갈색, 고추기름 빨강, 파 초록).
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 42.5f), FVector((W - 20.f) / 100.f, (D - 20.f) / 100.f, 0.85f), Or(InA, WoodColor));
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 87.f), FVector((W - 14.f) / 100.f, (D - 14.f) / 100.f, 0.04f), Or(InB, SteelColor));
		const FLinearColor Sauces[] = { FLinearColor(0.45f, 0.28f, 0.12f), RedBroth, FLinearColor(0.30f, 0.62f, 0.20f) };
		for (int32 i = 0; i < 3; ++i)
		{
			const float Along = (i - 1) * (W >= D ? W : D) * 0.28f;
			const FVector Location = W >= D ? FVector(Along, 0.f, 93.f) : FVector(0.f, Along, 93.f);
			AddPart(TEXT("Cylinder"), Location, FVector(0.28f, 0.28f, 0.08f), Sauces[i], false);
		}
	}
	else if (Style == TEXT("Tank"))
	{
		// 수조: 받침장 위 유리 수조(청록)와 물고기 둘.
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 30.f), FVector((W - 20.f) / 100.f, (D - 20.f) / 100.f, 0.6f), Or(InB, WoodColor));
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 97.f), FVector((W - 30.f) / 100.f, (D - 30.f) / 100.f, 0.74f), Or(InA, FLinearColor(0.35f, 0.75f, 0.85f)));
		AddPart(TEXT("Sphere"), FVector(W * 0.15f, D * 0.05f, 100.f), FVector(0.18f, 0.08f, 0.1f), FLinearColor(1.0f, 0.5f, 0.1f), false);
		AddPart(TEXT("Sphere"), FVector(-W * 0.2f, -D * 0.05f, 85.f), FVector(0.16f, 0.07f, 0.09f), FLinearColor(1.0f, 0.85f, 0.2f), false);
		SetLightAnchor(FVector(0.f, 0.f, 110.f), FLinearColor(0.3f, 0.7f, 1.0f), 8.f);
	}
	else if (Style == TEXT("Screen"))
	{
		// 병풍: 긴 변을 따라 지그재그로 선 판 여러 장. 판마다 A, B 색을 번갈아 칠한다.
		const bool bAlongX = W >= D;
		const float Length = (bAlongX ? W : D) - 20.f;
		const int32 Panels = FMath::Max(2, FMath::RoundToInt(Length / 45.f));
		const float PanelWidth = Length / Panels;
		for (int32 i = 0; i < Panels; ++i)
		{
			const float Along = -Length * 0.5f + PanelWidth * (i + 0.5f);
			const float Zig = (i % 2 == 0) ? 18.f : -18.f;
			const FLinearColor Color = (i % 2 == 0) ? Or(InA, FLinearColor(0.85f, 0.80f, 0.65f)) : Or(InB, FLinearColor(0.55f, 0.15f, 0.12f));
			const FVector Location = bAlongX ? FVector(Along, 0.f, 85.f) : FVector(0.f, Along, 85.f);
			const FRotator Rotation(0.f, (bAlongX ? 0.f : 90.f) + Zig, 0.f);
			AddPart(TEXT("Cube"), Location, FVector(PanelWidth / 100.f, 0.04f, 1.6f), Color, true, Rotation);
		}
	}
	else
	{
		// 모르는 Style: 칸 크기의 회색 상자. CSV에 오타가 있으면 이렇게 보인다.
		AddPart(TEXT("Cube"), FVector(0.f, 0.f, 40.f), FVector((W - 20.f) / 100.f, (D - 20.f) / 100.f, 0.8f), Or(InA, FLinearColor(0.5f, 0.5f, 0.5f)));
	}
}

// ---------------------------------------------------------------------------
// 상태
// ---------------------------------------------------------------------------

void AHGFurniture::MakeGhost()
{
	bGhost = true;
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		if (Parts[i])
		{
			ApplyCollision(Parts[i], PartCollides[i]);
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

void AHGFurniture::SetSteam(bool bOn, const FLinearColor& Color)
{
	// 다시 만들 때(복셀 전환) 같은 상태로 되살리려고 기억해 둔다.
	bSteamOn = bOn;
	if (bOn)
	{
		SteamColor = Color;
	}
	for (UStaticMeshComponent* Puff : SteamParts)
	{
		if (Puff)
		{
			if (bOn)
			{
				SetPartColor(Puff, Color);
			}
			Puff->SetVisibility(bOn);
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

// ---------------------------------------------------------------------------

AHGTable::AHGTable()
{
	FurnitureId = TEXT("Table");
}

AHGChair::AHGChair()
{
	FurnitureId = TEXT("Chair");
}

// ---------------------------------------------------------------------------
// 가구 데이터 도우미 (HGFurnitureData.h)
// ---------------------------------------------------------------------------

const UDataTable* HGFurnitureData::LoadTable()
{
	// 이미 불러온 에셋이면 찾기만 하고, 아니면 불러온다. 배치 컴포넌트가 UPROPERTY로 잡고 있어 게임 중에는 내려가지 않는다.
	if (const UDataTable* Found = FindObject<UDataTable>(nullptr, TablePath))
	{
		return Found;
	}
	return LoadObject<UDataTable>(nullptr, TablePath);
}

const FHGFurnitureRow* HGFurnitureData::FindRow(FName FurnitureId)
{
	const UDataTable* Table = LoadTable();
	return (Table && !FurnitureId.IsNone()) ? Table->FindRow<FHGFurnitureRow>(FurnitureId, TEXT("FurnitureData"), false) : nullptr;
}

FLinearColor HGFurnitureData::ParseColor(const FString& Hex, const FLinearColor& Fallback)
{
	FString Text = Hex.TrimStartAndEnd();
	Text.RemoveFromStart(TEXT("#"));
	if (Text.Len() != 6)
	{
		return Fallback;
	}
	// FColor::FromHex는 sRGB 값이다. 머티리얼 색 파라미터는 선형이므로 바꿔 준다 (엑셀에 적은 색이 화면에서 비슷하게 보이도록).
	return FLinearColor::FromSRGBColor(FColor::FromHex(Text));
}

TArray<FName> HGFurnitureData::ParseTags(const FString& Tags)
{
	TArray<FString> Parts;
	Tags.ParseIntoArray(Parts, TEXT("|"), true);
	TArray<FName> Result;
	for (FString& Part : Parts)
	{
		Part.TrimStartAndEndInline();
		if (!Part.IsEmpty())
		{
			Result.Add(FName(*Part));
		}
	}
	return Result;
}

const TCHAR* HGFurnitureData::CategoryName(EHGFurnitureCategory Category)
{
	switch (Category)
	{
	case EHGFurnitureCategory::Table: return TEXT("식탁");
	case EHGFurnitureCategory::Chair: return TEXT("의자");
	default:                          return TEXT("장식");
	}
}
