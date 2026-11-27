#include "Player/HGShopPlayerController.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "Game/HGGameInstance.h"
#include "HuoguoHuoguo.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Shop/Customer/HGCustomerDirector.h"
#include "Shop/Furniture/HGFurniture.h"
#include "Shop/Furniture/HGFurniturePlacer.h"
#include "Player/HGShopBossCharacter.h"
#include "World/Light/HGStationLighting.h"
#include "HAL/IConsoleManager.h"
#include "World/Travel/HGExplorerStart.h"
#include "World/Camera/HGShopCameraAnchor.h"
#include "World/Travel/HGTravelPad.h"

AHGShopPlayerController::AHGShopPlayerController()
{
	FurniturePlacer = CreateDefaultSubobject<UHGFurniturePlacer>(TEXT("FurniturePlacer"));
	CustomerDirector = CreateDefaultSubobject<UHGCustomerDirector>(TEXT("CustomerDirector"));
	StationLighting = CreateDefaultSubobject<UHGStationLighting>(TEXT("StationLighting"));
}

void AHGShopPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		InitializeFromLevel();
	}
}

namespace
{
	// 정거장 화면에 계속 떠 있는 접근 행성 목록의 고정 메시지 키.
	constexpr uint64 PlanetBoardKey = 0x4847'0002;
}

void AHGShopPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(PlanetBoardKey);
	}
	Super::EndPlay(EndPlayReason);
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
	UClass* BossToSpawn = ShopBossClass ? ShopBossClass.Get() : AHGShopBossCharacter::StaticClass();
	ShopBoss = World->SpawnActor<AHGShopBossCharacter>(BossToSpawn, StartTransform, Params);
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

	// 손님 흐름 시작. 계산 메시지는 화면 메시지로, 돈이 바뀌면 조작 안내 줄(가진 돈 표시)을 다시 쓴다.
	CustomerDirector->OnMessage = [this](const FString& Text) { ShowMessage(Text); };
	CustomerDirector->OnMoneyChanged = [this]() { UpdateControlsHint(); };
	CustomerDirector->Start(FurniturePlacer);

	// 정거장 조명을 지금 시간대에 맞춘다 (밤이면 해를 줄이고 가구 불빛을 켠다). 행성에서 돌아와 시간대가 넘어가면 아래에서 다시 맞춘다.
	StationLighting->Start();
	ApplyPhaseLighting();

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
		// 다이빙하고 돌아왔으면 시간대가 한 칸 넘어간다 (낮에 갔으면 밤, 밤에 갔으면 문 닫을 시간).
		UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
		if (GI && GI->FinishDive())
		{
			ShowMessage(FString::Printf(TEXT("행성에서 돌아오니 %s이 되었다"), *GI->DescribeTime()));
			UpdatePlanetBoard();
			ApplyPhaseLighting();
		}
		// 행성에서 가져온 재료를 알려 준다. 진열대/창고 UI가 생기면 그쪽으로 옮긴다.
		HGBag();
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
	Input->BindAction(CycleFurnitureAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnCycleFurniture);
	Input->BindAction(UnlockFurnitureAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnUnlockFurniture);
	Input->BindAction(PassTimeAction, ETriggerEvent::Started, this, &AHGShopPlayerController::OnPassTime);
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
	CycleFurnitureAction = MakeAction(TEXT("IA_Shop_CycleFurniture"), EInputActionValueType::Axis1D);
	UnlockFurnitureAction = MakeAction(TEXT("IA_Shop_UnlockFurniture"), EInputActionValueType::Boolean);
	PassTimeAction = MakeAction(TEXT("IA_Shop_PassTime"), EInputActionValueType::Boolean);

	// 숫자 키: 액션 하나에 키 여러 개를 매핑하고, 배율 수정자로 키마다 값을 1, 2, 3...으로 다르게 낸다.
	// 받는 쪽은 값을 반올림해서 몇 번인지 안다. 키마다 액션을 따로 만들지 않아도 된다.
	const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };

	// 평소: 이동, 카메라, 행성 선택(1~4), 시간 넘기기(T).
	ShopContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Shop"));
	MapCommonKeys(ShopContext);
	ShopContext->MapKey(PassTimeAction, EKeys::T);
	for (int32 i = 0; i < 4; ++i)
	{
		MapScaledKey(ShopContext, SelectPlanetAction, NumberKeys[i], static_cast<float>(i + 1));
	}

	// 배치 모드: 이동, 카메라는 그대로 두고, 숫자 키 1~3은 분류(식탁/의자/장식), Z/C는 분류 안에서 이전/다음 가구,
	// 좌클릭/R/X는 배치 조작으로 쓴다. 가구가 수십 종이라 숫자 키 하나에 가구 하나를 붙일 수 없다 (정식 UI는 가구 목록 화면).
	BuildContext = NewObject<UInputMappingContext>(this, TEXT("IMC_ShopBuild"));
	MapCommonKeys(BuildContext);
	BuildContext->MapKey(PlaceAction, EKeys::LeftMouseButton);
	BuildContext->MapKey(RotateFurnitureAction, EKeys::R);
	BuildContext->MapKey(RemoveFurnitureAction, EKeys::X);
	for (int32 i = 0; i < 3; ++i)
	{
		MapScaledKey(BuildContext, SelectFurnitureAction, NumberKeys[i], static_cast<float>(i + 1));
	}
	MapDirectionalKey(BuildContext, CycleFurnitureAction, EKeys::Z, false, true);
	MapDirectionalKey(BuildContext, CycleFurnitureAction, EKeys::C, false, false);
	BuildContext->MapKey(UnlockFurnitureAction, EKeys::U);
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
		// 유령을 다른 칸으로 옮겨 콤보 미리보기가 바뀌었으면 안내 줄을 다시 쓴다.
		if (FurniturePlacer->GetPreviewText() != LastPreviewText)
		{
			LastPreviewText = FurniturePlacer->GetPreviewText();
			UpdateControlsHint();
		}
	}
}

void AHGShopPlayerController::UpdateControlsHint() const
{
	if (FurniturePlacer->IsBuilding())
	{
		const FString& Preview = FurniturePlacer->GetPreviewText();
		SetControlsHint(FString::Printf(TEXT("[가구 배치] 손에 든 것: %s\n1 식탁 / 2 의자 / 3 장식 | Z/C 이전/다음 가구 | U 잠긴 가구 열기 | 좌클릭 놓기/집기 | R 회전 | X 치우기/취소 | B 끝%s%s"),
			*FurniturePlacer->DescribeHeld(), Preview.IsEmpty() ? TEXT("") : TEXT("\n>> "), *Preview));
	}
	else
	{
		const UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
		SetControlsHint(FString::Printf(TEXT("[경영] 가진 돈 %d | WASD 사장님 이동 | 우클릭 드래그 회전 | Q/E 90도 회전 | 휠 확대 | 1~4 행성 선택 | B 가구 배치 | T 시간 넘기기 | 청록 발판 = 다이빙"),
			GI ? GI->GetMoney() : 0));
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
	HGBuildCategory(FMath::RoundToInt(Value.Get<float>()));
}

void AHGShopPlayerController::OnCycleFurniture(const FInputActionValue& Value)
{
	const float Dir = Value.Get<float>();
	if (!FMath::IsNearlyZero(Dir))
	{
		FurniturePlacer->CycleInCategory(Dir > 0.f ? 1 : -1);
		UpdateControlsHint();
	}
}

void AHGShopPlayerController::OnPassTime(const FInputActionValue& Value)
{
	HGPassTime();
}

void AHGShopPlayerController::HGPassTime()
{
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (!GI || bTraveling)
	{
		return;
	}
	if (GI->PassTime())
	{
		ShowMessage(FString::Printf(TEXT("하루를 마쳤다. %s: 새 행성들이 접근한다"), *GI->DescribeTime()));
	}
	else if (GI->GetDayPhase() == EHGDayPhase::Closing)
	{
		ShowMessage(FString::Printf(TEXT("%s. 오늘은 더 다이빙할 수 없다. T로 하루를 마친다"), *GI->DescribeTime()));
	}
	else
	{
		ShowMessage(FString::Printf(TEXT("시간이 흘렀다: %s"), *GI->DescribeTime()));
	}
	UpdatePlanetBoard();
	ApplyPhaseLighting();
}

void AHGShopPlayerController::ApplyPhaseLighting()
{
	const UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	StationLighting->Apply(GI && GI->GetDayPhase() != EHGDayPhase::Day);
}

void AHGShopPlayerController::HGVoxel(int32 On)
{
	// 콘솔 변수를 바꾸고 맵의 가구(유령 포함)를 모두 다시 만든다. 밤 불빛과 김 상태는 가구가 기억해 두었다가 되살린다.
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("hg.Voxel")))
	{
		Var->Set(On != 0 ? 1 : 0, ECVF_SetByConsole);
	}
	int32 Count = 0;
	for (TActorIterator<AHGFurniture> It(GetWorld()); It; ++It)
	{
		It->Rebuild();
		++Count;
	}
	ShowMessage(FString::Printf(TEXT("가구 생김새: %s (%d개 다시 만듦)"), On != 0 ? TEXT("복셀") : TEXT("기본 도형"), Count));
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

void AHGShopPlayerController::OnUnlockFurniture(const FInputActionValue& Value)
{
	HGUnlock();
}

void AHGShopPlayerController::HGUnlock()
{
	ShowMessage(FurniturePlacer->TryUnlockHeld());
	UpdateControlsHint();
}

void AHGShopPlayerController::HGGiveMoney(int32 Amount)
{
	if (UHGGameInstance* GI = GetGameInstance<UHGGameInstance>())
	{
		GI->AddMoney(Amount);
		ShowMessage(FString::Printf(TEXT("돈 %+d, 가진 돈 %d"), Amount, GI->GetMoney()));
		UpdateControlsHint();
	}
}

void AHGShopPlayerController::HGBuildCategory(int32 Number)
{
	const EHGFurnitureCategory Categories[] = { EHGFurnitureCategory::Table, EHGFurnitureCategory::Chair, EHGFurnitureCategory::Decor };
	if (Number < 1 || Number > 3 || !FurniturePlacer->SelectCategory(Categories[Number - 1]))
	{
		ShowMessage(FString::Printf(TEXT("%d번 분류에 가구가 없다"), Number), FColor::Red);
		return;
	}
	UpdateControlsHint();
}

void AHGShopPlayerController::HGBuildSelect(FName FurnitureId)
{
	if (!FurniturePlacer->SelectFurniture(FurnitureId))
	{
		ShowMessage(FString::Printf(TEXT("가구 목록에 '%s'가 없다 (Data/Furniture.csv의 Name)"), *FurnitureId.ToString()), FColor::Red);
		return;
	}
	ShowMessage(FString::Printf(TEXT("배치할 가구: %s"), *FurniturePlacer->DescribeHeld()));
	UpdateControlsHint();
}

void AHGShopPlayerController::HGFurnitureList()
{
	for (const FName& Id : FurniturePlacer->GetCatalog())
	{
		if (const FHGFurnitureRow* Row = HGFurnitureData::FindRow(Id))
		{
			const FIntPoint Size = Row->GetFootprint();
			UE_LOG(LogHuoguo, Log, TEXT("[Furniture] %s %s [%s] %dx%d, Style %s, Pot %s, 값 %d, 손님 +%d%%, 꼬리표 %s"),
				HGFurnitureData::CategoryName(Row->Category), *Row->DisplayName.ToString(), *Id.ToString(), Size.X, Size.Y,
				*Row->Style.ToString(), *Row->Pot.ToString(), Row->Price, Row->BonusPercent, *Row->Tags);
		}
	}
}

void AHGShopPlayerController::HGCursorAt(float X, float Y)
{
	FurniturePlacer->SetCursorWorldOverride(FVector(X, Y, FurniturePlacer->Grid.FloorZ));
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

void AHGShopPlayerController::HGCustomers()
{
	UE_LOG(LogHuoguo, Log, TEXT("[Customer] %s"), *CustomerDirector->DescribeCustomers());
}

void AHGShopPlayerController::HGSpawnCustomer()
{
	ShowMessage(CustomerDirector->SpawnCustomerNow());
}

void AHGShopPlayerController::HGCombos()
{
	UE_LOG(LogHuoguo, Log, TEXT("[Combo] %s"), *CustomerDirector->DescribeTables());
}

void AHGShopPlayerController::HGGiveIngredient(FName IngredientId, int32 Count)
{
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (!GI || !GI->FindIngredient(IngredientId))
	{
		ShowMessage(FString::Printf(TEXT("재료 표에 없는 ID: %s"), *IngredientId.ToString()), FColor::Red);
		return;
	}
	GI->AddToBag(IngredientId, Count);
	HGBag();
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
		ShowMessage(FString::Printf(TEXT("궤도 주기 %d (%s): 새 행성들이 접근한다"), GI->GetOrbitCycle(), *GI->DescribeTime()));
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
	// 첫 줄에 지금 시간대를 붙인다. 낮에 내려가면 낮 재료, 밤에 내려가면 밤 재료가 보이므로 다이빙 전에 알아야 한다.
	FString Board = GI->CanDive()
		? FString::Printf(TEXT("== %s | 접근 중인 행성 | 숫자 키로 선택, 청록 발판으로 다이빙 (지금 내려가면 %s 재료) =="),
			*GI->DescribeTime(), HGDay::PhaseName(GI->GetDayPhase()))
		: FString::Printf(TEXT("== %s | 오늘은 다이빙 끝. T로 하루를 마치면 새 행성들이 접근한다 =="), *GI->DescribeTime());
	const TArray<FHGPlanetSeed>& Planets = GI->GetApproachingPlanets();
	for (int32 i = 0; i < Planets.Num(); ++i)
	{
		Board += FString::Printf(TEXT("\n%s %d) %s"), i == GI->GetSelectedPlanetIndex() ? TEXT(">>") : TEXT("  "), i + 1, *Planets[i].Describe());
	}
	GEngine->AddOnScreenDebugMessage(PlanetBoardKey, 100000.f, FColor(244, 178, 62), Board);
}

FString AHGShopPlayerController::DescribeForSimKey() const
{
	return FString::Printf(TEXT(", 카메라 Yaw %.1f, Pitch %.1f"), GetControlRotation().Yaw, GetControlRotation().Pitch);
}
