#include "Shop/Customer/HGCustomer.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Debug/HGVoxelShapes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 의자 좌석 윗면 높이(45)에 맞춰 앉을 때 몸을 올리는 높이.
	constexpr float SeatLift = 40.f;
	const FVector CarriedPlateLocation(38.f, 0.f, 66.f);

	void SetPropMesh(UStaticMeshComponent* Part, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale)
	{
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
	}
}

AHGCustomer::AHGCustomer()
{
	PrimaryActorTick.bCanEverTick = true;

	// 액터 원점 = 발밑 바닥. 몸(Body)은 원점 위에서 걸을 때 통통 튀고, 앉을 때 좌석 높이로 올라간다.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// 몸통: 지름 50, 높이 90. 머리: 지름 42, 몸통 위.
	Torso = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Torso"));
	Torso->SetupAttachment(Body);
	Torso->SetStaticMesh(CylinderFinder.Object);
	Torso->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	Torso->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.9f));

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Body);
	Head->SetStaticMesh(SphereFinder.Object);
	Head->SetRelativeLocation(FVector(0.f, 0.f, 112.f));
	Head->SetRelativeScale3D(FVector(0.42f));

	PlateAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("PlateAnchor"));
	PlateAnchor->SetupAttachment(Body);
	PlateAnchor->SetRelativeLocation(CarriedPlateLocation);
	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkewerPlate"));
	Plate->SetupAttachment(PlateAnchor);
	SetPropMesh(Plate, CylinderFinder.Object, FVector::ZeroVector, FVector(0.62f, 0.53f, 0.045f));
	Plate->SetVisibility(false);

	LeftGrip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftPlateGrip"));
	RightGrip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPlateGrip"));
	for (UStaticMeshComponent* Grip : { LeftGrip.Get(), RightGrip.Get() })
	{
		Grip->SetupAttachment(PlateAnchor);
		const float Side = Grip == LeftGrip ? -1.f : 1.f;
		SetPropMesh(Grip, SphereFinder.Object, FVector(-12.f, Side * 28.f, -4.f), FVector(0.21f, 0.14f, 0.13f));
		Grip->SetVisibility(false);
	}

	BrothPrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BrothPrompt"));
	BrothPrompt->SetupAttachment(Root);
	BrothPrompt->SetRelativeLocation(FVector(0.f, 0.f, 211.f));
	BrothPrompt->SetText(FText::FromString(TEXT("[F]")));
	BrothPrompt->SetHorizontalAlignment(EHTA_Center);
	BrothPrompt->SetVerticalAlignment(EVRTA_TextCenter);
	BrothPrompt->SetTextRenderColor(FColor(255, 219, 122));
	BrothPrompt->SetWorldSize(32.f);
	BrothPrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BrothPrompt->SetCastShadow(false);
	BrothPrompt->SetVisibility(false);

	for (UStaticMeshComponent* Part : { Torso.Get(), Head.Get() })
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
	}
}

void AHGCustomer::SetBodyColor(const FLinearColor& Color)
{
	// 머리는 몸통 색을 밝게 한 색. 종족마다 머리와 몸 색이 함께 달라 보이게 한다.
	const FLinearColor HeadColor = FMath::Lerp(Color, FLinearColor::White, 0.45f);
	HGPlaceholder::Paint(Torso, Color);
	HGPlaceholder::Paint(Head, HeadColor);
	HGPlaceholder::Paint(Plate, FLinearColor(0.80f, 0.94f, 1.f));
	HGPlaceholder::Paint(LeftGrip, HeadColor);
	HGPlaceholder::Paint(RightGrip, HeadColor);

	// 복셀 그림체: 같은 모양(몸통 원통, 머리 구)을 복셀로 만들어 Body 아래에 붙이고, 얼굴 앞(+X)에 눈 두 개를 더한다.
	// Body에 붙이므로 걸을 때 통통 튀고 앉을 때 올라가는 움직임을 그대로 따른다. 한 번만 만든다.
	if (HGVoxel::IsEnabled() && !bVoxelBody)
	{
		TArray<FHGVoxelShape> Shapes;
		auto Add = [&Shapes](const TCHAR* Shape, const FVector& Location, const FVector& Scale, const FLinearColor& ShapeColor)
		{
			FHGVoxelShape& S = Shapes.AddDefaulted_GetRef();
			S.Shape = Shape;
			S.Location = Location;
			S.Scale = Scale;
			S.Color = ShapeColor;
		};
		Add(TEXT("Cylinder"), FVector(0.f, 0.f, 45.f), FVector(0.5f, 0.5f, 0.9f), Color);
		Add(TEXT("Sphere"), FVector(0.f, 0.f, 112.f), FVector(0.42f), HeadColor);
		for (const float Side : { -7.f, 7.f })
		{
			Add(TEXT("Cube"), FVector(19.f, Side, 116.f), FVector(0.05f, 0.05f, 0.07f), FLinearColor(0.08f, 0.06f, 0.06f));
		}
		HGVoxel::Build(this, Body, Shapes);
		Torso->SetVisibility(false);
		Head->SetVisibility(false);
		bVoxelBody = true;
	}
}

void AHGCustomer::WalkPath(const TArray<FVector>& Points)
{
	Path = Points;
	WalkTime = 0.f;
}

void AHGCustomer::SitAt(const FVector& ChairLocation, float Yaw)
{
	Path.Reset();
	bSeated = true;
	SeatFloor = ChairLocation;
	SetActorLocationAndRotation(ChairLocation, FRotator(0.f, Yaw, 0.f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, SeatLift));
	UpdateStackSway(1.f, false);
}

void AHGCustomer::StandUp()
{
	bSeated = false;
	SetActorLocation(SeatFloor);
	Body->SetRelativeLocation(FVector::ZeroVector);
}

void AHGCustomer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickMeal(DeltaSeconds);
	if (Path.IsEmpty())
	{
		if (!bSeated)
		{
			Body->SetRelativeLocation(FVector::ZeroVector);
		}
		UpdateStackSway(DeltaSeconds, false);
		return;
	}

	// 다음 지점으로 곧게 걸어간다. 이번 프레임에 갈 거리가 남은 거리보다 길면 그 지점에 도착하고 다음 지점으로 넘어간다.
	FVector Location = GetActorLocation();
	float Budget = WalkSpeed * DeltaSeconds;
	while (!Path.IsEmpty() && Budget > 0.f)
	{
		const FVector ToTarget = Path[0] - Location;
		const float Distance = ToTarget.Size2D();
		if (Distance <= Budget)
		{
			Location = Path[0];
			Budget -= Distance;
			Path.RemoveAt(0);
			continue;
		}
		const FVector Step = ToTarget.GetSafeNormal2D() * Budget;
		Location += Step;
		Budget = 0.f;
		// 가는 쪽을 본다.
		SetActorRotation(FRotator(0.f, Step.Rotation().Yaw, 0.f));
	}
	SetActorLocation(Location);

	// 걸을 때 통통 튄다 (자리 표시 도형에 걷는 느낌을 주는 연출).
	WalkTime += DeltaSeconds;
	Body->SetRelativeLocation(FVector(0.f, 0.f, FMath::Abs(FMath::Sin(WalkTime * 11.f)) * 5.f));
	UpdateStackSway(DeltaSeconds, !Path.IsEmpty());
}

void AHGCustomer::UpdateStackSway(float DeltaSeconds, bool bWalking)
{
	if (Stack.IsEmpty() || (!bWalking && StackSwayWeight == 0.f))
	{
		return;
	}

	// 걸음에 맞춰 흔들리되 위의 꼬치일수록 살짝 늦고 크게 따라온다.
	StackSwayWeight = FMath::FInterpConstantTo(StackSwayWeight, bWalking ? 1.f : 0.f, DeltaSeconds, 6.f);
	for (int32 Layer = 0; Layer < Stack.Num(); ++Layer)
	{
		AHGSkewer* Skewer = Stack[Layer];
		if (!IsValid(Skewer) || Skewer == PickingSkewer)
		{
			continue;
		}

		const float Height = Layer * AHGSkewer::StackSpacing;
		const float Phase = WalkTime * 11.f - Layer * 0.16f;
		const float Swing = StackSwayWeight * Height;
		const float Side = FMath::Sin(Phase) * Swing * 0.09f;
		const float Forward = FMath::Cos(Phase) * Swing * 0.035f;
		Skewer->SetActorRelativeLocation(FVector(Forward, Side, 8.f + Height));
		Skewer->SetActorRelativeRotation(FRotator(0.f, 90.f + Side * 0.38f, 0.f));
	}
}

// ---------------------------------------------------------------------------

void AHGCustomer::ShowPlate()
{
	Plate->SetVisibility(true);
	LeftGrip->SetVisibility(true);
	RightGrip->SetVisibility(true);
}

void AHGCustomer::AddSkewer(const FLinearColor& Color, const FVector& From, float Duration)
{
	if (AHGSkewer* Skewer = GetWorld()->SpawnActor<AHGSkewer>())
	{
		Skewer->AttachToComponent(PlateAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		PickupStart = PlateAnchor->GetComponentTransform().InverseTransformPosition(From);
		Skewer->SetActorRelativeLocation(PickupStart);
		Skewer->SetActorRelativeRotation(FRotator(0.f, 90.f, 0.f));
		Skewer->SetFoodColor(Color);
		Stack.Add(Skewer);
		PickingSkewer = Skewer;
		PickupTime = 0.f;
		PickupDuration = Duration;
	}
}

void AHGCustomer::PlacePlate(const FVector& Location)
{
	PlateAnchor->SetWorldLocation(Location);
	LeftGrip->SetVisibility(false);
	RightGrip->SetVisibility(false);
}

void AHGCustomer::SetWaitingForBroth(bool bWaiting)
{
	BrothPrompt->SetVisibility(bWaiting);
}

void AHGCustomer::StartDip(const FVector& BrothLocation, float Duration)
{
	if (IsValid(DippingSkewer))
	{
		DippingSkewer->Destroy();
		DippingSkewer = nullptr;
	}
	if (!Stack.IsEmpty())
	{
		DippingSkewer = Stack.Pop();
		DippingSkewer->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		DipStart = DippingSkewer->GetActorLocation();
		DipStartRotation = DippingSkewer->GetActorRotation();
		DipTarget = BrothLocation;
		DipDuration = Duration;
		DipTime = 0.f;
	}
}

void AHGCustomer::TickMeal(float DeltaSeconds)
{
	if (IsValid(PickingSkewer))
	{
		PickupTime += DeltaSeconds;
		const float Alpha = FMath::Min(PickupTime / PickupDuration, 1.f);
		const FVector StackSlot(0.f, 0.f, 8.f + (Stack.Num() - 1) * AHGSkewer::StackSpacing);
		const FVector Arc = FVector::UpVector * (FMath::Sin(Alpha * PI) * 24.f);
		PickingSkewer->SetActorRelativeLocation(FMath::Lerp(PickupStart, StackSlot, Alpha) + Arc);
		if (Alpha >= 1.f)
		{
			PickingSkewer = nullptr;
		}
	}
	if (BrothPrompt->IsVisible())
	{
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
		{
			BrothPrompt->SetWorldRotation((PC->PlayerCameraManager->GetCameraLocation() - BrothPrompt->GetComponentLocation()).Rotation());
		}
	}
	if (!IsValid(DippingSkewer))
	{
		return;
	}
	DipTime += DeltaSeconds;
	const float Alpha = DipTime / DipDuration;
	if (Alpha >= 1.f)
	{
		DippingSkewer->Destroy();
		DippingSkewer = nullptr;
		return;
	}

	const FVector ToPot = (DipTarget - GetActorLocation()).GetSafeNormal2D();
	const FRotator DipRotation(-60.f, ToPot.Rotation().Yaw, 0.f);
	if (Alpha < 0.45f)
	{
		const float Reach = Alpha / 0.45f;
		const FVector Arc = FVector::UpVector * (FMath::Sin(Reach * PI) * 42.f);
		DippingSkewer->SetActorLocationAndRotation(FMath::Lerp(DipStart, DipTarget, Reach) + Arc, FMath::Lerp(DipStartRotation, DipRotation, Reach));
	}
	else
	{
		const FVector Mouth = Body->GetComponentLocation() + FVector(0.f, 0.f, 105.f) + ToPot * 28.f;
		const float Lift = FMath::Clamp((Alpha - 0.64f) / 0.36f, 0.f, 1.f);
		DippingSkewer->SetActorLocationAndRotation(FMath::Lerp(DipTarget, Mouth, Lift), DipRotation);
	}
}

void AHGCustomer::ClearMeal()
{
	for (AHGSkewer* Skewer : Stack)
	{
		if (IsValid(Skewer))
		{
			Skewer->Destroy();
		}
	}
	Stack.Reset();
	StackSwayWeight = 0.f;
	PickingSkewer = nullptr;
	if (IsValid(DippingSkewer))
	{
		DippingSkewer->Destroy();
	}
	DippingSkewer = nullptr;
	Plate->SetVisibility(false);
	LeftGrip->SetVisibility(false);
	RightGrip->SetVisibility(false);
	SetWaitingForBroth(false);
}

void AHGCustomer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearMeal();
	Super::EndPlay(EndPlayReason);
}

AHGSkewer::AHGSkewer()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Stick = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stick"));
	Stick->SetupAttachment(Root);
	SetPropMesh(Stick, CubeFinder.Object, FVector::ZeroVector, FVector(0.59f, 0.037f, 0.037f));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FName Name(*FString::Printf(TEXT("Food%d"), Index));
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Piece->SetupAttachment(Root);
		SetPropMesh(Piece, SphereFinder.Object, FVector(5.f + (Index - 1) * 14.f, 0.f, 0.f), FVector(0.15f, 0.17f, 0.13f));
		FoodPieces.Add(Piece);
	}
}

void AHGSkewer::SetFoodColor(const FLinearColor& Color)
{
	HGPlaceholder::Paint(Stick, FLinearColor(0.64f, 0.38f, 0.15f));
	for (UStaticMeshComponent* Piece : FoodPieces)
	{
		HGPlaceholder::Paint(Piece, Color);
	}
}

// ---------------------------------------------------------------------------

AHGMoneyPopup::AHGMoneyPopup()
{
	PrimaryActorTick.bCanEverTick = true;

	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	SetRootComponent(Text);
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetWorldSize(48.f);
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Text->SetCastShadow(false);
}

void AHGMoneyPopup::SetupText(const FString& InText, const FColor& Color)
{
	Text->SetText(FText::FromString(InText));
	Text->SetTextRenderColor(Color);
}

void AHGMoneyPopup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age >= LifeSeconds)
	{
		Destroy();
		return;
	}

	AddActorWorldOffset(FVector(0.f, 0.f, RiseSpeed * DeltaSeconds));

	// 글자 앞면(+X)이 카메라를 보게 돌린다. 경영 카메라가 어느 방향으로 돌아 있어도 읽힌다.
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			const FVector ToCamera = PC->PlayerCameraManager->GetCameraLocation() - GetActorLocation();
			SetActorRotation(ToCamera.Rotation());
		}
	}

	// 마지막 0.4초 동안 작아지며 사라진다.
	const float Remaining = LifeSeconds - Age;
	SetActorScale3D(FVector(FMath::Clamp(Remaining / 0.4f, 0.05f, 1.f)));
}
