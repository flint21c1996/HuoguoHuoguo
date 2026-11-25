#include "Explore/HGExplorePlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "Explore/HGExplorerCharacter.h"
#include "Explore/HGPlanetMinimap.h"
#include "Explore/HGCodexWidget.h"
#include "Explore/HGScanWave.h"
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
	const bool bField = Planet && Planet->GetPlanet().bAsteroidField;
	const bool bDerelict = Planet && Planet->GetPlanet().bDerelict;
	if (bDerelict)
	{
		// 폐우주선: 우주선을 바라보며 떠서 시작한다 (무중력 제트팩 비행). 에어록으로 들어가면 인공 중력.
		Explorer->SetFloating(Planet->GetShipUp());
	}
	else if (bField)
	{
		// 소행성 지대: 본 소행성 + 둘레 소행성 모두가 중력을 가진다 (뛰어 건너기 모드).
		Explorer->SetGravityBodies(Planet->GetGravityBodies());
	}
	else if (Planet)
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

	if (bDerelict)
	{
		SetControlsHint(TEXT("[폐우주선] WASD = 보는 쪽으로 분사 | Space/Ctrl = 위/아래 분사 | Shift = 브레이크 | 마우스 시점 | E 채집/문/단말 | J 도감 | 금색 포드 = 귀환"));
	}
	else if (bField)
	{

		SetControlsHint(TEXT("[소행성 지대] WASD 이동 | 마우스 시점 | Space 점프(길게 = 높이, 달리며 이어 뛰면 2단/3단) | 공중에서 Space = 제트팩 | E 채집 | F 스캔 | J 도감 | 금색 발판 = 귀환"));
	}
	else
	{
		SetControlsHint(TEXT("[탐험] WASD 이동 | 마우스 시점 | Space 점프 | E 재료 채집 | F 스캔 | J 도감 | M 지도 범위 | 금색 발판 = 정거장으로 귀환"));
	}
	// 행성은 표면을 펼친 지도, 소행성 지대와 폐우주선은 레이더 (UHGPlanetMinimap이 행성 종류를 보고 고른다).
	if (Planet)
	{
		CreateMinimap();
		// 도감: 다녀온 장소 (군계 x 종류)
		if (UHGGameInstance* PlaceGI = GetGameInstance<UHGGameInstance>())
		{
			const int32 Kind = bDerelict ? 2 : bField ? 1 : 0;
			if (PlaceGI->DiscoverPlace(UHGGameInstance::PlaceKey(static_cast<int32>(Planet->GetPlanet().Biome), Kind)))
			{
				ShowMessage(TEXT("도감: 새 장소 기록 (J로 보기)"), FColor(150, 230, 255));
			}
		}
	}
	if (!ArrivalTag.IsNone())
	{
		FadeInAfterTravel();
	}
	if (Planet)
	{
		// 다이빙한 시간대에 따라 보이는 재료가 다르다. 정거장에 돌아가면 시간대가 한 칸 넘어간다.
		const UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
		const TCHAR* PhaseName = HGDay::PhaseName(GI ? GI->GetDayPhase() : EHGDayPhase::Day);
		ShowMessage(FString::Printf(TEXT("착륙 (%s): %s | %s에 나는 재료 %d개"), PhaseName, *Planet->GetPlanet().Describe(),
			PhaseName, Planet->GetRemainingIngredientCount()));
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

	// 카메라 기준 좌표계: 사장님의 중력 기준을 그대로 쓰지 않고 부드럽게 늦게 따라간다.
	// 소행성 지대에서 가까운 소행성으로 뛰면 짧은 사이에 "위"가 크게 뒤집히고, 착지할 때 남은 각도를 몸이 한 번에 맞춘다.
	// 카메라가 이걸 그대로 따라 돌면 시야가 휙 튀어 순간이동처럼 보였다. 그래서 카메라는 몸과 따로, 지수적으로(시간 상수
	// 1 / CameraUpFollowSpeed초) 따라가게 한다. 한 행성을 걸을 때는 "위"가 천천히 바뀌어 늦게 따라가도 차이가 거의 없다.
	const FQuat Target = Explorer->GetGravityFrame();
	const FQuat Frame = bHasLastGravityFrame
		? FQuat::Slerp(LastGravityFrame, Target, 1.f - FMath::Exp(-CameraUpFollowSpeed * DeltaTime)).GetNormalized()
		: Target;
	FQuat ViewWorld = GetControlRotation().Quaternion();

	// 확인용 기록: 공중에 있는 동안 카메라 기준이 가장 빨리 돈 속도(도/초)와, 착지 순간 몸과 카메라의 "위" 차이를 로그로 남긴다.
	if (bHasLastGravityFrame && DeltaTime > 0.f)
	{
		const float Rate = FMath::RadiansToDegrees(Frame.AngularDistance(LastGravityFrame)) / DeltaTime;
		if (Explorer->GetCharacterMovement()->IsFalling())
		{
			MaxCameraTurnRate = FMath::Max(MaxCameraTurnRate, Rate);
			bCameraWasAirborne = true;
		}
		else if (bCameraWasAirborne)
		{
			bCameraWasAirborne = false;
			UE_LOG(LogHuoguo, Log, TEXT("[Cam] 공중에서 카메라 기준 최대 회전 %.0f도/초, 착지 때 몸과 카메라의 위 차이 %.0f도"),
				MaxCameraTurnRate, FMath::RadiansToDegrees(Frame.AngularDistance(Target)));
			MaxCameraTurnRate = 0.f;
		}
	}

	// 지난 프레임 이후 카메라 기준이 돌아간 만큼 시점도 함께 돌린다.
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
	Input->BindAction(MapAction, ETriggerEvent::Started, this, &AHGExplorePlayerController::OnToggleMap);
	Input->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AHGExplorePlayerController::OnZoom);
	Input->BindAction(ScanAction, ETriggerEvent::Started, this, &AHGExplorePlayerController::OnScan);
	Input->BindAction(CodexAction, ETriggerEvent::Started, this, &AHGExplorePlayerController::OnCodex);
	Input->BindAction(DownAction, ETriggerEvent::Started, this, &AHGExplorePlayerController::OnDown);
	Input->BindAction(DownAction, ETriggerEvent::Completed, this, &AHGExplorePlayerController::OnDown);
	Input->BindAction(BrakeAction, ETriggerEvent::Started, this, &AHGExplorePlayerController::OnBrake);
	Input->BindAction(BrakeAction, ETriggerEvent::Completed, this, &AHGExplorePlayerController::OnBrake);
}

void AHGExplorePlayerController::CreateMinimap()
{
	Minimap = CreateWidget<UHGPlanetMinimap>(this, UHGPlanetMinimap::StaticClass());
	if (!Minimap)
	{
		return;
	}
	Minimap->Init(Planet, Explorer);
	// 화면 전체를 덮는 위젯으로 붙이고, 지도는 위젯이 직접 오른쪽 위 모서리에 그린다 (UHGPlanetMinimap::NativePaint).
	// 뷰포트 슬롯의 앵커/정렬 설정은 위치 설정 함수가 앵커를 되돌리는 등 순서에 민감해서 쓰지 않는다.
	Minimap->AddToViewport(10);
	UE_LOG(LogHuoguo, Log, TEXT("[Map] 미니맵 생성: 화면에 붙음 %s"), Minimap->IsInViewport() ? TEXT("예") : TEXT("아니오"));
}

void AHGExplorePlayerController::OnToggleMap(const FInputActionValue& Value)
{
	if (Minimap)
	{
		Minimap->ToggleWholePlanet();
		UE_LOG(LogHuoguo, Log, TEXT("[Map] 미니맵 범위: %s"), Minimap->IsWholePlanet() ? TEXT("행성 전체") : TEXT("근거리"));
	}
}

void AHGExplorePlayerController::HGMapInfo()
{
	FVector2D PadMapCm;
	if (!Minimap || !Minimap->GetReturnPadOnMap(PadMapCm))
	{
		ShowMessage(TEXT("미니맵 또는 귀환 발판이 없다"), FColor::Red);
		return;
	}
	// 방위: 지도 위쪽(카메라가 보는 쪽) = 0도, 오른쪽으로 돌수록 +.
	const double Bearing = FMath::RadiansToDegrees(FMath::Atan2(PadMapCm.X, PadMapCm.Y));
	FVector PadLocation;
	Planet->GetReturnPadLocation(PadLocation);
	UE_LOG(LogHuoguo, Log, TEXT("[Map] 귀환 발판: 지도 방위 %.1f도, 표면 거리 %.1fm (직선 거리 %.1fm)"),
		Bearing, PadMapCm.Size() / 100.0, FVector::Dist(Explorer->GetActorLocation(), PadLocation) / 100.0);
}

void AHGExplorePlayerController::HGFacePad()
{
	FVector2D PadMapCm;
	if (!Minimap || !Explorer || !Minimap->GetReturnPadOnMap(PadMapCm))
	{
		return;
	}
	// 지도 방위만큼 "위"(행성 바깥 방향) 축으로 시점을 돌린다. 양의 각도는 위에서 내려다볼 때 시계 방향(오른쪽)이다.
	const double Bearing = FMath::Atan2(PadMapCm.X, PadMapCm.Y);
	const FVector Up = (Explorer->GetActorLocation() - Planet->GetGravityCenter()).GetSafeNormal();
	const FQuat Turn(Up, Bearing);
	SetControlRotation((Turn * GetControlRotation().Quaternion()).Rotator());
	UE_LOG(LogHuoguo, Log, TEXT("[Map] 귀환 발판 쪽으로 %.1f도 돌림"), FMath::RadiansToDegrees(Bearing));
}

void AHGExplorePlayerController::HGLook(float YawDegrees, float PitchDegrees)
{
	if (!Explorer || !Planet)
	{
		return;
	}
	// 좌우: HGFacePad와 같이 행성 바깥 방향 축으로 돌린다. 위아래: 돌린 뒤의 카메라 오른쪽 축으로 돌린다.
	// 오른쪽 축 기준으로 음의 각도로 돌리면 앞 방향이 위로 들린다 (오른손 법칙).
	const FVector Up = (Explorer->GetActorLocation() - Planet->GetGravityCenter()).GetSafeNormal();
	FQuat Rot = FQuat(Up, FMath::DegreesToRadians(YawDegrees)) * GetControlRotation().Quaternion();
	const FVector Right = Rot.GetRightVector();
	Rot = FQuat(Right, -FMath::DegreesToRadians(PitchDegrees)) * Rot;
	SetControlRotation(Rot.Rotator());
	UE_LOG(LogHuoguo, Log, TEXT("[Look] 시점 좌우 %.0f도, 위아래 %.0f도"), YawDegrees, PitchDegrees);
}

void AHGExplorePlayerController::HGWarpToLandmark(int32 Index)
{
	if (!Explorer || !Planet || !Planet->GetLandmarkLocations().IsValidIndex(Index))
	{
		ShowMessage(TEXT("그 번호의 지형 볼거리가 없다"), FColor::Red);
		return;
	}
	// 볼거리에서 옆으로 25m 간 지점의 지표 2m 위에 세우고(중력이 내려 앉힌다), 볼거리 쪽을 지표와 나란하게 바라보게 한다.
	const FVector Center = Planet->GetGravityCenter();
	const FVector Target = Planet->GetLandmarkLocations()[Index];
	const FVector TargetUp = (Target - Center).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(TargetUp, FMath::Abs(TargetUp.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	const FVector Dir = (Target + Side * 2500.f - Center).GetSafeNormal();
	const FVector Stand = Center + Dir * (Planet->SurfaceRadiusAt(Dir) + 200.f);
	Explorer->TeleportTo(Stand, Explorer->GetActorRotation(), false, true);
	const FVector Look = FVector::VectorPlaneProject(Target - Stand, Dir).GetSafeNormal();
	SetControlRotation(FRotationMatrix::MakeFromXZ(Look, Dir).Rotator());
	UE_LOG(LogHuoguo, Log, TEXT("[Landmark] %d번 볼거리 옆으로 이동"), Index);
}

void AHGExplorePlayerController::HGWarpToRock(int32 Index, int32 Target)
{
	const TArray<FHGGravityBody>& Bodies = Planet ? Planet->GetGravityBodies() : TArray<FHGGravityBody>();
	if (!Explorer || !Bodies.IsValidIndex(Index))
	{
		ShowMessage(TEXT("그 번호의 소행성이 없다 (소행성 지대에서 0번 = 본 소행성)"), FColor::Red);
		return;
	}
	// 바라볼 소행성: 주어지지 않았으면 가장 가까운 다른 소행성.
	if (!Bodies.IsValidIndex(Target) || Target == Index)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Bodies.Num(); ++i)
		{
			const float Gap = FVector::Dist(Bodies[i].Center, Bodies[Index].Center) - Bodies[i].Radius;
			if (i != Index && Gap < Best)
			{
				Best = Gap;
				Target = i;
			}
		}
	}
	// 목표 쪽에서 60도쯤 비켜 선 지표 위에 세우고(중력이 내려 앉힌다), 목표를 향해 지표와 나란하게 바라보게 한다.
	// 그래야 앞으로 달려 뛰면 목표 쪽으로 날아간다 (목표 바로 밑에 서면 목표가 머리 위에 있어 달릴 방향이 없다).
	const FHGGravityBody& Rock = Bodies[Index];
	const FVector ToTarget = (Bodies[Target].Center - Rock.Center).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(ToTarget, FMath::Abs(ToTarget.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	const FVector Up = (ToTarget * 0.6f + Side).GetSafeNormal();
	const FVector Stand = Rock.Center + Up * (Rock.Radius * 1.35f + 150.f);
	Explorer->TeleportTo(Stand, Explorer->GetActorRotation(), false, true);
	Explorer->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	const FVector Look = FVector::VectorPlaneProject(Bodies[Target].Center - Stand, Up).GetSafeNormal();
	SetControlRotation(FRotationMatrix::MakeFromXZ(Look, Up).Rotator());
	UE_LOG(LogHuoguo, Log, TEXT("[Hop] %d번 소행성으로 이동, %d번 소행성 쪽을 봄 (지표 사이 %.1fm)"), Index, Target,
		(FVector::Dist(Rock.Center, Bodies[Target].Center) - Rock.Radius - Bodies[Target].Radius) / 100.f);
}

void AHGExplorePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateHarvestTarget();
	UpdateHopHud();

	// 시험용 자동 달리기 (HGAutoRun): W를 누른 것과 같은 이동 입력을 넣는다.
	if (AutoRunSeconds > 0.f && Explorer && !bTraveling)
	{
		AutoRunSeconds -= DeltaTime;
		Explorer->Move(FVector2D(0.f, 1.f));
	}
}

void AHGExplorePlayerController::HGTune(const FString& Name, const FString& Value)
{
	// 찾는 순서: 컨트롤러 -> 사장님 -> 사장님 이동 컴포넌트. 이름이 같은 값이 둘이면 앞의 것을 쓴다.
	UObject* Targets[] = { this, Explorer.Get(), Explorer ? Explorer->GetCharacterMovement() : nullptr };

	// 목록: 컨트롤러와 사장님은 우리 게임 코드(모듈 /Script/HuoguoHuoguo)에서 정한 고칠 수 있는(EditAnywhere) 소수 값을 모두,
	// 엔진이 정한 값은 자주 맞추는 것만 보여 준다 (엔진 값은 네트워크 설정 등 수십 개라 다 보이면 읽기 어렵다).
	// 목록에 없어도 이름을 알면 바꿀 수 있다.
	if (Name.IsEmpty())
	{
		static const FName OurPackage(TEXT("/Script/HuoguoHuoguo"));
		static const TCHAR* EngineNames[] = { TEXT("JumpMaxHoldTime"), TEXT("AirControl"), TEXT("MaxWalkSpeed"), TEXT("MaxAcceleration"),
			TEXT("BrakingDecelerationWalking"), TEXT("GroundFriction") };
		FString List;
		for (int32 t = 0; t < 2; ++t)
		{
			if (!Targets[t])
			{
				continue;
			}
			for (TFieldIterator<FFloatProperty> It(Targets[t]->GetClass()); It; ++It)
			{
				if (It->HasAnyPropertyFlags(CPF_Edit) && It->GetOwnerClass() && It->GetOwnerClass()->GetPackage()->GetFName() == OurPackage)
				{
					List += FString::Printf(TEXT("%s=%.3g  "), *It->GetName(), It->GetPropertyValue_InContainer(Targets[t]));
				}
			}
		}
		for (const TCHAR* EngineName : EngineNames)
		{
			for (UObject* Target : Targets)
			{
				const FFloatProperty* Prop = Target ? FindFProperty<FFloatProperty>(Target->GetClass(), EngineName) : nullptr;
				if (Prop)
				{
					List += FString::Printf(TEXT("%s=%.3g  "), EngineName, Prop->GetPropertyValue_InContainer(Target));
					break;
				}
			}
		}
		UE_LOG(LogHuoguo, Log, TEXT("[Tune] %s"), *List);
		ShowMessage(TEXT("[Tune] 바꿀 수 있는 값 목록을 로그(Output Log)에 남겼다. 예) HGTune CameraUpFollowSpeed 2"), FColor::Cyan);
		return;
	}

	for (UObject* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}
		const FFloatProperty* Prop = FindFProperty<FFloatProperty>(Target->GetClass(), *Name);
		if (!Prop)
		{
			continue;
		}
		float* Ptr = Prop->ContainerPtrToValuePtr<float>(Target);
		const float Old = *Ptr;
		if (Value.IsEmpty())
		{
			ShowMessage(FString::Printf(TEXT("[Tune] %s = %g"), *Prop->GetName(), Old), FColor::Cyan);
			return;
		}
		*Ptr = FCString::Atof(*Value);
		ShowMessage(FString::Printf(TEXT("[Tune] %s: %g -> %g"), *Prop->GetName(), Old, *Ptr), FColor::Cyan);
		return;
	}
	ShowMessage(FString::Printf(TEXT("[Tune] %s 라는 값이 없다. HGTune만 치면 목록이 나온다"), *Name), FColor::Red);
}

void AHGExplorePlayerController::HGWarpToInteract(int32 Index)
{
	if (!Explorer || !Planet || Index < 0 || Index >= Planet->GetInteractableCount())
	{
		ShowMessage(TEXT("그 번호의 상호작용 지점이 없다 (폐우주선에서만 있다)"), FColor::Red);
		return;
	}
	const FHGInteractable& Point = Planet->GetInteractable(Index);
	// 바깥 문은 밖에서 떠 있는 채로, 안쪽 문과 단말은 우주선 안에서(인공 중력) 쓴다.
	Explorer->TeleportTo(Point.Location, Explorer->GetActorRotation(), false, true);
	Explorer->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	if (Point.Type == EHGInteractType::EnterShip)
	{
		Explorer->SetFloating(Planet->GetShipUp());
	}
	else
	{
		Explorer->SetFixedGravity(Planet->GetShipUp(), Planet->GetPlanet().GravityScale);
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Derelict] 상호작용 지점 %d 앞으로 이동 (종류 %d)"), Index, static_cast<int32>(Point.Type));
}

void AHGExplorePlayerController::HGAutoRun(float Seconds)
{
	AutoRunSeconds = Seconds;
	UE_LOG(LogHuoguo, Log, TEXT("[Hop] 자동 달리기 %.1f초"), Seconds);
}

void AHGExplorePlayerController::UpdateHarvestTarget()
{
	NearestMarker = INDEX_NONE;
	NearestInteract = INDEX_NONE;
	if (Planet && Explorer && !bTraveling)
	{
		NearestMarker = Planet->FindNearestIngredient(Explorer->GetActorLocation(), HarvestRadius);
		NearestInteract = Planet->FindNearestInteractable(Explorer->GetActorLocation(), InteractRadius);
		// 폐우주선은 벽 하나를 사이에 두고 안과 밖이 붙어 있다. 벽 너머의 단말/문/재료는 쓰지 못하게 시야를 검사한다.
		if (Planet->GetPlanet().bDerelict)
		{
			if (NearestInteract != INDEX_NONE && !HasLineOfSight(Planet->GetInteractable(NearestInteract).Location))
			{
				NearestInteract = INDEX_NONE;
			}
			if (NearestMarker != INDEX_NONE && !HasLineOfSight(Planet->GetMarkerLocation(NearestMarker)))
			{
				NearestMarker = INDEX_NONE;
			}
		}
		// 재료와 문/단말이 둘 다 가까우면 더 가까운 쪽 하나만 E로 쓴다 (문 옆 재료를 캐려다 문이 열리지 않게).
		if (NearestInteract != INDEX_NONE && NearestMarker != INDEX_NONE)
		{
			const FVector Here = Explorer->GetActorLocation();
			if (FVector::Dist(Here, Planet->GetMarkerLocation(NearestMarker)) < FVector::Dist(Here, Planet->GetInteractable(NearestInteract).Location))
			{
				NearestInteract = INDEX_NONE;
			}
			else
			{
				NearestMarker = INDEX_NONE;
			}
		}
	}

	FString Prompt;
	if (NearestInteract != INDEX_NONE)
	{
		Prompt = Planet->DescribeInteractable(NearestInteract);
	}
	else if (NearestMarker != INDEX_NONE)
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
	if (NearestInteract != INDEX_NONE)
	{
		UseInteractable(NearestInteract);
		UpdateHarvestTarget();
		return;
	}
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
		GI->AddHarvestTotal(Id); // 도감: 캔 횟수 (처음이면 발견도)
	}
	ShowMessage(FString::Printf(TEXT("채집: %s +1 (가방에 %d개, 이 행성 남은 재료 %d)"),
		GI ? *GI->GetIngredientName(Id) : *Id.ToString(), GI ? GI->GetBagCount(Id) : 1, Planet->GetRemainingIngredientCount()));
	UE_LOG(LogHuoguo, Log, TEXT("[Harvest] 표식 %d (%s), 사장님과 거리 %.0f"), MarkerId, *Id.ToString(),
		FVector::Dist(Explorer->GetActorLocation(), Planet->GetMarkerLocation(MarkerId)));

	UpdateHarvestTarget();
}

void AHGExplorePlayerController::HGWarpToIngredient(int32 Index)
{
	if (!Planet || !Explorer)
	{
		ShowMessage(TEXT("행성 맵이 아니다"), FColor::Red);
		return;
	}
	// 번호를 주면 그 표식, 아니면 가장 가까운 남은 표식.
	const int32 MarkerId = Planet->GetIngredientMarkers().IsValidIndex(Index) ? Index
		: Planet->FindNearestIngredient(Explorer->GetActorLocation(), TNumericLimits<float>::Max());
	if (MarkerId == INDEX_NONE)
	{
		ShowMessage(TEXT("남은 재료가 없다"), FColor::Red);
		return;
	}

	// 표식에서 위로 조금 띄운 곳에 옆으로 1m 비켜 세운다. 중력이 지표로 끌어내린다.
	// "위"는 행성이면 행성 바깥쪽, 폐우주선이면 우주선의 위 (행성 중심 기준으로 잡으면 우주선 벽 안쪽에 서 버렸다).
	const FVector MarkerLocation = Planet->GetMarkerLocation(MarkerId);
	const FVector Up = Planet->GetPlanet().bDerelict ? Planet->GetShipUp() : (MarkerLocation - Planet->GetGravityCenter()).GetSafeNormal();
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
	MapAction = MakeAction(TEXT("IA_Explore_Map"), EInputActionValueType::Boolean);
	ZoomAction = MakeAction(TEXT("IA_Explore_Zoom"), EInputActionValueType::Axis1D);
	DownAction = MakeAction(TEXT("IA_Explore_Down"), EInputActionValueType::Boolean);
	ScanAction = MakeAction(TEXT("IA_Explore_Scan"), EInputActionValueType::Boolean);
	CodexAction = MakeAction(TEXT("IA_Explore_Codex"), EInputActionValueType::Boolean);
	BrakeAction = MakeAction(TEXT("IA_Explore_Brake"), EInputActionValueType::Boolean);

	ExploreContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Explore"));
	// 마우스 휠: 카메라와 사장님 사이 거리 (경영 파트의 휠 확대와 같은 감각).
	ExploreContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
	// 무중력 비행: Ctrl = 아래로 분사, Shift = 브레이크 (행성/소행성에서는 쓰지 않는다).
	ExploreContext->MapKey(DownAction, EKeys::LeftControl);
	ExploreContext->MapKey(ScanAction, EKeys::F);
	ExploreContext->MapKey(CodexAction, EKeys::J);
	ExploreContext->MapKey(BrakeAction, EKeys::LeftShift);
	MapWASD(ExploreContext, MoveAction);
	ExploreContext->MapKey(LookAction, EKeys::Mouse2D);
	ExploreContext->MapKey(JumpAction, EKeys::SpaceBar);
	// 상호작용(채집). 나중에 IMC_ 에셋으로 옮기면 키를 바꿀 수 있다.
	ExploreContext->MapKey(InteractAction, EKeys::E);
	ExploreContext->MapKey(MapAction, EKeys::M);
}

void AHGExplorePlayerController::OnZoom(const FInputActionValue& Value)
{
	// 휠을 앞으로 굴리면(+) 가까이, 뒤로 굴리면(-) 멀리.
	if (Explorer && !bTraveling)
	{
		Explorer->AddZoomInput(Value.Get<float>());
	}
}

void AHGExplorePlayerController::OnMove(const FInputActionValue& Value)
{
	++MoveEventCount;
	if (Explorer && !bTraveling)
	{
		// 무중력에서는 보는 쪽으로 분사, 땅에서는 지표를 따라 걷는다.
		if (Explorer->IsFloating())
		{
			Explorer->FloatMove(Value.Get<FVector2D>());
		}
		else
		{
			Explorer->Move(Value.Get<FVector2D>());
		}
	}
}

bool AHGExplorePlayerController::HasLineOfSight(const FVector& Target) const
{
	// 사장님 몸 가운데에서 대상까지 선을 그어 무언가(벽)에 걸리면 안 보이는 것이다. 대상 30cm 앞에서 걸린 것은 대상 자체로 친다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HGInteractSight), false, Explorer);
	FHitResult Hit;
	const FVector Start = Explorer->GetActorLocation();
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, Target, ECC_Visibility, Params))
	{
		return true;
	}
	return FVector::Dist(Hit.ImpactPoint, Target) < 30.f;
}

void AHGExplorePlayerController::UseInteractable(int32 Index)
{
	const FHGInteractable& Point = Planet->GetInteractable(Index);
	switch (Point.Type)
	{
	case EHGInteractType::EnterShip:
		// 에어록 안쪽으로 옮기고 인공 중력을 켠다 (우주선 바닥 쪽으로 끈다).
		Explorer->TeleportTo(Point.Destination, Explorer->GetActorRotation(), false, true);
		Explorer->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		Explorer->SetFixedGravity(Planet->GetShipUp(), Planet->GetPlanet().GravityScale);
		ShowMessage(TEXT("에어록 통과: 인공 중력이 아직 살아 있다"), FColor::Cyan);
		break;
	case EHGInteractType::ExitShip:
		Explorer->TeleportTo(Point.Destination, Explorer->GetActorRotation(), false, true);
		Explorer->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		Explorer->SetFloating(Planet->GetShipUp());
		ShowMessage(TEXT("에어록 밖으로: 무중력"), FColor::Cyan);
		break;
	default:
		ShowMessage(Planet->UseBlueprint(Index), FColor(255, 210, 80));
		break;
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Derelict] 상호작용 %d (종류 %d)"), Index, static_cast<int32>(Point.Type));
}

void AHGExplorePlayerController::OnScan(const FInputActionValue& Value)
{
	if (!Planet || !Explorer || bTraveling)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastScanTime < ScanCooldown)
	{
		ShowMessage(FString::Printf(TEXT("스캐너 충전 중 (%.0f초)"), ScanCooldown - (Now - LastScanTime)), FColor(150, 150, 150));
		return;
	}
	// 지금 서 있는 땅이 있어야 한다 (폐우주선은 떠 있거나 우주선 안이라 훑을 지면이 없다).
	if (Planet->GetPlanet().bDerelict)
	{
		ShowMessage(TEXT("스캐너는 디디고 선 행성 지면을 훑는다. 여기에는 훑을 지면이 없다"), FColor(150, 150, 150));
		return;
	}
	// 지금 서 있는 천체: 소행성 지대면 밟고 있는(또는 끌리는) 소행성, 아니면 행성.
	FVector Center = Planet->GetGravityCenter();
	float Radius = Planet->GetPlanet().Radius;
	if (Explorer->IsHopMode() && Planet->GetGravityBodies().IsValidIndex(Explorer->GetCurrentBody()))
	{
		const FHGGravityBody& Body = Planet->GetGravityBodies()[Explorer->GetCurrentBody()];
		Center = Body.Center;
		Radius = Body.Radius;
	}
	LastScanTime = Now;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AHGScanWave* Wave = GetWorld()->SpawnActor<AHGScanWave>(AHGScanWave::StaticClass(), FTransform::Identity, Params))
	{
		TWeakObjectPtr<AHGExplorePlayerController> WeakThis(this);
		Wave->Duration = ScanDuration;
		Wave->Begin(Planet, Center, Radius, Explorer->GetActorLocation(), ScanRange, [WeakThis](const FString& Result)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->ShowMessage(Result, FColor(120, 255, 220));
			}
		});
	}
}

void AHGExplorePlayerController::OnCodex(const FInputActionValue& Value)
{
	if (!Codex)
	{
		Codex = CreateWidget<UHGCodexWidget>(this, UHGCodexWidget::StaticClass());
	}
	if (!Codex)
	{
		return;
	}
	if (Codex->IsInViewport())
	{
		Codex->RemoveFromParent();
	}
	else
	{
		Codex->AddToViewport(20);
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Codex] 도감 %s"), Codex->IsInViewport() ? TEXT("열림") : TEXT("닫힘"));
}

void AHGExplorePlayerController::OnDown(const FInputActionValue& Value)
{
	if (Explorer)
	{
		Explorer->SetDownHeld(Value.Get<bool>());
	}
}

void AHGExplorePlayerController::OnBrake(const FInputActionValue& Value)
{
	if (Explorer)
	{
		Explorer->SetBrakeHeld(Value.Get<bool>());
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
	// 점프(연속 점프, 코요테 타임, 착지 직전 미리 받기), 소행성 지대 공중이면 제트팩 (HGExplorerCharacter::JumpPressed).
	if (Explorer && !bTraveling)
	{
		Explorer->JumpPressed();
	}
}

void AHGExplorePlayerController::OnJumpCompleted(const FInputActionValue& Value)
{
	if (Explorer)
	{
		Explorer->JumpReleased();
	}
}

void AHGExplorePlayerController::UpdateHopHud()
{
	// 소행성 지대(뛰어 건너기)와 폐우주선(제트팩 비행)에서 산소를 쓴다.
	if (!Explorer || !Planet || (!Explorer->IsHopMode() && !Planet->GetPlanet().bDerelict))
	{
		return;
	}
	const FString Message = Explorer->ConsumeHopMessage();
	if (!Message.IsEmpty())
	{
		ShowMessage(Message, FColor(255, 170, 60));
	}

	// 산소가 바닥나면 비상 귀환: 본 소행성 착륙 지점으로 옮기고 산소를 절반 채운다.
	if (Explorer->GetOxygen() <= 0.f && Planet)
	{
		const FTransform Landing = Planet->GetLandingTransform();
		Explorer->TeleportTo(Landing.GetLocation(), Explorer->GetActorRotation(), false, true);
		Explorer->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		if (Planet->GetPlanet().bDerelict)
		{
			Explorer->SetFloating(Planet->GetShipUp());
		}
		Explorer->RefillOxygen(50.f);
		ShowMessage(TEXT("산소 부족! 비상 귀환 장치가 본 소행성으로 되돌렸다 (산소 50)"), FColor::Red);
	}

	// 산소와 제트팩 상태를 한 줄로 늘 보여 준다 (조작 안내, 행성 목록, 채집 안내와 다른 고정 키).
	FString Line = FString::Printf(TEXT("산소 %.0f%%"), Explorer->GetOxygen());
	if (Explorer->IsJetpackReady())
	{
		Line += TEXT(" | 제트팩 준비 (Space)");
	}
	constexpr uint64 HopKey = 0x4847'0004;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(HopKey, 1.f, Explorer->GetOxygen() < 30.f ? FColor::Red : FColor(120, 255, 220), Line);
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
