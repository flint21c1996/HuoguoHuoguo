#include "Debug/HGPlaceholderPaint.h"

#include "Components/MeshComponent.h"
#include "HuoguoHuoguo.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UMaterialInstanceDynamic* HGPlaceholder::Paint(UMeshComponent* Component, const FLinearColor& Color)
{
	if (!Component)
	{
		return nullptr;
	}

	// 엔진 콘텐츠의 머티리얼은 게임 내내 메모리에 남아 있으므로 한 번만 불러와 재사용한다.
	// AddToRoot로 가비지 컬렉션 대상에서 빼서, 캐시해 둔 포인터가 무효가 되지 않게 한다.
	static UMaterialInterface* BaseMaterial = nullptr;
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BaseMaterial)
		{
			BaseMaterial->AddToRoot();
		}
		else
		{
			UE_LOG(LogHuoguo, Error, TEXT("BasicShapeMaterial을 불러오지 못했다. 자리 표시 도형이 기본 머티리얼로 보인다"));
			return nullptr;
		}
	}

	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
	Mid->SetVectorParameterValue(TEXT("Color"), Color);
	Component->SetMaterial(0, Mid);
	return Mid;
}
