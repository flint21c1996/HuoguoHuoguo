// 폐우주선 (AHGPlanetGenerator의 일부). 행성 대신 우주에 버려진 우주선 한 척을 만든다.
// 땅 지형, 소행성 같은 행성 쪽 코드와 섞이지 않게 파일을 나눴다.

#include "World/Planet/HGPlanetGenerator.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Debug/HGVoxelShapes.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/HGGameInstance.h"
#include "Game/HGIngredientTypes.h"
#include "HuoguoHuoguo.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "Shop/Furniture/HGFurnitureData.h"
#include "World/Travel/HGTravelPad.h"

namespace
{
	// 설계도 단말의 "썼음" 기록은 재료 캔 기록(행성 시드 + 번호)을 같이 쓴다. 재료 번호와 겹치지 않게 큰 수를 더한다.
	constexpr int32 BlueprintRecordBase = 100000;

	// 방 하나 (우주선 기준 좌표, cm). 바닥 Z = 0, X 구간과 Y 반폭, 천장 높이.
	struct FRoom
	{
		const TCHAR* Name;
		float X0;
		float X1;
		float HalfY;
		float Height;
		FLinearColor LightColor;
	};
}

void AHGPlanetGenerator::BuildDerelict()
{
	FRandomStream Rng(static_cast<int32>(HashCombine(GetTypeHash(Planet.Seed), 0xde7e1c7u)));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));

	// 우주선은 살짝 기울어 떠 있다 (버려져 천천히 돈 듯한 자세). 안쪽 인공 중력은 이 기울어진 바닥 쪽으로 끈다.
	ShipFrame = FTransform(FRotator(Rng.FRandRange(-8.f, 8.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(8.f, 18.f)), FVector::ZeroVector);
	USceneComponent* ShipRoot = NewObject<USceneComponent>(this, TEXT("Ship"), RF_Transient);
	ShipRoot->SetupAttachment(Terrain);
	ShipRoot->SetRelativeTransform(ShipFrame);
	ShipRoot->RegisterComponent();
	const FTransform ShipToWorld = ShipRoot->GetComponentTransform();

	// 상자 묶음: 이름마다 색 하나, 인스턴스 메시 하나. 충돌이 있는 묶음(선체, 바닥, 벽, 가구)과 없는 묶음(장식, 불빛, 잔해)을 나눈다.
	struct FBatch
	{
		FLinearColor Color;
		bool bCollide = true;
		float Glow = 0.f; // 0보다 크면 발광 머티리얼
		TArray<FTransform> Boxes;
	};
	TMap<FName, FBatch> Batches;
	auto Batch = [&Batches](const TCHAR* Name, const FLinearColor& Color, bool bCollide, float Glow = 0.f)
	{
		FBatch& B = Batches.FindOrAdd(Name);
		B.Color = Color;
		B.bCollide = bCollide;
		B.Glow = Glow;
	};
	Batch(TEXT("Hull"), FLinearColor(0.42f, 0.46f, 0.55f), true);
	Batch(TEXT("HullDark"), FLinearColor(0.24f, 0.26f, 0.31f), true);
	Batch(TEXT("Rust"), FLinearColor(0.46f, 0.26f, 0.15f), true);
	Batch(TEXT("Floor"), FLinearColor(0.30f, 0.30f, 0.34f), true);
	Batch(TEXT("Panel"), FLinearColor(0.12f, 0.20f, 0.45f), true);
	Batch(TEXT("Furniture"), FLinearColor(0.55f, 0.50f, 0.42f), true);
	Batch(TEXT("Crate"), FLinearColor(0.62f, 0.55f, 0.30f), true);
	Batch(TEXT("Debris"), FLinearColor(0.38f, 0.40f, 0.46f), false);
	Batch(TEXT("DoorGlow"), FLinearColor(0.3f, 1.f, 0.9f), false, 6.f);     // 문틀 (산소 청록)
	Batch(TEXT("EmergencyGlow"), FLinearColor(1.f, 0.25f, 0.15f), false, 5.f); // 비상등 (빨강)
	Batch(TEXT("ScreenGlow"), FLinearColor(1.f, 0.75f, 0.2f), false, 6.f);  // 설계도 단말 화면 (금색)
	Batch(TEXT("EngineGlow"), FLinearColor(1.f, 0.45f, 0.1f), false, 2.f);  // 꺼져 가는 엔진
	// 우주선 기준 위치 Center, 크기 Size(cm)의 상자. Rot으로 상자만 돌린다.
	auto Box = [&Batches](const TCHAR* Name, const FVector& Center, const FVector& Size, const FRotator& Rot = FRotator::ZeroRotator)
	{
		Batches.FindChecked(Name).Boxes.Add(FTransform(Rot, Center, Size / 100.f));
	};

	// ---- 방 (안을 걸어 다닌다) ----
	// X 방향으로 이어진 방 넷: 에어록 -> 복도 -> 주방 -> 창고. 이웃한 방 사이 벽에는 문 구멍을 낸다.
	const float T = 25.f;           // 벽 두께
	const float DoorHalf = 80.f;    // 문 반폭
	const float DoorH = 240.f;      // 문 높이
	const FRoom Rooms[] = {
		{ TEXT("Airlock"), -200.f, 200.f, 200.f, 300.f, FLinearColor(0.4f, 1.f, 0.9f) },
		{ TEXT("Corridor"), 200.f, 1000.f, 150.f, 280.f, FLinearColor(1.f, 0.35f, 0.25f) },
		{ TEXT("Galley"), 1000.f, 1600.f, 300.f, 320.f, FLinearColor(1.f, 0.8f, 0.55f) },
		{ TEXT("Storage"), 1600.f, 2200.f, 250.f, 300.f, FLinearColor(0.7f, 0.8f, 1.f) },
	};
	const int32 RoomCount = UE_ARRAY_COUNT(Rooms);
	for (const FRoom& R : Rooms)
	{
		const float CX = (R.X0 + R.X1) * 0.5f;
		const float LX = R.X1 - R.X0;
		Box(TEXT("Floor"), FVector(CX, 0.f, -T * 0.5f), FVector(LX + 2.f * T, 2.f * R.HalfY + 2.f * T, T));
		Box(TEXT("Hull"), FVector(CX, 0.f, R.Height + T * 0.5f), FVector(LX + 2.f * T, 2.f * R.HalfY + 2.f * T, T));
		for (const float Side : { -1.f, 1.f })
		{
			Box(TEXT("Hull"), FVector(CX, Side * (R.HalfY + T * 0.5f), R.Height * 0.5f), FVector(LX, T, R.Height));
		}
	}
	// X 방향 벽: 우주선 양 끝(막힘)과 방 사이(문 구멍). 이웃한 두 방 중 넓고 높은 쪽에 맞춰 틈이 없게 한다.
	for (int32 i = 0; i <= RoomCount; ++i)
	{
		const FRoom& A = Rooms[FMath::Max(i - 1, 0)];
		const FRoom& B = Rooms[FMath::Min(i, RoomCount - 1)];
		const float X = i < RoomCount ? B.X0 : A.X1;
		const float HalfY = FMath::Max(A.HalfY, B.HalfY) + T;
		const float H = FMath::Max(A.Height, B.Height);
		const bool bDoor = i > 0 && i < RoomCount;
		if (!bDoor)
		{
			Box(TEXT("Hull"), FVector(X, 0.f, H * 0.5f), FVector(T, 2.f * HalfY, H));
			continue;
		}
		const float SideLen = HalfY - DoorHalf;
		for (const float Side : { -1.f, 1.f })
		{
			Box(TEXT("Hull"), FVector(X, Side * (DoorHalf + SideLen * 0.5f), H * 0.5f), FVector(T, SideLen, H));
		}
		Box(TEXT("Hull"), FVector(X, 0.f, (DoorH + H) * 0.5f), FVector(T, 2.f * DoorHalf, H - DoorH));
		// 문틀 불빛 (안에서 다음 방으로 가는 길이 보이게)
		Box(TEXT("DoorGlow"), FVector(X, 0.f, DoorH + 6.f), FVector(T + 6.f, 2.f * DoorHalf, 8.f));
	}

	// 방 안 소품과 조명. 조명은 방마다 천장 가운데 점광 하나 (해는 천장에 가려 안이 어둡다).
	for (const FRoom& R : Rooms)
	{
		const float CX = (R.X0 + R.X1) * 0.5f;
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
		Light->SetupAttachment(ShipRoot);
		Light->SetRelativeLocation(FVector(CX, 0.f, R.Height - 40.f));
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(12.f);
		Light->SetLightColor(R.LightColor);
		Light->SetAttenuationRadius(FMath::Max(R.X1 - R.X0, 2.f * R.HalfY) * 1.2f);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		Box(TEXT("EmergencyGlow"), FVector(CX, 0.f, R.Height - 6.f), FVector(60.f, 20.f, 8.f));
	}
	// 복도 바닥 양옆 비상 유도등
	for (float X = 260.f; X < 1000.f; X += 120.f)
	{
		for (const float Side : { -1.f, 1.f })
		{
			Box(TEXT("EmergencyGlow"), FVector(X, Side * 135.f, 4.f), FVector(30.f, 8.f, 6.f));
		}
	}
	// 주방: 식탁과 조리대 (훠궈 가게 사장님이 들를 만한 곳)
	Box(TEXT("Furniture"), FVector(1300.f, 0.f, 75.f), FVector(220.f, 120.f, 10.f));
	for (const float Y : { -40.f, 40.f })
	{
		Box(TEXT("Furniture"), FVector(1300.f + Y * 2.f, Y, 35.f), FVector(10.f, 10.f, 70.f));
	}
	Box(TEXT("Furniture"), FVector(1300.f, 255.f, 50.f), FVector(500.f, 70.f, 100.f));
	// 창고: 짐 상자 (일부는 재료가 든 상자, 아래 재료 배치에서 위에 표식을 놓는다)
	TArray<FVector> CrateTops;
	for (int32 k = 0; k < 6; ++k)
	{
		const FVector C(Rng.FRandRange(1680.f, 2120.f), Rng.FRandRange(-190.f, 190.f), 0.f);
		const float S = Rng.FRandRange(60.f, 90.f);
		Box(TEXT("Crate"), C + FVector(0.f, 0.f, S * 0.5f), FVector(S), FRotator(0.f, Rng.FRandRange(0.f, 40.f), 0.f));
		CrateTops.Add(C + FVector(0.f, 0.f, S));
	}

	// 설계도 단말: 주방과 창고에 하나씩 (금색 화면). 잠긴 가구 중에서 시드로 고른다.
	const FVector Terminals[] = { FVector(1100.f, -270.f, 0.f), FVector(2150.f, 200.f, 0.f) };
	TArray<FName> Locked;
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (const UDataTable* FurnitureTable = HGFurnitureData::LoadTable())
	{
		for (const FName& Id : FurnitureTable->GetRowNames())
		{
			const FHGFurnitureRow* Row = HGFurnitureData::FindRow(Id);
			if (Row && GI && !HGFurnitureUnlock::IsUnlocked(GI, Id, *Row))
			{
				Locked.Add(Id);
			}
		}
	}
	Interactables.Reset();
	for (int32 k = 0; k < UE_ARRAY_COUNT(Terminals); ++k)
	{
		const FVector Base = Terminals[k];
		Box(TEXT("Panel"), Base + FVector(0.f, 0.f, 60.f), FVector(60.f, 40.f, 120.f));
		const float Face = Base.Y < 0.f ? 1.f : -1.f; // 방 가운데 쪽
		Box(TEXT("ScreenGlow"), Base + FVector(0.f, Face * 21.f, 95.f), FVector(44.f, 4.f, 34.f));
		if (Locked.IsEmpty())
		{
			continue;
		}
		FHGInteractable Terminal;
		Terminal.Type = EHGInteractType::Blueprint;
		Terminal.Location = GetActorLocation() + ShipToWorld.TransformVectorNoScale(Base + FVector(0.f, Face * 90.f, 90.f));
		Terminal.Furniture = Locked[Rng.RandRange(0, Locked.Num() - 1)];
		Terminal.bUsed = GI && GI->IsMarkerHarvested(Planet.Seed, BlueprintRecordBase + Interactables.Num());
		Interactables.Add(Terminal);
	}

	// ---- 바깥 (떠다니며 본다) ----
	// 선체 갈비대: 300cm마다 방 단면을 한 바퀴 두르는 어두운 띠 (밖에서 보면 우주선 마디처럼 보인다).
	for (float X = -100.f; X < 2200.f; X += 300.f)
	{
		const FRoom* R = &Rooms[0];
		for (const FRoom& Room : Rooms)
		{
			if (X >= Room.X0 && X < Room.X1)
			{
				R = &Room;
			}
		}
		const float W = 2.f * R->HalfY + 2.f * T + 30.f;
		Box(TEXT("HullDark"), FVector(X, 0.f, R->Height + T + 12.f), FVector(40.f, W, 24.f));
		Box(TEXT("HullDark"), FVector(X, 0.f, -T - 12.f), FVector(40.f, W, 24.f));
		for (const float Side : { -1.f, 1.f })
		{
			Box(TEXT("HullDark"), FVector(X, Side * (R->HalfY + T + 12.f), R->Height * 0.5f), FVector(40.f, 24.f, R->Height + 2.f * T));
		}
	}
	// 녹슨 자국: 선체 위와 옆에 얇은 판
	for (int32 k = 0; k < 14; ++k)
	{
		const FRoom& R = Rooms[Rng.RandRange(0, RoomCount - 1)];
		const float X = Rng.FRandRange(R.X0 + 40.f, R.X1 - 40.f);
		const FVector Size(Rng.FRandRange(60.f, 180.f), Rng.FRandRange(40.f, 120.f), 6.f);
		if (Rng.FRand() < 0.5f)
		{
			Box(TEXT("Rust"), FVector(X, Rng.FRandRange(-R.HalfY, R.HalfY) * 0.7f, R.Height + T + 3.f), Size);
		}
		else
		{
			const float Side = Rng.FRand() < 0.5f ? -1.f : 1.f;
			Box(TEXT("Rust"), FVector(X, Side * (R.HalfY + T + 3.f), Rng.FRandRange(60.f, R.Height - 60.f)), FVector(Size.X, 6.f, Size.Y));
		}
	}
	// 조종석 (창고 앞, 안으로는 못 들어간다): 앞으로 갈수록 작아지는 상자 셋 + 어두운 창
	for (int32 k = 0; k < 3; ++k)
	{
		const float Shrink = 1.f - 0.22f * k;
		Box(TEXT("Hull"), FVector(2200.f + T + 90.f + k * 170.f, 0.f, 150.f), FVector(180.f, 520.f * Shrink, 300.f * Shrink));
	}
	Box(TEXT("Panel"), FVector(2730.f, 0.f, 190.f), FVector(30.f, 260.f, 80.f));
	// 엔진 (에어록 뒤): 큰 몸통 + 노즐 둘, 안쪽에 꺼져 가는 주황 불빛
	Box(TEXT("HullDark"), FVector(-200.f - T - 250.f, 0.f, 150.f), FVector(500.f, 380.f, 340.f));
	for (const float Y : { -100.f, 100.f })
	{
		Box(TEXT("Hull"), FVector(-200.f - T - 560.f, Y, 150.f), FVector(120.f, 150.f, 150.f));
		Box(TEXT("EngineGlow"), FVector(-200.f - T - 625.f, Y, 150.f), FVector(10.f, 100.f, 100.f));
	}
	// 태양광 날개: 복도 양옆. 한쪽은 부러져 비스듬히 매달려 있고, 떨어진 조각 하나가 근처에 떠 있다.
	Box(TEXT("HullDark"), FVector(600.f, -(150.f + T + 60.f), 140.f), FVector(60.f, 120.f, 30.f));
	Box(TEXT("Panel"), FVector(600.f, -(150.f + T + 120.f + 700.f), 140.f), FVector(500.f, 1400.f, 12.f));
	Box(TEXT("HullDark"), FVector(600.f, 150.f + T + 60.f, 140.f), FVector(60.f, 120.f, 30.f));
	Box(TEXT("Panel"), FVector(600.f, 150.f + T + 120.f + 300.f, 220.f), FVector(500.f, 650.f, 12.f), FRotator(0.f, 0.f, 22.f));
	Box(TEXT("Panel"), FVector(900.f, 1700.f, 700.f), FVector(480.f, 600.f, 12.f), FRotator(20.f, 35.f, 50.f));
	// 안테나 (주방 지붕)
	Box(TEXT("HullDark"), FVector(1300.f, 0.f, 320.f + T + 250.f), FVector(15.f, 15.f, 500.f));
	Box(TEXT("Hull"), FVector(1300.f, 0.f, 320.f + T + 500.f), FVector(120.f, 120.f, 20.f), FRotator(25.f, 0.f, 0.f));
	Box(TEXT("EmergencyGlow"), FVector(1300.f, 0.f, 320.f + T + 520.f), FVector(16.f, 16.f, 16.f));
	// 떠 있는 잔해 (충돌 없음)
	for (int32 k = 0; k < 30; ++k)
	{
		const FVector P(Rng.FRandRange(-1500.f, 3500.f), Rng.FRandRange(-1800.f, 1800.f), Rng.FRandRange(-900.f, 1200.f));
		if (FMath::Abs(P.Y) < 450.f && P.Z > -200.f && P.Z < 600.f && P.X > -900.f && P.X < 2900.f)
		{
			continue; // 우주선 몸통 안은 비운다
		}
		Box(TEXT("Debris"), P, FVector(Rng.FRandRange(20.f, 90.f), Rng.FRandRange(20.f, 90.f), Rng.FRandRange(8.f, 40.f)),
			FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f)));
	}

	// 바깥 에어록 문 (에어록 방의 -Y 벽): 어두운 문짝 + 청록 문틀. 안쪽에도 같은 자리에 나가는 문틀.
	const float OuterY = -(200.f + T);
	Box(TEXT("Panel"), FVector(0.f, OuterY - 6.f, 110.f), FVector(160.f, 12.f, 220.f));
	for (const float S : { -1.f, 1.f })
	{
		Box(TEXT("DoorGlow"), FVector(S * 88.f, OuterY - 10.f, 115.f), FVector(10.f, 10.f, 230.f));
		Box(TEXT("DoorGlow"), FVector(S * 88.f, -200.f + 6.f, 115.f), FVector(10.f, 10.f, 230.f));
	}
	Box(TEXT("DoorGlow"), FVector(0.f, OuterY - 10.f, 232.f), FVector(186.f, 10.f, 10.f));
	Box(TEXT("DoorGlow"), FVector(0.f, -200.f + 6.f, 232.f), FVector(186.f, 10.f, 10.f));
	{
		FHGInteractable Enter;
		Enter.Type = EHGInteractType::EnterShip;
		Enter.Location = GetActorLocation() + ShipToWorld.TransformVectorNoScale(FVector(0.f, OuterY - 120.f, 110.f));
		Enter.Destination = GetActorLocation() + ShipToWorld.TransformVectorNoScale(FVector(0.f, 0.f, 110.f));
		Interactables.Add(Enter);
		FHGInteractable Exit;
		Exit.Type = EHGInteractType::ExitShip;
		Exit.Location = GetActorLocation() + ShipToWorld.TransformVectorNoScale(FVector(0.f, -200.f + 90.f, 110.f));
		Exit.Destination = GetActorLocation() + ShipToWorld.TransformVectorNoScale(FVector(0.f, OuterY - 260.f, 140.f));
		Interactables.Add(Exit);
	}

	// 상자 묶음을 인스턴스 메시로
	int32 BoxCount = 0;
	for (TPair<FName, FBatch>& Pair : Batches)
	{
		if (Pair.Value.Boxes.IsEmpty())
		{
			continue;
		}
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("Ship_%s"), *Pair.Key.ToString()), RF_Transient);
		Ism->SetupAttachment(ShipRoot);
		Ism->SetStaticMesh(Cube);
		Ism->SetCanEverAffectNavigation(false);
		if (Pair.Value.bCollide)
		{
			Ism->SetCollisionProfileName(Terrain->GetCollisionProfileName());
		}
		else
		{
			Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Ism->SetCastShadow(false);
		}
		Ism->RegisterComponent();
		Ism->AddInstances(Pair.Value.Boxes, false, false);
		if (Pair.Value.Glow > 0.f)
		{
			HGVoxel::PaintGlow(Ism, Pair.Value.Color, Pair.Value.Glow);
		}
		else
		{
			HGPlaceholder::Paint(Ism, Pair.Value.Color);
		}
		BoxCount += Pair.Value.Boxes.Num();
	}

	// ---- 재료 ----
	// 바깥: 선체 위와 옆에 자란 재료 (이 시드 군계의 고향 재료를 반 정도). 안: 창고 상자 위의 얼어붙은 식료 (아무 재료나).
	// 번호는 ScatterProps가 만든 표식 뒤에 이어 붙인다 (폐우주선에서는 0개). 캔 기록과 시간대 규칙은 행성과 같다.
	int32 Outside = 0;
	int32 Inside = 0;
	const UDataTable* Table = GI ? GI->GetIngredientTable() : nullptr;
	if (Table && !IngredientIds.IsEmpty() && IngredientMarkers.Num() == IngredientIds.Num())
	{
		TArray<int32> NativeKinds;
		for (int32 Kind = 0; Kind < IngredientIds.Num(); ++Kind)
		{
			const FHGIngredientRow* Row = Table->FindRow<FHGIngredientRow>(IngredientIds[Kind], TEXT("BuildDerelict"));
			if (Row && Row->HomeBiome == Planet.Biome)
			{
				NativeKinds.Add(Kind);
			}
		}
		const EHGDayPhase Phase = GI->GetDayPhase();
		const FTransform ToWorld = Terrain->GetComponentTransform();
		auto AddMarker = [&](int32 Kind, const FVector& ShipLocal, const FVector& ShipUp)
		{
			const FVector Local = ShipFrame.TransformPosition(ShipLocal + ShipUp * 22.f);
			const FQuat Rot = FRotationMatrix::MakeFromZ(ShipFrame.TransformVectorNoScale(ShipUp)).ToQuat();
			const FTransform T2(Rot, Local, FVector(MarkerScale));
			const int32 Index = Markers.Num();
			FHGIngredientMarker& Marker = Markers.AddDefaulted_GetRef();
			Marker.Kind = Kind;
			Marker.Instance = IngredientMarkers[Kind]->AddInstance(T2);
			Marker.Location = ToWorld.TransformPosition(Local);
			const FHGIngredientRow* RowData = Table->FindRow<FHGIngredientRow>(IngredientIds[Kind], TEXT("BuildDerelict"));
			if (GI->IsMarkerHarvested(Planet.Seed, Index))
			{
				Marker.bHarvested = true;
				HideInstance(Index);
			}
			else if (RowData && !HGDay::IsAvailable(RowData->AvailableAt, Phase))
			{
				Marker.bOtherTime = true;
				HideInstance(Index);
			}
		};
		for (int32 k = 0; k < 10; ++k)
		{
			const int32 Kind = (!NativeKinds.IsEmpty() && Rng.FRand() < 0.5f) ? NativeKinds[Rng.RandRange(0, NativeKinds.Num() - 1)] : Rng.RandRange(0, IngredientIds.Num() - 1);
			const FRoom& R = Rooms[Rng.RandRange(0, RoomCount - 1)];
			const float X = Rng.FRandRange(R.X0 + 40.f, R.X1 - 40.f);
			if (Rng.FRand() < 0.5f)
			{
				AddMarker(Kind, FVector(X, Rng.FRandRange(-R.HalfY, R.HalfY) * 0.8f, R.Height + T), FVector::UpVector);
			}
			else
			{
				const float Side = Rng.FRand() < 0.5f ? -1.f : 1.f;
				AddMarker(Kind, FVector(X, Side * (R.HalfY + T), Rng.FRandRange(60.f, R.Height - 60.f)), FVector(0.f, Side, 0.f));
			}
			++Outside;
		}
		for (const FVector& Top : CrateTops)
		{
			if (Rng.FRand() < 0.7f)
			{
				AddMarker(Rng.RandRange(0, IngredientIds.Num() - 1), Top, FVector::UpVector);
				++Inside;
			}
		}
	}

	// ---- 시작 자리와 귀환 포드 ----
	// 에어록 문 쪽(-Y) 25m 밖, 우주선보다 조금 높이 떠서 우주선을 바라본다. 귀환 포드(정거장 귀환 발판)는 시작 자리 옆에 떠 있다.
	const FVector StartLocal(0.f, -2800.f, 400.f);
	const FVector StartWorld = GetActorLocation() + ShipToWorld.TransformVectorNoScale(StartLocal);
	const FVector ShipCenterWorld = GetActorLocation() + ShipToWorld.TransformVectorNoScale(FVector(1000.f, 0.f, 150.f));
	DerelictStart = FTransform(FRotationMatrix::MakeFromXZ(ShipCenterWorld - StartWorld, GetShipUp()).ToQuat(), StartWorld);
	if (!ReturnLevel.IsNull())
	{
		const FVector PadLocal(-500.f, -2700.f, 250.f);
		const FVector PadWorld = GetActorLocation() + ShipToWorld.TransformVectorNoScale(PadLocal);
		const FTransform PadTransform(ShipFrame.GetRotation(), PadWorld);
		AHGTravelPad* Pad = GetWorld()->SpawnActorDeferred<AHGTravelPad>(AHGTravelPad::StaticClass(), PadTransform, this);
		Pad->DestinationLevel = ReturnLevel;
		Pad->ArrivalTag = ReturnArrivalTag;
		Pad->DestinationName = ReturnName;
		Pad->PadColor = FLinearColor(0.95f, 0.62f, 0.15f);
		Pad->FinishSpawning(PadTransform);
		bHasReturnPad = true;
		ReturnPadLocation = PadWorld;
	}

	UE_LOG(LogHuoguo, Log, TEXT("[Derelict] 폐우주선: 방 %d개, 상자 %d개, 재료 바깥 %d / 안 %d, 설계도 단말 %d (잠긴 가구 %d종), 기울기 %s"),
		RoomCount, BoxCount, Outside, Inside, Interactables.Num() - 2, Locked.Num(), *ShipFrame.Rotator().ToCompactString());
}

TArray<FVector> AHGPlanetGenerator::GetShipOutline() const
{
	TArray<FVector> Corners;
	if (!Planet.bDerelict)
	{
		return Corners;
	}
	// 엔진 끝(-X)부터 조종석 끝(+X)까지, 가장 넓은 방 폭(+여유)의 직사각형. 높이는 방 가운데쯤.
	const FTransform ToWorld = ShipFrame * Terrain->GetComponentTransform();
	for (const FVector2D& C : { FVector2D(-850.0, -350.0), FVector2D(2760.0, -350.0), FVector2D(2760.0, 350.0), FVector2D(-850.0, 350.0) })
	{
		Corners.Add(ToWorld.TransformPosition(FVector(C.X, C.Y, 150.0)));
	}
	return Corners;
}

int32 AHGPlanetGenerator::FindNearestInteractable(const FVector& From, float MaxDistance) const
{
	int32 Best = INDEX_NONE;
	float BestDist = MaxDistance;
	for (int32 i = 0; i < Interactables.Num(); ++i)
	{
		if (Interactables[i].Type == EHGInteractType::Blueprint && Interactables[i].bUsed)
		{
			continue;
		}
		const float Dist = FVector::Dist(From, Interactables[i].Location);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = i;
		}
	}
	return Best;
}

FString AHGPlanetGenerator::DescribeInteractable(int32 Index) const
{
	if (!Interactables.IsValidIndex(Index))
	{
		return FString();
	}
	switch (Interactables[Index].Type)
	{
	case EHGInteractType::EnterShip:
		return TEXT("[E] 에어록을 열고 우주선 안으로 들어가기");
	case EHGInteractType::ExitShip:
		return TEXT("[E] 에어록을 열고 밖으로 나가기 (무중력)");
	default:
		return TEXT("[E] 설계도 단말 살펴보기");
	}
}

FString AHGPlanetGenerator::UseBlueprint(int32 Index)
{
	if (!Interactables.IsValidIndex(Index) || Interactables[Index].Type != EHGInteractType::Blueprint || Interactables[Index].bUsed)
	{
		return FString();
	}
	FHGInteractable& Terminal = Interactables[Index];
	Terminal.bUsed = true;
	UHGGameInstance* GI = GetGameInstance<UHGGameInstance>();
	if (GI)
	{
		GI->MarkHarvested(Planet.Seed, BlueprintRecordBase + Index);
	}
	const FHGFurnitureRow* Row = HGFurnitureData::FindRow(Terminal.Furniture);
	const FString Name = Row ? Row->DisplayName.ToString() : Terminal.Furniture.ToString();
	if (GI && Row && HGFurnitureUnlock::IsUnlocked(GI, Terminal.Furniture, *Row))
	{
		return FString::Printf(TEXT("설계도: %s (이미 열린 가구)"), *Name);
	}
	if (GI)
	{
		GI->UnlockFurniture(Terminal.Furniture);
		GI->AddBlueprintFound();
	}
	UE_LOG(LogHuoguo, Log, TEXT("[Derelict] 설계도 단말 %d: %s 해금"), Index, *Terminal.Furniture.ToString());
	return FString::Printf(TEXT("설계도 발견! 가게 가구 해금: %s"), *Name);
}
