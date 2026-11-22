#pragma once

#include "CoreMinimal.h"

class UMeshComponent;
class UMaterialInstanceDynamic;

namespace HGPlaceholder
{
	/**
	 * 자리 표시용 도형에 단색을 칠한다.
	 *
	 * 엔진 기본 도형(Cube, Sphere 등)에 실제로 붙어 오는 머티리얼은 색 파라미터가 없는 WorldGridMaterial이다.
	 * 그래서 "Color" 벡터 파라미터를 가진 /Engine/BasicShapes/BasicShapeMaterial을 직접 불러와
	 * 그것을 부모로 하는 동적 머티리얼 인스턴스를 만든 뒤 컴포넌트에 붙인다.
	 * 동적 인스턴스는 부모 셰이더를 공유하고 파라미터 값만 따로 가지므로, 색마다 셰이더를 새로 컴파일하지 않는다.
	 */
	HUOGUOHUOGUO_API UMaterialInstanceDynamic* Paint(UMeshComponent* Component, const FLinearColor& Color);
}
