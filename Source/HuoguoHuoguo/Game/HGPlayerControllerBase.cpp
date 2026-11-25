#include "Game/HGPlayerControllerBase.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Game/HGGameInstance.h"
#include "HuoguoHuoguo.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Player/HGBossCharacter.h"
#include "TimerManager.h"
#include "World/HGExplorerStart.h"
#include "World/HGTravelPad.h"

namespace
{
	// 시험용: HGSeq 명령 간격(초). 0이면 컨트롤러 기본값(DevSequenceInterval). 맵을 옮긴 뒤 이어지는 명령에도 적용된다.
	// 점프처럼 짧은 간격으로 키를 이어 눌러야 하는 시험에 쓴다. 예) HGSeq hg.SeqInterval 0.3|HGDive 3|...
	TAutoConsoleVariable<float> CVarSeqInterval(
		TEXT("hg.SeqInterval"), 0.f,
		TEXT("시험용: HGSeq 명령 간격(초). 0이면 기본 1.5초. 맵을 옮긴 뒤 이어지는 명령에도 적용된다."),
		ECVF_Default);
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
