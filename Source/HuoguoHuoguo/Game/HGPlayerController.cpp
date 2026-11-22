#include "Game/HGPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Game/HGGameInstance.h"
#include "HuoguoHuoguo.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Player/HGExplorerCharacter.h"
#include "Player/HGShopCameraPawn.h"
#include "TimerManager.h"
#include "World/HGExplorerStart.h"
#include "World/HGShopCameraAnchor.h"
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

	if (IsLocalController())
	{
		InitializeFromLevel();
	}
}

void AHGPlayerController::InitializeFromLevel()
{
	if (bModesInitialized)
	{
		return;
	}
	bModesInitialized = true;
	UWorld* World = GetWorld();

	// 1) 도착 태그: 다른 맵에서 발판을 타고 왔다면 GameInstance에 맡겨 둔 태그가 있다.
	FName ArrivalTag = NAME_None;
	if (UHGGameInstance* GI = GetGameInstance<UHGGameInstance>())
	{
		ArrivalTag = GI->ConsumePendingArrival();
	}
	const bool bArrivedByTravel = !ArrivalTag.IsNone();
	const FName WantedTag = bArrivedByTravel ? ArrivalTag : FName(TEXT("Default"));

	// 2) 사장님 시작점: 태그가 맞는 것 > "Default" > 아무거나 순서로 고른다.
	AHGExplorerStart* Start = nullptr;
	AHGExplorerStart* Fallback = nullptr;
	for (TActorIterator<AHGExplorerStart> It(World); It; ++It)
	{
		if (It->StartTag == WantedTag)
		{
			Start = *It;
			break;
		}
		if (!Fallback || It->StartTag == TEXT("Default"))
		{
			Fallback = *It;
		}
	}
	if (!Start)
	{
		Start = Fallback;
		if (bArrivedByTravel)
		{
			UE_LOG(LogHuoguo, Warning, TEXT("도착 태그 '%s'인 HGExplorerStart가 없어서 다른 시작점을 쓴다"), *ArrivalTag.ToString());
		}
	}

	FTransform StartTransform(FVector(0.f, 0.f, 120.f));
	bool bSpaceSuit = false;
	if (Start)
	{
		StartTransform = Start->GetActorTransform();
		StartTransform.SetScale3D(FVector::OneVector);
		bSpaceSuit = Start->bSpaceSuit;
	}
	else
	{
		UE_LOG(LogHuoguo, Warning, TEXT("맵에 HGExplorerStart가 없다. 원점에 사장님을 만든다"));
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Explorer = World->SpawnActor<AHGExplorerCharacter>(AHGExplorerCharacter::StaticClass(), StartTransform, Params);
	Explorer->SetSuitVisible(bSpaceSuit);

	// 3) 경영 카메라: 기준점이 배치된 맵에만 만든다.
	AHGShopCameraAnchor* Anchor = nullptr;
	for (TActorIterator<AHGShopCameraAnchor> It(World); It; ++It)
	{
		Anchor = *It;
		break;
	}
	if (Anchor)
	{
		ShopPawn = World->SpawnActor<AHGShopCameraPawn>(AHGShopCameraPawn::StaticClass(), FTransform(Anchor->GetActorLocation()));
		ShopPawn->SnapTo(Anchor->GetActorLocation(), Anchor->GetActorRotation().Yaw, Anchor->PanRadius);
	}

	// 4) 시작 모드: 가게가 있고 발판을 타고 온 것이 아니면 경영, 그 밖에는 탐험.
	if (ShopPawn && !bArrivedByTravel)
	{
		PlayMode = EHGPlayMode::Shop;
		Possess(ShopPawn);
		SetViewTarget(ShopPawn);
	}
	else
	{
		PlayMode = EHGPlayMode::Explore;
		Possess(Explorer);
		SetControlRotation(FRotator(-12.f, StartTransform.Rotator().Yaw, 0.f));
		SetViewTarget(Explorer);
	}
	ApplyMappingContexts();
	UpdateControlsHint();

	// 발판을 타고 왔다면 이전 맵에서 어두워진 화면을 다시 밝힌다.
	if (bArrivedByTravel && PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.5f, FLinearColor::Black, false, false);
	}

	UE_LOG(LogHuoguo, Log, TEXT("맵 초기화 완료: %s, 시작 모드 %s, 시작점 %s, 경영 화면 %s"),
		*World->GetMapName(), PlayMode == EHGPlayMode::Shop ? TEXT("경영") : TEXT("탐험"),
		*GetNameSafe(Start), ShopPawn ? TEXT("있음") : TEXT("없음"));
}

void AHGPlayerController::SetPlayMode(EHGPlayMode NewMode)
{
	if (!Explorer || NewMode == PlayMode)
	{
		return;
	}
	if (!ShopPawn)
	{
		// 경영 카메라는 가게만 비춘다. 가게가 없는 맵(파밍지)에서는 먼저 정거장으로 돌아가야 한다.
		ShowMessage(TEXT("여기서는 경영 화면을 열 수 없다. 금색 귀환 발판으로 정거장에 돌아가자"), FColor(255, 90, 79));
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
	}
	else
	{
		// 경영 카메라의 초점을 사장님이 서 있는 곳으로 옮겨 두고 돌아간다.
		ShopPawn->FocusOn(Explorer->GetActorLocation());
		Possess(ShopPawn);
		SetViewTargetWithBlend(ShopPawn, ModeBlendTime, VTBlend_EaseInOut, ModeBlendExp);
	}

	ApplyMappingContexts();
	UpdateControlsHint();
	UE_LOG(LogHuoguo, Log, TEXT("모드 전환: %s"), NewMode == EHGPlayMode::Shop ? TEXT("경영") : TEXT("탐험"));
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

	if (Pad->DestinationLevel.IsNull())
	{
		UE_LOG(LogHuoguo, Error, TEXT("%s 발판에 DestinationLevel이 비어 있다"), *GetNameSafe(Pad));
		bTraveling = false;
		return;
	}

	// 완전히 어두워진 뒤에 맵을 연다. 도착 태그는 맵이 바뀌어도 남는 GameInstance에 맡긴다.
	// 람다에는 값만 복사해 넘긴다. 맵이 바뀌면 발판 액터는 사라지기 때문이다.
	const TSoftObjectPtr<UWorld> Level = Pad->DestinationLevel;
	const FName ArrivalTag = Pad->ArrivalTag;
	GetWorldTimerManager().SetTimer(TravelTimer, FTimerDelegate::CreateWeakLambda(this, [this, Level, ArrivalTag]()
	{
		if (UHGGameInstance* GI = GetGameInstance<UHGGameInstance>())
		{
			GI->SetPendingArrival(ArrivalTag);
		}
		else
		{
			UE_LOG(LogHuoguo, Warning, TEXT("GameInstance가 UHGGameInstance가 아니라서 도착 위치를 넘기지 못한다"));
		}
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, Level);
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
	if (PlayMode != EHGPlayMode::Explore)
	{
		HGMode(TEXT("explore"));
	}

	// 이 맵의 발판 중 목적지 맵 경로에 입력한 글자가 들어간 것을 찾아서 밟은 것처럼 처리한다.
	for (TActorIterator<AHGTravelPad> It(GetWorld()); It; ++It)
	{
		if (Where.IsEmpty() || It->DestinationLevel.ToString().Contains(Where, ESearchCase::IgnoreCase))
		{
			RequestTravel(*It);
			return;
		}
	}
	ShowMessage(FString::Printf(TEXT("'%s'(으)로 가는 이동 발판을 찾지 못했다"), *Where), FColor::Red);
}

void AHGPlayerController::HGSimKey(const FString& KeyName, float HoldSeconds)
{
	const FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		ShowMessage(FString::Printf(TEXT("알 수 없는 키: %s"), *KeyName), FColor::Red);
		return;
	}

	// 매핑 컨텍스트를 바꾼 직후에는 이미 눌려 있던 키를 무시하므로, 재구성이 끝나도록 잠깐 기다렸다가 누른다.
	GetWorldTimerManager().SetTimer(SimKeyTimer, FTimerDelegate::CreateWeakLambda(this, [this, Key, HoldSeconds]()
	{
		const FVector Start = Explorer ? Explorer->GetActorLocation() : FVector::ZeroVector;
		MoveEventCount = 0;
		InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.f));
		UE_LOG(LogHuoguo, Log, TEXT("[SimKey] %s 누름 (모드 %d)"), *Key.ToString(), static_cast<int32>(PlayMode));

		GetWorldTimerManager().SetTimer(SimKeyTimer, FTimerDelegate::CreateWeakLambda(this, [this, Key, Start]()
		{
			InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
			const FVector End = Explorer ? Explorer->GetActorLocation() : FVector::ZeroVector;
			UE_LOG(LogHuoguo, Log, TEXT("[SimKey] %s 뗌. Move 이벤트 %d회, 사장님 이동 거리 %.1f, 빙의 대상 %s, 경영 카메라 방향 %.1f"),
				*Key.ToString(), MoveEventCount, FVector::Dist2D(Start, End), *GetNameSafe(GetPawn()), ShopPawn ? ShopPawn->GetViewYaw() : 0.f);
		}), FMath::Max(HoldSeconds, 0.1f), false);
	}), 0.3f, false);
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
	++MoveEventCount;
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

void AHGPlayerController::UpdateControlsHint() const
{
	if (!GEngine)
	{
		return;
	}

	// 같은 Key로 다시 부르면 새 줄을 쌓지 않고 기존 줄의 내용만 바뀐다. 표시 시간을 길게 잡아 계속 떠 있게 한다.
	// 정식 UI(UMG)가 생기면 이 함수만 위젯 갱신으로 바꾸면 된다.
	constexpr uint64 HintKey = 0x4847'0001;
	FString Hint;
	if (PlayMode == EHGPlayMode::Shop)
	{
		Hint = TEXT("[경영] WASD 카메라 이동 | Q/E 회전 | 휠 확대 | Tab 사장님 조작");
	}
	else if (ShopPawn)
	{
		Hint = TEXT("[탐험] WASD 이동 | 마우스 시점 | Space 점프 | 청록 발판 = 우주로 | Tab 경영 화면");
	}
	else
	{
		Hint = TEXT("[탐험] WASD 이동 | 마우스 시점 | Space 점프 | 금색 발판 = 정거장으로 귀환");
	}
	GEngine->AddOnScreenDebugMessage(HintKey, 100000.f, FColor(82, 214, 232), Hint);
}
