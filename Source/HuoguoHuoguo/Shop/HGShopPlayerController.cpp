#include "Shop/HGShopPlayerController.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "Game/HGGameInstance.h"
#include "HuoguoHuoguo.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Shop/HGFurniture.h"
#include "Shop/HGFurniturePlacer.h"
#include "Shop/HGShopBossCharacter.h"
#include "World/HGExplorerStart.h"
#include "World/HGShopCameraAnchor.h"
#include "World/HGTravelPad.h"

AHGShopPlayerController::AHGShopPlayerController()
{
	FurniturePlacer = CreateDefaultSubobject<UHGFurniturePlacer>(TEXT("FurniturePlacer"));
}

void AHGShopPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		InitializeFromLevel();
	}
}

void AHGShopPlayerController::InitializeFromLevel()
{
	UWorld* World = GetWorld();
	const FName ArrivalTag = ConsumeArrivalTag();
	const bool bArrivedByTravel = !ArrivalTag.IsNone();

	FTransform StartTransform(FVector(0.f, 0.f, 120.f));
	AHGExplorerStart* Start = FindStart(ArrivalTag);
	if (Start)
	{
		StartTransform = Start->GetActorTransform();
		StartTransform.SetScale3D(FVector::OneVector);
	}
	else
	{
		UE_LOG(LogHuoguo, Warning, TEXT("정거장 맵에 HGExplorerStart가 없다. 원점에 사장님을 만든다"));
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ShopBoss = World->SpawnActor<AHGShopBossCharacter>(AHGShopBossCharacter::StaticClass(), StartTransform, Params);
	Boss = ShopBoss;
	Possess(ShopBoss);
	SetViewTarget(ShopBoss);

	// 카메라 처음 방향: 가게 기준점의 화살표 방향. 가게를 디자인한 각도로 시작하게 하려는 것이다.
	float Yaw = StartTransform.Rotator().Yaw;
	for (TActorIterator<AHGShopCameraAnchor> It(World); It; ++It)
	{
		Yaw = It->GetActorRotation().Yaw;
		break;
	}
	SetControlRotation(FRotator(ShopBoss->GetCameraPitch(), Yaw, 0.f));

	// 행성에 다녀왔으면 사장님이 바꿔 둔 가구 배치로 되돌린다. 처음이면 맵의 배치를 기록해 둔다.
	FurniturePlacer->RestoreLayout();

	BuildInputObjects();
	ApplyMappingContext(ShopContext);

	// 경영은 커서가 보이는 클릭 조작이다. 마우스를 창 안에 가두지 않는다.
	bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	UpdateControlsHint();
	UpdatePlanetBoard();

	if (bArrivedByTravel)
	{
		FadeInAfterTravel();
	}

	UE_LOG(LogHuoguo, Log, TEXT("경영 파트 시작: %s, 시작점 %s, 카메라 Yaw %.1f"), *World->GetMapName(), *GetNameSafe(Start), Yaw);
}

void AHGShopPlayerController::UpdateRotation(float DeltaTime)
{
	Super::UpdateRotation(DeltaTime);

	// 좌우(Yaw)만 남긴다. Pitch 입력이 섞여 들어와도(마우스를 위아래로 끌어도) 내려다보는 각도는 그대로다.
	if (ShopBoss)
	{
		FRotator Rotation = GetControlRotation();
		if (!FMath::IsNearlyEqual(Rotation.Pitch, ShopBoss->GetCameraPitch()) || !FMath::IsNearlyZero(Rotation.Roll))
		{
			Rotation.Pitch = ShopBoss->GetCameraPitch();
			Rotation.Roll = 0.f;
			SetControlRotation(Rotation);
		}
	}
}

void AHGShopPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInputObjects();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		UE_LOG(LogHuoguo, Error, TEXT("EnhancedInputComponent가 아니다. DefaultInput.ini의 DefaultInputComponentClass를 확인할 것"));
		return;
	}

	// Started = 누르는 순간 한 번, Triggered = 누르고 있는 동안 매 프레임, Completed = 뗄 때 한 번.
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHGShopPlayerController::OnMove);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHGShopPlayerController::OnLook);
	Input->BindAction(RotateHoldAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnRotateHoldStarted);
	Input->BindAction(RotateHoldAction, ETriggerEvent::Completed, this, &AHGShopPlayerController::OnRotateHoldCompleted);
	Input->BindAction(RotateStepAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnRotateStep);
	Input->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AHGShopPlayerController::OnZoom);
	Input->BindAction(SelectPlanetAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnSelectPlanet);
	Input->BindAction(ToggleBuildAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnToggleBuild);
	Input->BindAction(PlaceAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnPlace);
	Input->BindAction(RotateFurnitureAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnRotateFurniture);
	Input->BindAction(RemoveFurnitureAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnRemoveFurniture);
	Input->BindAction(SelectFurnitureAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnSelectFurniture);
}

void AHGShopPlayerController::BuildInputObjects()
{
	if (ShopContext)
	{
		return;
	}

	MoveAction = MakeAction(TEXT("IA_Shop_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Shop_Look"), EInputActionValueType::Axis2D);
	RotateHoldAction = MakeAction(TEXT("IA_Shop_RotateHold"), EInputActionValueType::Boolean);
	RotateStepAction = MakeAction(TEXT("IA_Shop_RotateStep"), EInputActionValueType::Axis1D);
	ZoomAction = MakeAction(TEXT("IA_Shop_Zoom"), EInputActionValueType::Axis1D);
	SelectPlanetAction = MakeAction(TEXT("IA_Shop_SelectPlanet"), EInputActionValueType::Axis1D);
	ToggleBuildAction = MakeAction(TEXT("IA_Shop_ToggleBuild"), EInputActionValueType::Boolean);
	PlaceAction = MakeAction(TEXT("IA_Shop_Place"), EInputActionValueType::Boolean);
	RotateFurnitureAction = MakeAction(TEXT("IA_Shop_RotateFurniture"), EInputActionValueType::Boolean);
	RemoveFurnitureAction = MakeAction(TEXT("IA_Shop_RemoveFurniture"), EInputActionValueType::Boolean);
	SelectFurnitureAction = MakeAction(TEXT("IA_Shop_SelectFurniture"), EInputActionValueType::Axis1D);

	// 숫자 키: 액션 하나에 키 여러 개를 매핑하고, 배율 수정자로 키마다 값을 1, 2, 3...으로 다르게 낸다.
	// 받는 쪽은 값을 반올림해서 몇 번인지 안다. 키마다 액션을 따로 만들지 않아도 된다.
	const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };

	// 평소: 이동, 카메라, 행성 선택(1~4).
	ShopContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Shop"));
	MapCommonKeys(ShopContext);
	for (int32 i = 0; i < 4; ++i)
	{
		MapScaledKey(ShopContext, SelectPlanetAction, NumberKeys[i], static_cast<float>(i + 1));
	}

	// 배치 모드: 이동, 카메라는 그대로 두고 숫자 키는 가구 고르기로, 좌클릭/R/X는 배치 조작으로 쓴다.
	BuildContext = NewObject<UInputMappingContext>(this, TEXT("IMC_ShopBuild"));
	MapCommonKeys(BuildContext);
	BuildContext->MapKey(PlaceAction, EKeys::LeftMouseButton);
	BuildContext->MapKey(RotateFurnitureAction, EKeys::R);
	BuildContext->MapKey(RemoveFurnitureAction, EKeys::X);
	const int32 FurnitureCount = FMath::Min(FurniturePlacer->Catalog.Num(), static_cast<int32>(UE_ARRAY_COUNT(NumberKeys)));
	for (int32 i = 0; i < FurnitureCount; ++i)
	{
		MapScaledKey(BuildContext, SelectFurnitureAction, NumberKeys[i], static_cast<float>(i + 1));
	}
}

void AHGShopPlayerController::MapCommonKeys(UInputMappingContext* Context)
{
	MapWASD(Context, MoveAction);
	Context->MapKey(LookAction, EKeys::Mouse2D);
	Context->MapKey(RotateHoldAction, EKeys::RightMouseButton);
	MapDirectionalKey(Context, RotateStepAction, EKeys::Q, false, true);
	MapDirectionalKey(Context, RotateStepAction, EKeys::E, false, false);
	Context->MapKey(ZoomAction, EKeys::MouseWheelAxis);
	Context->MapKey(ToggleBuildAction, EKeys::B);
}

void AHGShopPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (FurniturePlacer->IsBuilding())
	{
		FurniturePlacer->UpdateCursor();
	}
}

void AHGShopPlayerController::UpdateControlsHint() const
{
	if (FurniturePlacer->IsBuilding())
	{
		FString Choices;
		for (int32 i = 0; i < FurniturePlacer->Catalog.Num(); ++i)
		{
			if (const UClass* Class = FurniturePlacer->Catalog[i])
			{
				Choices += FString::Printf(TEXT("%s%d %s"), i > 0 ? TEXT(" / ") : TEXT(""), i + 1,
					*Class->GetDefaultObject<AHGFurniture>()->DisplayName.ToString());
			}
		}
		SetControlsHint(FString::Printf(TEXT("[가구 배치] 손에 든 것: %s | %s | 좌클릭 놓기/집기 | R 회전 | X 치우기/취소 | B 끝"),
			*FurniturePlacer->GetHeldName().ToString(), *Choices));
	}
	else
	{
		SetControlsHint(TEXT("[경영] WASD 사장님 이동 | 우클릭 드래그 회전 | Q/E 90도 회전 | 휠 확대 | 1~4 행성 선택 | B 가구 배치 | 청록 발판 = 다이빙"));
	}
}

void AHGShopPlayerController::OnMove(const FInputActionValue& Value)
{
	++MoveEventCount;
	if (ShopBoss && !bTraveling)
	{
		ShopBoss->Move(Value.Get<FVector2D>());
	}
}

void AHGShopPlayerController::OnLook(const FInputActionValue& Value)
{
	// 우클릭을 누르고 있는 동안만 돈다. 그 밖의 마우스 움직임은 커서 이동이다.
	// 좌우 움직임(X)만 쓴다. 위아래 움직임(Y)은 버린다.
	if (bRotateHeld)
	{
		AddYawInput(Value.Get<FVector2D>().X * RotateSensitivity);
	}
}

void AHGShopPlayerController::OnRotateHoldStarted(const FInputActionValue& Value)
{
	bRotateHeld = true;
}

void AHGShopPlayerController::OnRotateHoldCompleted(const FInputActionValue& Value)
{
	bRotateHeld = false;
}

void AHGShopPlayerController::OnRotateStep(const FInputActionValue& Value)
{
	const float Dir = Value.Get<float>();
	if (!FMath::IsNearlyZero(Dir))
	{
		HGRotate(Dir > 0.f ? RotateStepDegrees : -RotateStepDegrees);
	}
}

void AHGShopPlayerController::OnZoom(const FInputActionValue& Value)
{
	if (ShopBoss)
	{
		ShopBoss->AddZoomInput(Value.Get<float>());
	}
}

void AHGShopPlayerController::OnSelectPlanet(const FInputActionValue& Value)
{
	HGSelectPlanet(FMath::RoundToInt(Value.Get<float>()));
}

void AHGShopPlayerController::OnToggleBuild(const FInputActionValue& Value)
{
	HGBuild();
}

void AHGShopPlayerController::OnPlace(const FInputActionValue& Value)
{
	const FString Result = FurniturePlacer->Click();
	if (!Result.IsEmpty())
	{
		ShowMessage(Result);
	}
	UpdateControlsHint(); // 집어 들었으면 손에 든 것이 바뀐다.
}

void AHGShopPlayerController::OnRotateFurniture(const FInputActionValue& Value)
{
	FurniturePlacer->RotateHeld(FurniturePlacer->RotateStepDegrees);
}

void AHGShopPlayerController::OnRemoveFurniture(const FInputActionValue& Value)
{
	const FString Result = FurniturePlacer->RemoveOrCancel();
	if (!Result.IsEmpty())
	{
		ShowMessage(Result);
	}
	UpdateControlsHint();
}

void AHGShopPlayerController::OnSelectFurniture(const FInputActionValue& Value)
{
	HGBuildSelect(FMath::RoundToInt(Value.Get<float>()));
}

void AHGShopPlayerController::HGBuild()
{
	if (bTraveling)
	{
		return;
	}
	if (FurniturePlacer->IsBuilding())
	{
		FurniturePlacer->EndBuild();
		ApplyMappingContext(ShopContext);
		ShowMessage(TEXT("가구 배치 끝"));
	}
	else
	{
		FurniturePlacer->BeginBuild();
		ApplyMappingContext(BuildContext);
		ShowMessage(TEXT("가구 배치 시작"));
	}
	// 회전용 우클릭이 눌린 채로 컨텍스트가 바뀌면 뗌(Completed)을 못 받을 수 있어서 회전 상태를 풀어 둔다.
	bRotateHeld = false;
	UpdateControlsHint();
}

void AHGShopPlayerController::HGBuildSelect(int32 Number)
{
	if (!FurniturePlacer->SelectCatalog(Number - 1))
	{
		ShowMessage(FString::Printf(TEXT("%d번 가구는 목록에 없다"), Number), FColor::Red);
		return;
	}
	ShowMessage(FString::Printf(TEXT("배치할 가구: %s"), *FurniturePlacer->GetHeldName().ToString()));
	UpdateControlsHint();
}

void AHGShopPlayerController::HGCursorAt(float X, float Y)
{
	FurniturePlacer->SetCursorWorldOverride(FVector(X, Y, FurniturePlacer->FloorZ));
	UE_LOG(LogHuoguo, Log, TEXT("[Furniture] 시험 커서를 바닥 (%.0f, %.0f)에 둠"), X, Y);
}

void AHGShopPlayerController::HGCursorClear()
{
	FurniturePlacer->SetCursorWorldOverride(TOptional<FVector>());
}

void AHGShopPlayerController::HGFurniture()
{
	FurniturePlacer->UpdateCursor();
	UE_LOG(LogHuoguo, Log, TEXT("[Furniture] %s"), *FurniturePlacer->DescribeLayout());
	UE_LOG(LogHuoguo, Log, TEXT("[Furniture] 커서: %s"), *FurniturePlacer->DescribeCursor());
}

void AHGShopPlayerController::HGRotate(float Degrees)
{
	// 컨트롤러 회전은 바로 바뀌고, 화면은 스프링 암의 회전 지연 때문에 부드럽게 따라 돈다.
	FRotator Rotation = GetControlRotation();
	Rotation.Yaw = FRotator::NormalizeAxis(Rotation.Yaw + Degrees);
	SetControlRotation(Rotation);
}

void AHGShopPlayerController::HGPlanets()
{
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		return;
	}
	const TArray<FHGPlanetSeed>& Planets = GI->GetApproachingPlanets();
	for (int32 i = 0; i < Planets.Num(); ++i)
	{
		UE_LOG(LogHuoguo, Log, TEXT("[Planets] %s%d) %s, 시드 %d"),
			i == GI->GetSelectedPlanetIndex() ? TEXT(">") : TEXT(" "), i + 1, *Planets[i].Describe(), Planets[i].Seed);
	}
	UpdatePlanetBoard();
}

void AHGShopPlayerController::HGSelectPlanet(int32 Number)
{
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (!GI || !GI->SelectPlanet(Number - 1))
	{
		ShowMessage(FString::Printf(TEXT("%d번 행성은 목록에 없다"), Number), FColor::Red);
		return;
	}
	ShowMessage(FString::Printf(TEXT("다이빙 목적지: %s"), *GI->GetSelectedPlanet()->Describe()));
	UpdatePlanetBoard();
}

void AHGShopPlayerController::HGDive(int32 Number)
{
	if (Number > 0)
	{
		HGSelectPlanet(Number);
	}
	for (TActorIterator<AHGTravelPad> It(GetWorld()); It; ++It)
	{
		if (It->bDiveToSelectedPlanet)
		{
			RequestTravel(*It);
			return;
		}
	}
	ShowMessage(TEXT("이 맵에는 다이빙 발판이 없다"), FColor::Red);
}

void AHGShopPlayerController::HGNextCycle()
{
	if (UHGGameInstance* GI = GetGameInstance<UHGGameInstance>())
	{
		GI->AdvanceOrbitCycle();
		ShowMessage(FString::Printf(TEXT("궤도 주기 %d: 새 행성들이 접근한다"), GI->GetOrbitCycle()));
		HGPlanets();
	}
}

void AHGShopPlayerController::UpdatePlanetBoard() const
{
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (!GEngine || !GI)
	{
		return;
	}

	// 정거장의 "도착 전광판" 임시판. 정식 UI가 생기면 이 내용을 위젯으로 옮긴다.
	constexpr uint64 BoardKey = 0x4847'0002;
	FString Board = FString::Printf(TEXT("== 접근 중인 행성 (궤도 주기 %d) | 숫자 키로 선택, 청록 발판으로 다이빙 =="), GI->GetOrbitCycle());
	const TArray<FHGPlanetSeed>& Planets = GI->GetApproachingPlanets();
	for (int32 i = 0; i < Planets.Num(); ++i)
	{
		Board += FString::Printf(TEXT("\n%s %d) %s"), i == GI->GetSelectedPlanetIndex() ? TEXT(">>") : TEXT("  "), i + 1, *Planets[i].Describe());
	}
	GEngine->AddOnScreenDebugMessage(BoardKey, 100000.f, FColor(244, 178, 62), Board);
}

FString AHGShopPlayerController::DescribeForSimKey() const
{
	return FString::Printf(TEXT(", 카메라 Yaw %.1f, Pitch %.1f"), GetControlRotation().Yaw, GetControlRotation().Pitch);
}
