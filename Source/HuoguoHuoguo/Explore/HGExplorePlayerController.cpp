#include "Explore/HGExplorePlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "Explore/HGExplorerCharacter.h"
#include "Game/HGGameInstance.h"
#include "HuoguoHuoguo.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Planet/HGPlanetGenerator.h"
#include "World/HGExplorerStart.h"

void AHGExplorePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		InitializeFromLevel();
	}
}

void AHGExplorePlayerController::InitializeFromLevel()
{
	UWorld* World = GetWorld();
	const FName ArrivalTag = ConsumeArrivalTag();

	// 행성 표면 맵: 시작점을 맵에 미리 둘 수 없다(지형이 시드마다 다르다). 생성기가 지형을 먼저 만들고 착륙 지점을 알려 준다.
	// 액터들의 BeginPlay 순서는 정해져 있지 않으므로 여기서 EnsureGenerated를 직접 불러 지형이 먼저 있게 한다.
	for (TActorIterator<AHGPlanetGenerator> It(World); It; ++It)
	{
		Planet = *It;
		break;
	}

	FTransform StartTransform(FVector(0.f, 0.f, 120.f));
	bool bSpaceSuit = true;
	AHGExplorerStart* Start = nullptr;
	if (Planet)
	{
		Planet->EnsureGenerated();
		StartTransform = Planet->GetLandingTransform();
	}
	else if ((Start = FindStart(ArrivalTag)) != nullptr)
	{
		StartTransform = Start->GetActorTransform();
		StartTransform.SetScale3D(FVector::OneVector);
		bSpaceSuit = Start->bSpaceSuit;
	}
	else
	{
		UE_LOG(LogHuoguo, Warning, TEXT("맵에 HGPlanetGenerator도 HGExplorerStart도 없다. 원점에 사장님을 만든다"));
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Explorer = World->SpawnActor<AHGExplorerCharacter>(AHGExplorerCharacter::StaticClass(), StartTransform, Params);
	Boss = Explorer;
	Explorer->SetSuitVisible(bSpaceSuit);
	if (Planet)
	{
		Explorer->SetPlanetGravity(Planet->GetGravityCenter(), Planet->GetPlanet().GravityScale);
	}

	Possess(Explorer);
	SetViewTarget(Explorer);
	SetControlRotation(FRotator(-12.f, StartTransform.Rotator().Yaw, 0.f));

	BuildInputObjects();
	ApplyMappingContext(ExploreContext);
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	SetControlsHint(TEXT("[탐험] WASD 이동 | 마우스 시점 | Space 점프 | E 재료 채집 | 금색 발판 = 정거장으로 귀환"));
	if (!ArrivalTag.IsNone())
	{
		FadeInAfterTravel();
	}
	if (Planet)
	{
		ShowMessage(FString::Printf(TEXT("착륙: %s"), *Planet->GetPlanet().Describe()));
	}

	UE_LOG(LogHuoguo, Log, TEXT("탐험 파트 시작: %s, 시작 %s, 행성 %s"), *World->GetMapName(),
		Planet ? TEXT("착륙 지점") : *GetNameSafe(Start), Planet ? *Planet->GetPlanet().Name : TEXT("없음"));
}

void AHGExplorePlayerController::UpdateRotation(float DeltaTime)
{
	if (!Explorer || GetPawn() != Explorer || !Explorer->HasPlanetGravity())
	{
		bHasLastGravityFrame = false;
		Super::UpdateRotation(DeltaTime);
		return;
	}

	const FQuat Frame = Explorer->GetGravityFrame();
	FQuat ViewWorld = GetControlRotation().Quaternion();

	// 지난 프레임 이후 중력 기준이 돌아간 만큼 시점도 함께 돌린다.
	// 이게 없으면 행성을 한 바퀴 걷는 동안 카메라는 처음 방향에 머물러서 점점 땅을 파고들거나 하늘만 보게 된다.
	if (bHasLastGravityFrame)
	{
		ViewWorld = Frame * LastGravityFrame.Inverse() * ViewWorld;
	}
	LastGravityFrame = Frame;
	bHasLastGravityFrame = true;

	// 중력 좌표계에서 기본 처리(마우스 입력 더하기, Pitch 제한)를 하고 Roll은 버린 뒤 월드로 돌려놓는다.
	FRotator ViewLocal = (Frame.Inverse() * ViewWorld).Rotator();
	FRotator DeltaRot(RotationInput);
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ProcessViewRotation(DeltaTime, ViewLocal, DeltaRot);
	}
	ViewLocal.Roll = 0.f;
	SetControlRotation((Frame * ViewLocal.Quaternion()).Rotator());

	if (APawn* const P = GetPawnOrSpectator())
	{
		P->FaceRotation(GetControlRotation(), DeltaTime);
	}
}

void AHGExplorePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInputObjects();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		UE_LOG(LogHuoguo, Error, TEXT("EnhancedInputComponent가 아니다. DefaultInput.ini의 DefaultInputComponentClass를 확인할 것"));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHGExplorePlayerController::OnMove);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHGExplorePlayerController::OnLook);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AHGExplorePlayerController::OnJumpStarted);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHGExplorePlayerController::OnJumpCompleted);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AHGExplorePlayerController::OnInteract);
}

void AHGExplorePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateHarvestTarget();
}

void AHGExplorePlayerController::UpdateHarvestTarget()
{
	NearestMarker = INDEX_NONE;
	if (Planet && Explorer && !bTraveling)
	{
		NearestMarker = Planet->FindNearestIngredient(Explorer->GetActorLocation(), HarvestRadius);
	}

	FString Prompt;
	if (NearestMarker != INDEX_NONE)
	{
		UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
		const FName Id = Planet->GetMarkerIngredient(NearestMarker);
		Prompt = FString::Printf(TEXT("[E] 채집: %s"), GI ? *GI->GetIngredientName(Id) : *Id.ToString());
	}
	SetPrompt(Prompt);
}

void AHGExplorePlayerController::OnInteract(const FInputActionValue& Value)
{
	if (bTraveling)
	{
		return;
	}
	// 누른 순간의 위치로 다시 찾는다. 안내가 뜬 뒤 한 프레임 사이에 움직였을 수 있기 때문이다.
	UpdateHarvestTarget();
	if (NearestMarker == INDEX_NONE)
	{
		return;
	}

	const int32 MarkerId = NearestMarker;
	const FName Id = Planet->HarvestIngredient(MarkerId);
	if (Id.IsNone())
	{
		return;
	}

	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (GI)
	{
		GI->AddToBag(Id, 1);
	}
	ShowMessage(FString::Printf(TEXT("채집: %s +1 (가방에 %d개, 이 행성 남은 재료 %d)"),
		GI ? *GI->GetIngredientName(Id) : *Id.ToString(), GI ? GI->GetBagCount(Id) : 1, Planet->GetRemainingIngredientCount()));
	UE_LOG(LogHuoguo, Log, TEXT("[Harvest] 표식 %d (%s), 사장님과 거리 %.0f"), MarkerId, *Id.ToString(),
		FVector::Dist(Explorer->GetActorLocation(), Planet->GetMarkerLocation(MarkerId)));

	UpdateHarvestTarget();
}

void AHGExplorePlayerController::HGWarpToIngredient()
{
	if (!Planet || !Explorer)
	{
		ShowMessage(TEXT("행성 맵이 아니다"), FColor::Red);
		return;
	}
	const int32 MarkerId = Planet->FindNearestIngredient(Explorer->GetActorLocation(), TNumericLimits<float>::Max());
	if (MarkerId == INDEX_NONE)
	{
		ShowMessage(TEXT("남은 재료가 없다"), FColor::Red);
		return;
	}

	// 표식에서 행성 바깥쪽(위)으로 조금 띄운 곳에 옆으로 1m 비켜 세운다. 중력이 지표로 끌어내린다.
	const FVector MarkerLocation = Planet->GetMarkerLocation(MarkerId);
	const FVector Up = (MarkerLocation - Planet->GetGravityCenter()).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(Up, FMath::Abs(Up.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	Explorer->TeleportTo(MarkerLocation + Side * 100.f + Up * 150.f, Explorer->GetActorRotation(), false, true);
	UE_LOG(LogHuoguo, Log, TEXT("[Harvest] 표식 %d (%s) 옆으로 이동"), MarkerId, *Planet->GetMarkerIngredient(MarkerId).ToString());
}

void AHGExplorePlayerController::HGIngredients()
{
	if (Planet)
	{
		UE_LOG(LogHuoguo, Log, TEXT("[Harvest] %s: 남은 재료 표식 %d"), *Planet->GetPlanet().Name, Planet->GetRemainingIngredientCount());
	}
	HGBag();
}

void AHGExplorePlayerController::BuildInputObjects()
{
	if (ExploreContext)
	{
		return;
	}

	MoveAction = MakeAction(TEXT("IA_Explore_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Explore_Look"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Explore_Jump"), EInputActionValueType::Boolean);
	InteractAction = MakeAction(TEXT("IA_Explore_Interact"), EInputActionValueType::Boolean);

	ExploreContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Explore"));
	MapWASD(ExploreContext, MoveAction);
	ExploreContext->MapKey(LookAction, EKeys::Mouse2D);
	ExploreContext->MapKey(JumpAction, EKeys::SpaceBar);
	// 상호작용(채집). 나중에 IMC_ 에셋으로 옮기면 키를 바꿀 수 있다.
	ExploreContext->MapKey(InteractAction, EKeys::E);
}

void AHGExplorePlayerController::OnMove(const FInputActionValue& Value)
{
	++MoveEventCount;
	if (Explorer && !bTraveling)
	{
		Explorer->Move(Value.Get<FVector2D>());
	}
}

void AHGExplorePlayerController::OnLook(const FInputActionValue& Value)
{
	if (Explorer)
	{
		Explorer->Look(Value.Get<FVector2D>());
	}
}

void AHGExplorePlayerController::OnJumpStarted(const FInputActionValue& Value)
{
	if (Explorer && !bTraveling)
	{
		Explorer->Jump();
	}
}

void AHGExplorePlayerController::OnJumpCompleted(const FInputActionValue& Value)
{
	if (Explorer)
	{
		Explorer->StopJumping();
	}
}

FString AHGExplorePlayerController::DescribeForSimKey() const
{
	if (Planet && Explorer)
	{
		return FString::Printf(TEXT(", 행성 중심 거리 %.1f"), FVector::Dist(Explorer->GetActorLocation(), Planet->GetGravityCenter()));
	}
	return FString();
}
