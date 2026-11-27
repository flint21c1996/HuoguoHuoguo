#include "Player/HGPlayerControllerBase.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UObjectIterator.h"
#include "Components/PostProcessComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Game/HGGameInstance.h"
#include "Game/HGLookSettings.h"
#include "HuoguoHuoguo.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/HGBossCharacter.h"
#include "TimerManager.h"
#include "World/Travel/HGExplorerStart.h"
#include "World/Travel/HGTravelPad.h"

namespace
{
	// 시험용: HGSeq 명령 간격(초). 0이면 컨트롤러 기본값(DevSequenceInterval). 맵을 옮긴 뒤 이어지는 명령에도 적용된다.
	// 점프처럼 짧은 간격으로 키를 이어 눌러야 하는 시험에 쓴다. 예) HGSeq hg.SeqInterval 0.3|HGDive 3|...
	TAutoConsoleVariable<float> CVarSeqInterval(
		TEXT("hg.SeqInterval"), 0.f,
		TEXT("시험용: HGSeq 명령 간격(초). 0이면 기본 1.5초. 맵을 옮긴 뒤 이어지는 명령에도 적용된다."),
		ECVF_Default);

	// 그림체 비교 스위치. 콘솔 변수라 맵을 옮겨도 값이 남는다.
	TAutoConsoleVariable<int32> CVarLook(
		TEXT("hg.Look"), 0,
		TEXT("그림체 비교. 0 = 지금 그대로, 1 = 셀 셰이딩(빛 밝기 계단 + 윤곽선). 게임 중 L로 번갈아 바꾼다."),
		ECVF_Default);

	constexpr int32 LookCount = 2;

	// 머티리얼 인스턴스의 값이 원하는 값과 다를 때만 넣는다 (매 프레임 부르므로, 같으면 아무것도 하지 않는다).
	void SyncScalar(UMaterialInstanceDynamic* Mid, const TCHAR* Name, float Value)
	{
		if (!FMath::IsNearlyEqual(Mid->K2_GetScalarParameterValue(Name), Value))
		{
			Mid->SetScalarParameterValue(Name, Value);
			UE_LOG(LogHuoguo, Log, TEXT("[Look] %s %s = %.4f"), *Mid->Parent->GetName(), Name, Value);
		}
	}

	void SyncColor(UMaterialInstanceDynamic* Mid, const TCHAR* Name, const FLinearColor& Value)
	{
		if (!Mid->K2_GetVectorParameterValue(Name).Equals(Value, 1.e-4f))
		{
			Mid->SetVectorParameterValue(Name, Value);
			UE_LOG(LogHuoguo, Log, TEXT("[Look] %s %s = %s"), *Mid->Parent->GetName(), Name, *Value.ToString());
		}
	}

	const TCHAR* LookName(int32 Look)
	{
		switch (Look)
		{
		case 1: return TEXT("셀 셰이딩");
		default: return TEXT("지금 그대로");
		}
	}
}

void AHGPlayerControllerBase::EnsureLookEffect()
{
	if (LookEffect || !GetWorld())
	{
		return;
	}
	// 컨트롤러 액터는 늘 숨김(bHidden) 상태라서, 컨트롤러에 붙인 포스트 프로세스는 렌더링에서 빠진다
	// (USceneComponent::ShouldRender가 주인 액터가 숨김이면 false). 그래서 숨기지 않은 빈 액터를 하나 만들어 거기에 붙인다.
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;
	LookHolder = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!LookHolder)
	{
		return;
	}
	LookEffect = NewObject<UPostProcessComponent>(LookHolder, TEXT("LookEffect"));
	// 경계가 없어서(bUnbound) 위치와 상관없이 화면 전체에 얹힌다.
	LookEffect->bUnbound = true;
	// 맵의 포스트 프로세스 볼륨(노출 고정)보다 나중에 섞이게 한다. 스캐너 파동(10)보다는 먼저.
	LookEffect->Priority = 5.f;
	LookHolder->SetRootComponent(LookEffect);
	LookEffect->RegisterComponent();
}

void AHGPlayerControllerBase::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// L(Look): 두 파트 모두 쓰지 않는 키이고, PIE에서도 에디터 단축키와 겹치지 않는다.
	if (WasInputKeyJustPressed(EKeys::L))
	{
		const int32 Next = (FMath::Clamp(CVarLook.GetValueOnGameThread(), 0, LookCount - 1) + 1) % LookCount;
		HGStyle(Next);
	}
	const int32 Wanted = FMath::Clamp(CVarLook.GetValueOnGameThread(), 0, LookCount - 1);
	if (Wanted != AppliedLook)
	{
		ApplyLook(Wanted);
	}
	// 조절 값은 프로젝트 설정 → Game → HG 그림체(UHGLookSettings, DefaultGame.ini)에 있다.
	// 설정 번호(Revision)가 바뀌었을 때(PIE 중에 설정 화면에서 값을 바꿈)나 머티리얼 인스턴스를 새로 만들었을 때만 넣는다.
	// 평소 프레임에는 정수 비교 하나로 끝난다.
	const UHGLookSettings* Settings = GetDefault<UHGLookSettings>();
	if (Settings->Revision == AppliedSettingsRevision)
	{
		return;
	}
	AppliedSettingsRevision = Settings->Revision;
	if (ToonMaterial)
	{
		SyncScalar(ToonMaterial, TEXT("LineWidth"), FMath::Max(Settings->LineWidth, 0.f));
		SyncColor(ToonMaterial, TEXT("LineColor"), Settings->LineColor);
		SyncScalar(ToonMaterial, TEXT("LineDepth"), Settings->LineDepth);
		SyncScalar(ToonMaterial, TEXT("Ratio"), Settings->Ratio);
		SyncScalar(ToonMaterial, TEXT("FaintLineDepth"), Settings->FaintLineDepth);
		SyncScalar(ToonMaterial, TEXT("FaintRatio"), Settings->FaintRatio);
		SyncScalar(ToonMaterial, TEXT("FaintLine"), FMath::Clamp(Settings->FaintLine, 0.f, 1.f));
		SyncScalar(ToonMaterial, TEXT("Stops"), FMath::Max(Settings->Bands, 0.f));
		SyncScalar(ToonMaterial, TEXT("Soft"), FMath::Clamp(Settings->BandSoft, 0.f, 0.5f));
		SyncScalar(ToonMaterial, TEXT("Saturation"), FMath::Max(Settings->Saturation, 0.f));
	}
}

void AHGPlayerControllerBase::HGStyle(int32 Look)
{
	Look = FMath::Clamp(Look, 0, LookCount - 1);
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("hg.Look")))
	{
		Var->Set(Look, ECVF_SetByConsole);
	}
	ApplyLook(Look);
	ShowMessage(FString::Printf(TEXT("그림체 %d: %s (L로 바꾸기)"), Look, LookName(Look)));
}

void AHGPlayerControllerBase::ApplyLook(int32 Look)
{
	EnsureLookEffect();
	if (!LookEffect)
	{
		return;
	}
	AppliedLook = Look;
	LookEffect->Settings.WeightedBlendables.Array.Reset();
	if (Look >= 1)
	{
		// 머티리얼은 처음 필요할 때 한 번만 불러 동적 인스턴스로 만든다 (조절 값을 프로젝트 설정에서 넣기 위해서).
		if (!ToonMaterial)
		{
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/HG/Materials/M_HG_LookToon.M_HG_LookToon")))
			{
				ToonMaterial = UMaterialInstanceDynamic::Create(Base, this);
				// 머티리얼 인스턴스를 새로 만들었으니 다음 PlayerTick에서 설정 값을 넣게 한다.
				AppliedSettingsRevision = -1;
			}
			else
			{
				UE_LOG(LogHuoguo, Warning, TEXT("[Look] 화면 효과 머티리얼 M_HG_LookToon이 없다 (Tools/Invoke-HGMapGen.ps1로 만든다)"));
			}
		}
		if (ToonMaterial)
		{
			LookEffect->Settings.AddBlendable(ToonMaterial, 1.f);
		}
	}

	// 윤곽선에서 빼는 물체(스텐실 1, 행성 외계 식물)는 셀 그림체일 때만 커스텀 깊이 패스에 그린다.
	// 지금 그림체(0)에서는 커스텀 깊이 패스에 아무것도 없어서 엔진이 그 패스를 건너뛴다 (식물 수천 포기를 한 번 더 그리는 비용이 0).
	// 그림체를 바꿀 때만 도는 반복이라 비용은 무시할 만하다.
	const bool bNeedStencil = Look >= 1;
	int32 Toggled = 0;
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		UPrimitiveComponent* Prim = *It;
		if (Prim->GetWorld() == GetWorld() && Prim->CustomDepthStencilValue == HGLook::NoOutlineStencil && Prim->bRenderCustomDepth != bNeedStencil)
		{
			Prim->SetRenderCustomDepth(bNeedStencil);
			++Toggled;
		}
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Look] 그림체 %d: %s, 화면 효과 %d개, 커스텀 깊이 %s %d개"), Look, LookName(Look),
		LookEffect->Settings.WeightedBlendables.Array.Num(), bNeedStencil ? TEXT("켬") : TEXT("끔"), Toggled);
}

float AHGPlayerControllerBase::GetSequenceInterval() const
{
	const float Override = CVarSeqInterval.GetValueOnGameThread();
	return Override > 0.f ? Override : DevSequenceInterval;
}

void AHGPlayerControllerBase::BeginPlay()
{
	Super::BeginPlay();

	// 이전 맵에서 HGSeq로 넣어 둔 명령이 남아 있으면 이어서 실행한다.
	// 첫 명령은 한 간격 뒤에 나가므로 하위 클래스의 맵 시작 처리(사장님 스폰 등)가 먼저 끝난다.
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (IsLocalController() && GI && !GI->GetDevSequence().IsEmpty())
	{
		GetWorldTimerManager().SetTimer(DevSequenceTimer, this, &AHGPlayerControllerBase::RunNextDevCommand, GetSequenceInterval(), true);
	}
}

void AHGPlayerControllerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetPrompt(FString());
	if (LookHolder)
	{
		LookHolder->Destroy();
		LookHolder = nullptr;
		LookEffect = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AHGPlayerControllerBase::HGSeq(const FString& Commands)
{
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		return;
	}
	TArray<FString> Parts;
	Commands.ParseIntoArray(Parts, TEXT("|"), true);
	for (FString& Part : Parts)
	{
		Part.TrimStartAndEndInline();
	}
	GI->GetDevSequence() = MoveTemp(Parts);
	UE_LOG(LogHuoguo, Log, TEXT("[Seq] 명령 %d개를 %.1f초 간격으로 실행"), GI->GetDevSequence().Num(), GetSequenceInterval());
	GetWorldTimerManager().SetTimer(DevSequenceTimer, this, &AHGPlayerControllerBase::RunNextDevCommand, GetSequenceInterval(), true);
}

void AHGPlayerControllerBase::RunNextDevCommand()
{
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (!GI || GI->GetDevSequence().IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(DevSequenceTimer);
		UE_LOG(LogHuoguo, Log, TEXT("[Seq] 끝"));
		return;
	}

	// 맵 이동 중에는 기다린다. 명령은 새 맵의 컨트롤러가 이어서 꺼낸다.
	// 시험용 동작(자동 조종 등)이 진행 중이어도 기다린다. 끝난 뒤 다음 간격에 이어서 꺼낸다.
	if (bTraveling || IsDevSequenceBusy())
	{
		return;
	}

	const FString Command = GI->GetDevSequence()[0];
	GI->GetDevSequence().RemoveAt(0);
	if (Command.IsEmpty() || Command.Equals(TEXT("wait"), ESearchCase::IgnoreCase))
	{
		return;
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Seq] %s"), *Command);
	ConsoleCommand(Command);
}

void AHGPlayerControllerBase::RequestTravel(AHGTravelPad* Pad)
{
	if (!Pad || !Boss || bTraveling)
	{
		return;
	}
	if (Pad->DestinationLevel.IsNull())
	{
		UE_LOG(LogHuoguo, Error, TEXT("%s 발판에 DestinationLevel이 비어 있다"), *GetNameSafe(Pad));
		return;
	}

	FString DestinationText = Pad->DestinationName.ToString();
	if (Pad->bDiveToSelectedPlanet)
	{
		// 다이빙 발판: 지금 고른 접근 행성을 표면 맵에 넘길 값으로 고정한다. 표면 맵의 생성기가 이것으로 지형을 만든다.
		UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
		const FHGPlanetSeed* Selected = GI ? GI->GetSelectedPlanet() : nullptr;
		if (!Selected)
		{
			ShowMessage(TEXT("다이빙할 행성이 없다"), FColor::Red);
			return;
		}
		if (!GI->CanDive())
		{
			// 하루 구조: 낮이나 밤에만 내려간다. 문 닫을 시간에는 하루를 마쳐야 다음 날 다시 내려갈 수 있다.
			ShowMessage(TEXT("문 닫을 시간에는 다이빙할 수 없다. T로 하루를 마친다"), FColor::Red);
			return;
		}
		GI->CommitDiveToSelectedPlanet();
		DestinationText = FString::Printf(TEXT("%s (다이빙)"), *Selected->Name);
	}

	bTraveling = true;
	UE_LOG(LogHuoguo, Log, TEXT("이동 시작: 발판 %s, 사장님 위치 %s, 맵 시작 후 %.2f초"),
		*Pad->GetActorNameOrLabel(), *Boss->GetActorLocation().ToCompactString(), GetWorld()->GetTimeSeconds());

	// 화면을 검게 만든다. bHoldWhenFinished = true라서 다음 페이드를 부를 때까지 검은 화면을 유지한다.
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(0.f, 1.f, TravelFadeTime, FLinearColor::Black, false, true);
	}
	ShowMessage(FString::Printf(TEXT("이동 중: %s"), *DestinationText));

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

FName AHGPlayerControllerBase::ConsumeArrivalTag() const
{
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	return GI ? GI->ConsumePendingArrival() : NAME_None;
}

AHGExplorerStart* AHGPlayerControllerBase::FindStart(FName WantedTag) const
{
	if (WantedTag.IsNone())
	{
		WantedTag = TEXT("Default");
	}

	AHGExplorerStart* Fallback = nullptr;
	for (TActorIterator<AHGExplorerStart> It(GetWorld()); It; ++It)
	{
		if (It->StartTag == WantedTag)
		{
			return *It;
		}
		if (!Fallback || It->StartTag == TEXT("Default"))
		{
			Fallback = *It;
		}
	}
	if (Fallback)
	{
		UE_LOG(LogHuoguo, Warning, TEXT("태그 '%s'인 HGExplorerStart가 없어서 %s를 쓴다"), *WantedTag.ToString(), *Fallback->GetName());
	}
	return Fallback;
}

void AHGPlayerControllerBase::FadeInAfterTravel()
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.5f, FLinearColor::Black, false, false);
	}
}

void AHGPlayerControllerBase::ApplyMappingContext(UInputMappingContext* Context)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->ClearAllMappings();
		Subsystem->AddMappingContext(Context, 0);
	}
}

void AHGPlayerControllerBase::HGTravel(const FString& Where)
{
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

void AHGPlayerControllerBase::HGSimKey(const FString& KeyName, float HoldSeconds)
{
	const FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		ShowMessage(FString::Printf(TEXT("알 수 없는 키: %s"), *KeyName), FColor::Red);
		return;
	}

	// 맵 시작 직후에는 매핑 컨텍스트 재구성이 끝나지 않았을 수 있어서 잠깐 기다렸다가 누른다.
	GetWorldTimerManager().SetTimer(SimKeyTimer, FTimerDelegate::CreateWeakLambda(this, [this, Key, HoldSeconds]()
	{
		const FVector Start = Boss ? Boss->GetActorLocation() : FVector::ZeroVector;
		MoveEventCount = 0;
		InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.f));
		UE_LOG(LogHuoguo, Log, TEXT("[SimKey] %s 누름"), *Key.ToString());

		GetWorldTimerManager().SetTimer(SimKeyTimer, FTimerDelegate::CreateWeakLambda(this, [this, Key, Start]()
		{
			InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.f));
			const FVector End = Boss ? Boss->GetActorLocation() : FVector::ZeroVector;
			const FVector Delta = End - Start;
			UE_LOG(LogHuoguo, Log, TEXT("[SimKey] %s 뗌. Move 이벤트 %d회, 사장님 이동 거리 %.1f, 이동 방향 Yaw %.1f, 위치 %s%s"),
				*Key.ToString(), MoveEventCount, Delta.Size(), Delta.Rotation().Yaw, *End.ToCompactString(), *DescribeForSimKey());
		}), FMath::Max(HoldSeconds, 0.1f), false);
	}), 0.3f, false);
}

void AHGPlayerControllerBase::ShowMessage(const FString& Text, const FColor& Color) const
{
	// 임시 안내 문구. 정식 UI(UMG)가 생기기 전까지 화면 왼쪽 위 디버그 메시지로 보여 준다.
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, Color, Text);
	}
	UE_LOG(LogHuoguo, Log, TEXT("%s"), *Text);
}

void AHGPlayerControllerBase::SetControlsHint(const FString& Hint) const
{
	// 같은 Key로 다시 부르면 새 줄을 쌓지 않고 기존 줄의 내용만 바뀐다. 표시 시간을 길게 잡아 계속 떠 있게 한다.
	// 정식 UI(UMG)가 생기면 이 함수만 위젯 갱신으로 바꾸면 된다.
	constexpr uint64 HintKey = 0x4847'0001;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(HintKey, 100000.f, FColor(82, 214, 232), Hint);
	}
}

void AHGPlayerControllerBase::SetPrompt(const FString& Prompt)
{
	if (Prompt == CurrentPrompt)
	{
		return;
	}
	CurrentPrompt = Prompt;

	// 조작 안내(0x48470001), 행성 목록(0x48470002)과 겹치지 않는 고정 키. 정식 UI가 생기면 화면 가운데 아래 위젯으로 옮긴다.
	constexpr uint64 PromptKey = 0x4847'0003;
	if (GEngine)
	{
		if (Prompt.IsEmpty())
		{
			GEngine->RemoveOnScreenDebugMessage(PromptKey);
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(PromptKey, 100000.f, FColor(150, 255, 150), Prompt);
		}
	}
}

void AHGPlayerControllerBase::HGBag()
{
	if (UHGGameInstance* GI = GetGameInstance<UHGGameInstance>())
	{
		ShowMessage(FString::Printf(TEXT("가방: %s"), *GI->DescribeBag()));
	}
}

UInputAction* AHGPlayerControllerBase::MakeAction(const TCHAR* Name, EInputActionValueType Type)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
	Action->ValueType = Type;
	return Action;
}

void AHGPlayerControllerBase::MapDirectionalKey(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate)
{
	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
	if (bSwizzle)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}
	if (bNegate)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
}

void AHGPlayerControllerBase::MapWASD(UInputMappingContext* Context, const UInputAction* Action)
{
	MapDirectionalKey(Context, Action, EKeys::W, true, false);
	MapDirectionalKey(Context, Action, EKeys::S, true, true);
	MapDirectionalKey(Context, Action, EKeys::A, false, true);
	MapDirectionalKey(Context, Action, EKeys::D, false, false);
}

void AHGPlayerControllerBase::MapScaledKey(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key, float Scale)
{
	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
	if (!FMath::IsNearlyEqual(Scale, 1.f))
	{
		UInputModifierScalar* Scalar = NewObject<UInputModifierScalar>(this);
		Scalar->Scalar = FVector(Scale, 1.0, 1.0);
		Mapping.Modifiers.Add(Scalar);
	}
}
