#pragma once

#include "CoreMinimal.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class USceneComponent;

/**
 * 기본 도형 조합을 복셀(작은 정육면체)로 바꿔 그리는 도우미. 그림체가 "복셀 + 조명"으로 정해져서(기획서 "그림체")
 * 가구, 손님, 사장님이 같이 쓴다. 정식 아트(MagicaVoxel 모델 등)가 생기기 전까지의 자리 표시 생김새다.
 *
 * 쓰는 법: 도형(Cube / Cylinder / Sphere, 엔진 기본 도형과 같은 크기 규칙: Scale 1 = 1m, 중심이 원점)을 모아 Build에 넘긴다.
 * Build는 부모 컴포넌트 기준 좌표의 VoxelSize 칸마다 칸 가운데가 도형 안에 드는지 보고, 같은 무리(Group)와 색의 칸끼리
 * 인스턴스 메시 컴포넌트 하나로 그린다. 같은 칸은 나중 도형이 덮어쓴다 (냄비 위 국물, 얼굴 위 눈).
 * 칸마다 밝기를 세 단계(0.9 / 1.0 / 1.1배) 중 하나로 흔들어 얼룩 질감을 낸다 (칸 좌표로 정해서 늘 같은 무늬).
 * 얇은 도형(1cm 국물 등)이 사라지지 않게 도형의 반너비는 최소 반 칸으로 친다.
 */
struct FHGVoxelShape
{
	FString Shape;              // "Cube", "Cylinder", "Sphere"
	FVector Location = FVector::ZeroVector;
	FVector Scale = FVector::OneVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FLinearColor Color = FLinearColor::White;
	int32 Group = 0;            // 따로 켜고 끌 무리 (예: 가구 0 = 몸, 1 = 김, 2 = 발광)
	bool bGlow = false;         // 발광 머티리얼(M_HG_Glow)로 칠한다 (밤에 빛나는 등)
};

struct FHGVoxelGroup
{
	UInstancedStaticMeshComponent* Component = nullptr;
	FLinearColor Color = FLinearColor::White;
	int32 Group = 0;
	bool bGlow = false;
};

namespace HGVoxel
{
	// 복셀 한 칸 기본 크기(cm). 처음 10에서 니발리스처럼 잘게 보이도록 5로 줄였다 (칸 수는 8배, 같은 색끼리 묶어 그려서 감당한다).
	inline constexpr float DefaultSize = 5.f;

	// 복셀 그림체를 쓰는가 (콘솔 변수 hg.Voxel, 기본 1). 0이면 예전처럼 매끈한 기본 도형으로 그린다 (비교용).
	HUOGUOHUOGUO_API bool IsEnabled();

	/**
	 * 도형들을 복셀로 바꿔 Owner에 컴포넌트로 붙인다 (Parent 아래, 실행 중 생성, 맵에 저장 안 됨, 충돌 없음).
	 * bClampAboveZero면 Z < 0 칸은 버린다 (바닥 아래로 내려가지 않게, 가구용).
	 * 색은 칠해서 돌려준다 (발광 무리는 발광 머티리얼, 처음 밝기 0).
	 */
	HUOGUOHUOGUO_API TArray<FHGVoxelGroup> Build(AActor* Owner, USceneComponent* Parent, const TArray<FHGVoxelShape>& Shapes,
		float VoxelSize = DefaultSize, bool bCastShadow = true, bool bClampAboveZero = false);

	// 발광 머티리얼(/Game/HG/Materials/M_HG_Glow, 맵 생성 도구가 만든다)로 칠한다. 없으면 일반 자리 표시 색.
	// 파라미터: Color(색), Glow(발광 세기, 0이면 꺼짐).
	HUOGUOHUOGUO_API UMaterialInstanceDynamic* PaintGlow(class UMeshComponent* Component, const FLinearColor& Color, float Glow);
}
