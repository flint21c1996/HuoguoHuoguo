# Player - 경영 카메라 Pawn과 탐험 캐릭터

> 경로: `Source/HuoguoHuoguo/Player/` · 마지막 갱신: 2026-11-23

## 역할

플레이어가 조종하는 두 Pawn. 컨트롤러가 모드에 따라 둘 중 하나에 빙의한다. 두 Pawn은 맵에 동시에 존재한다.

## 파일

### HGShopCameraPawn (.h/.cpp)
몸체 없이 초점(FocusRoot)과 스프링 암 + 카메라만 있는 경영용 Pawn.
- 가짜 아이소메트릭: FOV 15도, Pitch -35도, 암 길이 기본 6000 (3000~11000), Yaw 45 + 90도 단위.
- `AddPanInput`: 카메라 수평 방향 기준 초점 이동. 속도는 암 길이에 비례. 초점은 `PanRadius` 원 안으로 제한.
- `AddZoomInput`: 암 길이를 비율(12%)로 바꾼다. `RotateStep(±1)`: 목표 Yaw를 90도 돌린다.
- `SnapTo(Focus, Yaw, PanRadius)`: 즉시 맞춤 (맵 시작). `FocusOn(Focus)`: 부드럽게 이동 (탐험에서 돌아올 때).
- `Tick`: Yaw, 암 길이, 초점을 목표값으로 보간 (`InterpSpeed` 8).
- `GetViewYaw()`: 탐험 카메라가 같은 방향을 이어받는 데 쓴다.

### HGExplorerCharacter (.h/.cpp)
사장님. 3인칭 캐릭터.
- 캡슐 반지름 40, 반높이 92. 몸은 이동 방향으로 돈다(`bOrientRotationToMovement`), 카메라는 컨트롤러 회전을 따른다.
- 이동: MaxWalkSpeed 520, JumpZVelocity 620, AirControl 0.45, RotationRate (720, 720, 720).
  Pitch/Roll 회전 속도는 행성에서 몸을 새 "위"에 맞춰 세우기 위해 필요하다.
- 카메라: 스프링 암 420, 충돌 검사, 소켓 오프셋 Z 60, 카메라 랙, FOV 75.
- 외형: 엔진 기본 도형 자리 표시 (원기둥 몸통 빨강, 구 머리, 헬멧 구 청록). `SetSuitVisible`로 헬멧 표시.
- `Move(Input)`: 시점을 중력 좌표계로 옮겨 Yaw만 남기고 앞/오른쪽을 구한다. 정거장에서는 좌표계가 항등이라 일반 3인칭과 같다.
- `Look(Input)`: Yaw/Pitch 입력 (Pitch는 부호 반전).
- 구형 중력
  - `SetPlanetGravity(Center, GravityScale)`: 켜면 매 Tick `UpdatePlanetGravity()`가 `CharacterMovement->SetGravityDirection(Center 방향)`을 부른다 (UE 5.4+ 임의 중력 지원).
  - `GetGravityFrame()`: "중력 기준 위 = 지표 바깥"인 회전. 매 프레임 변화량만 이어 붙이는 평행 이동 방식이라 남극에서도 축이 튀지 않는다.
  - CharacterMovement는 캐릭터 Tick 뒤에 틱하므로 바뀐 중력이 같은 프레임에 쓰인다.

## 흐름 / 규칙

- 경영 ↔ 탐험 전환 연출은 컨트롤러의 뷰 블렌드다. Pawn은 카메라 값만 제공한다.
- 스켈레탈 메시가 생기면 기본 `Mesh`에 넣고 자리 표시 컴포넌트를 숨긴다.

## 알려진 문제와 다음 할 일

- 실제 마우스 조작감, 남극 근처 카메라 안정성은 사람이 직접 플레이해서 확인하지 않았다 (스모크 테스트로는 적도 너머까지 걷기만 확인).
- 경영 카메라를 돌리면 정거장 벽이 시야를 가린다. 벽 자동 낮추기가 필요하다.
