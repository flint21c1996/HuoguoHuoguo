# Player - 사장님 캐릭터 공통 몸체

> 경로: `Source/HuoguoHuoguo/Player/` · 마지막 갱신: 2026-11-23

## 역할

경영 파트와 탐험 파트의 사장님이 함께 쓰는 몸체. 카메라와 중력처럼 파트마다 다른 것은 `Shop/`, `Explore/`의 하위 클래스가 정한다.

## 파일

### HGBossCharacter (.h/.cpp)
추상 클래스 (`AHGShopBossCharacter`, `AHGExplorerCharacter`가 상속).
- 캡슐 반지름 40, 반높이 92. 몸은 이동 방향으로 돈다(`bOrientRotationToMovement`), 컨트롤러 회전은 몸에 쓰지 않는다.
- 이동: MaxWalkSpeed 520, JumpZVelocity 620, AirControl 0.45, RotationRate (720, 720, 720).
  Pitch/Roll 회전 속도는 행성에서 몸을 새 "위"에 맞춰 세우기 위해 필요하다 (평면에서는 영향 없음).
- 외형: 엔진 기본 도형 자리 표시 (원기둥 몸통 빨강, 구 머리, 헬멧 구 청록). `SetSuitVisible`로 헬멧 표시.
  - 외형 축: 캡슐 가운데 VisualPivot(몸 전체 회전, 예: 3단 점프 공중제비) 아래 발 위치 VisualRoot(크기 = 눌림/늘어남, 발이 땅에 붙은 채로). 부품과 복셀은 모두 VisualRoot 아래에 FeetOffset(92)만큼 올려 붙인다.
  - 복셀 그림체(`hg.Voxel`, 기본)면 `BeginPlay`에서 같은 모양에 눈 두 개를 더해 복셀로 만들고 자리 표시 도형을 숨긴다 (`BuildVoxelBody`). 헬멧은 따로 복셀화해서 헬멧을 숨겨도 머리가 남는다.
- `Move(Input)`: 카메라(컨트롤러) 회전을 `GetMoveFrame()` 좌표계로 옮겨 Yaw만 남기고 앞/오른쪽을 구한다.
  카메라를 어느 방향으로 돌려도 W = 화면 위쪽(카메라가 보는 쪽).
- `GetMoveFrame()`: 기본 항등 회전. 구형 중력을 쓰는 하위 클래스가 바꾼다.

## 흐름 / 규칙

- 두 파트에 공통인 외형/이동 규칙만 여기에 둔다.
- 스켈레탈 메시가 생기면 기본 `Mesh`에 넣고 자리 표시 컴포넌트를 숨긴다.

## 알려진 문제와 다음 할 일

- 캐릭터 에셋, 애니메이션 없음.
