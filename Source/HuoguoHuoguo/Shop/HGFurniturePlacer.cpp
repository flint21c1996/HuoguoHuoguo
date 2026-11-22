#include "Shop/HGFurniturePlacer.h"

#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/HGGameInstance.h"
#include "GameFramework/PlayerController.h"
#include "HuoguoHuoguo.h"
#include "Shop/HGFurniture.h"

UHGFurniturePlacer::UHGFurniturePlacer()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 기본 목록: 1 = 식탁, 2 = 의자.
	Catalog.Add(AHGTable::StaticClass());
	Catalog.Add(AHGChair::StaticClass());
}

APlayerController* UHGFurniturePlacer::GetPC() const
{
	return Cast<APlayerController>(GetOwner());
}

TSubclassOf<AHGFurniture> UHGFurniturePlacer::GetHeldClass() const
{
	if (bMoving)
	{
		return MovingClass;
	}
	return Catalog.IsValidIndex(CatalogIndex) ? Catalog[CatalogIndex] : nullptr;
}

FText UHGFurniturePlacer::GetHeldName() const
{
	const TSubclassOf<AHGFurniture> Class = GetHeldClass();
	return Class ? Class->GetDefaultObject<AHGFurniture>()->DisplayName : FText::GetEmpty();
}

// ---------------------------------------------------------------------------
// 배치 기록
// ---------------------------------------------------------------------------

void UHGFurniturePlacer::RestoreLayout()
{
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		UE_LOG(LogHuoguo, Warning, TEXT("GameInstance가 UHGGameInstance가 아니라서 가구 배치를 기록하지 못한다"));
		return;
	}

	if (!GI->HasShopLayout())
	{
		// 게임을 켜고 처음 정거장에 왔다. 맵에 배치된 가구가 처음 배치다.
		SaveLayout();
		UE_LOG(LogHuoguo, Log, TEXT("가구 배치: 맵에 배치된 가구 %d개를 처음 배치로 기록"), GI->GetShopLayout().Num());
		return;
	}

	// 행성에 다녀와 정거장 맵이 새로 열렸다. 맵에 배치된 처음 가구를 치우고 기록대로 다시 놓는다.
	TArray<AHGFurniture*> MapFurniture;
	for (TActorIterator<AHGFurniture> It(GetWorld()); It; ++It)
	{
		MapFurniture.Add(*It);
	}
	for (AHGFurniture* Furniture : MapFurniture)
	{
		Furniture->Destroy();
	}

	int32 Restored = 0;
	for (const FHGPlacedFurniture& Item : GI->GetShopLayout())
	{
		if (Item.Class && Item.Class->IsChildOf(AHGFurniture::StaticClass()))
		{
			Restored += SpawnFurniture(Item.Class.Get(), Item.Location, Item.Yaw) ? 1 : 0;
		}
	}
	UE_LOG(LogHuoguo, Log, TEXT("가구 배치: 기록에서 %d개를 다시 놓음 (맵의 처음 가구 %d개는 치움)"), Restored, MapFurniture.Num());
}

void UHGFurniturePlacer::SaveLayout() const
{
	UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		return;
	}

	// TActorIterator는 Destroy()로 없애는 중인 액터를 건너뛴다. 그래서 방금 치운 가구는 기록에 들어가지 않는다.
	TArray<FHGPlacedFurniture> Layout;
	for (TActorIterator<AHGFurniture> It(GetWorld()); It; ++It)
	{
		if (It->IsGhost())
		{
			continue;
		}
		FHGPlacedFurniture& Item = Layout.AddDefaulted_GetRef();
		Item.Class = It->GetClass();
		Item.Location = It->GetActorLocation();
		Item.Yaw = It->GetActorRotation().Yaw;
	}
	GI->SetShopLayout(Layout);
}

AHGFurniture* UHGFurniturePlacer::SpawnFurniture(TSubclassOf<AHGFurniture> Class, const FVector& Location, float Yaw) const
{
	if (!Class)
	{
		return nullptr;
	}
	// 자리 검사는 이미 CheckPlacement에서 끝냈다. 엔진의 스폰 충돌 처리가 위치를 몰래 옮기지 않도록 항상 그 자리에 만든다.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<AHGFurniture>(Class, Location, FRotator(0.f, Yaw, 0.f), Params);
}

// ---------------------------------------------------------------------------
// 배치 모드
// ---------------------------------------------------------------------------

void UHGFurniturePlacer::BeginBuild()
{
	if (bBuilding)
	{
		return;
	}
	bBuilding = true;
	RespawnGhost();
	UpdateCursor();
}

void UHGFurniturePlacer::EndBuild()
{
	if (!bBuilding)
	{
		return;
	}
	CancelMove();
	SetHovered(nullptr);
	DestroyGhost();
	bBuilding = false;
}

bool UHGFurniturePlacer::SelectCatalog(int32 Index)
{
	if (!Catalog.IsValidIndex(Index) || !Catalog[Index])
	{
		return false;
	}
	CancelMove();
	CatalogIndex = Index;
	if (bBuilding)
	{
		RespawnGhost();
	}
	return true;
}

void UHGFurniturePlacer::RotateHeld(float Degrees)
{
	HeldYaw = FRotator::NormalizeAxis(HeldYaw + Degrees);
	if (Ghost)
	{
		Ghost->SetActorRotation(FRotator(0.f, HeldYaw, 0.f));
	}
}

void UHGFurniturePlacer::RespawnGhost()
{
	DestroyGhost();
	const TSubclassOf<AHGFurniture> Class = GetHeldClass();
	if (!Class)
	{
		return;
	}

	// 미뤄서 스폰(SpawnActorDeferred)하고, 스폰을 끝내기(FinishSpawning) 전에 유령으로 바꾼다.
	// 그래야 충돌이 켜진 실제 가구 상태로 한 프레임도 존재하지 않는다. 처음에는 숨겨 두고 커서 위치가 정해지면 보인다.
	const FTransform SpawnTransform(FRotator(0.f, HeldYaw, 0.f), FVector(0.f, 0.f, FloorZ));
	Ghost = GetWorld()->SpawnActorDeferred<AHGFurniture>(Class, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Ghost)
	{
		Ghost->MakeGhost();
		Ghost->SetActorHiddenInGame(true);
		Ghost->FinishSpawning(SpawnTransform);
		Ghost->SetPreviewState(EHGFurniturePreview::Invalid);
	}
	bGhostValid = false;
}

void UHGFurniturePlacer::DestroyGhost()
{
	if (Ghost)
	{
		Ghost->Destroy();
		Ghost = nullptr;
	}
	bGhostValid = false;
}

void UHGFurniturePlacer::SetHovered(AHGFurniture* NewHovered)
{
	if (Hovered == NewHovered)
	{
		return;
	}
	if (IsValid(Hovered))
	{
		Hovered->SetPreviewState(EHGFurniturePreview::None);
	}
	Hovered = NewHovered;
	if (Hovered)
	{
		Hovered->SetPreviewState(EHGFurniturePreview::Hover);
	}
}

void UHGFurniturePlacer::CancelMove()
{
	if (!bMoving)
	{
		return;
	}
	SpawnFurniture(MovingClass, MovingFrom.GetLocation(), MovingFrom.Rotator().Yaw);
	bMoving = false;
	MovingClass = nullptr;
	SaveLayout();
	if (bBuilding)
	{
		RespawnGhost();
	}
}

// ---------------------------------------------------------------------------
// 커서
// ---------------------------------------------------------------------------

bool UHGFurniturePlacer::GetCursorRay(FVector& OutOrigin, FVector& OutDirection) const
{
	APlayerController* PC = GetPC();
	if (!PC)
	{
		return false;
	}

	FVector2D Screen;
	if (CursorWorldOverride.IsSet())
	{
		// 시험용: 월드 지점을 화면 좌표로 바꿔서 마우스 위치인 것처럼 쓴다.
		if (!PC->ProjectWorldLocationToScreen(CursorWorldOverride.GetValue(), Screen))
		{
			return false;
		}
	}
	else
	{
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (!PC->GetMousePosition(MouseX, MouseY))
		{
			return false; // 커서가 게임 창 밖에 있다.
		}
		Screen = FVector2D(MouseX, MouseY);
	}

	// 화면 좌표 -> 카메라 위치에서 그 픽셀을 지나 세계 안쪽으로 나가는 광선.
	return PC->DeprojectScreenPositionToWorld(Screen.X, Screen.Y, OutOrigin, OutDirection);
}

void UHGFurniturePlacer::UpdateCursor()
{
	if (!bBuilding)
	{
		return;
	}
	if (!Ghost)
	{
		RespawnGhost();
		if (!Ghost)
		{
			return;
		}
	}

	auto HideGhost = [this]()
	{
		Ghost->SetActorHiddenInGame(true);
		bGhostValid = false;
	};

	FVector Origin;
	FVector Direction;
	if (!GetCursorRay(Origin, Direction))
	{
		SetHovered(nullptr);
		HideGhost();
		return;
	}

	// 1. 커서가 이미 놓인 가구를 가리키는가. 가구를 옮기는 중에는 새로 집을 수 없으므로 보지 않는다.
	//    가구 부품은 BlockAll이라 Visibility 추적에 걸리고, 유령은 겹침뿐이라 걸리지 않는다.
	AHGFurniture* NewHovered = nullptr;
	if (!bMoving)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HGFurnitureHover), false, Ghost);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 100000.f, ECC_Visibility, Params))
		{
			NewHovered = Cast<AHGFurniture>(Hit.GetActor());
			if (NewHovered && NewHovered->IsGhost())
			{
				NewHovered = nullptr;
			}
		}
	}
	SetHovered(NewHovered);
	if (Hovered)
	{
		HideGhost();
		return;
	}

	// 2. 광선이 바닥 높이의 수평면과 만나는 점. 카메라가 늘 내려다보므로 Direction.Z < 0이다.
	//    Origin + Direction * T 의 Z가 FloorZ가 되는 T를 구한다.
	if (Direction.Z > -KINDA_SMALL_NUMBER)
	{
		HideGhost();
		return;
	}
	const double T = (FloorZ - Origin.Z) / Direction.Z;
	FVector Point = Origin + Direction * T;

	// 격자에 맞춘다. 가구 중심이 GridSize 배수 좌표에만 놓여서 줄 맞추기가 쉽다.
	Point.X = FMath::GridSnap(Point.X, static_cast<double>(GridSize));
	Point.Y = FMath::GridSnap(Point.Y, static_cast<double>(GridSize));
	Point.Z = FloorZ;

	Ghost->SetActorLocationAndRotation(Point, FRotator(0.f, HeldYaw, 0.f));
	Ghost->SetActorHiddenInGame(false);
	bGhostValid = CheckPlacement(Ghost);
	Ghost->SetPreviewState(bGhostValid ? EHGFurniturePreview::Valid : EHGFurniturePreview::Invalid);
}

bool UHGFurniturePlacer::CheckPlacement(const AHGFurniture* Candidate) const
{
	LastRejectReason.Reset();
	UWorld* World = GetWorld();

	TArray<UStaticMeshComponent*> Parts;
	Candidate->GetCollidingParts(Parts);
	if (Parts.IsEmpty())
	{
		LastRejectReason = TEXT("충돌 부품이 없는 가구");
		return false;
	}

	// 1. 겹침: 부품의 충돌 모양을 그 자리(조금 띄움)에 두었을 때 막는 물체와 겹치는가.
	//    물체 종류로 묻는다: 고정물(벽, 카운터, 바닥), 움직이는 물체(가구, 발판 트리거), 폰(사장님, 손님), 물리 물체.
	FComponentQueryParams Params(SCENE_QUERY_STAT(HGFurniturePlace), Candidate);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);

	const FVector Lift(0.f, 0.f, PlacementLift);
	TArray<FOverlapResult> Overlaps;
	for (const UStaticMeshComponent* Part : Parts)
	{
		World->ComponentOverlapMulti(Overlaps, Part, Part->GetComponentLocation() + Lift, Part->GetComponentQuat(), Params, Objects);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			const AActor* Other = Overlap.GetActor();
			const AHGFurniture* OtherFurniture = Cast<AHGFurniture>(Other);
			if (!Other || Other == Candidate || (OtherFurniture && OtherFurniture->IsGhost()))
			{
				continue;
			}
			LastRejectReason = FString::Printf(TEXT("겹침: %s"), *Other->GetActorNameOrLabel());
			return false;
		}
	}

	// 2. 바닥: 외곽 상자의 네 모서리(조금 안쪽)와 중심 바로 아래에 가게 바닥이 있는가.
	//    가게 밖은 받침대 윗면(바닥보다 30 낮음)이나 허공이라서 여기서 걸린다.
	const FBox Bounds = Candidate->GetComponentsBoundingBox(false);
	constexpr double Inset = 4.0;
	const FVector2D Samples[] =
	{
		FVector2D(Bounds.GetCenter()),
		FVector2D(Bounds.Min.X + Inset, Bounds.Min.Y + Inset),
		FVector2D(Bounds.Min.X + Inset, Bounds.Max.Y - Inset),
		FVector2D(Bounds.Max.X - Inset, Bounds.Min.Y + Inset),
		FVector2D(Bounds.Max.X - Inset, Bounds.Max.Y - Inset),
	};
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(HGFurnitureFloor), false, Candidate);
	for (const FVector2D& Sample : Samples)
	{
		FHitResult Hit;
		const FVector Start(Sample.X, Sample.Y, FloorZ + 30.f);
		const FVector End(Sample.X, Sample.Y, FloorZ - 30.f);
		// 다른 가구의 바닥 받침(높이 4)을 밟는 것은 허용한다. 실제 겹침은 1번에서 이미 걸렀다.
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, TraceParams) || FMath::Abs(Hit.ImpactPoint.Z - FloorZ) > 6.f)
		{
			LastRejectReason = FString::Printf(TEXT("(%.0f, %.0f) 아래에 바닥이 없음"), Sample.X, Sample.Y);
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// 클릭과 지우기
// ---------------------------------------------------------------------------

FString UHGFurniturePlacer::Click()
{
	if (!bBuilding)
	{
		return FString();
	}
	UpdateCursor();

	// 가리킨 가구를 집는다: 지우고 같은 종류, 같은 방향의 유령을 손에 든다. 원래 자리는 취소용으로 남긴다.
	if (Hovered && !bMoving)
	{
		AHGFurniture* Picked = Hovered;
		SetHovered(nullptr);
		bMoving = true;
		MovingClass = Picked->GetClass();
		MovingFrom = Picked->GetActorTransform();
		HeldYaw = MovingFrom.Rotator().Yaw;
		const FString Name = Picked->DisplayName.ToString();
		Picked->Destroy();
		RespawnGhost();
		UpdateCursor();
		return FString::Printf(TEXT("집었다: %s. 클릭으로 놓기, X로 취소"), *Name);
	}

	if (!Ghost || Ghost->IsHidden() || !bGhostValid)
	{
		return LastRejectReason.IsEmpty()
			? FString(TEXT("여기에는 놓을 수 없다"))
			: FString::Printf(TEXT("여기에는 놓을 수 없다: %s"), *LastRejectReason);
	}

	const FVector Location = Ghost->GetActorLocation();
	AHGFurniture* Placed = SpawnFurniture(GetHeldClass(), Location, HeldYaw);
	if (!Placed)
	{
		return TEXT("가구를 만들지 못했다");
	}
	if (bMoving)
	{
		bMoving = false;
		MovingClass = nullptr;
		RespawnGhost();
	}
	SaveLayout();
	return FString::Printf(TEXT("놓았다: %s (%.0f, %.0f, 방향 %.0f)"), *Placed->DisplayName.ToString(), Location.X, Location.Y, HeldYaw);
}

FString UHGFurniturePlacer::RemoveOrCancel()
{
	if (!bBuilding)
	{
		return FString();
	}
	if (bMoving)
	{
		CancelMove();
		return TEXT("옮기기를 취소하고 원래 자리에 되돌렸다");
	}

	UpdateCursor();
	if (!Hovered)
	{
		return TEXT("치울 가구를 커서로 가리킨다");
	}
	AHGFurniture* Target = Hovered;
	SetHovered(nullptr);
	const FString Name = Target->DisplayName.ToString();
	Target->Destroy();
	SaveLayout();
	return FString::Printf(TEXT("치웠다: %s"), *Name);
}

// ---------------------------------------------------------------------------
// 개발용 설명
// ---------------------------------------------------------------------------

FString UHGFurniturePlacer::DescribeLayout() const
{
	const UHGGameInstance* GI = GetWorld()->GetGameInstance<UHGGameInstance>();
	if (!GI)
	{
		return TEXT("GameInstance 없음");
	}
	const TArray<FHGPlacedFurniture>& Layout = GI->GetShopLayout();
	FString Text = FString::Printf(TEXT("가구 %d개"), Layout.Num());
	for (int32 i = 0; i < Layout.Num(); ++i)
	{
		const FHGPlacedFurniture& Item = Layout[i];
		const AHGFurniture* Default = Item.Class ? Cast<AHGFurniture>(Item.Class->GetDefaultObject()) : nullptr;
		Text += FString::Printf(TEXT("\n  %d) %s (%.0f, %.0f) 방향 %.0f"), i + 1,
			Default ? *Default->DisplayName.ToString() : TEXT("?"), Item.Location.X, Item.Location.Y, Item.Yaw);
	}
	return Text;
}

FString UHGFurniturePlacer::DescribeCursor() const
{
	if (!bBuilding)
	{
		return TEXT("배치 모드 아님");
	}
	const FString GhostText = (Ghost && !Ghost->IsHidden())
		? FString::Printf(TEXT("%s 유령 (%.0f, %.0f) 방향 %.0f %s"), *GetHeldName().ToString(),
			Ghost->GetActorLocation().X, Ghost->GetActorLocation().Y, HeldYaw,
			bGhostValid ? TEXT("놓을 수 있음") : *FString::Printf(TEXT("놓을 수 없음(%s)"), *LastRejectReason))
		: FString(TEXT("유령 숨김"));
	return FString::Printf(TEXT("%s, 가리킨 가구 %s, 옮기는 중 %s"), *GhostText,
		Hovered ? *Hovered->DisplayName.ToString() : TEXT("없음"), bMoving ? TEXT("예") : TEXT("아니오"));
}
