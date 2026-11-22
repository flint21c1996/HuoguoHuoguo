#include "Game/HGPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Explore/HGExplorerCharacter.h"
#include "Game/HGGameMode.h"
#include "HuoguoHuoguo.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Shop/HGShopCameraPawn.h"
#include "TimerManager.h"
#include "World/HGTravelPad.h"

namespace
{
	/**
	 * 키 하나를 2D 입력의 한 방향으로 매핑한다.
	 *
	 * Enhanced Input에서 키보드 키는 1D 값(눌림 = 1)만 낸다. 이것을 2D 벡터의 한 방향으로 만들려면 수정자를 붙인다.
	 *   D: 그대로 -> (1, 0)            오른쪽
	 *   A: Negate -> (-1, 0)           왼쪽
	 *   W: Swizzle(YXZ) -> (0, 1)      앞쪽. 축을 바꿔서 X에 들어온 1을 Y로 옮긴다.
	 *   S: Swizzle 후 Negate -> (0, -1) 뒤쪽
	 * 같은 액션에 여러 키가 매핑되면 값이 합쳐지므로 W+D를 같이 누르면 (1, 1) 대각선이 된다.
	 */
	void MapDirectionalKey(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate, UObject* Outer)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Outer);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Outer));
		}
	}

	void MapWASD(UInputMappingContext* Context, const UInputAction* Action, UObject* Outer)
	{
		MapDirectionalKey(Context, Action, EKeys::W, true, false, Outer);
		MapDirectionalKey(Context, Action, EKeys::S, true, true, Outer);
		MapDirectionalKey(Context, Action, EKeys::A, false, true, Outer);
		MapDirectionalKey(Context, Action, EKeys::D, false, false, Outer);
	}
}

AHGPlayerController::AHGPlayerController()
{
	// 빙의(Possess)할 때 엔진이 뷰 타깃을 즉시 새 Pawn으로 바꾸는 기본 동작을 끈다.
	// 카메라 전환은 SetViewTargetWithBlend로 직접 처리해서 부드럽게 이어지게 한다.
	bAutoManageActiveCameraTarget = false;
}

void AHGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 게임 모드가 StartPlay에서 이미 초기화했다면 아무 일도 하지 않는다.
	// 게임 모드보다 늦게 생긴 컨트롤러(늦은 접속 등)만 여기서 초기화된다.
	if (!bModesInitialized)
	{
		if (const AHGGameMode* GM = GetWorld()->GetAuthGameMode<AHGGameMode>())
		{
			if (GM->GetLayout().bValid)
			{
				InitializeModes(GM->GetLayout());
			}
		}
	}
}

void AHGPlayerController::InitializeModes(const FHGWorldLayout& Layout)
{
	if (bModesInitialized || !IsLocalController())
	{
		return;
	}
	bModesInitialized = true;
	CachedLayout = Layout;

	// 1) 경영 카메라: 게임 모드가 기본 Pawn으로 만들어 준 것을 쓰고, 없으면 직접 만든다.
	ShopPawn = Cast<AHGShopCameraPawn>(GetPawn());
	if (!ShopPawn)
	{
		ShopPawn = GetWorld()->SpawnActor<AHGShopCameraPawn>(AHGShopCameraPawn::StaticClass(), FTransform(Layout.ShopFocus));
		Possess(ShopPawn);
	}
	ShopPawn->SnapTo(Layout.ShopFocus, Layout.ShopYaw, Layout.ShopPanRadius);

	// 2) 탐험 캐릭터: 가게 안에 미리 세워 둔다. 경영 모드에서도 월드에 보이고, 빙의만 안 된 상태이다.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Explorer = GetWorld()->SpawnActor<AHGExplorerCharacter>(AHGExplorerCharacter::StaticClass(), Layout.ExplorerStart, Params);

	// 3) 경영 모드로 시작. 처음에는 블렌드 없이 바로 경영 카메라를 쓴다.
	PlayMode = EHGPlayMode::Shop;
	SetViewTarget(ShopPawn);
	ApplyMappingContexts();

	ShowMessage(TEXT("경영 모드. WASD 이동, Q/E 회전, 휠 확대. Tab으로 사장님 조작"), FColor::White);
	UE_LOG(LogHuoguo, Log, TEXT("모드 초기화 완료: 경영 모드로 시작"));
}

void AHGPlayerController::SetPlayMode(EHGPlayMode NewMode)
{
	if (!ShopPawn || !Explorer || NewMode == PlayMode)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastModeSwitchTime < ModeBlendTime || bTraveling)
	{
		return;
	}
	LastModeSwitchTime = Now;
	PlayMode = NewMode;

	if (NewMode == EHGPlayMode::Explore)
	{
		// 탐험 카메라가 경영 카메라와 같은 수평 방향을 이어받게 한다.
		// 그래야 블렌드할 때 카메라가 옆으로 크게 돌지 않고 "그대로 내려앉는" 모양이 된다.
		const float Yaw = ShopPawn->GetViewYaw();
		Possess(Explorer);
		SetControlRotation(FRotator(-15.f, Yaw, 0.f));
		Explorer->SetActorRotation(FRotator(0.f, Yaw, 0.f));
		SetViewTargetWithBlend(Explorer, ModeBlendTime, VTBlend_EaseInOut, ModeBlendExp);
		ShowMessage(TEXT("탐험 모드. WASD 이동, 마우스 시점, Space 점프. 에어록 발판으로 우주에 나간다"), FColor::White);
	}
	else
	{
		// 경영 카메라의 초점을 사장님이 서 있는 곳으로 옮겨 두고 돌아간다.
		ShopPawn->FocusOn(Explorer->GetActorLocation());
		Possess(ShopPawn);
		SetViewTargetWithBlend(ShopPawn, ModeBlendTime, VTBlend_EaseInOut, ModeBlendExp);
		ShowMessage(TEXT("경영 모드"), FColor::White);
	}

	ApplyMappingContexts();
}

void AHGPlayerController::RequestTravel(AHGTravelPad* Pad)
{
	if (!Pad || !Explorer || bTraveling || PlayMode != EHGPlayMode::Explore)
	{
		return;
	}
	bTraveling = true;

	// 화면을 검게 만든다. bHoldWhenFinished = true라서 다음 페이드를 부를 때까지 검은 화면을 유지한다.
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(0.f, 1.f, TravelFadeTime, FLinearColor::Black, false, true);
	}
	ShowMessage(FString::Printf(TEXT("이동 중: %s"), *Pad->DestinationName.ToString()));

	// 완전히 어두워진 뒤에 위치를 옮긴다. 람다에 값으로 복사해 넘겨서 발판이 사라져도 안전하게 한다.
	const FTransform Destination = Pad->Destination;
	const bool bToSpace = Pad->bLeadsToSpace;
	GetWorldTimerManager().SetTimer(TravelTimer, FTimerDelegate::CreateWeakLambda(this, [this, Destination, bToSpace]()
	{
		FinishTravel(Destination, bToSpace);
	}), TravelFadeTime + 0.05f, false);
}

void AHGPlayerController::HGMode(const FString& Mode)
{
	// 개발 명령은 블렌드 대기 시간을 무시하고 바로 바꾼다.
	LastModeSwitchTime = -100.0;
	SetPlayMode(Mode.Equals(TEXT("explore"), ESearchCase::IgnoreCase) ? EHGPlayMode::Explore : EHGPlayMode::Shop);
}

void AHGPlayerController::HGTravel(const FString& Where)
{
	const bool bWantSpace = Where.Equals(TEXT("space"), ESearchCase::IgnoreCase);
	if (PlayMode != EHGPlayMode::Explore)
	{
		HGMode(TEXT("explore"));
	}

	// 월드의 발판 중 원하는 방향으로 가는 것을 찾아서 밟은 것처럼 처리한다.
	for (TActorIterator<AHGTravelPad> It(GetWorld()); It; ++It)
	{
		if (It->bLeadsToSpace == bWantSpace)
		{
			RequestTravel(*It);
			return;
		}
	}
	ShowMessage(TEXT("이동 발판을 찾지 못했다"), FColor::Red);
}

void AHGPlayerController::FinishTravel(FTransform Destination, bool bToSpace)
{
	if (Explorer)
	{
		Explorer->TeleportWithCamera(Destination);
		Explorer->SetSuitVisible(bToSpace);
		SetControlRotation(FRotator(-12.f, Destination.Rotator().Yaw, 0.f));
	}
	bExplorerInSpace = bToSpace;

	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 0.f, TravelFadeTime, FLinearColor::Black, false, false);
	}
	bTraveling = false;
}

void AHGPlayerController::SetupInputComponent()
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
	Input->BindAction(ToggleModeAction, ETriggerEvent::Started, this, &AHGPlayerController::OnToggleMode);
	Input->BindAction(ShopPanAction, ETriggerEvent::Triggered, this, &AHGPlayerController::OnShopPan);
	Input->BindAction(ShopZoomAction, ETriggerEvent::Triggered, this, &AHGPlayerController::OnShopZoom);
	Input->BindAction(ShopRotateAction, ETriggerEvent::Started, this, &AHGPlayerController::OnShopRotate);
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHGPlayerController::OnMove);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHGPlayerController::OnLook);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AHGPlayerController::OnJumpStarted);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHGPlayerController::OnJumpCompleted);
}

void AHGPlayerController::BuildInputObjects()
{
	if (CommonContext)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
		Action->ValueType = Type;
		return Action;
	};

	ToggleModeAction = MakeAction(TEXT("IA_ToggleMode"), EInputActionValueType::Boolean);
	ShopPanAction = MakeAction(TEXT("IA_ShopPan"), EInputActionValueType::Axis2D);
	ShopZoomAction = MakeAction(TEXT("IA_ShopZoom"), EInputActionValueType::Axis1D);
	ShopRotateAction = MakeAction(TEXT("IA_ShopRotate"), EInputActionValueType::Axis1D);
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);

	// 공용: 어느 모드에서든 살아 있는 키.
	CommonContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Common"));
	CommonContext->MapKey(ToggleModeAction, EKeys::Tab);

	// 경영: 카메라 이동, 회전, 확대.
	ShopContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Shop"));
	MapWASD(ShopContext, ShopPanAction, this);
	ShopContext->MapKey(ShopZoomAction, EKeys::MouseWheelAxis);
	MapDirectionalKey(ShopContext, ShopRotateAction, EKeys::Q, false, true, this);
	MapDirectionalKey(ShopContext, ShopRotateAction, EKeys::E, false, false, this);

	// 탐험: 캐릭터 이동, 시점, 점프.
	ExploreContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Explore"));
	MapWASD(ExploreContext, MoveAction, this);
	ExploreContext->MapKey(LookAction, EKeys::Mouse2D);
	ExploreContext->MapKey(JumpAction, EKeys::SpaceBar);
}

void AHGPlayerController::ApplyMappingContexts()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}
	BuildInputObjects();

	// 컨텍스트를 전부 내리고 공용 + 현재 모드용만 다시 올린다. 숫자가 클수록 우선순위가 높다.
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(CommonContext, 0);
	Subsystem->AddMappingContext(PlayMode == EHGPlayMode::Shop ? ShopContext.Get() : ExploreContext.Get(), 1);

	// 경영은 마우스 커서가 보이는 클릭 조작, 탐험은 커서를 숨기고 마우스로 시점을 돌리는 조작이다.
	if (PlayMode == EHGPlayMode::Shop)
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
	}
}

void AHGPlayerController::OnToggleMode(const FInputActionValue& Value)
{
	if (PlayMode == EHGPlayMode::Explore && bExplorerInSpace)
	{
		// 경영 카메라는 가게만 비춘다. 우주에 있는 동안에는 먼저 귀환해야 한다.
		ShowMessage(TEXT("우주에서는 경영 화면으로 바꿀 수 없다. 금색 귀환 발판으로 돌아가자"), FColor(255, 90, 79));
		return;
	}
	SetPlayMode(PlayMode == EHGPlayMode::Shop ? EHGPlayMode::Explore : EHGPlayMode::Shop);
}

void AHGPlayerController::OnShopPan(const FInputActionValue& Value)
{
	if (ShopPawn && PlayMode == EHGPlayMode::Shop)
	{
		ShopPawn->AddPanInput(Value.Get<FVector2D>());
	}
}

void AHGPlayerController::OnShopZoom(const FInputActionValue& Value)
{
	if (ShopPawn && PlayMode == EHGPlayMode::Shop)
	{
		ShopPawn->AddZoomInput(Value.Get<float>());
	}
}

void AHGPlayerController::OnShopRotate(const FInputActionValue& Value)
{
	if (ShopPawn && PlayMode == EHGPlayMode::Shop)
	{
		const float Dir = Value.Get<float>();
		if (!FMath::IsNearlyZero(Dir))
		{
			ShopPawn->RotateStep(Dir > 0.f ? 1 : -1);
		}
	}
}

void AHGPlayerController::OnMove(const FInputActionValue& Value)
{
	if (Explorer && PlayMode == EHGPlayMode::Explore && !bTraveling)
	{
		Explorer->Move(Value.Get<FVector2D>());
	}
}

void AHGPlayerController::OnLook(const FInputActionValue& Value)
{
	if (Explorer && PlayMode == EHGPlayMode::Explore)
	{
		Explorer->Look(Value.Get<FVector2D>());
	}
}

void AHGPlayerController::OnJumpStarted(const FInputActionValue& Value)
{
	if (Explorer && PlayMode == EHGPlayMode::Explore && !bTraveling)
	{
		Explorer->Jump();
	}
}

void AHGPlayerController::OnJumpCompleted(const FInputActionValue& Value)
{
	if (Explorer)
	{
		Explorer->StopJumping();
	}
}

void AHGPlayerController::ShowMessage(const FString& Text, const FColor& Color) const
{
	// 임시 안내 문구. 정식 UI(UMG)가 생기기 전까지 화면 왼쪽 위 디버그 메시지로 보여 준다.
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, Color, Text);
	}
	UE_LOG(LogHuoguo, Log, TEXT("%s"), *Text);
}
