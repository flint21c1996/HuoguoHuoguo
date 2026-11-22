#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/HGWorldLayout.h"
#include "HGGreyboxBuilder.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

/**
 * 개발용 회색 박스 월드 생성기.
 *
 * 빈 레벨에서 게임을 시작해도 바로 테스트할 수 있도록, 엔진 기본 도형(상자, 원기둥, 구)으로
 * 가게 한 칸과 소행성 착륙지, 조명을 코드로 만든다. 레벨 에셋(.umap)을 편집하지 않고도
 * 배치를 바꿀 수 있게 하려는 것이며, 정식 레벨이 생기면 이 액터는 쓰지 않게 된다.
 *
 * 좌표 기준 (언리얼 단위, 1유닛 = 1cm):
 *   가게 바닥 윗면이 Z=0이고 가게 중심이 원점이다. 가게 크기는 12m x 10m.
 *   소행성 착륙지는 가게에서 멀리 떨어진 AsteroidOrigin에 둔다. 두 장소는 같은 월드에 있지만
 *   서로 보이지 않을 만큼 떨어져 있어서, 이동 발판의 암전 연출로 "다른 장소"처럼 느끼게 한다.
 */
UCLASS()
class HUOGUOHUOGUO_API AHGGreyboxBuilder : public AActor
{
	GENERATED_BODY()

public:
	AHGGreyboxBuilder();

	// 가게, 소행성, 조명을 만들고 각 모드의 시작 위치를 돌려준다.
	FHGWorldLayout Build();

protected:
	// 소행성 착륙지의 중심. 가게와 충분히 멀리 둔다.
	UPROPERTY(EditAnywhere, Category = "Greybox")
	FVector AsteroidOrigin = FVector(40000.f, 0.f, -6000.f);

	// 레벨에 이미 조명이 있으면 false로 두어 중복 생성을 막는다.
	UPROPERTY(EditAnywhere, Category = "Greybox")
	bool bSpawnLighting = true;

private:
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;

	// 도형 하나를 이 액터의 컴포넌트로 추가한다. Scale 1 = 1m 크기.
	UStaticMeshComponent* AddPiece(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
		const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator, bool bCollide = true);

	void BuildLighting();
	FHGWorldLayout BuildShop();
	void BuildAsteroid();
	void SpawnTravelPads(const FHGWorldLayout& Layout);

	// 탐험 캐릭터가 소행성에 도착하는 위치.
	FTransform AsteroidArrival() const;
};
