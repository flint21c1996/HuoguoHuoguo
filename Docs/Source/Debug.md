# Debug - 개발용 도우미

> 경로: `Source/HuoguoHuoguo/Debug/` · 마지막 갱신: 2026-11-23

## 파일

### HGPlaceholderPaint (.h/.cpp)
- `HGPlaceholder::Paint(UMeshComponent*, FLinearColor)`: 자리 표시 도형에 단색을 칠한다.
- 엔진 기본 도형에 실제로 붙는 머티리얼은 파라미터가 없는 `WorldGridMaterial`이라서, `/Engine/BasicShapes/BasicShapeMaterial`을 직접 불러(AddToRoot) 동적 머티리얼 인스턴스를 만들고 `Color` 파라미터를 넣는다.
- 캐릭터 자리 표시, 이동 발판, 행성 지형/바위/재료가 쓴다.

## 알려진 문제와 다음 할 일

- 정식 아트 머티리얼이 생기면 사용처를 하나씩 바꾸고 이 도우미를 없앤다.
