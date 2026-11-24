#include "Debug/HGVoxelShapes.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Debug/HGPlaceholderPaint.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// 복셀 묶음 열쇠: 무리, 발광 여부, 색이 같으면 같은 인스턴스 메시에 넣는다.
	struct FVoxelGroupKey
	{
		int32 Group;
		bool bGlow;
		uint32 Color;
		int32 Shade;
		bool operator==(const FVoxelGroupKey& Other) const { return Group == Other.Group && bGlow == Other.bGlow && Color == Other.Color && Shade == Other.Shade; }
		friend uint32 GetTypeHash(const FVoxelGroupKey& Key) { return HashCombine(HashCombine(HashCombine(::GetTypeHash(Key.Group), ::GetTypeHash(Key.bGlow)), Key.Color), ::GetTypeHash(Key.Shade)); }
	};

	TAutoConsoleVariable<int32> CVarVoxel(
		TEXT("hg.Voxel"), 1,
		TEXT("복셀 그림체. 1 = 복셀(기본), 0 = 매끈한 기본 도형(비교용). 가구는 HGVoxel 명령으로 바로 다시 만들고, 손님/사장님은 새로 나타날 때 바뀐다."),
		ECVF_Default);

	bool IsInside(const FHGVoxelShape& S, const FVector& Half, const FVector& D)
	{
		if (S.Shape == TEXT("Cube"))
		{
			return FMath::Abs(D.X) <= Half.X && FMath::Abs(D.Y) <= Half.Y && FMath::Abs(D.Z) <= Half.Z;
		}
		if (S.Shape == TEXT("Cylinder"))
		{
			return FMath::Square(D.X / Half.X) + FMath::Square(D.Y / Half.Y) <= 1.f && FMath::Abs(D.Z) <= Half.Z;
		}
		return FMath::Square(D.X / Half.X) + FMath::Square(D.Y / Half.Y) + FMath::Square(D.Z / Half.Z) <= 1.f;
	}
}

bool HGVoxel::IsEnabled()
{
	return CVarVoxel.GetValueOnGameThread() != 0;
}

TArray<FHGVoxelGroup> HGVoxel::Build(AActor* Owner, USceneComponent* Parent, const TArray<FHGVoxelShape>& Shapes,
	float VoxelSize, bool bCastShadow, bool bClampAboveZero)
{
	// 칸 -> 그 칸을 마지막으로 칠한 도형 번호. 나중 도형이 덮어쓴다.
	TMap<FIntVector, int32> Cells;
	for (int32 Index = 0; Index < Shapes.Num(); ++Index)
	{
		const FHGVoxelShape& S = Shapes[Index];
		const FVector Half(FMath::Max(S.Scale.X * 50.f, VoxelSize * 0.5f), FMath::Max(S.Scale.Y * 50.f, VoxelSize * 0.5f), FMath::Max(S.Scale.Z * 50.f, VoxelSize * 0.5f));
		FBox Bounds(ForceInit);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Local((Corner & 1) ? Half.X : -Half.X, (Corner & 2) ? Half.Y : -Half.Y, (Corner & 4) ? Half.Z : -Half.Z);
			Bounds += S.Location + S.Rotation.RotateVector(Local);
		}
		const FIntVector Min(FMath::FloorToInt(Bounds.Min.X / VoxelSize), FMath::FloorToInt(Bounds.Min.Y / VoxelSize), FMath::FloorToInt(Bounds.Min.Z / VoxelSize));
		const FIntVector Max(FMath::FloorToInt(Bounds.Max.X / VoxelSize), FMath::FloorToInt(Bounds.Max.Y / VoxelSize), FMath::FloorToInt(Bounds.Max.Z / VoxelSize));
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
			{
				for (int32 Z = bClampAboveZero ? FMath::Max(Min.Z, 0) : Min.Z; Z <= Max.Z; ++Z)
				{
					const FVector Center((X + 0.5f) * VoxelSize, (Y + 0.5f) * VoxelSize, (Z + 0.5f) * VoxelSize);
					if (IsInside(S, Half, S.Rotation.UnrotateVector(Center - S.Location)))
					{
						Cells.Add(FIntVector(X, Y, Z), Index);
					}
				}
			}
		}
	}

	// (무리, 발광, 색, 명암)이 같은 칸끼리 묶는다.
	// 명암: 칸 좌표로 정한 세 단계 밝기(0.9 / 1.0 / 1.1배). 같은 부품이어도 칸마다 색이 조금씩 달라서 도트 그림 같은 얼룩 질감이 난다
	// (참고: 니발리스의 복셀 바닥과 벽). 좌표로 정하므로 다시 만들어도 같은 무늬다.
	static const float ShadeScale[] = { 0.9f, 1.0f, 1.1f };
	TMap<FVoxelGroupKey, TArray<FIntVector>> ByKey;
	TMap<FVoxelGroupKey, FLinearColor> ColorOf;
	for (const TPair<FIntVector, int32>& Cell : Cells)
	{
		const FHGVoxelShape& S = Shapes[Cell.Value];
		const int32 Shade = static_cast<int32>(GetTypeHash(Cell.Key) % 3u);
		const FVoxelGroupKey Key{ S.Group, S.bGlow, S.Color.ToFColor(false).DWColor(), Shade };
		ByKey.FindOrAdd(Key).Add(Cell.Key);
		FLinearColor Shaded = S.Color * ShadeScale[Shade];
		Shaded.A = 1.f;
		ColorOf.Add(Key, Shaded);
	}

	TArray<FHGVoxelGroup> Result;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	const float Scale = VoxelSize / 100.f;
	for (const TPair<FVoxelGroupKey, TArray<FIntVector>>& Group : ByKey)
	{
		UInstancedStaticMeshComponent* Voxels = NewObject<UInstancedStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Voxels->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Voxels->SetupAttachment(Parent);
		Voxels->SetStaticMesh(Cube);
		Voxels->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Voxels->SetCanEverAffectNavigation(false);
		Voxels->SetCastShadow(bCastShadow);
		Voxels->RegisterComponent();
		for (const FIntVector& Cell : Group.Value)
		{
			const FVector Location((Cell.X + 0.5f) * VoxelSize, (Cell.Y + 0.5f) * VoxelSize, (Cell.Z + 0.5f) * VoxelSize);
			Voxels->AddInstance(FTransform(FRotator::ZeroRotator, Location, FVector(Scale)), false);
		}
		const FLinearColor Color = ColorOf[Group.Key];
		if (Group.Key.bGlow)
		{
			PaintGlow(Voxels, Color, 0.f);
		}
		else
		{
			HGPlaceholder::Paint(Voxels, Color);
		}
		Result.Add(FHGVoxelGroup{ Voxels, Color, Group.Key.Group, Group.Key.bGlow });
	}
	return Result;
}

UMaterialInstanceDynamic* HGVoxel::PaintGlow(UMeshComponent* Component, const FLinearColor& Color, float Glow)
{
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/HG/Materials/M_HG_Glow.M_HG_Glow"));
	if (!Parent || !Component)
	{
		return HGPlaceholder::Paint(Component, Color);
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, Component);
	Mid->SetVectorParameterValue(TEXT("Color"), Color);
	Mid->SetScalarParameterValue(TEXT("Glow"), Glow);
	Component->SetMaterial(0, Mid);
	return Mid;
}
