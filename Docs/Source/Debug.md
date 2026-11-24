# Debug - 개발용 도우미

> 경로: `Source/HuoguoHuoguo/Debug/` · 마지막 갱신: 2026-11-24

## 파일

### HGPlaceholderPaint (.h/.cpp)
- `HGPlaceholder::Paint(UMeshComponent*, FLinearColor)`: 자리 표시 도형에 단색을 칠한다.
- 엔진 기본 도형에 실제로 붙는 머티리얼은 파라미터가 없는 `WorldGridMaterial`이라서, `/Engine/BasicShapes/BasicShapeMaterial`을 직접 불러(AddToRoot) 동적 머티리얼 인스턴스를 만들고 `Color` 파라미터를 넣는다.
- 캐릭터 자리 표시, 이동 발판, 행성 지형/바위/재료가 쓴다.

### HGVoxelShapes (.h/.cpp) - 복셀 그림체 도우미
그림체가 "복셀 + 조명"으로 정해져서(기획서 "그림체") 가구, 손님, 사장님이 같이 쓰는 자리 표시 생김새 도우미.
- `HGVoxel::IsEnabled`: 콘솔 변수 `hg.Voxel`(기본 1). 0이면 예전처럼 매끈한 기본 도형 (비교용, `HGVoxel 0` 명령).
- 칸 크기 `HGVoxel::DefaultSize` 5cm (처음 10cm, 니발리스처럼 잘게 보이도록 줄임). 칸마다 밝기를 칸 좌표로 정한 세 단계(0.9 / 1.0 / 1.1배) 중 하나로 흔들어 얼룩 질감을 낸다 (같은 색 묶음이 명암 셋으로 나뉜다).
- `HGVoxel::Build(Owner, Parent, Shapes, VoxelSize=5, bCastShadow, bClampAboveZero)`: `FHGVoxelShape`(Cube/Cylinder/Sphere, 엔진 기본 도형 크기 규칙, 색, 무리, 발광)들을 받아 부모 기준 10cm 칸마다 칸 가운데가 도형 안에 드는지 보고, (무리, 발광, 색)이 같은 칸끼리 `UInstancedStaticMeshComponent` 하나로 그린다. 같은 칸은 나중 도형이 덮는다. 얇은 도형은 반너비를 최소 반 칸으로 친다. 충돌 없음, 맵에 저장 안 됨.
  - 덮어쓰기 때문에 따로 켜고 끌 껍데기(사장님 헬멧처럼 다른 부품을 감싸는 것)는 Build를 따로 불러야 한다. 같이 넣으면 감싼 부품의 칸이 껍데기로 넘어가서 껍데기를 숨기면 안쪽까지 사라진다.
- `HGVoxel::PaintGlow`: 발광 머티리얼 `/Game/HG/Materials/M_HG_Glow`(Color, Glow)로 칠한다. 밤에 빛나는 등이 쓴다.
- 이것은 모델링도 셰이더도 아니다: 실행할 때 CPU가 도형 안의 칸을 세서 작은 정육면체를 인스턴스로 놓는 진짜 지오메트리다. 정식 아트는 MagicaVoxel 모델(.vox)을 메시로 가져오는 쪽을 생각한다.

## 알려진 문제와 다음 할 일

- 정식 아트 머티리얼이 생기면 사용처를 하나씩 바꾸고 이 도우미를 없앤다.
